#include "ClosePassService.h"

void ClosePassService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_CLOSEPASS_UUID);
  mEventCharacteristic = mService->createCharacteristic(SERVICE_CLOSEPASS_CHAR_EVENT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
}

bool ClosePassService::shouldAdvertise() {
  return true;
}

BLEService* ClosePassService::getService() {
  return mService;
}

void ClosePassService::newPassEvent(const uint32_t millis, const uint16_t leftValue, const uint16_t rightValue) {
  auto transmitValue = String(millis) + ";button;" + valueAsString(leftValue) + ";" + valueAsString(rightValue);
  mEventCharacteristic->setValue(transmitValue.c_str());
  mEventCharacteristic->notify();
}
