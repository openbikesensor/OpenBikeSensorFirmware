#ifndef OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H
#define OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H

#include <cstdint>
#include <esp_adc_cal.h>

class VoltageMeter {
  public:
    VoltageMeter(uint8_t batteryPin = 34, adc1_channel_t channel = ADC1_GPIO34_CHANNEL);
    /* Returns the (smoothed) value in Volts. */
    double read();
    uint8_t readPercentage();
    bool hasReadings();
    bool isWarningLevel();

  private:
    /* This one is typically NOT used, our ESP32 dos have
     * Vref stored in the eFuse which takes priority. */
    static const uint16_t REF_VOLTAGE_MILLI_VOLT = 1100;
    static const int16_t MINIMUM_SAMPLES = 64;
    static const int16_t SAMPLES_DIVIDE = 128;

    /* Below this value a warning will be printed during startup. */
    constexpr static const double BATTERY_WARNING_LEVEL = 3.47;
    /* Below this value it is assumed there is no voltage reading at all. */
    constexpr static const double BATTERY_NO_READ_LEVEL = 3.00;

    const uint8_t mBatteryPin;
    const adc1_channel_t mBatteryAdcChannel;
    esp_adc_cal_characteristics_t adc_chars;
    int16_t lastSmoothedReading;
    int readSmoothed();
    int readRaw() const;
};


#endif //OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H
