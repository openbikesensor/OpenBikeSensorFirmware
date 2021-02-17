/*
  Copyright (C) 2019 Zweirat
  Contact: https://openbikesensor.org

  This file is part of the OpenBikeSensor project.

  The OpenBikeSensor sensor firmware is free software: you can redistribute
  it and/or modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  The OpenBikeSensor sensor firmware is distributed in the hope that it will
  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
  Public License for more details.

  You should have received a copy of the GNU General Public License along with
  the OpenBikeSensor sensor firmware.  If not, see <http://www.gnu.org/licenses/>.
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
#ifdef DEVELOP
  Serial.print("Initializing VoltageMeter.\n");
#endif
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
#ifdef DEVELOP
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    Serial.printf("Characterized using Two Point Value\n");
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("Characterized using eFuse Vref\n");
  } else {
    Serial.printf("Characterized using Default Vref\n");
  }
  //Check if TP is burned into eFuse
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
    Serial.printf("eFuse Two Point: Supported\n");
  } else {
    Serial.printf("eFuse Two Point: NOT supported\n");
  }
  //Check Vref is burned into eFuse
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
    Serial.printf("eFuse Vref: Supported\n");
  } else {
    Serial.printf("eFuse Vref: NOT supported\n");
  }
#endif
  lastSmoothedReading = readRaw();
  for (int i = 0; i < MINIMUM_SAMPLES; i++) {
    readSmoothed();
    yield();
  }
#ifdef DEVELOP
  Serial.printf("VoltageMeter initialized got %03.2fV.\n", read());
#endif
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

uint8_t VoltageMeter::readPercentage() {
  if (!hasReadings()) {
    return -1;
  }
  /* TODO: Use the voltage returned by read() as input for the formula.
  const int adc_value = readSmoothed();
  // from get_batterie_percent
  uint8_t percentage;
  if (adc_value > 3193) {
    percentage = 100;
  } else if (adc_value > 2750) {
    percentage = 0.113 * adc_value - 260.384;
  } else if (adc_value > 2620) {
    percentage = 0.192 * adc_value - 478.846;
  } else if (adc_value > 2370) {
    percentage = 0.1 * adc_value - 237;
  } else {
    percentage = 0;
  }
  return percentage;
*/

  // TODO: Better formula from Benjamin!
  // 4.22 == 100% / 3.5 = 0%
  int16_t result = ((read() - 3.5) / 0.0072);
  if (result < 0) {
    result = 0;
  } else if (result > 100) {
    result = 100;
  }
  return (uint8_t) result;
}

int VoltageMeter::readSmoothed() {
  lastSmoothedReading =
    (lastSmoothedReading * (SAMPLES_DIVIDE - 1) + readRaw()) / SAMPLES_DIVIDE;
  return lastSmoothedReading;
}

int VoltageMeter::readRaw() const {
  return adc1_get_raw(mBatteryAdcChannel);
}

