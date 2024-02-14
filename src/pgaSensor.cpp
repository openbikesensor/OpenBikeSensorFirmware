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
  digitalWrite(sensorInfo.io_pin, LOW);
  pinMode(sensorInfo.io_pin, INPUT);  // IO pin is open drain
  digitalWrite(sensorInfo.sck_pin, LOW);
  pinMode(sensorInfo.sck_pin, OUTPUT);
  pinMode(sensorInfo.mosi_pin, OUTPUT);
  pinMode(sensorInfo.miso_pin, INPUT);

  while(1)
  {
    uint8_t reg = spiRegRead(sensor_addr, 0x4c);
    Serial.printf("Reg: 0x%02x\n", reg);
    delay(100);
  }
  while(1);

  // Reset the TCI
  //tciReset(sensor_addr);

  /*uint8_t data[1];
  while(1)
  {
    //tciWriteBit(sensor_addr, true);
    //tciWriteBit(sensor_addr, false);
    tciReset(sensor_addr);
    tciReadData(sensor_addr, 0, data, 8);
    safe_usleep(10000);
  }*/

}

// Reset the TCI interface by driving the IO pin low for >15 ms
void PGASensorManager::tciReset(int sensor_addr)
{
  PGASensorInfo &sensorInfo = m_sensors[sensor_addr];
  pinMode(sensorInfo.io_pin, OUTPUT);
  safe_usleep(20000);
  pinMode(sensorInfo.io_pin, INPUT);
  safe_usleep(100);
}

void PGASensorManager::tciWriteBit(int sensor_addr, bool val)
{
  PGASensorInfo &sensorInfo = m_sensors[sensor_addr];
  pinMode(sensorInfo.io_pin, OUTPUT);
  if(val)
    usleep(100-22);
  else
    usleep(200-22);
  pinMode(sensorInfo.io_pin, INPUT);
  if(val)
    usleep(200-22);
  else
    usleep(100-22);
}

void PGASensorManager::tciWriteByte(int sensor_addr, uint8_t val)
{

}

bool PGASensorManager::tciReadData(int sensor_addr, uint8_t index, uint8_t *data, int bits)
{
  uint8_t io_pin = m_sensors[sensor_addr].io_pin;

  noInterrupts();
  pinMode(io_pin, OUTPUT);
  safe_usleep(1270-22);  // tCFG_TCI, Device configuration command period
  pinMode(io_pin, INPUT);
  safe_usleep(100-22);  // tDT_TCI, Command processing dead-time
  tciWriteBit(sensor_addr, false);  // Read
  safe_usleep(100-22);  // tDT_TCI, Command processing dead-time

  // Write register index, lower 4 bits, MSB first
  for(int i = 0; i < 4; i++)
  {
    tciWriteBit(sensor_addr, !!(index & 0x08));
    index <<= 1;
  }

  // Reset the checksum state
  checksum_clear();

  // Read data bits
  int byte_offset = 0;
  data[0] = 0;
  uint8_t bit_offset = 7;
  for(int i = 0; i < bits; i++)
  {
    uint8_t val = tciReadBit(io_pin);
    checksum_append_bit(val);  // Update checksum
    data[byte_offset] |= val << bit_offset;
    if(bit_offset == 0)
    {
      byte_offset++;
      data[byte_offset] = 0;
      bit_offset = 7;
    }
    else
      bit_offset--;
  }

  // Read checksum
  uint8_t checksum = 0;
  for(uint8_t i = 0; i < 8; i++)
    checksum |= tciReadBit(io_pin) << (7 - i);
  interrupts();

  Serial.printf(", Data: ");
  for(int i = 0; i < (bits + 7)/8; i++)
    Serial.printf("0x%02x, ", data[i]);
  Serial.printf("Checksum (pga): 0x%02x, ", checksum);
  Serial.printf("Checksum (local): 0x%02x\n", checksum_get());

  return checksum == checksum_get();
}

uint8_t PGASensorManager::tciReadBit(uint8_t io_pin)
{
  while(!digitalRead(io_pin));  // Wait for high level
  while(digitalRead(io_pin));  // Wait for falling edge
  safe_usleep(150-10);  // Wait for sample time

  uint8_t val = digitalRead(io_pin);  // Sample the bit
  pinMode(io_pin, OUTPUT);
  pinMode(io_pin, INPUT);

  return val;
}

// A usleep function that also works if interrupts are disabled
void PGASensorManager::safe_usleep(unsigned long us)
{
  unsigned long tstart = micros();
  while(micros() - tstart < us);
}

uint8_t PGASensorManager::spiTransfer(uint8_t sensor_addr, uint8_t data_out)
{
  PGASensorInfo &sensorInfo = m_sensors[sensor_addr];
  uint8_t data_in = 0;
  // The bit timing may not be faster than 18 MHz clock signal. The arduino framework is so slow, that this seems to be no problem...
  for(uint8_t i = 0; i < 8; i++)
  {
    digitalWrite(sensorInfo.sck_pin, HIGH);
    digitalWrite(sensorInfo.mosi_pin, data_out&0x01);
    data_out >>= 1;
    digitalWrite(sensorInfo.sck_pin, LOW);
    data_in >>= 1;
    data_in |= digitalRead(sensorInfo.miso_pin)<<7;
  }
  return data_in;
}

// Returns the register value or -1 on checksum error
int PGASensorManager::spiRegRead(uint8_t sensor_addr, uint8_t reg_addr)
{
  PGASensorInfo &sensorInfo = m_sensors[sensor_addr];

  // Transmit
  checksum_clear();
  spiTransfer(sensor_addr, 0x55);  // Sync byte
  spiTransfer(sensor_addr, 0<<5 | 9); // Command: chip address + register read
  checksum_append_byte(0<<5 | 9);
  spiTransfer(sensor_addr, reg_addr); // Register address
  checksum_append_byte(reg_addr);
  spiTransfer(sensor_addr, checksum_get());  // Checksum
  uint8_t tx_checksum = checksum_get();

  // Receive
  checksum_clear();
  uint8_t diag_data = spiTransfer(sensor_addr, 0x00);
  checksum_append_byte(diag_data);
  uint8_t reg_val = spiTransfer(sensor_addr, 0x00);
  checksum_append_byte(reg_val);
  uint8_t checksum = spiTransfer(sensor_addr, 0x00);

  //Serial.printf("Data: 0x%02x, Checksum (pga): 0x%02x, Checksum (local): 0x%02x, Checksum (tx): %02x\n", reg_val, checksum, checksum_get(), tx_checksum);

  if(checksum != checksum_get())
    return -1;

  return reg_val;
}

void PGASensorManager::checksum_clear()
{
  checksum_sum = 0;
  checksum_tmp_data = 0;
  checksum_offset = 7;
  checksum_carry = 0;
}

void PGASensorManager::checksum_append_bit(uint8_t val)
{
  checksum_tmp_data |= (val << checksum_offset);
  if(!checksum_offset)
  {
    // Add tmp_data and last carry to sum and calculate new carry
    checksum_append_byte(checksum_tmp_data);
  }
  else
    checksum_offset--;
}

void PGASensorManager::checksum_append_byte(uint8_t val)
{
    // Add tmp_data and last carry to sum and calculate new carry
    uint16_t sum = (uint16_t)checksum_sum + (uint16_t)val + (uint16_t)checksum_carry;
    checksum_carry = sum >> 8;
    checksum_sum = (uint8_t)sum;
    checksum_offset = 7;
    checksum_tmp_data = 0;
}

uint8_t PGASensorManager::checksum_get()
{
  // Add zero bits until we added the last padded byte
  while(checksum_offset != 7)
    checksum_append_bit(0);
  return ~(checksum_sum + checksum_carry);  // Checksum is the inverted sum + carry
}

#endif  // OBSPro
