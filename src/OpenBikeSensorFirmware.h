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

#ifndef OBS_OPENBIKESENSORFIRMWARE_H
#define OBS_OPENBIKESENSORFIRMWARE_H

#include <Arduino.h>
#define CIRCULAR_BUFFER_INT_SAFE
#include <CircularBuffer.h>

#include "config.h"
#include "configServer.h"
#include "displays.h"
#include "globals.h"
#include "gps.h"
#include "sensor.h"
#include "writer.h"
#include "battery.h"

#include <Adafruit_BMP280.h>
#include <VL53L0X.h>

#include "bluetooth/BluetoothManager.h"

#endif
