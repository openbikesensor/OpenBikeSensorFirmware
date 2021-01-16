//
// Created by Andreas on 23.12.2020.
//

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