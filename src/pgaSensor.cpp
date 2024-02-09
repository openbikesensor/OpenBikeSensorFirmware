#include "pgaSensor.h"

#ifdef OBSPRO

PGASensorManager::PGASensorManager()
{
  for(int i = 0; i < NUMBER_OF_TOF_SENSORS; i++)
    sensorValues[i] = 0;
}

PGASensorManager::~PGASensorManager()
{

}

void PGASensorManager::registerSensor(const PGASensorInfo &sensorInfo, uint8_t sensor_addr)
{
  if (sensor_addr >= NUMBER_OF_TOF_SENSORS) {
    log_e("Can not register sensor for index %d, only %d tof sensors supported", sensor_addr, NUMBER_OF_TOF_SENSORS);
    return;
  }
  m_sensors[sensor_addr] = sensorInfo;
  m_sensors[sensor_addr].numberOfTriggers = 0;
  setupSensor(sensor_addr);
}

// Guess: Return true if the last measurement is complete and a new one is triggered?
bool PGASensorManager::pollDistancesAlternating()
{
  return false;
}

uint16_t PGASensorManager::getCurrentMeasureIndex()
{
  return 1;
}

uint16_t PGASensorManager::getRawMedianDistance(uint8_t sensorId)
{
  return 30000;
}

uint32_t PGASensorManager::getMaxDurationUs(uint8_t sensorId)
{
  return 3000000;
}

uint32_t PGASensorManager::getMinDurationUs(uint8_t sensorId)
{
  return 4000000;
}

uint32_t PGASensorManager::getLastDelayTillStartUs(uint8_t sensorId)
{
  return 5000000;
}

uint32_t PGASensorManager::getNoSignalReadings(const uint8_t sensorId)
{
  return 6000000;
}

uint32_t PGASensorManager::getNumberOfLowAfterMeasurement(const uint8_t sensorId)
{
  return 7000000;
}

uint32_t PGASensorManager::getNumberOfToLongMeasurement(const uint8_t sensorId)
{
  return 8000000;
}

uint32_t PGASensorManager::getNumberOfInterruptAdjustments(const uint8_t sensorId)
{
  return 9000000;
}

void PGASensorManager::setupSensor(int sensor_addr)
{
  PGASensorInfo &sensorInfo = m_sensors[sensor_addr];
  // Set pin modes
  pinMode(sensorInfo.io_pin, INPUT);  // IO pin is open drain
  digitalWrite(sensorInfo.io_pin, LOW);

  // Reset the TCI "bus"
  tciReset(sensor_addr);

  // Write the UART address using TCI
  //tciWriteReg()
}

// Reset the TCI interface by driving it low for >15 ms
void PGASensorManager::tciReset(int sensor_addr)
{
  PGASensorInfo &sensorInfo = m_sensors[sensor_addr];
  pinMode(sensorInfo.io_pin, OUTPUT);
  delay(20);
  pinMode(sensorInfo.io_pin, INPUT);
}

#endif  // OBSPro
