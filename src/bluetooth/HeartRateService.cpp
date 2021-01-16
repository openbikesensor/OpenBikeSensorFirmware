#include "HeartRateService.h"

const unsigned long measurementInterval = 500;

const BLEUUID HeartRateService::SERVICE_UUID = BLEUUID((uint16_t)ESP_GATT_UUID_HEART_RATE_SVC);

void HeartRateService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_UUID); // Keep the defaults!!, 5);
  mService->addCharacteristic(&mHeartRateMeasurementCharacteristics);
  mHeartRateMeasurementCharacteristics.addDescriptor(new BLE2902());
  mValue[0] = mValue[1] = mValue[2] = 0;
  mHeartRateMeasurementCharacteristics.setValue(mValue, 2);

  mCollectionStartTime = millis();
}

bool HeartRateService::shouldAdvertise() {
  return true;
}

BLEService* HeartRateService::getService() {
  return mService;
}

void HeartRateService::newSensorValues(
    const uint32_t millis, const uint16_t leftValue, const uint16_t rightValue) {
  if (leftValue < mMinimumDistance) {
    mMinimumDistance = leftValue;
  }
  if ((millis - mCollectionStartTime) < measurementInterval) {
    return;
  }

  mValue[0] = mMinimumDistance <= UINT8_MAX ? 0 : 1; // 8/16 bit data no other flags set;
  mValue[1] = mMinimumDistance & 0xFFu;
  mValue[2] = mMinimumDistance >> 8u;

  mHeartRateMeasurementCharacteristics.setValue(mValue, mMinimumDistance <= UINT8_MAX ? 2 : 3);
  mHeartRateMeasurementCharacteristics.notify();

  // Reset values
  mMinimumDistance = MAX_SENSOR_VALUE;
  mCollectionStartTime = millis;
}
