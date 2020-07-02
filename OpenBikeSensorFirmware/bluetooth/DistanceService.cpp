#include "DistanceService.h"

void DistanceService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_DISTANCE_UUID);
  mCharacteristic = mService->createCharacteristic(SERVICE_DISTANCE_CHAR_50MS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
}

bool DistanceService::shouldAdvertise() {
  return true;
}

BLEService* DistanceService::getService() {
  return mService;
}

void DistanceService::newSensorValue(uint8_t value) {
  String transmitValue = String(millis()) + "," + String(value);

  mCharacteristic->setValue(transmitValue.c_str());
  mCharacteristic->notify();
}
