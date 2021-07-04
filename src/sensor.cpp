/*
 * Copyright (C) 2019-2021 OpenBikeSensor Contributors
 * Contact: https://openbikesensor.org
 *
 * This file is part of the OpenBikeSensor firmware.
 *
 * The OpenBikeSensor firmware is free software: you can
 * redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * OpenBikeSensor firmware is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the OpenBikeSensor firmware.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include <rom/queue.h>
#include "sensor.h"
#include "FunctionalInterrupt.h"
/*
 * Sensor types:
 *  getLastDelayTillStartUs:
 *  - HC-SR04       = ~2900us / ~290us
 *  - JSN-SR04T-2.0 =  ~290us
 *
 *  getMaxDurationUs:
 *  - HC-SR04       =   70800us (102804µs!?)
 *  - JSN-SR04T-2.0 =  ~58200us
 *
 *  getMinDurationUs:
 *  - HC-SR04       =     82us
 *  - JSN-SR04T-2.0 =  ~1125us
 */

const uint16_t MAX_SENSOR_VALUE = 999;

static const uint16_t MIN_DISTANCE_MEASURED_CM =   2;
static const uint16_t MAX_DISTANCE_MEASURED_CM = 320; // candidate to check I could not get good readings above 300

static const uint32_t MIN_DURATION_MICRO_SEC = MIN_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER;
const uint32_t MAX_DURATION_MICRO_SEC = MAX_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER;

/* This time is maximum echo pin high time if the sensor gets no response
 * HC-SR04: observed 71ms, docu 60ms
 * JSN-SR04T: observed 58ms
 */
static const uint32_t MAX_TIMEOUT_MICRO_SEC = 75000;

/* To avoid that we receive a echo from a former measurement, we do not
 * start a new measurement within the given time from the start of the
 * former measurement of the opposite sensor.
 */
static const uint32_t SENSOR_QUIET_PERIOD_AFTER_OPPOSITE_START_MICRO_SEC = 50 * 1000;

/* The last end (echo goes to low) of a measurement must be this far
 * away before a new measurement is started.
 */
static const uint32_t SENSOR_QUIET_PERIOD_AFTER_END_MICRO_SEC = 10 * 1000;

/* The last start of a new measurement must be as long ago as given here
    away, until we start a new measurement.
   - With 35ms I could get stable readings down to 19cm (new sensor)
   - With 30ms I could get stable readings down to 25/35cm only (new sensor)
   - It looked fine with the old sensor for all values
 */
static const uint32_t SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC = 30 * 1000;

/* Value of HCSR04SensorInfo::end during an ongoing measurement. */
static const uint32_t MEASUREMENT_IN_PROGRESS = 0;

/* Some calculations:
 *
 * Assumption:
 *  Small car length: 4m (average 2017 4,4m, really smallest is 2.3m )
 *  Speed difference: 100km/h = 27.8m/s
 *  Max interesting distance: MAX_DISTANCE_MEASURED_CM = 250cm
 *
 * Measured:
 *  MAX_TIMEOUT_MICRO_SEC == ~ 75_000 micro seconds, if the sensor gets no echo
 *
 * Time of the overtake process:
 *  4m / 27.8m/s = 0.143 sec
 *
 * We need to have time for 3 measures to get the car
 *
 * 0.148 sec / 3 measures = 48 milli seconds pre measure = 48_000 micro_sec
 *
 * -> If we wait till MAX_TIMEOUT_MICRO_SEC (75ms) because the right
 *    sensor has no measure we are to slow. (3x75 = 225ms > 143ms)
 *
 *    If we only waif tor the primary (left) sensor we are good again,
 *    and so trigger one sensor while the other is still in "timeout-state"
 *    we save some time. Assuming 1st measure is just before the car appears
 *    besides the sensor:
 *
 *    75ms open sensor + 2x SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC (35ms) = 145ms which is
 *    close to the needed 148ms
 */

// Hack to get the pointers to the isr
static HCSR04SensorInfo * TOF_SENSOR[NUMBER_OF_TOF_SENSORS];

void HCSR04SensorManager::registerSensor(const HCSR04SensorInfo& sensorInfo, uint8_t idx) {
  if (idx >= NUMBER_OF_TOF_SENSORS) {
    log_e("Can not register sensor for index %d, only %d tof sensors supported", idx, NUMBER_OF_TOF_SENSORS);
    return;
  }
  m_sensors[idx] = sensorInfo;
  pinMode(sensorInfo.triggerPin, OUTPUT);
  pinMode(sensorInfo.echoPin, INPUT_PULLUP); // hint from https://youtu.be/xwsT-e1D9OY?t=354
  sensorValues[idx] = MAX_SENSOR_VALUE;
  m_sensors[idx].median = new Median<uint16_t>(5, MAX_SENSOR_VALUE);

  TOF_SENSOR[idx] = &m_sensors[idx];
  attachSensorInterrupt(idx);
}

static void IRAM_ATTR isr(HCSR04SensorInfo* const sensor) {
  // since the measurement of start and stop use the same interrupt
  // mechanism we should see a similar delay.
  if (sensor->end == MEASUREMENT_IN_PROGRESS) {
    const uint32_t now = micros();
    if (HIGH == digitalRead(sensor->echoPin)) {
      sensor->start = now;
    } else { // LOW
      sensor->end = now;
    }
  }
}

static void IRAM_ATTR isr0() {
  isr(TOF_SENSOR[0]);
}

static void IRAM_ATTR isr1() {
  isr(TOF_SENSOR[1]);
}

void HCSR04SensorManager::attachSensorInterrupt(uint8_t idx) {
  // bad bad bad ....
  if (idx == 0) {
    attachInterrupt(TOF_SENSOR[idx]->echoPin, isr0, CHANGE);
  } else {
    attachInterrupt(TOF_SENSOR[idx]->echoPin, isr1, CHANGE);
  }

  // a solution like below leads to crashes with:
  // Guru Meditation Error: Core  1 panic'ed (Cache disabled but cached memory region accessed)
  // Core 1 was running in ISR context:
  //   attachInterrupt(sensorInfo.echoPin,
  //                  std::bind(&isr, sensorInfo.echoPin, &sensorInfo.start, &sensorInfo.end), CHANGE);
}

void HCSR04SensorManager::detachInterrupts() {
  for (auto & sensor : m_sensors) {
    detachInterrupt(sensor.echoPin);
  }
}

void HCSR04SensorManager::attachInterrupts() {
  for (size_t idx = 0; idx < NUMBER_OF_TOF_SENSORS; ++idx) {
    attachSensorInterrupt(idx);
  }
}

void HCSR04SensorManager::reset() {
  for (auto & sensor : m_sensors) {
    sensor.minDistance = MAX_SENSOR_VALUE;
    memset(&(sensor.echoDurationMicroseconds), 0, sizeof(sensor.echoDurationMicroseconds));
    sensor.numberOfTriggers = 0;
  }
  lastReadingCount = 0;
  memset(&(startOffsetMilliseconds), 0, sizeof(startOffsetMilliseconds));
  activeSensor = primarySensor;
  startReadingMilliseconds = millis();
}

void HCSR04SensorManager::setOffsets(std::vector<uint16_t> offsets) {
  for (size_t idx = 0; idx < NUMBER_OF_TOF_SENSORS; ++idx) {
    if (idx < offsets.size()) {
      m_sensors[idx].offset = offsets[idx];
    } else {
      m_sensors[idx].offset = 0;
    }
  }
}

/* The primary sensor defines the measurement interval, we trigger a measurement if this
 * sensor is ready.
 */
void HCSR04SensorManager::setPrimarySensor(uint8_t idx) {
  primarySensor = idx;
}


/* Reads left sensor alternating to the right sensor, while
 * one sensor is used the other one has time to settle down.
 */
void HCSR04SensorManager::getDistances() {
  setSensorTriggersToLow();
  waitTillSensorIsReady(activeSensor);
  sendTriggerToSensor(activeSensor);
  startOffsetMilliseconds[lastReadingCount] = millisSince(startReadingMilliseconds);
  // spec says 10, there are reports that the JSN-SR04T-2.0 behaves better if we wait 20 microseconds.
  // I did not observe this but others might be affected so we spend this time ;)
  // https://wolles-elektronikkiste.de/hc-sr04-und-jsn-sr04t-2-0-abstandssensoren
  delayMicroseconds(20);
  setSensorTriggersToLow();

  waitForEchosOrTimeout(activeSensor);
  collectSensorResult(activeSensor);
  activeSensor++;
  if (activeSensor >= NUMBER_OF_TOF_SENSORS) {
    activeSensor = 0;
  }
  setNoMeasureDate(activeSensor);
  if (lastReadingCount < MAX_NUMBER_MEASUREMENTS_PER_INTERVAL) {
    lastReadingCount++;
  }
}

/* Polls for new readings, if sensors are not ready, the
 * method simply exits and returns false. If readings are available,
 * data is read and updated. If the primary sensor is ready for
 * new measurement, a fresh measurement is triggered.
 * Method returns true if new data (no timeout measurement) was
 * generated.
 */
bool HCSR04SensorManager::pollDistancesParallel() {
  bool newMeasurements = false;
  if (isReadyForStart(primarySensor)) {
    setSensorTriggersToLow();
    newMeasurements = collectSensorResults();
    if (isReadyForStart(1 - primarySensor)) {
      sendTriggerToSensor(1 - primarySensor);
    }
    sendTriggerToSensor(primarySensor);
    delayMicroseconds(20);
    setSensorTriggersToLow();
  }
  return newMeasurements;
}

uint16_t HCSR04SensorManager::getCurrentMeasureIndex() {
  return lastReadingCount - 1;
}

/* Wait till the given sensor is ready.  */
void HCSR04SensorManager::waitTillSensorIsReady(uint8_t sensorId) {
  while (!isReadyForStart(sensorId)) {
    yield();
  }
}

/* Start measurement with all sensors that are ready to measure, wait
 * if there is no ready sensor at all.
 */
void HCSR04SensorManager::sendTriggerToSensor(uint8_t sensorId) {
  HCSR04SensorInfo* sensor = &(m_sensors[sensorId]);
  updateStatistics(sensor);
  sensor->trigger = sensor->start = micros(); // will be updated with HIGH signal
  sensor->end = MEASUREMENT_IN_PROGRESS; // will be updated with LOW signal
  sensor->numberOfTriggers++;
  digitalWrite(sensor->triggerPin, HIGH);
}

boolean HCSR04SensorManager::isReadyForStart(uint8_t sensorId) {
  HCSR04SensorInfo* sensor = &m_sensors[sensorId];
  boolean ready = false;
  const uint32_t now = micros();
  const uint32_t start = sensor->start;
  const uint32_t end = sensor->end;
  if (digitalRead(sensor->echoPin) == LOW && end != MEASUREMENT_IN_PROGRESS) { // no measurement in flight or just finished
    const uint32_t startOther = m_sensors[1 - sensorId].start;
    if (   (microsBetween(now, end) > SENSOR_QUIET_PERIOD_AFTER_END_MICRO_SEC)
        && (microsBetween(now, start) > SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC)
        && (microsBetween(now, startOther) > SENSOR_QUIET_PERIOD_AFTER_OPPOSITE_START_MICRO_SEC)) {
      ready = true;
    }
  } else if (microsBetween(now, start) > 2 * MAX_TIMEOUT_MICRO_SEC) {
    // signal or interrupt was lost altogether this is an error,
    // should we raise it?? Now pretend the sensor is ready, hope it helps to give it a trigger.
    ready = true;
    log_w("Timeout trigger for %s duration %u us - echo pin state: %d start: %u end: %u now: %u",
      sensor->sensorLocation, now - start, digitalRead(sensor->echoPin), start, end, now);
    sensor->numberOfNoSignals++;
  }
  return ready;
}

bool HCSR04SensorManager::collectSensorResults() {
  bool validReading = false;
  for (size_t idx = 0; idx < NUMBER_OF_TOF_SENSORS; ++idx) {
    if (collectSensorResult(idx)) {
      validReading = true;
    }
  }
  if (validReading) {
    registerReadings();
  }
  return validReading;
}

void HCSR04SensorManager::registerReadings() {
  startOffsetMilliseconds[lastReadingCount] = millisSince(startReadingMilliseconds);
  if (lastReadingCount < MAX_NUMBER_MEASUREMENTS_PER_INTERVAL - 1) {
    lastReadingCount++;
  }
}

/* Returns true if there was a no timeout reading. */
bool HCSR04SensorManager::collectSensorResult(uint8_t sensorId) {
  HCSR04SensorInfo* const sensor = &m_sensors[sensorId];
  if (sensor->trigger == 0) {
    return false; // already read
  }
  bool validReading = false;
  const uint32_t end = sensor->end;
  const uint32_t start = getFixedStart(sensorId, sensor);
  uint32_t duration;
  if (end == MEASUREMENT_IN_PROGRESS) {
    duration = microsSince(start);
    if (duration < MAX_DURATION_MICRO_SEC) {
      return false;  // still measuring
    }
    // measurement is still in flight! But the time we want to wait is up (> MAX_DURATION_MICRO_SEC)
    sensor->echoDurationMicroseconds[lastReadingCount] = -1;
  } else {
    duration = microsBetween(start, end);
    sensor->echoDurationMicroseconds[lastReadingCount] = duration;
  }
  uint16_t dist;
  if (duration < MIN_DURATION_MICRO_SEC || duration >= MAX_DURATION_MICRO_SEC) {
    dist = MAX_SENSOR_VALUE;
  } else {
    validReading = true;
    dist = static_cast<uint16_t>(duration / MICRO_SEC_TO_CM_DIVIDER);
  }
  sensor->rawDistance = dist;
  sensor->median->addValue(dist);
  sensorValues[sensorId] =
    sensor->distance = correctSensorOffset(medianMeasure(sensor, dist), sensor->offset);

#ifdef DEVELOP
  Serial.printf("Raw sensor[%d] distance read %03u / %03u (%03u, %03u, %03u) -> *%03ucm*, duration: %zu us - echo pin state: %d\n",
    sensorId, sensor->rawDistance, dist, sensor->distances[0], sensor->distances[1],
    sensor->distances[2], sensorValues[sensorId], duration, digitalRead(sensor->echoPin));
#endif

  if (sensor->distance > 0 && sensor->distance < sensor->minDistance) {
    sensor->minDistance = sensor->distance;
    sensor->lastMinUpdate = millis();
  }
  sensor->trigger = 0;
  return validReading;
}

uint16_t HCSR04SensorManager::getRawMedianDistance(uint8_t sensorId) {
 return m_sensors[sensorId].median->median();
}

uint32_t HCSR04SensorManager::getMaxDurationUs(uint8_t sensorId) {
  return m_sensors[sensorId].maxDurationUs;
}

uint32_t HCSR04SensorManager::getMinDurationUs(uint8_t sensorId) {
  return m_sensors[sensorId].minDurationUs;
}

uint32_t HCSR04SensorManager::getLastDelayTillStartUs(uint8_t sensorId) {
  return m_sensors[sensorId].lastDelayTillStartUs;
}

void HCSR04SensorManager::setNoMeasureDate(uint8_t sensorId) {
  m_sensors[sensorId].echoDurationMicroseconds[lastReadingCount] = -1;
}

/* During debugging I observed readings that did not get `start` updated
 * By the interrupt. Since we also set start when we send the pulse to the
 * sensor this adds 300 microseconds or 5 centimeters to the measured result.
 * After research, is a bug in the ESP!? See https://esp32.com/viewtopic.php?t=10124
 */
uint32_t HCSR04SensorManager::getFixedStart(
  size_t idx, const HCSR04SensorInfo *sensor) {
  uint32_t start = sensor->start;
  // the error appears if both sensors trigger the interrupt at the exact same
  // time, if this happens, trigger time == start time
  if (sensor->trigger == sensor->start) {
    for (size_t idx2 = 0; idx2 < NUMBER_OF_TOF_SENSORS; ++idx2) {
      if (idx2 != idx) {
        // it should be save to use the start value from the other sensor.
        const uint32_t alternativeStart = m_sensors[idx2].start;
        if (microsBetween(alternativeStart, start) < 500) { // typically 290-310 microseconds {
          start = alternativeStart;
        }
      }
    }
  }
  return start;
}

uint16_t HCSR04SensorManager::correctSensorOffset(uint16_t dist, uint16_t offset) {
  uint16_t  result;
  if (dist == MAX_SENSOR_VALUE) {
    result = MAX_SENSOR_VALUE;
  } else if (dist > offset) {
    result = dist - offset;
  } else {
    result = 0; // would be negative if corrected
  }
  return result;
}

void HCSR04SensorManager::setSensorTriggersToLow() {
  for (size_t idx = 0; idx < NUMBER_OF_TOF_SENSORS; ++idx) {
    digitalWrite(m_sensors[idx].triggerPin, LOW);
  }
}

void HCSR04SensorManager::waitForEchosOrTimeout(uint8_t sensorId) {
  HCSR04SensorInfo* const sensor = &m_sensors[sensorId];
  while ((sensor->end == MEASUREMENT_IN_PROGRESS)
    && (microsSince(sensor->start) < MAX_DURATION_MICRO_SEC)) { // max duration not expired
    yield();
  }
}

void HCSR04SensorManager::updateStatistics(HCSR04SensorInfo *sensor) {
  if (sensor->end != MEASUREMENT_IN_PROGRESS) {
    const uint32_t startDelay = sensor->start - sensor->trigger;
    if (startDelay != 0) {
      const uint32_t duration = sensor->end - sensor->start;
      if (duration > sensor->maxDurationUs) {
        sensor->maxDurationUs = duration;
      }
      if (duration < sensor->minDurationUs && duration > 1) {
        sensor->minDurationUs = duration;
      }
      sensor->lastDelayTillStartUs = startDelay;
    }
  }
}

/* Determines the microseconds between the 2 time counters given.
 * Internally we only count 32bit, this overflows after around 71 minutes,
 * so we take care for the overflow. Times we measure are way below the
 * possible maximum.
 * We should not use 64bit variables for "start" and "end" because access
 * to 64bit vars is not atomic for our 32bit cpu.
 */
uint32_t HCSR04SensorManager::microsBetween(uint32_t a, uint32_t b) {
  uint32_t result = a - b;
  if (result & 0x80000000) {
    result = -result;
  }
  return result;
}

/* Determines the microseconds between the time counter given and now.
 * Internally we only count 32bit, this overflows after around 71 minutes,
 * so we take care for the overflow. Times we measure are way below the
 * possible maximum.
 */
uint32_t HCSR04SensorManager::microsSince(uint32_t a) {
  return microsBetween(micros(), a);
}

uint16_t HCSR04SensorManager::millisSince(uint16_t milliseconds) {
  uint16_t result = ((uint16_t) millis()) - milliseconds;
  if (result & 0x8000) {
    result = -result;
  }
  return result;
}

uint16_t HCSR04SensorManager::medianMeasure(HCSR04SensorInfo *const sensor, uint16_t value) {
  sensor->distances[sensor->nextMedianDistance] = value;
  sensor->nextMedianDistance++;
  if (sensor->nextMedianDistance >= MEDIAN_DISTANCE_MEASURES) {
    sensor->nextMedianDistance = 0;
  }
  return median(sensor->distances[0], sensor->distances[1], sensor->distances[2]);
}

uint16_t HCSR04SensorManager::median(uint16_t a, uint16_t b, uint16_t c) {
  if (a < b) {
    if (a >= c) {
      return a;
    } else if (b < c) {
      return b;
    }
  } else {
    if (a < c) {
      return a;
    } else if (b >= c) {
      return b;
    }
  }
  return c;
}
