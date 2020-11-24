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
*/
const uint32_t MICRO_SEC_TO_CM_DIVIDER = 58; // sound speed 340M/S, 2 times back and forward


const uint16_t MEDIAN_DISTANCE_MEASURES = 3;
const uint16_t MAX_NUMBER_MEASUREMENTS_PER_INTERVAL = 60;

struct HCSR04SensorInfo {
  uint8_t triggerPin = 15;
  uint8_t echoPin = 4;
  uint16_t offset = 0;
  uint16_t rawDistance = 0;
  uint16_t distances[MEDIAN_DISTANCE_MEASURES] = {MAX_SENSOR_VALUE, MAX_SENSOR_VALUE, MAX_SENSOR_VALUE};
  uint16_t nextMedianDistance = 0;
  uint16_t minDistance = MAX_SENSOR_VALUE;
  uint16_t distance = MAX_SENSOR_VALUE;
  char *sensorLocation;
  unsigned long lastMinUpdate = 0;
  uint32_t trigger = 0;
  volatile uint32_t start = 0;
  /* if end == 0 - a measurement is in progress */
  volatile uint32_t end = 1;

  int32_t echoDurationMicroseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];
};

class HCSR04SensorManager {
  public:
    HCSR04SensorManager() {}

    virtual ~HCSR04SensorManager() {}

    Vector<HCSR04SensorInfo> m_sensors;
    Vector<uint16_t> sensorValues;

    void getDistances();

    void getDistancesParallel();

    void reset();

    void registerSensor(HCSR04SensorInfo);

    void setOffsets(Vector<uint16_t>);

    void setPrimarySensor(uint8_t idx);

    uint16_t lastReadingCount = 0;
    uint16_t startOffsetMilliseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];

    /* Index for CSV - starts with 1. */
    uint16_t getCurrentMeasureIndex();

  protected:

  private:
    void waitTillSensorIsReady(uint8_t sensorId);

    void sendTriggerToSensor(uint8_t sensorId);

    void waitForEchosOrTimeout(uint8_t sensorId);

    void collectSensorResult(uint8_t sensorId);

    void setNoMeasureDate(uint8_t sensorId);

    void waitTillPrimarySensorIsReady();

    void waitForEchosOrTimeout();

    void setSensorTriggersToLow();

    void collectSensorResults();

    void sendTriggerToReadySensor();

    void IRAM_ATTR isr(int idx);

    uint32_t getFixedStart(size_t idx, const HCSR04SensorInfo *sensor);

    static uint16_t medianMeasure(HCSR04SensorInfo *const sensor, uint16_t value);

    static uint16_t median(uint16_t a, uint16_t b, uint16_t c);

    static uint16_t correctSensorOffset(uint16_t dist, uint16_t offset);

    static boolean isReadyForStart(HCSR04SensorInfo *sensor);

    static uint32_t microsBetween(uint32_t a, uint32_t b);

    static uint32_t microsSince(uint32_t a);

    static uint16_t millisSince(uint16_t milliseconds);

    uint16_t startReadingMilliseconds = 0;
    /* The currently used sensor for alternating use. */
    uint32_t activeSensor = 0;
    uint8_t primarySensor = 1;
};

#endif
