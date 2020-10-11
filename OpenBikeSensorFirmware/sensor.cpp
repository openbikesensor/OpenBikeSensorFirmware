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

/*
   See also http://www.sengpielaudio.com/Rechner-schallgeschw.htm (german)
    - speed of sound depends on ambient temperature
      temp, Celsius  speed, m/sec    int factor     dist error introduced with fix int factor of 58
                     (331.5+(0.6*t)) (2000/speed)   (seed@58 / spped) - 1
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
const uint8_t MICRO_SEC_TO_CM_DIVIDER = 58; // sound speed 340M/S, 2 times back and forward

const uint32_t MAX_DURATION_MICRO_SEC = 255 * 58;

/*
  https://de.aliexpress.com/item/32737648330.html and other sources give 60ms here
  I've seen durations in 70ms area with the elder sensors so I use 75 here might be
  room to tune this to 60 back if needed.
  See comment below in getDistances how to tune.
*/
const long SENSOR_QUIET_PERIOD_MICROS = 75 * 1000;

void HCSR04SensorManager::registerSensor(HCSR04SensorInfo sensorInfo) {
  m_sensors.push_back(sensorInfo);
  pinMode(sensorInfo.triggerPin, OUTPUT);
  pinMode(sensorInfo.echoPin, INPUT);
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

void HCSR04SensorManager::setOffsets(Vector<uint8_t> offsets) {
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

void HCSR04SensorManager::setTimeouts() {
  long maxTimeout = 0;
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    m_sensors[idx].timeout = 15000 + (int)(m_sensors[idx].offset * MICRO_SEC_TO_CM_DIVIDER);
    if (m_sensors[idx].timeout > maxTimeout) {
      maxTimeout = m_sensors[idx].timeout;
    }
  }
  timeout = maxTimeout;
}

void HCSR04SensorManager::getDistances() {
  // should be already low
  setSensorTriggersToLow();
  sensorQuietPeriod();
  // send "trigger"
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    /* you can tune SENSOR_QUIET_PERIOD_MICROS here:
    if (m_sensors[idx].duration == 0) {
      Serial.printf("Raw sensor[%d] duration still 0! increase SENSOR_QUIET_PERIOD_MICROS, there might be still measurements ongoing.", idx);
    }
    */
    m_sensors[idx].duration = 0;
    digitalWrite(m_sensors[idx].triggerPin, HIGH);
  }
  delayMicroseconds(10);
  setSensorTriggersToLow();
  waitForEchosOrTimeout();
  collectSensorResults();
}

void HCSR04SensorManager::collectSensorResults() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    const unsigned long duration = m_sensors[idx].duration;
    uint8_t dist;
    if (duration == 0 || duration > m_sensors[idx].timeout || duration >= MAX_DURATION_MICRO_SEC)
    {
      dist = MAX_SENSOR_VALUE;
    }
    else
    {
      dist = duration / MICRO_SEC_TO_CM_DIVIDER;
    }

    dist = medianMeasure(idx, dist);
    m_sensors[idx].distance = dist;

    if (dist == MAX_SENSOR_VALUE) {
      sensorValues[idx] = MAX_SENSOR_VALUE;
    }
    else if (dist > m_sensors[idx].offset)
    {
      sensorValues[idx] = dist - m_sensors[idx].offset;
    }
    else
    {
      sensorValues[idx] = 0; // would be negative if corrected
    }

    /* Dump measurement data
    Serial.printf("Raw sensor[%d] distance read %03u (%03u, %03u, %03u) -> *%03ucm*, duration: %lu us\n",
      idx, m_sensors[idx].distance, m_sensors[idx].distances[0], m_sensors[idx].distances[1], m_sensors[idx].distances[2],
      sensorValues[idx], duration);
    */

    if (sensorValues[idx] < m_sensors[idx].minDistance)
    {
      m_sensors[idx].minDistance = sensorValues[idx];
      m_sensors[idx].lastMinUpdate = millis();
    }
  }
}

void HCSR04SensorManager::setSensorTriggersToLow() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    digitalWrite(m_sensors[idx].triggerPin, LOW);
  }
}

void HCSR04SensorManager::waitForEchosOrTimeout() {
  const unsigned long end = micros() + timeout;
  while (end > micros()) {
    boolean allFinished = true;
    for (size_t idx = 0; idx < m_sensors.size(); ++idx)
    {
      allFinished &= (m_sensors[idx].duration != 0);
    }
    if (allFinished)
    {
      break;
    }
    NOP();
  }
}

/*
 * Do not start measurements to fast. If time is used elsewhere we do not
 * force a delay here, but if getDistances() is called with little time delay, we
 * wait here for max SENSOR_QUIET_PERIOD_MICROS since last measurement start for
 * the sensors to settle.
 */
void HCSR04SensorManager::sensorQuietPeriod() {
  // calculate latest last start timne and add quiet period.
  unsigned long lastStart = 0;
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    // to be faster we could limit this to sensors with duration == 0, but this will force echos etc.
    if (/* m_sensors[idx].duration == 0 && */ m_sensors[idx].start > lastStart)
    {
      lastStart = m_sensors[idx].start;
    }
  }

  // avoid reverberation signals (60ms as in https://de.aliexpress.com/item/32737648330.html )
  const unsigned long delayTill = lastStart + SENSOR_QUIET_PERIOD_MICROS;
  while (delayTill > micros())
  {
    NOP();
  }
}

uint8_t HCSR04SensorManager::medianMeasure(size_t idx, uint8_t value) {
  HCSR04SensorInfo* const sensor = &m_sensors[idx];
  sensor->distances[sensor->nextDistance++] = value;
  if (sensor->nextDistance >= MEDIAN_DISTANCE_MEASURES)
  {
    sensor->nextDistance = 0;
  }
  return median(sensor->distances[0], sensor->distances[1], sensor->distances[2]);
}

uint8_t HCSR04SensorManager::median(uint8_t a, uint8_t b, uint8_t c) {
  if (a < b)
  {
    if (a >= c)
      return a;
    else if (b < c)
      return b;
  }
  else
  {
    if (a < c)
      return a;
    else if (b >= c)
      return b;
  }
  return c;
}

void IRAM_ATTR HCSR04SensorManager::isr(int idx) {
  // since the measurement of start and stop use the same interrupt
  // mechanism we should see a similiar delay.
  HCSR04SensorInfo* const sensor = &m_sensors[idx];
  if (sensor->duration == 0) // ignore unexpected signals
  {
    const unsigned long time = micros();
    switch(digitalRead(sensor->echoPin))
    {
      case HIGH:
        sensor->start = time;
        break;
      case LOW:
        sensor->duration = time - sensor->start;
        break;
    }
  }
}
