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

#include "sensor.h"

#include "writer.h"
#include "FunctionalInterrupt.h"
/*
 * Sensor types:
 *  getLastDelayTillStartUs:
 *  - HC-SR04       = ~2900us / ~290us
 *  - JSN-SR04T-2.0 =  ~290us
 *
 *  getMaxDurationUs:
 *  - HC-SR04       =   70800us (102804µs!?)
 *  - JSN-SR04T-2.0 =  ~58200us
 *
 *  getMinDurationUs:
 *  - HC-SR04       =     82us
 *  - JSN-SR04T-2.0 =  ~1125us
 */

const uint16_t MAX_SENSOR_VALUE = 999;

static const uint16_t MIN_DISTANCE_MEASURED_CM =   2;
static const uint16_t MAX_DISTANCE_MEASURED_CM = 320; // candidate to check I could not get good readings above 300

static const uint32_t MIN_DURATION_MICRO_SEC = MIN_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER;
const uint32_t MAX_DURATION_MICRO_SEC = MAX_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER;

void DistanceSensorManager::initSensors() {
  m_sensors[0].sensorLocation = "Right";
  m_sensors[1].sensorLocation = "Left";

  sensor_queue = xQueueCreate( 40, sizeof( LLMessage ));
  if (sensor_queue == 0) {
    log_e("Oh wow. Sensor queue was not created successfully :(");
  } else {
    log_i("Sensor queue created");
  }

  TOFManager::startupManager(sensor_queue);
}

void DistanceSensorManager::reset(uint32_t startMillisTicks) {
  log_d("Reset");

  for (auto & sensor : m_sensors) {
    sensor.minDistance = MAX_SENSOR_VALUE;
    memset(&(sensor.echoDurationMicroseconds), 0, sizeof(sensor.echoDurationMicroseconds));
    sensor.numberOfTriggers = 0;
    delete sensor.median;
    sensor.median = new Median<uint16_t>(5, MAX_SENSOR_VALUE);
    sensor.rawDistance = MAX_SENSOR_VALUE;
  }

  memset(&(captureOffsetMS), 0, sizeof(captureOffsetMS));

  lastReadingCount = 0;
  sensorStartTimestampMS = startMillisTicks;
}

void DistanceSensorManager::setOffsets(std::vector<uint16_t> offsets) {
  for (size_t idx = 0; idx < NUMBER_OF_TOF_SENSORS; ++idx) {
    if (idx < offsets.size()) {
      getSensor(idx)->offset = offsets[idx];
    } else {
      getSensor(idx)->offset = 0;
    }
  }
}

/* Polls for new readings, if sensors are not ready, the
 * method returns false.
 */
bool DistanceSensorManager::pollDistances() {
  bool newMeasurements = false;
  LLMessage message = {0};

  while(xQueueReceive(sensor_queue, &message, 5) == pdPASS) {
    // // woohoo! we received a new measurement
    newMeasurements = true;
    processSensorMessage(&message);
  }

  return newMeasurements;
}

uint16_t DistanceSensorManager::computeDistance(uint32_t travel_time) {
  uint16_t distance = MAX_SENSOR_VALUE;
  if (travel_time > MIN_DURATION_MICRO_SEC || travel_time <= MAX_DURATION_MICRO_SEC) {
    distance = static_cast<uint16_t>(travel_time / MICRO_SEC_TO_CM_DIVIDER);
  }
  return distance;
}

void DistanceSensorManager::proposeMinDistance(DistanceSensor* const sensor, uint16_t distance) {
 if ((distance > 0) && (distance < sensor->minDistance)) {
    sensor->minDistance = distance;
  }
}

// FIXME
/* Returns true if there was a no timeout reading. */
bool DistanceSensorManager::processSensorMessage(LLMessage* message) {
  log_d("Processing sensor message from sensor = %i", message->sensor_index);
  DistanceSensor* const sensor = &m_sensors[message->sensor_index];
  bool reading_valid = false;

  uint32_t duration = microsBetween(message->start, message->end);

  uint16_t rawDistance = computeDistance(duration);

  sensor->rawDistance = rawDistance;
  sensor->median->addValue(rawDistance);
  uint16_t offsetDistance = computeOffsetDistance(medianMeasure(sensor, rawDistance), sensor->offset);
  sensor->distance = offsetDistance;
  sensor->echoDurationMicroseconds[lastReadingCount] = duration;
  sensor->rawDistance = rawDistance;

  captureOffsetMS[lastReadingCount] = millisSince(sensorStartTimestampMS);

  proposeMinDistance(sensor, offsetDistance);

  if(message->sensor_index == 1) {
    if (lastReadingCount < MAX_NUMBER_MEASUREMENTS_PER_INTERVAL - 1) {
      lastReadingCount++;
    }
  }

  return reading_valid;
}

uint16_t DistanceSensorManager::getSensorRawDistance(uint8_t sensorIndex) {
  return getSensor(sensorIndex)->rawDistance;
}

DistanceSensor* DistanceSensorManager::getSensor(uint8_t sensorIndex) {
  return &m_sensors[sensorIndex];
}

boolean DistanceSensorManager::hasReadings(uint8_t sensorIndex) {
  return lastReadingCount > 0;
}

void DistanceSensorManager::copyData(DataSet* set) {
  log_d("Let's copy some data.");
  for (int i = 0; i < NUMBER_OF_TOF_SENSORS; i++){
    set->sensorValues.push_back(m_sensors[i].minDistance);
  }

  set->measurements = lastReadingCount;
  memcpy(&(set->readDurationsRightInMicroseconds),
         &(m_sensors[0].echoDurationMicroseconds), set->measurements * sizeof(int32_t));
  memcpy(&(set->readDurationsLeftInMicroseconds),
         &(m_sensors[1].echoDurationMicroseconds), set->measurements * sizeof(int32_t));
  memcpy(&(set->startOffsetMilliseconds),
         &(captureOffsetMS), set->measurements * sizeof(uint16_t));
}

uint16_t DistanceSensorManager::getLastValidRawDistance(uint8_t sensorIndex) {
  // if there was no measurement of this sensor for the current index, it is the
  // one before. This happens with fast confirmations.
  uint16_t val = 0;
  for (int i = lastReadingCount - 1; i >= 0; i--) {
    uint16_t maybeVal = getSensor(sensorIndex)->echoDurationMicroseconds[i];
    if (maybeVal != 0) {
      val = maybeVal;
      break;
    }
  }
  return val;
}

uint16_t DistanceSensorManager::getMedianRawDistance(uint8_t sensorId) {
 return m_sensors[sensorId].median->median();
}

uint32_t DistanceSensorManager::getMaxDurationUs(uint8_t sensorId) {
  return m_sensors[sensorId].maxDurationUs;
}

uint32_t DistanceSensorManager::getMinDurationUs(uint8_t sensorId) {
  return m_sensors[sensorId].minDurationUs;
}

uint32_t DistanceSensorManager::getLastDelayTillStartUs(uint8_t sensorId) {
  return m_sensors[sensorId].lastDelayTillStartUs;
}

uint32_t DistanceSensorManager::getNoSignalReadings(const uint8_t sensorId) {
  return m_sensors[sensorId].numberOfNoSignals;
}

uint32_t DistanceSensorManager::getNumberOfLowAfterMeasurement(const uint8_t sensorId) {
  return m_sensors[sensorId].numberOfLowAfterMeasurement;
}

uint32_t DistanceSensorManager::getNumberOfToLongMeasurement(const uint8_t sensorId) {
  return m_sensors[sensorId].numberOfToLongMeasurement;
}

uint32_t DistanceSensorManager::getNumberOfInterruptAdjustments(const uint8_t sensorId) {
  return m_sensors[sensorId].numberOfInterruptAdjustments;
}

/* During debugging I observed readings that did not get `start` updated
 * By the interrupt. Since we also set start when we send the pulse to the
 * sensor this adds 300 microseconds or 5 centimeters to the measured result.
 * After research, is a bug in the ESP!? See https://esp32.com/viewtopic.php?t=10124
 */
// FIXME
/* uint32_t HCSR04SensorManager::getFixedStart(
  uint32_t start = 0;
  size_t idx, HCSR04SensorInfo * const sensor) {
   uint32_t start = sensor->start;
  // the error appears if both sensors trigger the interrupt at the exact same
  // time, if this happens, trigger time == start time
  if (start == sensor->trigger && sensor->end != MEASUREMENT_IN_PROGRESS) {
    // it should be save to use the start value from the other sensor.
    const uint32_t alternativeStart = m_sensors[1 - idx].start;
    if (alternativeStart != m_sensors[1 - idx].trigger
      && microsBetween(alternativeStart, start) < 500) { // typically 290-310 microseconds {
      start = alternativeStart;
      sensor->numberOfInterruptAdjustments++;
    }
  }
  return start;
}
 */

uint16_t DistanceSensorManager::computeOffsetDistance(uint16_t dist, uint16_t offset) {
  uint16_t  result;
  if (dist == MAX_SENSOR_VALUE) {
    result = MAX_SENSOR_VALUE;
  } else if (dist > offset) {
    result = dist - offset;
  } else {
    result = 0; // would be negative if corrected
  }
  return result;
}

void DistanceSensorManager::updateStatistics(LLMessage* message) {
#if 1
  DistanceSensor* sensor = getSensor(message->sensor_index);

  auto start_delay = message->start - message->trigger;
  if (start_delay > 0) {
    sensor->lastDelayTillStartUs = start_delay;

    auto duration = message->end - message->start;
    if (duration > sensor->maxDurationUs) {
      sensor->maxDurationUs = duration;
    }
    if (duration < sensor->minDurationUs && duration > 1) {
      sensor->minDurationUs = duration;
    }
  } else {
    sensor->numberOfNoSignals++;
  }
#endif
}

/* Determines the microseconds between the 2 time counters given.
 * Internally we only count 32bit, this overflows after around 71 minutes,
 * so we take care for the overflow. Times we measure are way below the
 * possible maximum.
 * We should not use 64bit variables for "start" and "end" because access
 * to 64bit vars is not atomic for our 32bit cpu.
 */
uint32_t DistanceSensorManager::microsBetween(uint32_t a, uint32_t b) {
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
uint32_t DistanceSensorManager::microsSince(uint32_t a) {
  return microsBetween(micros(), a);
}

uint16_t DistanceSensorManager::millisSince(uint16_t milliseconds) {
  uint16_t result = ((uint16_t) millis()) - milliseconds;
  if (result & 0x8000) {
    result = -result;
  }
  return result;
}

uint16_t DistanceSensorManager::medianMeasure(DistanceSensor *const sensor, uint16_t value) {
  sensor->distances[sensor->nextMedianDistance] = value;
  sensor->nextMedianDistance++;

  // if we got "fantom" measures, they are <= the current measures, so remove
  // all values <= the current measure from the median data
  for (unsigned short & distance : sensor->distances) {
    if (distance < value) {
      distance = value;
    }
  }
  if (sensor->nextMedianDistance >= MEDIAN_DISTANCE_MEASURES) {
    sensor->nextMedianDistance = 0;
  }
  return median(sensor->distances[0], sensor->distances[1], sensor->distances[2]);
}

uint16_t DistanceSensorManager::median(uint16_t a, uint16_t b, uint16_t c) {
  if (a < b) {
    if (a >= c) {
      return a;
    } else if (b < c) {
      return b;
    }
  } else {
    if (a < c) {
      return a;
    } else if (b >= c) {
      return b;
    }
  }
  return c;
}

