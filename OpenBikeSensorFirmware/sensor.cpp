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

/*
   See also http://www.sengpielaudio.com/Rechner-schallgeschw.htm (german)
    - ignord all deltas after 3 digits, there are different sources
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

void HCSR04SensorManager::registerSensor(HCSR04SensorInfo sensorInfo) {
  m_sensors.push_back(sensorInfo);
  pinMode(sensorInfo.triggerPin, OUTPUT);
  pinMode(sensorInfo.echoPin, INPUT);
  sensorValues.push_back(0); //make sure sensorValues has same size as m_sensors
  assert(sensorValues.size() == m_sensors.size());
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
  setTimeouts();
}

void HCSR04SensorManager::setTimeouts() {
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    m_sensors[idx].timeout = 15000 + (int)(m_sensors[idx].offset * 29.1 * 2);
  }
}

void HCSR04SensorManager::getDistanceSimple(int idx) {
  float duration = 0;
  float distance = 0;

  digitalWrite(m_sensors[idx].triggerPin, LOW);
  delayMicroseconds(2);
  noInterrupts();
  digitalWrite(m_sensors[idx].triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(m_sensors[idx].triggerPin, LOW);
  duration = pulseIn(m_sensors[idx].echoPin, HIGH, m_sensors[idx].timeout); // Erfassung - Dauer in Mikrosekunden
  interrupts();

  distance = (duration / 2) / 29.1; // Distanz in CM
  if (distance < m_sensors[idx].minDistance)
  {
    m_sensors[idx].minDistance = distance;
    m_sensors[idx].lastMinUpdate = millis();
  }
}

void HCSR04SensorManager::getDistance(int idx) {
  HCSR04SensorInfo sensor = m_sensors[idx];

  digitalWrite(sensor.triggerPin, LOW);
  delayMicroseconds(2);

  noInterrupts();
  digitalWrite(sensor.triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(sensor.triggerPin, LOW);
  unsigned long durationMicroSec = pulseIn(sensor.echoPin, HIGH, sensor.timeout);
  interrupts();

  if (durationMicroSec > 0) {
    sensor.duration = durationMicroSec;
    sensor.distance = durationMicroSec / MICRO_SEC_TO_CM_DIVIDER;
    if (sensor.distance > sensor.offset)
    {
      sensorValues[idx] = sensor.distance - sensor.offset;
    }
    else
    {
      sensorValues[idx] = 0; // would be negative if corrected
    }
    Serial.printf("Raw sensor[%d] distance read %3d cm -> %3d cm\n", idx, sensor.distance, sensorValues[idx]);
  }
  else
  { // timeout
    sensor.duration = 0;
    sensor.distance = MAX_SENSOR_VALUE;
    sensorValues[idx] = MAX_SENSOR_VALUE;
    Serial.printf("Raw sensor[%d] distance timeout -> %3d cm\n", idx, sensor.distance);
  }

  if (sensorValues[idx] < sensor.minDistance)
  {
    sensor.minDistance = sensorValues[idx];
    sensor.lastMinUpdate = millis();
  }
}


void HCSR04SensorManager::getDistances() {
  // Loop all sensors, measure distance one after one
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    if (idx != 0) {
      // should be added elsewhere?
      delayMicroseconds(60); // avoid reverberation signals
    }
    getDistance(idx);
  }
}
