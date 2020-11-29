#include "HeartRateService.h"

const unsigned long measurementInterval = 1000;

void HeartRateService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_HEARTRATE_UUID);
  mCharacteristic = mService->createCharacteristic(SERVICE_HEARTRATE_CHAR_HEARTRATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

  auto *pDescriptor = new BLEDescriptor(SERVICE_HEARTRATE_DESCRIPTOR_UUID);
  mCharacteristic->addDescriptor(pDescriptor);
  uint8_t descriptorBuffer = 1;
  pDescriptor->setValue(&descriptorBuffer, 1);

  BLECharacteristic *pSensorLocationCharacteristic
    = mService->createCharacteristic(SERVICE_HEARTRATE_CHAR_SENSORLOCATION_UUID, BLECharacteristic::PROPERTY_READ);
  uint8_t locationValue = SERVICE_HEARTRATE_CHAR_SENSORLOCATION_VALUE;
  pSensorLocationCharacteristic->setValue(&locationValue, 1);

  mCollectionStartTime = millis();
}

bool HeartRateService::shouldAdvertise() {
  return true;
}

BLEService* HeartRateService::getService() {
  return mService;
}

void HeartRateService::newSensorValues(const uint16_t leftValue, const uint16_t rightValue) {
  const unsigned long now = millis();
  if (leftValue < mMinimumDistance) {
    mMinimumDistance = leftValue;
  }
  if ((now - mCollectionStartTime) < measurementInterval) {
    return;
  }

  uint8_t data[3];
  data[0] = mMinimumDistance <= UINT8_MAX ? 0 : 1; // 8/16 bit data no other flags set;
  data[1] = mMinimumDistance & 0xFFu;
  data[2] = mMinimumDistance >> 8u;

  mCharacteristic->setValue(data, mMinimumDistance <= UINT8_MAX ? 2 : 3);
  mCharacteristic->notify();

  // Reset values
  mMinimumDistance = MAX_SENSOR_VALUE;
  mCollectionStartTime = now;
}

void HeartRateService::buttonPressed() {
}
