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

#ifndef OBS_SENSOR_H
#define OBS_SENSOR_H

#include <Arduino.h>

#include "globals.h"
#include "vector.h"

const uint8_t MEDIAN_DISTANCE_MEASURES = 3;

struct HCSR04SensorInfo
{
  int triggerPin = 15;
  int echoPin = 4;
  int timeout = 15000;
  uint8_t offset = 0;
  uint8_t distance = 0;
  uint8_t distances[MEDIAN_DISTANCE_MEASURES] = { MAX_SENSOR_VALUE, MAX_SENSOR_VALUE, MAX_SENSOR_VALUE };
  uint8_t nextDistance = 0;
  char* sensorLocation;
  uint8_t minDistance=MAX_SENSOR_VALUE;
  unsigned long lastMinUpdate=0;
  volatile unsigned long start = 0;
  volatile unsigned long duration = 0;
};

class HCSR04SensorManager
{
  public:
    HCSR04SensorManager() {}
    virtual ~HCSR04SensorManager() {}
    Vector<HCSR04SensorInfo> m_sensors;
    Vector<uint8_t> sensorValues;
    void getDistances();
    void reset(bool resetMinDistance);
    void registerSensor(HCSR04SensorInfo);
    void setOffsets(Vector<uint8_t>);
    void setTimeouts();

  protected:

  private:
    void sensorQuietPeriod();
    void waitForEchosOrTimeout();
    void setSensorTriggersToLow();
    void collectSensorResults();
    void isr(int idx);
    uint8_t medianMeasure(size_t idx, uint8_t value);
    uint8_t median(uint8_t a, uint8_t b, uint8_t c);

    int timeout = 15000;
};

#endif
