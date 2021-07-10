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

#include "DistanceService.h"

void DistanceService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_DISTANCE_UUID);
  mCharacteristic = mService->createCharacteristic(SERVICE_DISTANCE_CHAR_50MS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
// SimRa does not like BLE2902
}

bool DistanceService::shouldAdvertise() {
  return false;
}

BLEService* DistanceService::getService() {
  return mService;
}

void DistanceService::newSensorValues(const uint32_t millis, const uint16_t leftValue, const uint16_t rightValue) {
  auto transmitValue = String(millis) + ";";
  transmitValue += valueAsString(leftValue) + ";";
  transmitValue += valueAsString(rightValue);

  mCharacteristic->setValue(transmitValue.c_str());
  mCharacteristic->notify();
}
