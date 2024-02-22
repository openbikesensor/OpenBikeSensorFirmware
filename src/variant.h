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
//#define OBSCLASSIC

// Settings specific to OBSPro
#ifdef OBSPRO
#define LCD_CS_PIN  12
#define LCD_DC_PIN  27
#define LCD_RESET_PIN  4

#define SENSOR1_SCK_PIN  25
#define SENSOR1_MOSI_PIN  33
#define SENSOR1_MISO_PIN  32

#define SENSOR2_SCK_PIN  14
#define SENSOR2_MOSI_PIN  15
#define SENSOR2_MISO_PIN  21
#define UBX_M10

#define POWER_KEEP_ALIVE_INTERVAL_MS  5000UL
#endif

// Settings specific to OBSClassic
#if OBSCLASSIC
#define UBS_M6
#endif


// General settings
#define NUMBER_OF_TOF_SENSORS 2
#define MAX_SENSOR_VALUE 999
#define MAX_NUMBER_MEASUREMENTS_PER_INTERVAL 30 //  is 1000/SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC/2
#define MEDIAN_DISTANCE_MEASURES 3

#define MIN_DISTANCE_MEASURED_CM 2
#ifdef OBSPRO
  #define MAX_DISTANCE_MEASURED_CM 600
#else
  #define MAX_DISTANCE_MEASURED_CM 320 // candidate to check I could not get good readings above 300
#endif

#define MICRO_SEC_TO_CM_DIVIDER 58 // sound speed 340M/S, 2 times back and forward

#define MIN_DURATION_MICRO_SEC (MIN_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER)
#define MAX_DURATION_MICRO_SEC (MAX_DISTANCE_MEASURED_CM * MICRO_SEC_TO_CM_DIVIDER)

#endif
