#include <Arduino.h>
#include "variant.h"

#ifndef OBS_PGASENSOR_H
#define OBS_PGASENSOR_H

#ifdef OBSPRO  // PGASensor is only available on OBSPro

#define PGA_CMD_BURST_AND_LISTEN_1  0
#define PGA_CMD_BURST_AND_LISTEN_2  1
#define PGA_CMD_ULTRASONIC_RESULT  5
#define PGA_CMD_DATA_DUMP 7
#define PGA_CMD_REGISTER_READ 9
#define PGA_CMD_REGISTER_WRITE 10

#define PGA_REG_USER_DATA1  0x00
#define PGA_REG_USER_DATA2  0x01
#define PGA_REG_USER_DATA3  0x02
#define PGA_REG_USER_DATA4  0x03
#define PGA_REG_USER_DATA5  0x04
#define PGA_REG_USER_DATA6  0x05
#define PGA_REG_USER_DATA7  0x06
#define PGA_REG_USER_DATA8  0x07
#define PGA_REG_USER_DATA9  0x08
#define PGA_REG_USER_DATA10  0x09
#define PGA_REG_USER_DATA11  0x0a
#define PGA_REG_USER_DATA12  0x0b
#define PGA_REG_USER_DATA13  0x0c
#define PGA_REG_USER_DATA14  0x0d
#define PGA_REG_USER_DATA15  0x0e
#define PGA_REG_USER_DATA16  0x0f
#define PGA_REG_USER_DATA17  0x10
#define PGA_REG_USER_DATA18  0x11
#define PGA_REG_USER_DATA19  0x12
#define PGA_REG_USER_DATA20  0x13

#define PGA_REG_TVGAIN0  0x14
#define PGA_REG_TVGAIN1  0x15
#define PGA_REG_TVGAIN2  0x16
#define PGA_REG_TVGAIN3  0x17
#define PGA_REG_TVGAIN4  0x18
#define PGA_REG_TVGAIN5  0x19
#define PGA_REG_TVGAIN6  0x1a

#define PGA_REG_INIT_GAIN  0x1b
#define PGA_BPF_BW(val) ((val&0x3) << 6)
#define PGA_GAIN_INIT(val) (val&0x3f)  // Init_Gain = 0.5 × (GAIN_INIT+1) + value(AFE_GAIN_RNG) [dB]
#define PGA_REG_FREQUENCY  0x1c
#define PGA_REG_DEADTIME  0x1d
#define PGA_REG_PULSE_P1  0x1e
#define PGA_IO_IF_SEL(val) ((val&0x01) << 7)
#define PGA_UART_DIAG(val) ((val&0x01) << 6)
#define PGA_IO_DIS(val) ((val&0x01) << 5)
#define P1_PULSE(val) (val&0x1f)
#define PGA_REG_PULSE_P2  0x1f
#define PGA_REG_CURR_LIM_P1  0x20
#define PGA_DIS_CL(val) ((val&0x01) << 7)  // Disable Current Limit for Preset1 and Preset2
#define PGA_CURR_LIM1(val) (val&0x3f)  // Current_Limit = 7 × CURR_LIM1 + 50 [mA]
#define PGA_REG_CURR_LIM_P2  0x21
#define PGA_LPF_CO(val) ((val&0x03) << 6)  // Lowpass filter cutoff frequency = LPF_CO + 1 [kHz]
#define PGA_CURR_LIM2(val) (val&0x3f)  // Current limit = 7 × CURR_LIM2 + 50 [mA]
#define PGA_REG_REC_LENGTH  0x22
#define PGA_P1_REC(val) ((val&0x0f) << 4)  // Record time = 4.096 × (P1_REC + 1) [ms]
#define PGA_P2_REC(val) (val&0x0f)  // Record time = 4.096 × (P1_REC + 1) [ms]
#define PGA_FREQ_DIAG 0x23
#define PGA_FDIAG_LEN(val) ((val&0x0f) << 4)
#define PGA_FDIAG_START(val) (val&0x0f)
#define PGA_REG_SAT_FDIAG_TH  0x24
#define PGA_FDIAG_ERR_TH(val) ((val&0x07) << 5)
#define PGA_SAT_TH(val) ((val&0x0f) << 1)
#define PGA_P1_NLS_EN(val) (val&0x01)
#define PGA_REG_FVOLT_DEC  0x25
#define PGA_REG_DECPL_TEMP  0x26
#define PGA_AFE_GAIN_RNG(val) ((val&0x03) << 6)
#define PGA_LPM_EN(val) ((val&0x01) << 5)
#define PGA_DECPL_TEMP_SEL(val) ((val&0x01) << 4)
#define PGA_DECPL_T(val) (val&0x0f)
#define PGA_REG_DSP_SCALE  0x27
#define PGA_REG_TEMP_TRIM  0x28
#define PGA_REG_P1_GAIN_CTRL  0x29
#define PGA_REG_P2_GAIN_CTRL  0x2a
#define PGA_REG_EE_CRC  0x2b
#define PGA_REG_EE_CNTRL  0x40
#define PGA_DATADUMP_EN(val) ((val&0x01) << 7)

#define PGA_REG_BPF_A2_MSB  0x41
#define PGA_REG_BPF_A2_LSB  0x42
#define PGA_REG_BPF_A3_MSB  0x43
#define PGA_REG_BPF_A3_LSB  0x44
#define PGA_REG_BPF_B1_MSB  0x45
#define PGA_REG_BPF_B1_LSB  0x46
#define PGA_REG_LPF_A2_MSB  0x47
#define PGA_REG_LPF_A2_LSB  0x48
#define PGA_REG_LPF_B1_MSB  0x49
#define PGA_REG_LPF_B1_LSB  0x4a

#define PGA_REG_TEST_MUX  0x4b
#define PGA_REG_DEV_STAT0  0x4c
#define PGA_REG_DEV_STAT1  0x4d

#define PGA_REG_P1_THR_0  0x5f
#define PGA_REG_P1_THR_1  0x60
#define PGA_REG_P1_THR_2  0x61
#define PGA_REG_P1_THR_3  0x62
#define PGA_REG_P1_THR_4  0x63
#define PGA_REG_P1_THR_5  0x64
#define PGA_REG_P1_THR_6  0x65
#define PGA_REG_P1_THR_7  0x66
#define PGA_REG_P1_THR_8  0x67
#define PGA_REG_P1_THR_9  0x68
#define PGA_REG_P1_THR_10  0x69
#define PGA_REG_P1_THR_11  0x6a
#define PGA_REG_P1_THR_12  0x6b
#define PGA_REG_P1_THR_13  0x6c
#define PGA_REG_P1_THR_14  0x6d
#define PGA_REG_P1_THR_15  0x6e

#define PGA_REG_P2_THR_0  0x6f
#define PGA_REG_P2_THR_1  0x70
#define PGA_REG_P2_THR_2  0x71
#define PGA_REG_P2_THR_3  0x72
#define PGA_REG_P2_THR_4  0x73
#define PGA_REG_P2_THR_5  0x74
#define PGA_REG_P2_THR_6  0x75
#define PGA_REG_P2_THR_7  0x76
#define PGA_REG_P2_THR_8  0x77
#define PGA_REG_P2_THR_9  0x78
#define PGA_REG_P2_THR_10  0x79
#define PGA_REG_P2_THR_11  0x7a
#define PGA_REG_P2_THR_12  0x7b
#define PGA_REG_P2_THR_13  0x7c
#define PGA_REG_P2_THR_14  0x7d
#define PGA_REG_P2_THR_15  0x7e
#define PGA_TH_P1_OFF(val) (val&0x0f)  // TODO: This conversion is not correct. This bit field uses signed values!

#define PGA_REG_THR_CRC  0x7f


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

struct PGAUSResult
{
  uint16_t tof;  // Time of flight [us]
  uint8_t width;
  uint8_t peakAmplitude;
};

#define TH_TIME_DELTA_100US  0x0
#define TH_TIME_DELTA_200US  0x1
#define TH_TIME_DELTA_300US  0x2
#define TH_TIME_DELTA_400US  0x3
#define TH_TIME_DELTA_600US  0x4
#define TH_TIME_DELTA_800US  0x5
#define TH_TIME_DELTA_1000US  0x6
#define TH_TIME_DELTA_1200US  0x7
#define TH_TIME_DELTA_1400US  0x8
#define TH_TIME_DELTA_2000US  0x9
#define TH_TIME_DELTA_2400US  0xa
#define TH_TIME_DELTA_3200US  0xb
#define TH_TIME_DELTA_4000US  0xc
#define TH_TIME_DELTA_5200US  0xd
#define TH_TIME_DELTA_6400US  0xe
#define TH_TIME_DELTA_8000US  0xf

struct PGAThresholds
{
  PGAThresholds(
    uint8_t t1 = TH_TIME_DELTA_100US, uint8_t l1 = 0,
    uint8_t t2 = TH_TIME_DELTA_100US, uint8_t l2 = 0,
    uint8_t t3 = TH_TIME_DELTA_100US, uint8_t l3 = 0,
    uint8_t t4 = TH_TIME_DELTA_100US, uint8_t l4 = 0,
    uint8_t t5 = TH_TIME_DELTA_100US, uint8_t l5 = 0,
    uint8_t t6 = TH_TIME_DELTA_100US, uint8_t l6 = 0,
    uint8_t t7 = TH_TIME_DELTA_100US, uint8_t l7 = 0,
    uint8_t t8 = TH_TIME_DELTA_100US, uint8_t l8 = 0,
    uint8_t t9 = TH_TIME_DELTA_100US, uint8_t l9 = 0,
    uint8_t t10 = TH_TIME_DELTA_100US, uint8_t l10 = 0,
    uint8_t t11 = TH_TIME_DELTA_100US, uint8_t l11 = 0,
    uint8_t t12 = TH_TIME_DELTA_100US, uint8_t l12 = 0)
  {
    this->t1 = t1 & 0x0f;
    this->l1 = l1 & 0x1f;
    this->t2 = t2 & 0x0f;
    this->l2 = l2 & 0x1f;
    this->t3 = t3 & 0x0f;
    this->l3 = l3 & 0x1f;
    this->t4 = t4 & 0x0f;
    this->l4 = l4 & 0x1f;
    this->t5 = t5 & 0x0f;
    this->l5 = l5 & 0x1f;
    this->t6 = t6 & 0x0f;
    this->l6 = l6 & 0x1f;
    this->t7 = t7 & 0x0f;
    this->l7 = l7 & 0x1f;
    this->t8 = t8 & 0x0f;
    this->l8 = l8 & 0x1f;
    this->t9 = t9 & 0x0f;
    this->l9 = l9 & 0x1f;
    this->t10 = t10 & 0x0f;
    this->l10 = l10;
    this->t11 = t11 & 0x0f;
    this->l11 = l11;
    this->t12 = t12 & 0x0f;
    this->l12 = l12;
  }

  uint8_t t1;  // 4 bits
  uint8_t l1;  // 5 bits
  uint8_t t2;  // 4 bits
  uint8_t l2;  // 5 bits
  uint8_t t3;  // 4 bits
  uint8_t l3;  // 5 bits
  uint8_t t4;  // 4 bits
  uint8_t l4;  // 5 bits
  uint8_t t5;  // 4 bits
  uint8_t l5;  // 5 bits
  uint8_t t6;  // 4 bits
  uint8_t l6;  // 5 bits
  uint8_t t7;  // 4 bits
  uint8_t l7;  // 5 bits
  uint8_t t8;  // 4 bits
  uint8_t l8;  // 5 bits
  uint8_t t9;  // 4 bits
  uint8_t l9;  // 8(!) bits
  uint8_t t10;  // 4 bits
  uint8_t l10;  // 8(!) bits
  uint8_t t11;  // 4 bits
  uint8_t l11;  // 8(!) bits
  uint8_t t12;  // 4 bits
  uint8_t l12;  // 8(!) bits
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

  bool spiDataDump(const uint8_t sensorId, uint8_t *data);

  // TODO: These should not be public!
  PGASensorInfo m_sensors[NUMBER_OF_TOF_SENSORS];
  uint16_t sensorValues[NUMBER_OF_TOF_SENSORS];
  uint16_t lastReadingCount = 0;
  uint16_t startOffsetMilliseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];

protected:
  void setupSensor(int idx);

  void safe_usleep(unsigned long us);

  // Synchronous UART mode (aka SPI without chip-select)
  uint8_t spiTransfer(uint8_t sensorId, uint8_t data_out);
  int spiRegRead(uint8_t sensorId, uint8_t reg_addr);
  void spiRegWrite(uint8_t sensorId, uint8_t reg_addr, uint8_t value);
  void spiRegWriteThesholds(uint8_t sensorId, uint8_t preset, PGAThresholds &thresholds);
  void spiBurstAndListen(uint8_t sensorId, uint8_t preset, uint8_t numberOfObjectsToDetect);
  bool spiUSResult(uint8_t sensorId, uint8_t numberOfObjectsToDetect, PGAUSResult *usResults);

  // Checksum
  uint8_t checksum_sum;
  uint8_t checksum_carry;
  uint8_t checksum_tmp_data;
  uint8_t checksum_offset;
  void checksum_clear();
  void checksum_append_bit(uint8_t val);
  void checksum_append_byte(uint8_t val);
  uint8_t checksum_get();

  // Alternating state
  unsigned long lastTriggerTimeMs;
};

#endif  // OBSPRO
#endif  // OBS_PGASENSOR_H
