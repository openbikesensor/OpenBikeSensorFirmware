#include <Arduino.h>
#include "variant.h"

#ifndef OBS_PGASENSOR_H
#define OBS_PGASENSOR_H
#ifdef OBSPRO  // PGASensor is only available on OBSPro

struct PGASensorInfo
{
  uint8_t io_pin;
  uint16_t numberOfTriggers;
  int32_t echoDurationMicroseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];
  uint16_t rawDistance = 42;  // Current distance value in cm
  uint16_t minDistance = MAX_SENSOR_VALUE;
  uint16_t distance = MAX_SENSOR_VALUE;
  const char* sensorLocation;
};

class PGASensorManager
{
public:
  PGASensorManager();
  ~PGASensorManager();
  void registerSensor(const PGASensorInfo &sensorInfo, uint8_t idx);
  bool pollDistancesAlternating();
  uint16_t getCurrentMeasureIndex();
  uint16_t getRawMedianDistance(uint8_t sensorId);
  uint32_t getMaxDurationUs(uint8_t sensorId);
  uint32_t getMinDurationUs(uint8_t sensorId);
  uint32_t getLastDelayTillStartUs(uint8_t sensorId);
  uint32_t getNoSignalReadings(const uint8_t sensorId);
  uint32_t getNumberOfLowAfterMeasurement(const uint8_t sensorId);
  uint32_t getNumberOfToLongMeasurement(const uint8_t sensorId);
  uint32_t getNumberOfInterruptAdjustments(const uint8_t sensorId);
  void detachInterrupts() {};
  void attachInterrupts() {};

  // TODO: These should not be public!
  PGASensorInfo m_sensors[NUMBER_OF_TOF_SENSORS];
  uint16_t sensorValues[NUMBER_OF_TOF_SENSORS];
  uint16_t lastReadingCount = 0;
  uint16_t startOffsetMilliseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];

protected:

  void setupSensor(int idx);
  void tciReset(int idx);
};

#endif  // OBSPRO
#endif  // OBS_PGASENSOR_H
