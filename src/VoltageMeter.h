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

#ifndef OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H
#define OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H

#include <cstdint>
#include <esp_adc_cal.h>

class VoltageMeter {
  public:
    explicit VoltageMeter(
      uint8_t batteryPin = 34,
      adc1_channel_t channel = ADC1_CHANNEL_6);
    /* Returns the (smoothed) value in Volts. */
    double read();
    int8_t readPercentage();
    bool hasReadings();
    bool isWarningLevel();
    /* If only zero is read, pin is pulled to GND. */
    bool isZeroOnly() const;

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
    int readRaw();
    bool readZeroOnly = true;
};


#endif //OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H
