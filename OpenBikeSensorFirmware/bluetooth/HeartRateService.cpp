#include "HeartRateService.h"

const unsigned long measurementInterval = 1000;

void HeartRateService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_HEARTRATE_UUID);
  mCharacteristic = mService->createCharacteristic(SERVICE_HEARTRATE_CHAR_HEARTRATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

  auto *pDescriptor = new BLEDescriptor(SERVICE_HEARTRATE_DESCRIPTOR_UUID);
  mCharacteristic->addDescriptor(pDescriptor);
  uint8_t descriptorbuffer;
  descriptorbuffer = 1;
  pDescriptor->setValue(&descriptorbuffer, 1);

  BLECharacteristic *pSensorLocationCharacteristic = mService->createCharacteristic(SERVICE_HEARTRATE_CHAR_SENSORLOCATION_UUID, BLECharacteristic::PROPERTY_READ);
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

void HeartRateService::newSensorValue(float value) {
  mDistances.push(value);

  if ((millis() - mCollectionStartTime) < measurementInterval) {
    return;
  }

  // Calculate average of measurement interval
  float distanceAvg = 0.0;
  using index_t = decltype(mDistances)::index_t;
  for (index_t i = 0; i < mDistances.size(); i++) {
    distanceAvg += mDistances[i] / (float) mDistances.size();
  }

  // Transmit average
  uint8_t buffer[2] = {
    0, // whether 8 or 16 bit
    uint8_t(distanceAvg)
  };
  mCharacteristic->setValue(buffer, 2);
  mCharacteristic->notify();

  // Reset values
  mDistances.clear();
  mCollectionStartTime = millis();
}
