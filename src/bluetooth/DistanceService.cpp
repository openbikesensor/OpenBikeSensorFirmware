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

void DistanceService::newSensorValues(const uint32_t millis, const uint16_t leftValue, const uint16_t rightValue) {
  auto transmitValue = String(millis) + ";";
  transmitValue += valueAsString(leftValue) + ";";
  transmitValue += valueAsString(rightValue);

  mCharacteristic->setValue(transmitValue.c_str());
  mCharacteristic->notify();
}
