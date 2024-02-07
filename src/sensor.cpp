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
 * High values can lead to the situation that we only poll the
 * primary sensor for a while!?
 */
static const uint32_t SENSOR_QUIET_PERIOD_AFTER_OPPOSITE_START_MICRO_SEC = 30 * 1000;

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
static const uint32_t SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC = 35 * 1000;

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

void HCSR04SensorManager::reset(uint32_t startMillisTicks) {
  for (auto & sensor : m_sensors) {
    sensor.minDistance = MAX_SENSOR_VALUE;
    memset(&(sensor.echoDurationMicroseconds), 0, sizeof(sensor.echoDurationMicroseconds));
    sensor.numberOfTriggers = 0;
  }
  lastReadingCount = 0;
  lastSensor = 1 - primarySensor;
  memset(&(startOffsetMilliseconds), 0, sizeof(startOffsetMilliseconds));
  startReadingMilliseconds = startMillisTicks;
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

/* Polls for new readings, if sensors are not ready, the
 * method returns false.
 */
bool HCSR04SensorManager::pollDistancesAlternating() {
  bool newMeasurements = false;
  if (lastSensor == primarySensor && isReadyForStart(1 - primarySensor)) {
    setSensorTriggersToLow();
    lastSensor = 1 - primarySensor;
    sendTriggerToSensor(1 - primarySensor);
  } else if (isReadyForStart(primarySensor)) {
    newMeasurements = collectSensorResults();
    setSensorTriggersToLow();
    lastSensor = primarySensor;
    sendTriggerToSensor(primarySensor);
  }
  return newMeasurements;
}

/* Polls for new readings, if sensors are not ready, the
 * method returns false. If sensors are ready and readings
 * are available, data is read and updated. If the primary
 * sensor is ready for new measurement, a fresh measurement
 * is triggered. Method returns true if new data (no timeout
 * measurement) was collected.
 */
bool HCSR04SensorManager::pollDistancesParallel() {
  bool newMeasurements = false;
  if (isReadyForStart(primarySensor)) {
    setSensorTriggersToLow();
    newMeasurements = collectSensorResults();
    const bool secondSensorIsReady = isReadyForStart(1 - primarySensor);
    sendTriggerToSensor(primarySensor);
    if (secondSensorIsReady) {
      sendTriggerToSensor(1 - primarySensor);
    }
  }
  return newMeasurements;
}

uint16_t HCSR04SensorManager::getCurrentMeasureIndex() {
  return lastReadingCount - 1;
}

/* Send echo trigger to the selected sensor and prepare measurement data
 * structures.
 */
void HCSR04SensorManager::sendTriggerToSensor(uint8_t sensorId) {
  HCSR04SensorInfo * const sensor = &(m_sensors[sensorId]);
  updateStatistics(sensor);
  sensor->end = MEASUREMENT_IN_PROGRESS; // will be updated with LOW signal
  sensor->numberOfTriggers++;
  sensor->measurementRead = false;
  const uint32_t now = micros();
  sensor->trigger = now; // will be updated with HIGH signal
  sensor->start = now;
  digitalWrite(sensor->triggerPin, HIGH);
  // 10us are specified but some sensors are more stable with 20us according
  // to internet reports
  delayMicroseconds(200);
  digitalWrite(sensor->triggerPin, LOW);
}

/* Checks if the given sensor is ready for a new measurement cycle.
 */
boolean HCSR04SensorManager::isReadyForStart(uint8_t sensorId) {
  HCSR04SensorInfo * const sensor = &m_sensors[sensorId];
  boolean ready = false;
  const uint32_t now = micros();
  const uint32_t start = sensor->start;
  const uint32_t end = sensor->end;
  if (end != MEASUREMENT_IN_PROGRESS) { // no measurement in flight or just finished
    const uint32_t startOther = m_sensors[1 - sensorId].start;
    if (   (microsBetween(now, end) > SENSOR_QUIET_PERIOD_AFTER_END_MICRO_SEC)
        && (microsBetween(now, start) > SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC)
        && (microsBetween(now, startOther) > SENSOR_QUIET_PERIOD_AFTER_OPPOSITE_START_MICRO_SEC)) {
      ready = true;
    }
    if (digitalRead(sensor->echoPin) != LOW) {
      log_e("Measurement done, but echo pin is high for %s sensor", sensor->sensorLocation);
      sensor->numberOfLowAfterMeasurement++;
    }
  } else if (microsBetween(now, start) > MAX_TIMEOUT_MICRO_SEC) {
    sensor->numberOfToLongMeasurement++;
    // signal or interrupt was lost altogether this is an error,
    // should we raise it?? Now pretend the sensor is ready, hope it helps to give it a trigger.
    ready = true;
    // do not log to much there might be devices out there with one sensor only
    log_d("Timeout trigger for %s duration %u us - echo pin state: %d trigger: %u "
        "start: %u end: %u now: %u",
      sensor->sensorLocation, now - start, digitalRead(sensor->echoPin), sensor->trigger,
        start, end, now);
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
  if (sensor->measurementRead) {
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

  log_v("Raw sensor[%d] distance read %03u / %03u (%03u, %03u, %03u) -> *%03ucm*, duration: %zu us - echo pin state: %d",
    sensorId, sensor->rawDistance, dist, sensor->distances[0], sensor->distances[1],
    sensor->distances[2], sensorValues[sensorId], duration, digitalRead(sensor->echoPin));

  if (sensor->distance > 0 && sensor->distance < sensor->minDistance) {
    sensor->minDistance = sensor->distance;
  }
  sensor->measurementRead = true;
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

uint32_t HCSR04SensorManager::getNoSignalReadings(const uint8_t sensorId) {
  return m_sensors[sensorId].numberOfNoSignals;
}

uint32_t HCSR04SensorManager::getNumberOfLowAfterMeasurement(const uint8_t sensorId) {
  return m_sensors[sensorId].numberOfLowAfterMeasurement;
}

uint32_t HCSR04SensorManager::getNumberOfToLongMeasurement(const uint8_t sensorId) {
  return m_sensors[sensorId].numberOfToLongMeasurement;
}

uint32_t HCSR04SensorManager::getNumberOfInterruptAdjustments(const uint8_t sensorId) {
  return m_sensors[sensorId].numberOfInterruptAdjustments;
}

/* During debugging I observed readings that did not get `start` updated
 * By the interrupt. Since we also set start when we send the pulse to the
 * sensor this adds 300 microseconds or 5 centimeters to the measured result.
 * After research, is a bug in the ESP!? See https://esp32.com/viewtopic.php?t=10124
 */
uint32_t HCSR04SensorManager::getFixedStart(
  size_t idx, HCSR04SensorInfo * const sensor) {
  uint32_t start = sensor->start;
  // the error appears if both sensors trigger the interrupt at the exact same
  // time, if this happens, trigger time == start time
  if (start == sensor->trigger && sensor->end != MEASUREMENT_IN_PROGRESS) {
    // it should be save to use the start value from the other sensor.
    const uint32_t alternativeStart = m_sensors[1 - idx].start;
    if (alternativeStart != m_sensors[1 - idx].trigger
      && microsBetween(alternativeStart, start) < 500) { // typically 290-310 microseconds {
      start = alternativeStart;
      sensor->numberOfInterruptAdjustments++;
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
  for (auto & m_sensor : m_sensors) {
    digitalWrite(m_sensor.triggerPin, LOW);
  }
}

void HCSR04SensorManager::updateStatistics(HCSR04SensorInfo * const sensor) {
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
  } else {
    sensor->numberOfNoSignals++;
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

  // if we got "fantom" measures, they are <= the current measures, so remove
  // all values <= the current measure from the median data
  for (unsigned short & distance : sensor->distances) {
    if (distance < value) {
      distance = value;
    }
  }
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
