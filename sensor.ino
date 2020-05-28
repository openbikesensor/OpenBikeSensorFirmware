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

/*
  void DistanceSensor::getMinDistance(uint8_t& min_distance) {
  float dist;
  dist = getDistance() - float(m_offset);
  if ((dist > 0.0) && (dist < float(min_distance)))
  {
    min_distance = uint8_t(dist);
  }
  else
  {
    dist = 0.0;
  }
  delay(20);
  }

  float HCSR04DistanceSensor::getDistance() {
  float duration = 0;
  float distance = 0;

  digitalWrite(m_triggerPin, LOW);
  delayMicroseconds(2);
  noInterrupts();
  digitalWrite(m_triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(m_triggerPin, LOW);
  duration = pulseIn(m_echoPin, HIGH, m_timeout); // Erfassung - Dauer in Mikrosekunden
  interrupts();

  distance = (duration / 2) / 29.1; // Distanz in CM
  return (distance);
  }*/

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

void HCSR04SensorManager::getDistance(int idx) {
  const uint32_t max_timeout_us = clockCyclesToMicroseconds(UINT_MAX);
  m_sensors[idx].timeout_cycles = microsecondsToClockCycles(m_sensors[idx].timeout);
  if (m_sensors[idx].timeout > max_timeout_us)
  {
    m_sensors[idx].timeout = max_timeout_us;
    //Serial.write(" Setting timeout to max value");
  }

  digitalWrite(m_sensors[idx].triggerPin, LOW);

  delayMicroseconds(2);
  noInterrupts();

  digitalWrite(m_sensors[idx].triggerPin, HIGH);
  m_sensors[idx].awaitingEcho = false;
  m_sensors[idx].echoBegins = false;
  m_sensors[idx].echoReceived = false;
  m_sensors[idx].duration = 0;

  delayMicroseconds(10);

  digitalWrite(m_sensors[idx].triggerPin, LOW);
  m_sensors[idx].start_cycle_count = xthal_get_ccount();

  bool state = HIGH;
  while (!(m_sensors[idx].echoReceived))
  {
    if (xthal_get_ccount() - m_sensors[idx].start_cycle_count > m_sensors[idx].timeout_cycles)
    {
      m_sensors[idx].awaitingEcho = true;
      m_sensors[idx].echoBegins = true;
      m_sensors[idx].echoReceived = true;
      m_sensors[idx].duration = 0;
    }
    if (m_sensors[idx].awaitingEcho == false)
    {
      if (digitalRead(m_sensors[idx].echoPin) != (state))
      {
        m_sensors[idx].awaitingEcho = true;
        //Serial.write(" awaiting Echo ! ");
      }
    }
    else if ( (m_sensors[idx].awaitingEcho == true) && (m_sensors[idx].echoBegins == false))
    {
      if (digitalRead(m_sensors[idx].echoPin) == (state))
      {
        m_sensors[idx].pulse_start_cycle_count = xthal_get_ccount();
        m_sensors[idx].echoBegins = true;
        //Serial.write(" Echo begins! ");
      }
    }
    else if ( (m_sensors[idx].awaitingEcho == true) && (m_sensors[idx].echoBegins == true) && (m_sensors[idx].echoReceived == false) )
    {
      if (digitalRead(m_sensors[idx].echoPin) != (state))
      {
        m_sensors[idx].duration = clockCyclesToMicroseconds(xthal_get_ccount() - m_sensors[idx].pulse_start_cycle_count);
        m_sensors[idx].distance = (m_sensors[idx].duration / 2) / 29.1; // Distanz in CM
        float dist;
        dist = m_sensors[idx].distance - float(m_sensors[idx].offset);
        if ((dist > 0.0) && (dist < float(sensorValues[idx])))
        {
          sensorValues[idx] = uint8_t(dist);
        }
        m_sensors[idx].echoReceived = true;
        //Serial.write(" Echo received! ");
      }
    }
  }
  interrupts();

  if (sensorValues[idx] < m_sensors[idx].minDistance)
  {
    m_sensors[idx].minDistance = sensorValues[idx];
    m_sensors[idx].lastMinUpdate = millis();
  }

}


void HCSR04SensorManager::getDistances() {
  // Loop all sensors, measure distance one after one
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    getDistance(idx);
    delayMicroseconds(10);
  }
}


/*
void HCSR04SensorManager::getDistancesParallel() {
  const uint32_t max_timeout_us = clockCyclesToMicroseconds(UINT_MAX);
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    m_sensors[idx].timeout_cycles = microsecondsToClockCycles(m_sensors[idx].timeout);
    if (m_sensors[idx].timeout > max_timeout_us)
    {
      m_sensors[idx].timeout = max_timeout_us;
      //Serial.write(" Setting timeout to max value");
    }
  }
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    digitalWrite(m_sensors[idx].triggerPin, LOW);
  }
  delayMicroseconds(2);
  noInterrupts();
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    digitalWrite(m_sensors[idx].triggerPin, HIGH);
    m_sensors[idx].awaitingEcho = false;
    m_sensors[idx].echoBegins = false;
    m_sensors[idx].echoReceived = false;
    m_sensors[idx].duration = 0;
  }
  //Serial.write(" Sensors triggered ");
  delayMicroseconds(10);
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    digitalWrite(m_sensors[idx].triggerPin, LOW);
    m_sensors[idx].start_cycle_count = xthal_get_ccount();
  }
  bool allEchoesIn = false;
  bool state = HIGH;
  while (!(allEchoesIn))
  {
    for (size_t idx = 0; idx < m_sensors.size(); ++idx)
    {
      if (xthal_get_ccount() - m_sensors[idx].start_cycle_count > m_sensors[idx].timeout_cycles)
      {
        m_sensors[idx].awaitingEcho = true;
        m_sensors[idx].echoBegins = true;
        m_sensors[idx].echoReceived = true;
        m_sensors[idx].duration = 0;
      }
      if (m_sensors[idx].awaitingEcho == false)
      {
        if (digitalRead(m_sensors[idx].echoPin) != (state))
        {
          m_sensors[idx].awaitingEcho = true;
          //Serial.write(" awaiting Echo ! ");
        }
      }
      else if ( (m_sensors[idx].awaitingEcho == true) && (m_sensors[idx].echoBegins == false))
      {
        if (digitalRead(m_sensors[idx].echoPin) == (state))
        {
          m_sensors[idx].pulse_start_cycle_count = xthal_get_ccount();
          m_sensors[idx].echoBegins = true;
          //Serial.write(" Echo begins! ");
        }
      }
      else if ( (m_sensors[idx].awaitingEcho == true) && (m_sensors[idx].echoBegins == true) && (m_sensors[idx].echoReceived == false) )
      {
        if (digitalRead(m_sensors[idx].echoPin) != (state))
        {
          m_sensors[idx].duration = clockCyclesToMicroseconds(xthal_get_ccount() - m_sensors[idx].pulse_start_cycle_count);
          m_sensors[idx].distance = (m_sensors[idx].duration / 2) / 29.1; // Distanz in CM
          float dist;
          dist = m_sensors[idx].distance - float(m_sensors[idx].offset);
          if ((dist > 0.0) && (dist < float(sensorValues[idx])))
          {
            sensorValues[idx] = uint8_t(dist);
          }
          m_sensors[idx].echoReceived = true;
          //Serial.write(" Echo received! ");
        }
      }
    }

    allEchoesIn = true;
    for (size_t idx = 0; idx < m_sensors.size(); ++idx)
    {
      allEchoesIn = allEchoesIn && m_sensors[idx].echoReceived;
    }
  }
  for (size_t idx = 0; idx < m_sensors.size(); ++idx)
  {
    unsigned long currentTime = millis();
    if(currentTime > m_sensors[idx].lastMinUpdate +1000)
    {
      m_sensors[idx].minDistance = MAX_SENSOR_VALUE;
    }
    if(sensorValues[idx] < m_sensors[idx].minDistance)
    {
      m_sensors[idx].minDistance = sensorValues[idx];
      m_sensors[idx].lastMinUpdate = currentTime;
    }
  }
}
*/
