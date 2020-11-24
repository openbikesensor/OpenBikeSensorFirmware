#ifndef OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H
#define OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H

#include <cstdint>
#include <esp_adc_cal.h>

class VoltageMeter {
  public:
    VoltageMeter();

    /* Returns the (smoothed) value in Volts. */
    double read();

  private:
    /* This one is typically NOT used, our ESP32 dos have
     * Vref stored in the eFuse which takes priority. */
    const uint16_t REF_VOLTAGE_MILLI_VOLT = 1100;
    const uint8_t BATTERY_PIN = 34;
    const adc1_channel_t BATTERY_ADC_CHANNEL = ADC1_GPIO34_CHANNEL;
//  const uint8_t REFERENCE_PIN = 35;
    const int16_t MINIMUM_SAMPLES = 64;
    const int16_t SAMPLES_DIVIDE = 128;
    esp_adc_cal_characteristics_t adc_chars;
    int16_t lastSmoothedReading;

    int readSmoothed();

    int readRaw();
};


#endif //OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H
