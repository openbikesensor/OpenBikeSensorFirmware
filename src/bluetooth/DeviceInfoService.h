/*
 * Copyright (C) 2019-2021 OpenBikeSensor Contributors
 * Contact: https://openbikesensor.org
 *
 * This file is part of the OpenBikeSensor sensor firmware.
 *
 * The OpenBikeSensor sensor firmware is free software: you can
 * redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * OpenBikeSensor sensor firmware is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the OpenBikeSensor sensor firmware.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#ifndef OBS_BLUETOOTH_DEVICEINFOSERVICE_H
#define OBS_BLUETOOTH_DEVICEINFOSERVICE_H

#include "_IBluetoothService.h"

/*
 * See https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=244369 for spec
 */
class DeviceInfoService : public IBluetoothService {
  public:
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService* getService() override;

  private:
    BLEService *mService = nullptr;
    BLECharacteristic mFirmwareRevisionCharacteristic
      = BLECharacteristic(FIRMWARE_VERSION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
    BLECharacteristic mManufacturerNameCharacteristic
      = BLECharacteristic(MANUFACTURER_NAME_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);

    static const BLEUUID SERVICE_UUID;
    static const BLEUUID FIRMWARE_VERSION_CHARACTERISTIC_UUID;
    static const BLEUUID MANUFACTURER_NAME_CHARACTERISTIC_UUID;
};

#endif
