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

#ifndef OPENBIKESENSORFIRMWARE_BATTERYSERVICE_H
#define OPENBIKESENSORFIRMWARE_BATTERYSERVICE_H

#include "_IBluetoothService.h"
#include <functional>

class BatteryLevelCallback : public BLECharacteristicCallbacks {
  public:
    explicit BatteryLevelCallback(uint8_t *value, std::function<uint8_t()> getLevel) :
      mValue(value), mGetLevel(getLevel) {};

    void onRead(BLECharacteristic *pCharacteristic) override;

  private:
    uint8_t *mValue;
    const std::function<uint8_t()> mGetLevel;
};

class BatteryService : public IBluetoothService {
  public:
    explicit BatteryService(std::function<uint8_t()> getLevel) : mBatteryLevelCallback(&mBatteryLevelValue, getLevel)  { };
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService* getService() override;

  private:
    BLEService *mService = nullptr;
    BLECharacteristic mBatteryLevelCharacteristic = BLECharacteristic(
        BATTERY_LEVEL_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);

    uint8_t mBatteryLevelValue = 42;
    BatteryLevelCallback mBatteryLevelCallback;

    static const BLEUUID BATTERY_SERVICE_UUID;
    static const BLEUUID BATTERY_LEVEL_CHARACTERISTIC_UUID;
};


#endif //OPENBIKESENSORFIRMWARE_BATTERYSERVICE_H
