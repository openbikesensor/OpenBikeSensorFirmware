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

#ifndef OBS_BLUETOOTH_BLUETOOTHMANAGER_H
#define OBS_BLUETOOTH_BLUETOOTHMANAGER_H

#include <Arduino.h>
#include <list>

#include "_IBluetoothService.h"
#include "DeviceInfoService.h"
#include "HeartRateService.h"
#include "BatteryService.h"
#include "ObsService.h"

class BluetoothManager: public BLEServerCallbacks  {
  public:
    /**
     * Initializes all defined services and starts the bluetooth server.
     */
    void init(const String &obsName,
              uint16_t leftOffset, uint16_t rightOffset,
              std::function<uint8_t()> batteryPercentage,
              const String &trackId);

    /**
     * Starts advertising all services that internally implement shouldAdvertise()
     * with `true`. The bluetooth server needs to be started before this method.
     */
    void activateBluetooth();

    /**
     * Stops advertising the bluetooth services. The bluetooth server will not be
     * stopped.
     */
    void deactivateBluetooth() const;

    /**
     * Disconnects the currently connected device/client.
     */
    void disconnectDevice() const;

    /**
     * Processes new sensor values by calling each services with the values.
     * @param millis sender millis counter at the time of measurement of the left value
     * @param leftValue sensor value of the left side (MAX_SENSOR_VALUE for no reading)
     * @param rightValues sensor value of the right side (MAX_SENSOR_VALUE for no reading)
     */
    void newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue);

    /**
     * Processes new confirmed pass event.
     * @param millis sender millis counter at the time of measurement of the left value
     * @param leftValue sensor value of the left side (MAX_SENSOR_VALUE for no reading)
     * @param rightValues sensor value of the right side (MAX_SENSOR_VALUE for no reading)
     */
    void newPassEvent(uint32_t millis, uint16_t leftValue, uint16_t rightValue);

    /* True if any client is currently connected. */
    bool hasConnectedClients();

  private:
    BLEServer *pServer;
    std::list<IBluetoothService*> services;
    void onDisconnect(BLEServer *pServer) override;
    void onConnect(BLEServer *pServer) override;
    void setFastAdvertising();
    void setSlowAdvertising();
    bool deviceConnected;
    bool fastAdvertising;
    uint32_t lastDisconnected;
    static const uint32_t HIGH_ADVERTISEMENT_TIME_MS;

};

#endif
