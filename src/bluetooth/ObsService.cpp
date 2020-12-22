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
  "Confirmed event: ms timer uint32; left distance cm uint16; right distance cm uint16; 0xffff = no reading");
const BLEUUID ObsService::OBS_SERVICE_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000000");
const BLEUUID ObsService::OBS_TIME_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000001");
const BLEUUID ObsService::OBS_DISTANCE_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000002");
const BLEUUID ObsService::OBS_BUTTON_CHARACTERISTIC_UUID = BLEUUID("1FE7FAF9-CE63-4236-0004-000000000003");

void ObsService::setup(BLEServer *pServer) {
  mService = pServer->createService(OBS_SERVICE_UUID);

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

  // init values
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