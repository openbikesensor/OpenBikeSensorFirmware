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

#include <Arduino.h>
#include <vector>

#include "utils/median.h"

#include "tofmanager.h"

struct DataSet;

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
const uint16_t MAX_NUMBER_MEASUREMENTS_PER_INTERVAL = 30; //  is 1000/SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC/2
extern const uint16_t MAX_SENSOR_VALUE;

const uint8_t NUMBER_OF_TOF_SENSORS = 2;

struct DistanceSensor {
  // offset from the sensor to the end of the handlebar (cm)
  uint16_t offset = 0;
  // the distance from the sensor to some object (cm)
  uint16_t rawDistance = 0;
  // median of the offset distance (cm)
  uint16_t distances[MEDIAN_DISTANCE_MEASURES] = { MAX_SENSOR_VALUE, MAX_SENSOR_VALUE, MAX_SENSOR_VALUE };
  // index into the previous member
  uint16_t nextMedianDistance = 0;
  // minimum offset distance the sensor noticed between resets (cm)
  uint16_t minDistance = MAX_SENSOR_VALUE;
  // current offset distance of the sensor (cm)
  uint16_t distance = MAX_SENSOR_VALUE;

  // UI string that denotes the sensor position
  char const* sensorLocation;

  // an array with all of the time-of-flight measurements since the last reset
  int32_t echoDurationMicroseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];

  // a data structure for computing the median offset distance since the last reset (cm)
  Median<uint16_t>*median = nullptr;

  // statistics

  // maximum time-of-flight the sensor measured since the last reset (microseconds)
  uint32_t maxDurationUs = 0;
  // minimum time-of-flight the sensor measured since the last reset (microseconds)
  uint32_t minDurationUs = UINT32_MAX;
  // internal delay to start metric
  uint32_t lastDelayTillStartUs = 0;

  // FIXME: internal statistics
  // counts how often no echo and also no timeout signal was received
  // should only happen with defect or missing sensors
  uint32_t numberOfNoSignals = 0;
  uint32_t numberOfLowAfterMeasurement = 0;
  uint32_t numberOfToLongMeasurement = 0;
  uint32_t numberOfInterruptAdjustments = 0;
  uint16_t numberOfTriggers = 0;
};

class DistanceSensorManager {
  public:
    DistanceSensorManager() {}
    virtual ~DistanceSensorManager() {}
    void reset(uint32_t startMillisTicks);
    void setOffsets(std::vector<uint16_t>);
    void initSensors();

    /* Returns the current raw median distance in cm for the
     * given sensor.
     */
    uint16_t getMedianRawDistance(uint8_t sensorId);
    /* Index for CSV. */
    uint32_t getMaxDurationUs(uint8_t sensorId);
    uint32_t getMinDurationUs(uint8_t sensorId);
    uint32_t getLastDelayTillStartUs(uint8_t sensorId);
    uint32_t getNoSignalReadings(const uint8_t sensorId);
    uint32_t getNumberOfLowAfterMeasurement(const uint8_t sensorId);
    uint32_t getNumberOfToLongMeasurement(const uint8_t sensorId);
    uint32_t getNumberOfInterruptAdjustments(const uint8_t sensorId);

    DistanceSensor* getSensor(uint8_t sensorIndex);
    uint16_t getSensorRawDistance(uint8_t sensorIndex);
    uint16_t getLastValidRawDistance(uint8_t sensorIndex);
    boolean hasReadings(uint8_t sensorIndex);
    void copyData(DataSet* set);

    bool pollDistances();

  protected:
    uint16_t captureOffsetMS[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];
    DistanceSensor m_sensors[NUMBER_OF_TOF_SENSORS];
    void updateStatistics(LLMessage* message);
    uint16_t lastReadingCount = 0;

    /* offer a new value for the minimum distance */
    void proposeMinDistance(DistanceSensor* const sensor, uint16_t distance);

    /* compute the distance to an object considering `travel_time` */
    uint16_t computeDistance(uint32_t travel_time);

  private:
    static uint16_t medianMeasure(DistanceSensor* const sensor, uint16_t value);
    static uint16_t median(uint16_t a, uint16_t b, uint16_t c);
    static uint16_t computeOffsetDistance(uint16_t dist, uint16_t offset);
    static uint32_t microsBetween(uint32_t a, uint32_t b);
    static uint32_t microsSince(uint32_t a);
    static uint16_t millisSince(uint16_t milliseconds);

    bool processSensorMessage(LLMessage* message);
    uint32_t getFixedStart(size_t idx, DistanceSensor * const sensor);
    uint32_t sensorStartTimestampMS = 0;

    QueueHandle_t sensor_queue;
    uint8_t* queue_buffer;
    StaticQueue_t *static_queue;
};

#endif
