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

void DistanceService::newSensorValues(const std::list<uint8_t>& leftValues, const std::list<uint8_t>& rightValues) {
  auto transmitValue = String(millis()) + ";";
  transmitValue += joinList(leftValues, ",") + ";";
  transmitValue += joinList(rightValues, ",");

  mCharacteristic->setValue(transmitValue.c_str());
  mCharacteristic->notify();
}

void DistanceService::buttonPressed() {
}
