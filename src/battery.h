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

#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <CircularBuffer.h>

#define MOVAV_LENGTH_BATTERY 800

extern CircularBuffer<uint16_t,MOVAV_LENGTH_BATTERY> voltageBuffer;
extern float BatterieVoltage_value;
extern float BatterieVoltage_movav;

int8_t get_batterie_percent(uint16_t ADC_value);
uint16_t movingaverage(CircularBuffer<uint16_t,MOVAV_LENGTH_BATTERY> *buffer,float *movav,int16_t input);
uint16_t batterie_voltage_read(uint8_t Batterie_PIN);

#endif
