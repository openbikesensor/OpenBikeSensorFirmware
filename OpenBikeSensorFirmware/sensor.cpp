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

/* About the speed of sound:
   See also http://www.sengpielaudio.com/Rechner-schallgeschw.htm (german)
    - speed of sound depends on ambient temperature
      temp, Celsius  speed, m/sec    int factor     dist error introduced with fix int factor of 58
                     (331.5+(0.6*t)) (2000/speed)   (speed@58 / speed) - 1
       35            352.1           (57)           -2.1% (-3.2cm bei 150cm)
       30            349.2
       25            346.3
       22.4          344.82           58             0
       20            343.4
       15            340.5
       12.5          338.98           (59)          +1.7% (2.6cm bei 150)
       10            337.5
       5             334.5
       0             331.5            (60)          +4%  (6cm bei 150cm)
      −5             328.5
      −10            325.4
      −15            322.3

      can so confirm that my math is correct?
      can so confirm that we accept this error rate?
*/
const uint32_t MICRO_SEC_TO_CM_DIVIDER = 58; // sound speed 340M/S, 2 times back and forward

const uint16_t MIN_DISTANCE_MEASURED_CM =   2;
const uint16_t MAX_DISTANCE_MEASURED_CM = 320; // candidate to check I could not get good readings above 300

const uint32_t MIN_DURATION_MICRO_SEC = MIN_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER;
const uint32_t MAX_DURATION_MICRO_SEC = MAX_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER;

/* This time is maximum echo pin high time if the sensor gets no response (observed 71ms, docu 60ms) */
const uint32_t MAX_TIMEOUT_MICRO_SEC = 75000;

/* Wait time after the last ping was sent by any sensor for the next
 * measurement. This should protect us from receiving unexpected, indirect
 * echos.
 */
const uint32_t SENSOR_ECHO_STILLED_TIME_MICRO_SEC = 350 * MICRO_SEC_TO_CM_DIVIDER;

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
 *    If we do not wait for both sensors we are good again, and so trigger
 *    one sensor while the other is still in "timeout-state" we save some time.
 *    Assuming 1st measure is just before the car appears besides the sensor:
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
  attachInterrupt(sensorInfo.echoPin, std::bind(&HCSR04SensorManager::isr, this, m_sensors.size() - 1), CHANGE);
}

void HCSR04SensorManager::reset(bool resetMinDistance) {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    sensorValues[idx] = MAX_SENSOR_VALUE;
    if (resetMinDistance) m_sensors[idx].minDistance = MAX_SENSOR_VALUE;
  }
}

void HCSR04SensorManager::setOffsets(Vector<uint16_t> offsets) {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    if (idx < offsets.size())
    {
      m_sensors[idx].offset = offsets[idx];
    }
    else
    {
      m_sensors[idx].offset = 0;
    }
  }
}

void HCSR04SensorManager::getDistances() {
  setSensorTriggersToLow();
  waitTillNoSignalsInFlight();
  sendTriggerToReadySensor();
  // spec says 10, there are reports that the JSN-SR04T-2.0 behaves better if we wait 20 microseconds.
  // I did not observe this but others might be affected so we spend this time ;)
  // https://wolles-elektronikkiste.de/hc-sr04-und-jsn-sr04t-2-0-abstandssensoren
  delayMicroseconds(20);
  setSensorTriggersToLow();

  waitForEchosOrTimeout();
  collectSensorResults();
}

/* Start measurement with all sensors that are ready to measure, wait
 * if there is no ready sensor at all.
 */
void HCSR04SensorManager::sendTriggerToReadySensor() {
  boolean startedMeasurement = false;
  while (!startedMeasurement) {
    for (size_t idx = 0; idx < m_sensors.size(); ++idx)
    {
      HCSR04SensorInfo* const sensor = &m_sensors[idx];
      if (isReadyForStart(sensor)) {
        sensor->start = micros(); // will be updated with HIGH signal
        sensor->end = MEASUREMENT_IN_PROGRESS; // will be updated with LOW signal
        digitalWrite(sensor->triggerPin, HIGH);
        startedMeasurement = true;
      }
    }
  }
}

boolean HCSR04SensorManager::isReadyForStart(HCSR04SensorInfo* sensor) {
  boolean ready = false;
  uint32_t now = micros();
  if (digitalRead(sensor->echoPin) == LOW && sensor->end != MEASUREMENT_IN_PROGRESS) { // no measurement in flight or just finished
    if ((microsBetween(now, sensor->end) > SENSOR_QUIET_PERIOD_AFTER_END_MICRO_SEC)
        && (microsBetween(now, sensor->start) > SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC)) {
      ready = true;
    }
  } else if (microsBetween(now, sensor->start) > (2 * MAX_TIMEOUT_MICRO_SEC)) {
    // signal or interrupt was lost altogether this is an error,
    // should we raise it?? Now pretend the sensor is ready, hope it helps to give him a trigger.
    ready = true;
#ifdef DEVELOP
    Serial.printf("!Timeout trigger for %s duration %zu us - echo pin state: %d\n",
      sensor->sensorLocation, sensor->start - now, digitalRead(sensor->echoPin));
#endif
  }
  return ready;
}

void HCSR04SensorManager::collectSensorResults() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    HCSR04SensorInfo* const sensor = &m_sensors[idx];
    const uint32_t end = sensor->end;
    const uint32_t start = sensor->start;
    uint32_t duration;
    if (end == MEASUREMENT_IN_PROGRESS)
    { // measurement is still in flight! But the time we want to wait is up (> MAX_DURATION_MICRO_SEC)
      duration = microsSince(start);
      // better save than sorry:
      if (duration < MAX_DURATION_MICRO_SEC) {
#ifdef DEVELOP
        Serial.printf("Collect called to early! Sensor[%d] duration: %zu us - echo pin state: %d\n",
          idx, duration, digitalRead(sensor->echoPin));
#endif
        duration = MAX_DURATION_MICRO_SEC;
      }
    }
    else
    {
      duration = microsBetween(start, end);
    }
    uint16_t dist;
    if (duration < MIN_DURATION_MICRO_SEC || duration >= MAX_DURATION_MICRO_SEC)
    {
      dist = MAX_SENSOR_VALUE;
    }
    else
    {
      dist = static_cast<uint16_t>(duration / MICRO_SEC_TO_CM_DIVIDER);
    }
    sensor->rawDistance = dist;
    sensorValues[idx] = correctSensorOffset(dist, sensor->offset);

#ifdef DEVELOP
    Serial.printf("Raw sensor[%d] distance read %03u / %03u -> *%03ucm*, duration: %zu us - echo pin state: %d\n",
      idx, sensor->rawDistance, dist, sensorValues[idx], duration, digitalRead(sensor->echoPin));
#endif

    if (sensorValues[idx] > 0 && sensorValues[idx] < sensor->minDistance)
    {
      sensor->minDistance = sensorValues[idx];
      sensor->lastMinUpdate = millis();
    }
  }
}

uint16_t HCSR04SensorManager::correctSensorOffset(uint16_t dist, uint16_t offset) {
  uint16_t  result;
  if (dist == MAX_SENSOR_VALUE)
  {
    result = MAX_SENSOR_VALUE;
  }
  else if (dist > offset)
  {
    result = dist - offset;
  }
  else
  {
    result = 0; // would be negative if corrected
  }
  return result;
}

void HCSR04SensorManager::setSensorTriggersToLow() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    digitalWrite(m_sensors[idx].triggerPin, LOW);
  }
}

void HCSR04SensorManager::waitForEchosOrTimeout() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    HCSR04SensorInfo* const sensor = &m_sensors[idx];
    while ((sensor->end == MEASUREMENT_IN_PROGRESS)
           && (microsSince(sensor->start) < MAX_DURATION_MICRO_SEC)) // max duration not expired
    {
      NOP();
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

/* Make sure we did not send a echo (either sensor) short time ago where a still
 * in flight echo could cause a false trigger of either of the sensors.
 */
void HCSR04SensorManager::waitTillNoSignalsInFlight() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    HCSR04SensorInfo* const sensor = &m_sensors[idx];
    while (microsSince(sensor->start) < SENSOR_ECHO_STILLED_TIME_MICRO_SEC)
    {
      NOP();
    }
  }
}

void IRAM_ATTR HCSR04SensorManager::isr(int idx) {
  // since the measurement of start and stop use the same interrupt
  // mechanism we should see a similar delay.
  HCSR04SensorInfo* const sensor = &m_sensors[idx];
  if (sensor->end == MEASUREMENT_IN_PROGRESS)
  {
    const uint32_t now = micros();
    if (HIGH == digitalRead(sensor->echoPin))
    {
      sensor->start = now;
    }
    else // LOW
    {
      sensor->end = now;
    }
  }
}
