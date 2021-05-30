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

#include "VoltageMeter.h"
#include "globals.h"

/* Class to encapsulate voltage readings and smoothing thereof.
 * Still the values are not as accurate as expected, and it is not
 * clear if this is a error in the code or limitation of the ESP.
 * I read() 3.96V for 4.02V on my cheep voltage meter.
 *
 * We might add features like a trend (charging/discharging) and
 * possibly guess percentage later. We also need a "alert" threshold
 * where we just save all end exit.
 * Using ESP32 calls not arduino lib calls here.
 * ESPCode: https://github.com/espressif/esp-idf/blob/master/components/esp_adc_cal/include/esp_adc_cal.h
 */
VoltageMeter::VoltageMeter(uint8_t batteryPin, adc1_channel_t channel) :
  mBatteryPin(batteryPin), mBatteryAdcChannel(channel) {
  log_i("Initializing VoltageMeter.");
  pinMode(mBatteryPin, INPUT);
  ESP_ERROR_CHECK_WITHOUT_ABORT(
    adc1_config_width(ADC_WIDTH_BIT_12));
  // Suggested range for ADC_ATTEN_DB_11 is 150 - 2450 mV, we are a bit above 4.22V * 2/3 == 2.81V
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#api-reference
  ESP_ERROR_CHECK_WITHOUT_ABORT(
    adc1_config_channel_atten(mBatteryAdcChannel, ADC_ATTEN_DB_11));
  __unused const esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
      ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12,
      REF_VOLTAGE_MILLI_VOLT, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    log_i("Characterized using Two Point Value");
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    log_i("Characterized using eFuse Vref");
  } else {
    log_i("Characterized using Default Vref");
  }
  //Check if TP is burned into eFuse
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
    log_i("eFuse Two Point: Supported");
  } else {
    log_i("eFuse Two Point: NOT supported");
  }
  //Check Vref is burned into eFuse
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
    log_i("eFuse Vref: Supported");
  } else {
    log_i("eFuse Vref: NOT supported");
  }
  lastSmoothedReading = readRaw();
  for (int i = 0; i < MINIMUM_SAMPLES; i++) {
    readSmoothed();
    yield();
  }
  log_i("VoltageMeter initialized got %03.2fV.", read());
}

bool VoltageMeter::isWarningLevel() {
  return hasReadings() && (read() < VoltageMeter::BATTERY_WARNING_LEVEL);
}

bool VoltageMeter::hasReadings() {
  return read() > VoltageMeter::BATTERY_NO_READ_LEVEL;
}

double VoltageMeter::read() {
  return esp_adc_cal_raw_to_voltage(readSmoothed(), &adc_chars)
    * 3.0 / 2000.0; // voltage divider @ OSB PCB
}

int8_t VoltageMeter::readPercentage() {
  if (!hasReadings()) {
    return -1;
  }
  auto voltage = read();
  int8_t percentage;
  if (voltage > 4.13) {
    percentage = 100;
  } else if (voltage > 3.67) { // 100% - 50%
    percentage = 108.696 * voltage - 348.914;
  } else if (voltage > 3.49) { //  50% - 25%
    percentage = 138.889 * voltage - 459.723;
  } else if (voltage > 3.12) { //  25% -  0%
    percentage = 67.568 * voltage - 210.812;
  } else {
    percentage = 0;
  }
  log_d("Voltage: %.2f  Percentage: %d", voltage, percentage);
  return percentage;
}

int VoltageMeter::readSmoothed() {
  lastSmoothedReading =
    (lastSmoothedReading * (SAMPLES_DIVIDE - 1) + readRaw()) / SAMPLES_DIVIDE;
  return lastSmoothedReading;
}

int VoltageMeter::readRaw() const {
  return adc1_get_raw(mBatteryAdcChannel);
}
