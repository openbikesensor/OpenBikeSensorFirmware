#include "pgaSensor.h"

#ifdef OBSPRO

PGASensorManager::PGASensorManager()
{
  for(int i = 0; i < NUMBER_OF_TOF_SENSORS; i++)
    sensorValues[i] = 0;
  lastTriggerTimeMs = millis();
}

PGASensorManager::~PGASensorManager()
{

}

void PGASensorManager::registerSensor(const PGASensorInfo &sensorInfo, uint8_t sensorId)
{
  if (sensorId >= NUMBER_OF_TOF_SENSORS) {
    log_e("Can not register sensor for index %d, only %d tof sensors supported", sensorId, NUMBER_OF_TOF_SENSORS);
    return;
  }
  m_sensors[sensorId] = sensorInfo;
  m_sensors[sensorId].numberOfTriggers = 0;
  sensorValues[sensorId] = MAX_SENSOR_VALUE;
  m_sensors[sensorId].median = new Median<uint16_t>(5, MAX_SENSOR_VALUE);
  setupSensor(sensorId);
}

// Guess: Returns true if both sensor results are available
bool PGASensorManager::pollDistancesAlternating()
{
  bool newMeasurements = false;

  // If current sensor is not ready, return
  if(spiIsBusy(lastSensor))
    return false;

  uint8_t nextSensor = 1 - lastSensor;

  // Check if the next sensor is the primary sensor, which means that all sensors have been
  // triggered. Then collect the results of both.
  if(nextSensor == primarySensor)
    newMeasurements = collectSensorResults();

  // Trigger next sensor
#if PGA_DUMP_ENABLE
  // DEBUG: request a raw data dump from time to time
  if(millis() - m_sensors[nextSensor].lastDumpTime > PGA_DUMP_TIME)
  {
    spiRegWrite(nextSensor, PGA_REG_EE_CNTRL, PGA_DATADUMP_EN(1));
    m_sensors[nextSensor].lastDumpTime = millis();
    m_sensors[nextSensor].lastMeasurementWasDump = true;
  }
  else
  {
    spiRegWrite(nextSensor, PGA_REG_EE_CNTRL, PGA_DATADUMP_EN(0));
    m_sensors[nextSensor].lastMeasurementWasDump = false;
  }
#endif
  spiBurstAndListen(nextSensor, 1, 1);
  m_sensors[nextSensor].numberOfTriggers++;

  lastSensor = nextSensor;
  return newMeasurements;
}

uint16_t PGASensorManager::getCurrentMeasureIndex()
{
  return lastReadingCount - 1;
}

void PGASensorManager::setOffsets(std::vector<uint16_t> offsets)
{
  for (size_t idx = 0; idx < NUMBER_OF_TOF_SENSORS; ++idx) {
    if (idx < offsets.size()) {
      m_sensors[idx].offset = offsets[idx];
    } else {
      m_sensors[idx].offset = 0;
    }
  }
}

/* The primary sensor defines the measurement interval, we trigger a measurement if this
 * sensor is ready.
 */
void PGASensorManager::setPrimarySensor(uint8_t idx) {
  primarySensor = idx;
}

void PGASensorManager::reset(uint32_t startMillisTicks) {
  for (auto & sensor : m_sensors) {
    sensor.minDistance = MAX_SENSOR_VALUE;
    memset(&(sensor.echoDurationMicroseconds), 0, sizeof(sensor.echoDurationMicroseconds));
    sensor.numberOfTriggers = 0;
  }
  lastReadingCount = 0;
  lastSensor = 1 - primarySensor;
  memset(&(startOffsetMilliseconds), 0, sizeof(startOffsetMilliseconds));
  startReadingMilliseconds = startMillisTicks;
}

void PGASensorManager::setupSensor(int sensorId)
{
  PGASensorInfo &sensorInfo = m_sensors[sensorId];

  // Set pin modes
  digitalWrite(sensorInfo.sck_pin, LOW);
  pinMode(sensorInfo.sck_pin, OUTPUT);
  pinMode(sensorInfo.mosi_pin, OUTPUT);
  pinMode(sensorInfo.miso_pin, INPUT);

  // Check communication
  // Rerad the DEV_STAT0 register, where the upper nibble should be 0x4
  // These are the values for REV_ID (0x2) and OPT_ID (0x0)
  if(spiRegRead(sensorId, PGA_REG_DEV_STAT0) & 0xf0 != 0x40)
  {
    log_e("Setup of sensor %d failed: Unexpected result in status register read", sensorId);
    return;
  }

  // Basic setup
  spiRegWrite(sensorId, PGA_REG_INIT_GAIN, PGA_BPF_BW(0) | PGA_GAIN_INIT(0));  // 2 kHz bandwidth, Init_Gain = 0.5 × (GAIN_INIT+1) + value(AFE_GAIN_RNG) [dB]
  spiRegWrite(sensorId, PGA_REG_FREQUENCY, 55);  // 55 = 41 kHz, Frequency = 0.2 × FREQ + 30 [kHz]
  //spiRegWrite(sensorId, PGA_REG_DEADTIME, 0x00);  // Deglitch not described in DS, deadtime only relevant for direct drive mode
  spiRegWrite(sensorId, PGA_REG_PULSE_P1, PGA_IO_IF_SEL(0) | PGA_UART_DIAG(0) | PGA_IO_DIS(0) | P1_PULSE(17)); // Number of pulses
  spiRegWrite(sensorId, PGA_REG_CURR_LIM_P1, PGA_DIS_CL(0) | PGA_CURR_LIM1(0));  // CURR_LIM1*7mA + 50mA
  spiRegWrite(sensorId, PGA_REG_CURR_LIM_P2, PGA_LPF_CO(0) | PGA_CURR_LIM2(0));  // CURR_LIM1*7mA + 50mA
  spiRegWrite(sensorId, PGA_REG_REC_LENGTH, PGA_P1_REC(8) | PGA_P2_REC(0));  // Record time = 4.096 × (Px_REC + 1) [ms], 8 = 36,9ms = 6m range
  spiRegWrite(sensorId, PGA_REG_DECPL_TEMP, PGA_AFE_GAIN_RNG(1) | PGA_LPM_EN(0) | PGA_DECPL_TEMP_SEL(0) | PGA_DECPL_T(0));  // Time = 4096 × (DECPL_T + 1) [μs], 0 = 4ms = 0,66m?!?
  spiRegWrite(sensorId, PGA_REG_EE_CNTRL, PGA_DATADUMP_EN(0));  // Disable data dump

  // Threshold map, then check DEV_STAT0.THR_CRC_ERR
  // th_t = np.array([2000, 5200, 2400, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000])
  // th_l = np.array([200, 80, 16, 16, 16, 24, 24, 24, 24, 24, 24, 24])
  PGAThresholds thresholds(
    TH_TIME_DELTA_2000US, 200/8,
    TH_TIME_DELTA_5200US, 80/8,
    TH_TIME_DELTA_2400US, 16/8,
    TH_TIME_DELTA_8000US, 16/8,
    TH_TIME_DELTA_8000US, 16/8,
    TH_TIME_DELTA_8000US, 24/8,
    TH_TIME_DELTA_8000US, 24/8,
    TH_TIME_DELTA_8000US, 24/8,
    TH_TIME_DELTA_8000US, 24,
    TH_TIME_DELTA_8000US, 24,
    TH_TIME_DELTA_8000US, 24,
    TH_TIME_DELTA_8000US, 24
  );
  spiRegWriteThesholds(sensorId, 1, thresholds);

  spiRegWrite(sensorId, PGA_REG_DEV_STAT1, 0x00);  // Clear stat1 register
  safe_usleep(1000);  // Wait a bit
  log_i("DEV_STAT0: 0x%02x", spiRegRead(sensorId, PGA_REG_DEV_STAT0));
  log_i("DEV_STAT1: 0x%02x", spiRegRead(sensorId, PGA_REG_DEV_STAT1));

  log_i("Setup of sensor %d done", sensorId);
}

// A usleep function that also works if interrupts are disabled
void PGASensorManager::safe_usleep(unsigned long us)
{
  unsigned long tstart = micros();
  while(micros() - tstart < us);
}

uint8_t PGASensorManager::spiTransfer(uint8_t sensorId, uint8_t data_out)
{
  assert(sensorId >= 0 && sensorId <= 1);
  PGASensorInfo &sensorInfo = m_sensors[sensorId];
  uint8_t data_in = 0;
  for(uint8_t i = 0; i < 8; i++)
  {
    digitalWrite(sensorInfo.sck_pin, HIGH);
    digitalWrite(sensorInfo.mosi_pin, data_out&0x01);
    data_out >>= 1;
    safe_usleep(2);
    digitalWrite(sensorInfo.sck_pin, LOW);
    data_in >>= 1;
    data_in |= digitalRead(sensorInfo.miso_pin)<<7;
    safe_usleep(2);
  }
  return data_in;
}

// Returns the register value or -1 on checksum error
// pdiag: return the value of the diag byte, can be nullptr
int PGASensorManager::spiRegRead(uint8_t sensorId, uint8_t reg_addr, uint8_t *pdiag)
{
  PGASensorInfo &sensorInfo = m_sensors[sensorId];

  // Transmit
  checksum_clear();
  spiTransfer(sensorId, 0x55);  // Sync byte
  spiTransfer(sensorId, 0<<5 | PGA_CMD_REGISTER_READ); // Command: chip address + register read
  checksum_append_byte(0<<5 | PGA_CMD_REGISTER_READ);
  spiTransfer(sensorId, reg_addr); // Register address
  checksum_append_byte(reg_addr);
  spiTransfer(sensorId, checksum_get());  // Checksum
  uint8_t tx_checksum = checksum_get();

  // Receive
  checksum_clear();
  uint8_t diag = spiTransfer(sensorId, 0x00);
  checksum_append_byte(diag);
  if(pdiag != nullptr)
    *pdiag = diag;
  uint8_t reg_val = spiTransfer(sensorId, 0x00);
  checksum_append_byte(reg_val);
  uint8_t checksum = spiTransfer(sensorId, 0x00);

  //Serial.printf("Data: 0x%02x, Checksum (pga): 0x%02x, Checksum (local): 0x%02x, Checksum (tx): %02x\n", reg_val, checksum, checksum_get(), tx_checksum);

  if(checksum != checksum_get())
    return -1;

  return reg_val;
}

void PGASensorManager::spiRegWrite(uint8_t sensorId, uint8_t reg_addr, uint8_t value)
{
  assert(sensorId >= 0 && sensorId <= 1);

  checksum_clear();
  spiTransfer(sensorId, 0x55);  // Sync byte
  spiTransfer(sensorId, 0<<5 | PGA_CMD_REGISTER_WRITE); // Command: chip address + register write
  checksum_append_byte(0<<5 | PGA_CMD_REGISTER_WRITE);
  spiTransfer(sensorId, reg_addr);  // Register address
  checksum_append_byte(reg_addr);
  spiTransfer(sensorId, value);  // Register address
  checksum_append_byte(value);
  spiTransfer(sensorId, checksum_get());  // Checksum

  // Wait a bit to apply changes, depending on the register
  if(reg_addr == PGA_REG_INIT_GAIN || (reg_addr >= PGA_REG_TVGAIN0 & reg_addr <= PGA_REG_TVGAIN6) || (reg_addr >= PGA_REG_P1_THR_0 && reg_addr <= PGA_REG_P2_THR_15) ||
    reg_addr == PGA_REG_P1_GAIN_CTRL || reg_addr == PGA_REG_P2_GAIN_CTRL)
    safe_usleep(61);
  else
    safe_usleep(5);
}

void PGASensorManager::spiRegWriteThesholds(uint8_t sensorId, uint8_t preset, PGAThresholds &thresholds)
{
  assert(sensorId >= 0 && sensorId <= 1);

  uint8_t regOffset = preset == 1 ? 0 : PGA_REG_P2_THR_0 - PGA_REG_P1_THR_0;
  spiRegWrite(sensorId, PGA_REG_P1_THR_0 + regOffset, thresholds.t1<<4 | thresholds.t2);
  spiRegWrite(sensorId, PGA_REG_P1_THR_1 + regOffset, thresholds.t3<<4 | thresholds.t4);
  spiRegWrite(sensorId, PGA_REG_P1_THR_2 + regOffset, thresholds.t5<<4 | thresholds.t6);
  spiRegWrite(sensorId, PGA_REG_P1_THR_3 + regOffset, thresholds.t7<<4 | thresholds.t8);
  spiRegWrite(sensorId, PGA_REG_P1_THR_4 + regOffset, thresholds.t9<<4 | thresholds.t10);
  spiRegWrite(sensorId, PGA_REG_P1_THR_5 + regOffset, thresholds.t11<<4 | thresholds.t12);
  // Now it gets messy with the 5 bit level values...
  spiRegWrite(sensorId, PGA_REG_P1_THR_6 + regOffset, thresholds.l1<<3 | thresholds.l2>>2);
  spiRegWrite(sensorId, PGA_REG_P1_THR_7 + regOffset, thresholds.l2<<6 | thresholds.l3<<1 | thresholds.l4>>4);
  spiRegWrite(sensorId, PGA_REG_P1_THR_8 + regOffset, thresholds.l4<<4 | thresholds.l5>>1);
  spiRegWrite(sensorId, PGA_REG_P1_THR_9 + regOffset, thresholds.l5<<7 | thresholds.l6<<2 | thresholds.l7>>3);
  spiRegWrite(sensorId, PGA_REG_P1_THR_10 + regOffset, thresholds.l7<<5 | thresholds.l8);
  // 8 bit level values are easy again...
  spiRegWrite(sensorId, PGA_REG_P1_THR_11 + regOffset, thresholds.l9);
  spiRegWrite(sensorId, PGA_REG_P1_THR_12 + regOffset, thresholds.l10);
  spiRegWrite(sensorId, PGA_REG_P1_THR_13 + regOffset, thresholds.l11);
  spiRegWrite(sensorId, PGA_REG_P1_THR_14 + regOffset, thresholds.l12);
  // Register 15 is a bit special...
  spiRegWrite(sensorId, PGA_REG_P1_THR_15 + regOffset, PGA_TH_P1_OFF(0));
}

// Start a burst-and-listen with preset 1 and detect (up to) numberOfObjectsToDetect objects
// preset is 1 or 2
// numberOfObjectsToDetect must be 1..8
void PGASensorManager::spiBurstAndListen(uint8_t sensorId, uint8_t preset, uint8_t numberOfObjectsToDetect)
{
  assert(sensorId >= 0 && sensorId <= 1);
  assert(preset >= 1 && preset <= 2);
  assert(numberOfObjectsToDetect >= 1 && numberOfObjectsToDetect <= 8);

  checksum_clear();
  spiTransfer(sensorId, 0x55);  // Sync byte
  uint8_t cmd = 0<<5 | preset == 1 ? PGA_CMD_BURST_AND_LISTEN_1 : PGA_CMD_BURST_AND_LISTEN_2;
  spiTransfer(sensorId, cmd); // Command
  checksum_append_byte(cmd);
  spiTransfer(sensorId, numberOfObjectsToDetect);
  checksum_append_byte(numberOfObjectsToDetect);
  spiTransfer(sensorId, checksum_get());
}

// Get the last ultrasonic results (triggered by spiBurstAndListen)
// usResults must be an array of at least numberOfObjectsToDetect elements
bool PGASensorManager::spiUSResult(uint8_t sensorId, uint8_t numberOfObjectsToDetect, PGAResult *usResults)
{
  assert(sensorId >= 0 && sensorId <= 1);
  assert(numberOfObjectsToDetect >= 1 && numberOfObjectsToDetect <= 8);

  spiTransfer(sensorId, 0x55);  // Sync byte
  spiTransfer(sensorId, 0<<5 | PGA_CMD_ULTRASONIC_RESULT); // Command

  // Get diag data
  checksum_clear();
  uint8_t diag = spiTransfer(sensorId, 0x00);
  checksum_append_byte(diag);

  // Read result data of 4 bytes for each object
  for(int obj = 0; obj < numberOfObjectsToDetect; obj++)
  {
    uint8_t data = spiTransfer(sensorId, 0x00);
    usResults[obj].tof = ((uint16_t)data)<<8;
    checksum_append_byte(data);
    data = spiTransfer(sensorId, 0x00);
    usResults[obj].tof |= data;
    checksum_append_byte(data);
    data = spiTransfer(sensorId, 0x00);
    usResults[obj].width = data;
    checksum_append_byte(data);
    data = spiTransfer(sensorId, 0x00);
    usResults[obj].peakAmplitude = data;
    checksum_append_byte(data);

    // Convert tof to distance in cm
    // TODO: Add temperature compensation by reading tempearture from sensor (somewhere else)
    const double tof = usResults[obj].tof*1e-6;
    const double v = 343.0;
    usResults[obj].distance = (uint16_t)(tof*v/2.0*100.0);
  }

  // Get checksum
  uint8_t checksum = spiTransfer(sensorId, 0x00);
  return checksum == checksum_get();
}

bool PGASensorManager::spiIsBusy(uint8_t sensorId)
{
  uint8_t diag;
  int res = spiRegRead(sensorId, PGA_REG_USER_DATA1, &diag);
  if(res == -1)
    return true;
  return !!(diag & PGA_DIAG_BUSY_MASK);
}

// data must point to at least 128 bytes
// Returns true if checksum was ok, false otherwise
bool PGASensorManager::spiDataDump(const uint8_t sensorId, uint8_t *data)
{
  assert(sensorId >= 0 && sensorId <= 1);

  spiTransfer(sensorId, 0x55);  // Sync byte
  spiTransfer(sensorId, 0<<5 | PGA_CMD_DATA_DUMP); // Command

  // Get diag data
  checksum_clear();
  uint8_t diag = spiTransfer(sensorId, 0x00);
  checksum_append_byte(diag);

  // Read data dump of 128 bytes
  for(int i = 0; i < 128; i++)
  {
    data[i] = spiTransfer(sensorId, 0x00);
    checksum_append_byte(data[i]);
  }

  // Get checksum
  uint8_t checksum = spiTransfer(sensorId, 0x00);
  return checksum == checksum_get();
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

// Gets the distances from the sensors
// Returns true if any of the sensors returned a range < 6m
bool PGASensorManager::collectSensorResults() {
  bool validReading = false;
  for (size_t sensorId = 0; sensorId < NUMBER_OF_TOF_SENSORS; ++sensorId) {
    PGASensorInfo* const sensor = &m_sensors[sensorId];
    PGAResult usResults[1];
    if(!spiUSResult(sensorId, 1, usResults))
      continue;
#if PGA_DUMP_ENABLE
    if(sensor->lastMeasurementWasDump)
    {
      uint8_t data[128];
      if(!spiDataDump(sensorId, data))
        log_e("Unable to get data dump: Checksum error");
      else
      {
        Serial.printf("dump,%d,", sensorId);
        for(int i = 0; i < 128; i++)
        {
          Serial.printf("%d", data[i]);
          if(i+1 != 128)
            Serial.print(",");
        }
        Serial.printf("\n");
      }
    }
    else
    {
    Serial.printf("meas,%d,%d,%d,%d\n", sensorId, usResults[0].tof, usResults[0].width, usResults[0].peakAmplitude);
#endif

      sensor->echoDurationMicroseconds[lastReadingCount] = usResults[0].tof;
      uint16_t dist;
      if(usResults[0].distance < MIN_DISTANCE_MEASURED_CM || usResults[0].distance >= MAX_DISTANCE_MEASURED_CM) {
        dist = MAX_SENSOR_VALUE;
      } else {
        validReading = true;
        dist = static_cast<uint16_t>(usResults[0].distance);
      }
      sensor->rawDistance = dist;
      sensor->median->addValue(dist);
      sensorValues[sensorId] = sensor->distance = correctSensorOffset(medianMeasure(sensor, dist), sensor->offset);

      log_v("Raw sensor[%d] distance read %03u / %03u (%03u, %03u, %03u) -> *%03ucm*, duration: %zu us",
        sensorId, sensor->rawDistance, dist, sensor->distances[0], sensor->distances[1],
        sensor->distances[2], sensorValues[sensorId], usResults[0].tof);

      if (sensor->distance > 0 && sensor->distance < sensor->minDistance) {
        sensor->minDistance = sensor->distance;
      }
#if PGA_DUMP_ENABLE
    }
#endif
  }
  if (validReading) {
    registerReadings();
  }
  return validReading;
}

// Store the relative times of this reading (a pair of measurements?)
void PGASensorManager::registerReadings() {
  startOffsetMilliseconds[lastReadingCount] = millisSince(startReadingMilliseconds);
  if (lastReadingCount < MAX_NUMBER_MEASUREMENTS_PER_INTERVAL - 1) {
    lastReadingCount++;
  }
}

uint16_t PGASensorManager::millisSince(uint16_t milliseconds) {
  uint16_t result = ((uint16_t) millis()) - milliseconds;
  if (result & 0x8000) {
    result = -result;
  }
  return result;
}

uint16_t PGASensorManager::correctSensorOffset(uint16_t dist, uint16_t offset) {
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

uint16_t PGASensorManager::medianMeasure(PGASensorInfo *const sensor, uint16_t value) {
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

uint16_t PGASensorManager::median(uint16_t a, uint16_t b, uint16_t c) {
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

#endif  // OBSPro
