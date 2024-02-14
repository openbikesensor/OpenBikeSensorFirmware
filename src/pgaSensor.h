#include <Arduino.h>
#include "variant.h"

#ifndef OBS_PGASENSOR_H
#define OBS_PGASENSOR_H
#ifdef OBSPRO  // PGASensor is only available on OBSPro

struct PGASensorInfo
{
  uint8_t io_pin;
  uint8_t sck_pin;
  uint8_t mosi_pin;
  uint8_t miso_pin;
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

  // TCI Interface
  void tciReset(int sensor_addr);
  void tciWriteBit(int sensor_addr, bool val);
  void tciWriteByte(int sensor_addr, uint8_t val);
  bool tciReadData(int sensor_addr, uint8_t index, uint8_t *data, int bits);
  uint8_t tciReadBit(uint8_t io_pin);
  void safe_usleep(unsigned long us);

  // Synchronous UART mode (aka SPI without chip-select)
  uint8_t spiTransfer(uint8_t sensor_addr, uint8_t data_out);
  int spiRegRead(uint8_t sensor_addr, uint8_t reg_addr);

  // Checksum
  uint8_t checksum_sum;
  uint8_t checksum_carry;
  uint8_t checksum_tmp_data;
  uint8_t checksum_offset;
  void checksum_clear();
  void checksum_append_bit(uint8_t val);
  void checksum_append_byte(uint8_t val);
  uint8_t checksum_get();
};

#endif  // OBSPRO
#endif  // OBS_PGASENSOR_H
