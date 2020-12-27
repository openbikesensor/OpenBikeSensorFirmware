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

const std::string ObsService::TIME_DESCRIPTION_TEXT("obs ms timer uint32");
const std::string ObsService::DISTANCE_DESCRIPTION_TEXT(
  "obs time ms uint32; left distance cm uint16; right distance cm uint16; 0xffff = no reading");
const std::string ObsService::BUTTON_DESCRIPTION_TEXT(
  "Confirmed pass event: ms time uint32; left cm uint16; right cm uint16; 0xffff = no reading");
const std::string ObsService::OFFSET_DESCRIPTION_TEXT(
  "Configured OBS offsets, left offset cm uint16, right offset cm uint16");
const std::string ObsService::TRACK_ID_DESCRIPTION_TEXT(
  "Textual UUID assigned to the current track recording");
const BLEUUID ObsService::OBS_SERVICE_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000000");
const BLEUUID ObsService::OBS_TIME_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000001");
const BLEUUID ObsService::OBS_DISTANCE_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000002");
const BLEUUID ObsService::OBS_BUTTON_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000003");
const BLEUUID ObsService::OBS_OFFSET_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000004");
const BLEUUID ObsService::OBS_TRACK_ID_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000005");

ObsService::ObsService(const uint16_t leftOffset, const uint16_t rightOffset, const String &trackId) {
  *(uint16_t*) mOffsetValue = leftOffset;
  *(uint16_t*) &mOffsetValue[2] = rightOffset;
  strncpy(reinterpret_cast<char *>(&mTrackIdValue), trackId.c_str(), sizeof(mTrackIdValue));
}

void ObsService::setup(BLEServer *pServer) {
  // Each characteristic needs 2 handles and descriptor 1 handle.
  mService = pServer->createService(OBS_SERVICE_UUID, 32);

  mService->addCharacteristic(&mTimeCharacteristic);
  mTimeCharacteristic.addDescriptor(&mTimeDescriptor);
  mTimeDescriptor.setValue(TIME_DESCRIPTION_TEXT);
  mTimeCharacteristic.setCallbacks(&mTimeCharacteristicsCallback);

  mService->addCharacteristic(&mDistanceCharacteristic);
  mDistanceCharacteristic.addDescriptor(&mDistanceDescriptor);
  mDistanceDescriptor.setValue(DISTANCE_DESCRIPTION_TEXT);
  mDistanceCharacteristic.addDescriptor(new BLE2902);

  mService->addCharacteristic(&mButtonCharacteristic);
  mButtonCharacteristic.addDescriptor(&mButtonDescriptor);
  mButtonDescriptor.setValue(BUTTON_DESCRIPTION_TEXT);
  mButtonCharacteristic.addDescriptor(new BLE2902);

  mService->addCharacteristic(&mOffsetCharacteristic);
  mOffsetCharacteristic.addDescriptor(&mOffsetDescriptor);
  mOffsetDescriptor.setValue(OFFSET_DESCRIPTION_TEXT);
  mOffsetCharacteristic.setValue(mOffsetValue, 4);

  mService->addCharacteristic(&mTrackIdCharacteristic);
  mTrackIdCharacteristic.addDescriptor(&mTrackIdDescriptor);
  mTrackIdDescriptor.setValue(TRACK_ID_DESCRIPTION_TEXT);
  mTrackIdCharacteristic.setValue(mTrackIdValue, 36);
}

bool ObsService::shouldAdvertise() {
  return true;
}

BLEService* ObsService::getService() {
  return mService;
}

void ObsService::newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue) {
  *(uint32_t*) mDistanceValue = millis;
  *(uint16_t*) &mDistanceValue[4] = leftValue == MAX_SENSOR_VALUE ? 0xffff : leftValue;
  *(uint16_t*) &mDistanceValue[6] = rightValue == MAX_SENSOR_VALUE ? 0xffff : rightValue;
  mDistanceCharacteristic.setValue(mDistanceValue, 8);
  mDistanceCharacteristic.notify();
}

void ObsService::newPassEvent(uint32_t millis, uint16_t leftValue, uint16_t rightValue) {
  *(uint32_t*) mButtonValue = millis;
  *(uint16_t*) &mButtonValue[4] = leftValue == MAX_SENSOR_VALUE ? 0xffff : leftValue;
  *(uint16_t*) &mButtonValue[6] = rightValue == MAX_SENSOR_VALUE ? 0xffff : rightValue;
  mButtonCharacteristic.setValue(mButtonValue, 8);
  mButtonCharacteristic.notify();
}

void ObsTimeServiceCallback::onRead(BLECharacteristic *pCharacteristic) {
  *mValue = millis();
  pCharacteristic->setValue(*mValue);
}