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

#include "HeartRateService.h"

const unsigned long measurementInterval = 500;

const BLEUUID HeartRateService::SERVICE_UUID = BLEUUID((uint16_t)ESP_GATT_UUID_HEART_RATE_SVC);

void HeartRateService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_UUID); // Keep the defaults!!, 5);
  mService->addCharacteristic(&mHeartRateMeasurementCharacteristics);
  mHeartRateMeasurementCharacteristics.addDescriptor(new BLE2902());
  mValue[0] = mValue[1] = mValue[2] = 0;
  mHeartRateMeasurementCharacteristics.setValue(mValue, 2);

  mCollectionStartTime = millis();
}

bool HeartRateService::shouldAdvertise() {
  return true;
}

BLEService* HeartRateService::getService() {
  return mService;
}

void HeartRateService::newSensorValues(
    const uint32_t millis, const uint16_t leftValue, const uint16_t rightValue) {
  if (leftValue < mMinimumDistance) {
    mMinimumDistance = leftValue;
  }
  if ((millis - mCollectionStartTime) < measurementInterval) {
    return;
  }

  mValue[0] = mMinimumDistance <= UINT8_MAX ? 0 : 1; // 8/16 bit data no other flags set;
  mValue[1] = mMinimumDistance & 0xFFu;
  mValue[2] = mMinimumDistance >> 8u;

  mHeartRateMeasurementCharacteristics.setValue(mValue, mMinimumDistance <= UINT8_MAX ? 2 : 3);
  mHeartRateMeasurementCharacteristics.notify();

  // Reset values
  mMinimumDistance = MAX_SENSOR_VALUE;
  mCollectionStartTime = millis;
}
