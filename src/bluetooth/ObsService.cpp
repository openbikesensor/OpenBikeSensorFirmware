/*
  Copyright (C) 2019 Zweirat
  Contact: https://openbikesensor.org

  This file is part of the OpenBikeSensor project.

  The OpenBikeSensor sensor firmware is free software: you can redistribute
  it and/or modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  The OpenBikeSensor sensor firmware is distributed in the hope that it will
  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
  Public License for more details.

  You should have received a copy of the GNU General Public License along with
  the OpenBikeSensor sensor firmware.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "ObsService.h"

const BLEUUID ObsService::OBS_SERVICE_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000000");
const BLEUUID ObsService::OBS_TIME_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000001");
const BLEUUID ObsService::OBS_DISTANCE_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000002");
const BLEUUID ObsService::OBS_BUTTON_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000003");
const BLEUUID ObsService::OBS_OFFSET_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000004");
const BLEUUID ObsService::OBS_TRACK_ID_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000005");

ObsService::ObsService(const uint16_t leftOffset, const uint16_t rightOffset, const String &trackId) {
  uint8_t offsets[4];
  memcpy(offsets, &leftOffset, 2);
  memcpy(&offsets[2], &rightOffset, 2);
  mOffsetCharacteristic.setValue(offsets, 4);
  mTrackIdCharacteristic.setValue(trackId.c_str());
}

void ObsService::setup(BLEServer *pServer) {
  // Each characteristic needs 2 handles and descriptor 1 handle.
  mService = pServer->createService(OBS_SERVICE_UUID); // Keep the default, 18);

  mService->addCharacteristic(&mTimeCharacteristic);
  mTimeCharacteristic.setCallbacks(&mTimeCharacteristicsCallback);

  mService->addCharacteristic(&mDistanceCharacteristic);
  mDistanceCharacteristic.addDescriptor(new BLE2902);

  mService->addCharacteristic(&mButtonCharacteristic);
  mButtonCharacteristic.addDescriptor(new BLE2902);

  mService->addCharacteristic(&mOffsetCharacteristic);

  mService->addCharacteristic(&mTrackIdCharacteristic);
}

bool ObsService::shouldAdvertise() {
  return true;
}

BLEService* ObsService::getService() {
  return mService;
}

void ObsService::newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue) {
  sendEventData(&mDistanceCharacteristic, millis, leftValue, rightValue);
}

void ObsService::newPassEvent(uint32_t millis, uint16_t leftValue, uint16_t rightValue) {
  sendEventData(&mButtonCharacteristic, millis, leftValue, rightValue);
}

void ObsTimeServiceCallback::onRead(BLECharacteristic *pCharacteristic) {
  uint32_t value = millis();
  pCharacteristic->setValue(value);
}

void ObsService::sendEventData(BLECharacteristic *characteristic, uint32_t millis, uint16_t leftValue, uint16_t rightValue) {
  uint8_t event[8];
  memcpy(event, &millis, sizeof(millis));
  if (leftValue == MAX_SENSOR_VALUE) {
    leftValue = 0xffff;
  }
  memcpy(&event[4], &leftValue, sizeof(leftValue));
  if (rightValue == MAX_SENSOR_VALUE) {
    rightValue = 0xffff;
  }
  memcpy(&event[6], &rightValue, sizeof(rightValue));
  characteristic->setValue(event, 8);
  characteristic->notify();
}
