#include <rom/queue.h>
/*
  Copyright (C) 2019 Zweirat
  Contact: https://openbikesensor.org

  This file is part of the OpenBikeSensor project.

  The OpenBikeSensor sensor firmware is free software: you can redistribute
  it and/or modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  The OpenBikeSensor sensor firmware is distributed in the hope that it will
  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
  Public License for more details.

  You should have received a copy of the GNU General Public License along with
  the OpenBikeSensor sensor firmware.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sensor.h"
#include "FunctionalInterrupt.h"

const uint16_t MIN_DISTANCE_MEASURED_CM =   2;
const uint16_t MAX_DISTANCE_MEASURED_CM = 320; // candidate to check I could not get good readings above 300

const uint32_t MIN_DURATION_MICRO_SEC = MIN_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER;
const uint32_t MAX_DURATION_MICRO_SEC = MAX_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER;

/* This time is maximum echo pin high time if the sensor gets no response
 * HC-SR04: observed 71ms, docu 60ms
 * JSN-SR04T: observed 58ms
 */
const uint32_t MAX_TIMEOUT_MICRO_SEC = 75000;

/* The last end (echo goes to low) of a measurement must be this far
 * away before a new measurement is started.
 */
const uint32_t SENSOR_QUIET_PERIOD_AFTER_END_MICRO_SEC = 10 * 1000;

/* The last start of a new measurement must be as long ago as given here
    away, until we start a new measurement.
   - With 35ms I could get stable readings down to 19cm (new sensor)
   - With 30ms I could get stable readings down to 25/35cm only (new sensor)
   - It looked fine with the old sensor for all values
 */
const uint32_t SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC = 35 * 1000;

/* Value of HCSR04SensorInfo::end during an ongoing measurement. */
const uint32_t MEASUREMENT_IN_PROGRESS = 0;

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

void HCSR04SensorManager::registerSensor(HCSR04SensorInfo sensorInfo) {
  m_sensors.push_back(sensorInfo);
  pinMode(sensorInfo.triggerPin, OUTPUT);
  pinMode(sensorInfo.echoPin, INPUT_PULLUP); // hint from https://youtu.be/xwsT-e1D9OY?t=354
  sensorValues.push_back(0); //make sure sensorValues has same size as m_sensors
  assert(sensorValues.size() == m_sensors.size());
  if (m_sensors[m_sensors.size() - 1].median == nullptr) {
    m_sensors[m_sensors.size() - 1].median = new Median<uint16_t>(5);
  }
  // only one interrupt per pin, can not split RISING/FALLING here
  attachInterrupt(sensorInfo.echoPin, std::bind(&HCSR04SensorManager::isr, this, m_sensors.size() - 1), CHANGE);
}

void HCSR04SensorManager::reset() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx) {
    m_sensors[idx].minDistance = MAX_SENSOR_VALUE;
    memset(&(m_sensors[idx].echoDurationMicroseconds), 0, sizeof(m_sensors[idx].echoDurationMicroseconds));
  }
  startReadingMilliseconds = 0; // cheat a bit, we start the clock just with the 1st measurement
  lastReadingCount = 0;
  memset(&(startOffsetMilliseconds), 0, sizeof(startOffsetMilliseconds));
  activeSensor = primarySensor;
}

void HCSR04SensorManager::setOffsets(std::vector<uint16_t> offsets) {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx) {
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
  if (startReadingMilliseconds == 0) {
    startReadingMilliseconds = millis();
  }
  startOffsetMilliseconds[lastReadingCount] = millisSince(startReadingMilliseconds);
  // spec says 10, there are reports that the JSN-SR04T-2.0 behaves better if we wait 20 microseconds.
  // I did not observe this but others might be affected so we spend this time ;)
  // https://wolles-elektronikkiste.de/hc-sr04-und-jsn-sr04t-2-0-abstandssensoren
  delayMicroseconds(20);
  setSensorTriggersToLow();

  waitForEchosOrTimeout(activeSensor);
  collectSensorResult(activeSensor);
  activeSensor++;
  if (activeSensor >= m_sensors.size()) {
    activeSensor = 0;
  }
  setNoMeasureDate(activeSensor);
  if (lastReadingCount < MAX_NUMBER_MEASUREMENTS_PER_INTERVAL) {
    lastReadingCount++;
  }
}


/* Method that reads the sensors in parallel. We observed false readings
 * that are likely caused by both sensors operated at the same time.
 * So once the alternating implementation is established this code must
 * be removed.
 */
void HCSR04SensorManager::getDistancesParallel() {
  setSensorTriggersToLow();
  waitTillPrimarySensorIsReady();
  sendTriggerToReadySensor();
  if (startReadingMilliseconds == 0) {
    startReadingMilliseconds = millis();
  }
  // spec says 10, there are reports that the JSN-SR04T-2.0 behaves better if we wait 20 microseconds.
  // I did not observe this but others might be affected so we spend this time ;)
  // https://wolles-elektronikkiste.de/hc-sr04-und-jsn-sr04t-2-0-abstandssensoren
  delayMicroseconds(20);
  setSensorTriggersToLow();

  waitForEchosOrTimeout();
  collectSensorResults();
}

uint16_t HCSR04SensorManager::getCurrentMeasureIndex() {
  return lastReadingCount;
}

/* Wait till the primary sensor is ready, this also defines the frequency of
 * measurements and ensures we do not over pace.
 */
void HCSR04SensorManager::waitTillPrimarySensorIsReady() {
  waitTillSensorIsReady(primarySensor);
}

/* Wait till the given sensor is ready.  */
void HCSR04SensorManager::waitTillSensorIsReady(uint8_t sensorId) {
  while (!isReadyForStart(&m_sensors[sensorId])) {
    yield();
  }
}

/* Start measurement with all sensors that are ready to measure, wait
 * if there is no ready sensor at all.
 */
void HCSR04SensorManager::sendTriggerToReadySensor() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx) {
    HCSR04SensorInfo* const sensor = &m_sensors[idx];
    if (idx == primarySensor || isReadyForStart(sensor)) {
      sensor->trigger = sensor->start = micros(); // will be updated with HIGH signal
      sensor->end = MEASUREMENT_IN_PROGRESS; // will be updated with LOW signal
      digitalWrite(sensor->triggerPin, HIGH);
    }
  }
}

/* Start measurement with all sensors that are ready to measure, wait
 * if there is no ready sensor at all.
 */
void HCSR04SensorManager::sendTriggerToSensor(uint8_t sensorId) {
  HCSR04SensorInfo* const sensor = &m_sensors.at(sensorId);
  sensor->trigger = sensor->start = micros(); // will be updated with HIGH signal
  sensor->end = MEASUREMENT_IN_PROGRESS; // will be updated with LOW signal
  digitalWrite(sensor->triggerPin, HIGH);
}

boolean HCSR04SensorManager::isReadyForStart(HCSR04SensorInfo* sensor) {
  boolean ready = false;
  const uint32_t now = micros();
  const uint32_t start = sensor->start;
  const uint32_t end = sensor->end;
  if (digitalRead(sensor->echoPin) == LOW && end != MEASUREMENT_IN_PROGRESS) { // no measurement in flight or just finished
    if ((microsBetween(now, end) > SENSOR_QUIET_PERIOD_AFTER_END_MICRO_SEC)
      && (microsBetween(now, start) > SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC)) {
      ready = true;
    }
  } else if (microsBetween(now, start) > 2 * MAX_TIMEOUT_MICRO_SEC) {
    // signal or interrupt was lost altogether this is an error,
    // should we raise it?? Now pretend the sensor is ready, hope it helps to give it a trigger.
    ready = true;
#ifdef DEVELOP
    Serial.printf("!Timeout trigger for %s duration %u us - echo pin state: %d start: %u end: %u now: %u\n",
      sensor->sensorLocation, now - start, digitalRead(sensor->echoPin), start, end, now);
#endif
  }
  return ready;
}

void HCSR04SensorManager::collectSensorResults() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx) {
    collectSensorResult(idx);
  }
  startOffsetMilliseconds[lastReadingCount] = millisSince(startReadingMilliseconds);
  if (lastReadingCount < MAX_NUMBER_MEASUREMENTS_PER_INTERVAL) {
    lastReadingCount++;
  }
}

void HCSR04SensorManager::collectSensorResult(uint8_t sensorId) {
  HCSR04SensorInfo* const sensor = &m_sensors[sensorId];
  const uint32_t end = sensor->end;
  const uint32_t start = getFixedStart(sensorId, sensor);
  uint32_t duration;
  if (end == MEASUREMENT_IN_PROGRESS) {
    // measurement is still in flight! But the time we want to wait is up (> MAX_DURATION_MICRO_SEC)
    duration = microsSince(start);
    sensor->echoDurationMicroseconds[lastReadingCount] = -1;
    // better save than sorry:
    if (duration < MAX_DURATION_MICRO_SEC) {
#ifdef DEVELOP
      Serial.printf("Collect called to early! Sensor[%d] duration: %zu us - echo pin state: %d\n",
        sensorId, duration, digitalRead(sensor->echoPin));
#endif
      duration = MAX_DURATION_MICRO_SEC;
    }
  } else {
    duration = microsBetween(start, end);
    sensor->echoDurationMicroseconds[lastReadingCount] = duration;
  }
  uint16_t dist;
  if (duration < MIN_DURATION_MICRO_SEC || duration >= MAX_DURATION_MICRO_SEC) {
    dist = MAX_SENSOR_VALUE;
  } else {
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
}

uint16_t HCSR04SensorManager::getRawMedianDistance(uint8_t sensorId) {
 return m_sensors[sensorId].median->median();
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
    for (size_t idx2 = 0; idx2 < m_sensors.size(); ++idx2) {
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
  for (size_t idx = 0; idx < m_sensors.size(); ++idx) {
    digitalWrite(m_sensors[idx].triggerPin, LOW);
  }
}

void HCSR04SensorManager::waitForEchosOrTimeout() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx) {
    waitForEchosOrTimeout(idx);
  }
}

void HCSR04SensorManager::waitForEchosOrTimeout(uint8_t sensorId) {
  HCSR04SensorInfo* const sensor = &m_sensors[sensorId];
  while ((sensor->end == MEASUREMENT_IN_PROGRESS)
    && (microsSince(sensor->start) < MAX_DURATION_MICRO_SEC)) { // max duration not expired
    yield();
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
  sensor->distances[sensor->nextMedianDistance++] = value;
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

void IRAM_ATTR HCSR04SensorManager::isr(int idx) {
  // since the measurement of start and stop use the same interrupt
  // mechanism we should see a similar delay.
  HCSR04SensorInfo* const sensor = &m_sensors[idx];
  if (sensor->end == MEASUREMENT_IN_PROGRESS) {
    const uint32_t now = micros();
    if (HIGH == digitalRead(sensor->echoPin)) {
      sensor->start = now;
    } else { // LOW
      sensor->end = now;
    }
  }
}
