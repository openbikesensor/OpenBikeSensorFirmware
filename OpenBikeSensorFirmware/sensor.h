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

const uint16_t MEDIAN_DISTANCE_MEASURES = 3;

struct HCSR04SensorInfo
{
  uint8_t triggerPin = 15;
  uint8_t echoPin = 4;
  uint16_t offset = 0;
  uint16_t rawDistance = 0;
  uint16_t minDistance=MAX_SENSOR_VALUE;
  char* sensorLocation;
  unsigned long lastMinUpdate=0;
  volatile uint32_t start = 0;
  /* if end == 0 - a measurement is in progress */
  volatile uint32_t end = 1;
};

class HCSR04SensorManager
{
  public:
    HCSR04SensorManager() {}
    virtual ~HCSR04SensorManager() {}
    Vector<HCSR04SensorInfo> m_sensors;
    Vector<uint16_t> sensorValues;
    void getDistances();
    void reset(bool resetMinDistance);
    void registerSensor(HCSR04SensorInfo);
    void setOffsets(Vector<uint16_t>);
    void setPrimarySensor(uint8_t idx);

  protected:

  private:
    uint8_t primarySensor = 1;
    void waitTillPrimarySensorIsReady();
    void waitForEchosOrTimeout();
    void setSensorTriggersToLow();
    void collectSensorResults();
    void sendTriggerToReadySensor();
    void isr(int idx);
    static uint16_t correctSensorOffset(uint16_t dist, uint16_t offset);
    static boolean isReadyForStart(HCSR04SensorInfo* sensor);
    static uint32_t microsBetween(uint32_t a, uint32_t b);
    static uint32_t microsSince(uint32_t a);
};

#endif
