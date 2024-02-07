/*
 * Copyright (C) 2019-2023 OpenBikeSensor Contributors
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

#ifndef VARIANT_H
#define VARIANT_H

// If set, the firmware is build for the OBSPro hardware variant
// The main differences are:
//   - The button is inverted
//   - The button is also responsible for soft-power-off
//   - The display is a JHD12864-G156BT which is SPI based
//   - The ultrasonic sensors are PGA460 based
#define OBSPRO

// If set, the firmware is build for the OBSClassic
// #define OBSCLASSIC

#ifdef OBSPRO
#define LCD_CS_PIN  12
#define LCD_DC_PIN  27
#define LCD_RESET_PIN  4
#endif

#endif
