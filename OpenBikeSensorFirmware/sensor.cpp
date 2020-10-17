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
const uint16_t MICRO_SEC_TO_CM_DIVIDER = 58; // sound speed 340M/S, 2 times back and forward

const uint16_t MIN_DISTANCE_MEASURED_CM =   2;
const uint16_t MAX_DISTANCE_MEASURED_CM = 320; // candidate to check I could not get good readings above 300

const uint32_t MIN_DURATION_MICRO_SEC = MIN_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER;
const uint32_t MAX_DURATION_MICRO_SEC = MAX_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER;

/* This time is maximum echo pin high time if the sensor gets no response (observed 71ms, docu 60ms) */
const uint32_t MAX_TIMEOUT_MICRO_SEC = 75000;

/* The last start of a new measurement must be as long ago as given here
    away, until we start a new measurement.
   - With 35ms I could get stable readings down to 19cm (new sensor)
   - With 30ms I could get stable readings down to 25/35cm only (new sensor)
   - It looked fine with the old sensor for all values
 */
const uint32_t SENSOR_QUIET_PERIOD_MICRO_SEC = 35 * 1000;

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
 *    75ms open sensor + 2x SENSOR_QUIET_PERIOD_MICRO_SEC (35ms) = 145ms which is
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
  sensorQuietPeriod();
  // send "trigger" if ready
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    HCSR04SensorInfo* const sensor = &m_sensors[idx];
    int echoPinState = digitalRead(sensor->echoPin);
    if (echoPinState == LOW)
    { // no measurement in flight
      if (sensor->end != MEASUREMENT_IN_PROGRESS
          || (sensor->start + MAX_TIMEOUT_MICRO_SEC) > micros() // signal was lost altogether
        )
      {
        sensor->start = micros(); // will be updated with HIGH signal
        sensor->end = 0; // will be updated with LOW signal
        digitalWrite(sensor->triggerPin, HIGH);
      }
    }
  }
  delayMicroseconds(10);
  setSensorTriggersToLow();

  waitForEchosOrTimeout();
  collectSensorResults();
}

void HCSR04SensorManager::collectSensorResults() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    HCSR04SensorInfo* const sensor = &m_sensors[idx];
    const uint32_t end =  sensor->end;
    const uint32_t start =  sensor->start;
    uint32_t duration;
    if (end == MEASUREMENT_IN_PROGRESS)
    { // measurement is still in flight! But the time we want to wait is up (> MAX_DURATION_MICRO_SEC)
      duration = micros() - start;
    }
    else
    {
      duration = end - start;
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
    sensorValues[idx]
      = correctSensorOffset(medianMeasure(idx, dist), sensor->offset);

#ifdef DEVELOP
    Serial.printf("Raw sensor[%d] distance read %03u / %03u (%03u, %03u, %03u) -> *%03ucm*, duration: %zu us - echo pin state: %d\n",
      idx, sensor->rawDistance, dist, sensor->distances[0], sensor->distances[1],
      sensor->distances[2], sensorValues[idx], duration, digitalRead(sensor->echoPin));
#endif

    if (sensorValues[idx] < sensor->minDistance)
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
           && ((sensor->start + MAX_DURATION_MICRO_SEC) > micros()) // max duration not expired
      )
    {
      NOP();
    }
  }
}

/*
 * Do not start measurements to fast. If time is used elsewhere we do not
 * force a delay here, but if getDistances() is called with little time delay, we
 * wait here for max SENSOR_QUIET_PERIOD_MICROS since last measurement start(!) for
 * the sensors to settle. This returns quick if the measurement is already in
 * timeout.
 */
void HCSR04SensorManager::sensorQuietPeriod() {
  // avoid that we over pace, if the sensors are both in timeout
  while ((lastCall + SENSOR_QUIET_PERIOD_MICRO_SEC) > micros())
  {
    NOP();
  }
  lastCall = micros();

  // we do not wait till all sensors are low, we ignore timed out inflight measurements
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    HCSR04SensorInfo* const sensor = &m_sensors[idx];
    while ((sensor->start + SENSOR_QUIET_PERIOD_MICRO_SEC) > micros()) // max duration not expired
    {
      NOP();
    }
  }
}

uint16_t HCSR04SensorManager::medianMeasure(size_t idx, uint16_t value) {
  HCSR04SensorInfo* const sensor = &m_sensors[idx];
  sensor->distances[sensor->nextMedianDistance++] = value;
  if (sensor->nextMedianDistance >= MEDIAN_DISTANCE_MEASURES)
  {
    sensor->nextMedianDistance = 0;
  }
  return median(sensor->distances[0], sensor->distances[1], sensor->distances[2]);
}

uint16_t HCSR04SensorManager::median(uint16_t a, uint16_t b, uint16_t c) {
  if (a < b)
  {
    if (a >= c)
    {
      return a;
    }
    else if (b < c)
    {
      return b;
    }
  }
  else
  {
    if (a < c)
    {
      return a;
    }
    else if (b >= c)
    {
      return b;
    }
  }
  return c;
}

void IRAM_ATTR HCSR04SensorManager::isr(int idx) {
  // since the measurement of start and stop use the same interrupt
  // mechanism we should see a similar delay.
  HCSR04SensorInfo* const sensor = &m_sensors[idx];
  if (sensor->end == MEASUREMENT_IN_PROGRESS)
  {
    const uint32_t time = micros();
    if (HIGH == digitalRead(sensor->echoPin))
    {
      sensor->start = time;
    }
    else // LOW
    {
      sensor->end = time;
    }
  }
}
