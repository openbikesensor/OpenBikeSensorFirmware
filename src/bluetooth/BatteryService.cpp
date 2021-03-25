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

#include "BatteryService.h"

const BLEUUID BatteryService::BATTERY_SERVICE_UUID = BLEUUID((uint16_t)0x180f);
const BLEUUID BatteryService::BATTERY_LEVEL_CHARACTERISTIC_UUID = BLEUUID((uint16_t)0x2a19);


void BatteryService::setup(BLEServer *pServer) {
  mService = pServer->createService(BATTERY_SERVICE_UUID);

  mService->addCharacteristic(&mBatteryLevelCharacteristic);
  mBatteryLevelCharacteristic.setCallbacks(&mBatteryLevelCallback);
}

bool BatteryService::shouldAdvertise() {
  return true;
}

BLEService* BatteryService::getService() {
  return mService;
}

void BatteryLevelCallback::onRead(BLECharacteristic *pCharacteristic) {
  *mValue = mGetLevel();
  pCharacteristic->setValue(mValue, 1);
}