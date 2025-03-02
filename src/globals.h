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

#ifndef OBS_GLOBALS_H
#define OBS_GLOBALS_H

#include <Arduino.h>

// Forward declare classes to build (because there is a cyclic dependency between sensor.h and displays.h)
class DisplayDevice;
class HCSR04SensorManager;


#include "utils/obsutils.h"
#include "config.h"
#include "displays.h"
#include "sensor.h"
#include "pgaSensor.h"
#include "VoltageMeter.h"
#include "utils/button.h"
#include "variant.h"

// This file should contain declarations of all variables that will be used globally.
// The variables don't have to be set here, but need to be declared.

// Version
extern const char *OBSVersion;

// PINs
extern const int PUSHBUTTON_PIN;

extern int confirmedMeasurements;
extern int numButtonReleased;

extern Config config;


extern DisplayDevice* obsDisplay;

#ifdef OBSPRO
extern PGASensorManager *sensorManager;
#endif
#ifdef OBSCLASSIC
extern HCSR04SensorManager* sensorManager;
#endif

extern VoltageMeter* voltageMeter;

extern Button button;

class Gps;
extern Gps gps;

extern const uint8_t LEFT_SENSOR_ID;
extern const uint8_t RIGHT_SENSOR_ID;
extern const uint16_t BUTTON_PRESS_THRESHOLD_FOR_SHUTDOWN_MS;

#endif
