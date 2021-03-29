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

#include "DeviceInfoService.h"

const BLEUUID DeviceInfoService::SERVICE_UUID = BLEUUID((uint16_t)0x180a);
const BLEUUID DeviceInfoService::FIRMWARE_VERSION_CHARACTERISTIC_UUID = BLEUUID((uint16_t)0x2a26);
const BLEUUID DeviceInfoService::MANUFACTURER_NAME_CHARACTERISTIC_UUID = BLEUUID((uint16_t)0x2a29);


void DeviceInfoService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_UUID);
  mService->addCharacteristic(&mFirmwareRevisionCharacteristic);
  mFirmwareRevisionCharacteristic.setValue(OBSVersion);
  mService->addCharacteristic(&mManufacturerNameCharacteristic);
  mManufacturerNameCharacteristic.setValue(std::string("openbikesensor.org"));
}

bool DeviceInfoService::shouldAdvertise() {
  return false;
}

BLEService* DeviceInfoService::getService() {
  return mService;
}
