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

#ifndef OBS_BLUETOOTH_IBLUETOOTHSERVICE_H
#define OBS_BLUETOOTH_IBLUETOOTHSERVICE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <list>
#include "globals.h"

/**
 * This class interface defines how a bluetooth service should work.
 * Any child class should override every public method.
 */
class IBluetoothService {
  public:
    /**
     * This method initializes the class and expects the instance to create its
     * own BLEService and its characteristics.
     * @param pServer pointer to the bluetooth server
     */
    virtual void setup(BLEServer *pServer) = 0;

    /**
     * Whether this bluetooth service should be advertised or not.
     */
    virtual bool shouldAdvertise() = 0;

    /**
     * Gets the only BLEService that was created in setup().
     */
    virtual BLEService* getService() = 0;

    /**
     * Processes new sensor values from both sides.
     * @param millis sender millis counter at the time of measurement of the left value
     * @param leftValue sensor value of the left side (MAX_SENSOR_VALUE for no reading)
     * @param rightValues sensor value of the right side (MAX_SENSOR_VALUE for no reading)
     */
    virtual void newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue) {
      // empty default implementation
    }

    /**
     * Processes new confirmed overtake event.
     * @param millis sender millis counter at the time of measurement of the left value
     * @param leftValue sensor value of the left side (MAX_SENSOR_VALUE for no reading)
     * @param rightValues sensor value of the right side (MAX_SENSOR_VALUE for no reading)
     */
    virtual void newPassEvent(uint32_t millis, uint16_t leftValue, uint16_t rightValue) {
      // empty default implementation
    }

  protected:
    static String valueAsString(uint16_t value) {
      if (value == MAX_SENSOR_VALUE || value == 0) {
        return String("");
      } else{
        return String(value);
      }
    }
};

#endif
