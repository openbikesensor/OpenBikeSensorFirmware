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

#ifndef OBS_SENSOR_H
#define OBS_SENSOR_H

#include <vector>
#include <Arduino.h>

#include "variant.h"
#include "globals.h"
#include "utils/median.h"

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

struct HCSR04SensorInfo {
  uint8_t triggerPin = 15;
  uint8_t echoPin = 4;
  uint16_t offset = 0;
  uint16_t rawDistance = 0;
  uint16_t distances[MEDIAN_DISTANCE_MEASURES] = { MAX_SENSOR_VALUE, MAX_SENSOR_VALUE, MAX_SENSOR_VALUE };
  uint16_t nextMedianDistance = 0;
  uint16_t minDistance = MAX_SENSOR_VALUE;
  uint16_t distance = MAX_SENSOR_VALUE;
  char* sensorLocation;
  // timestamp when the trigger signal was sent in us micros()
  uint32_t trigger = 0;
  volatile uint32_t start = 0;
  /* if end == 0 - a measurement is in progress */
  volatile uint32_t end = 1;

  int32_t echoDurationMicroseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];
  Median<uint16_t>*median = nullptr;
  // statistics
  uint32_t maxDurationUs = 0;
  uint32_t minDurationUs = UINT32_MAX;
  uint32_t lastDelayTillStartUs = 0;
  // counts how often no echo and also no timeout signal was received
  // should only happen with defect or missing sensors
  uint32_t numberOfNoSignals = 0;
  uint32_t numberOfLowAfterMeasurement = 0;
  uint32_t numberOfToLongMeasurement = 0;
  uint32_t numberOfInterruptAdjustments = 0;
  uint16_t numberOfTriggers = 0;
  bool measurementRead;
};

class HCSR04SensorManager {
  public:
    HCSR04SensorManager() {}
    virtual ~HCSR04SensorManager() {}
    void reset(uint32_t startMillisTicks);
    void registerSensor(const HCSR04SensorInfo &, uint8_t idx);
    void setOffsets(std::vector<uint16_t>);
    void setPrimarySensor(uint8_t idx);
    void detachInterrupts();
    void attachInterrupts();
    /* Returns the current raw median distance in cm for the
     * given sensor.
     */
    uint16_t getRawMedianDistance(uint8_t sensorId);
    /* Index for CSV. */
    uint16_t getCurrentMeasureIndex();
    uint32_t getMaxDurationUs(uint8_t sensorId);
    uint32_t getMinDurationUs(uint8_t sensorId);
    uint32_t getLastDelayTillStartUs(uint8_t sensorId);
    uint32_t getNoSignalReadings(const uint8_t sensorId);
    uint32_t getNumberOfLowAfterMeasurement(const uint8_t sensorId);
    uint32_t getNumberOfToLongMeasurement(const uint8_t sensorId);
    uint32_t getNumberOfInterruptAdjustments(const uint8_t sensorId);

    HCSR04SensorInfo m_sensors[NUMBER_OF_TOF_SENSORS];
    uint16_t sensorValues[NUMBER_OF_TOF_SENSORS];
    uint16_t lastReadingCount = 0;
    uint16_t startOffsetMilliseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];
    bool pollDistancesParallel();
    bool pollDistancesAlternating();

  protected:

  private:
    void sendTriggerToSensor(uint8_t sensorId);
    bool collectSensorResult(uint8_t sensorId);
    void setSensorTriggersToLow();
    bool collectSensorResults();
    void attachSensorInterrupt(uint8_t idx);
    uint32_t getFixedStart(size_t idx, HCSR04SensorInfo * const sensor);
    boolean isReadyForStart(uint8_t sensorId);
    void registerReadings();
    static uint16_t medianMeasure(HCSR04SensorInfo* const sensor, uint16_t value);
    static uint16_t median(uint16_t a, uint16_t b, uint16_t c);
    static uint16_t correctSensorOffset(uint16_t dist, uint16_t offset);
    static uint32_t microsBetween(uint32_t a, uint32_t b);
    static uint32_t microsSince(uint32_t a);
    static uint16_t millisSince(uint16_t milliseconds);
    static void updateStatistics(HCSR04SensorInfo * const sensor);
    uint32_t startReadingMilliseconds = 0;
    uint8_t primarySensor = 1;
    uint8_t lastSensor;
};

#endif
