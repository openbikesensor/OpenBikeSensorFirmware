#ifndef OBS_BLUETOOTH_HEARTRATESERVICE_H
#define OBS_BLUETOOTH_HEARTRATESERVICE_H

#include "_IBluetoothService.h"

class HeartRateService : public IBluetoothService {
  public:
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService* getService() override;
    void newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue) override;

  private:
    BLEService *mService = nullptr;
    BLECharacteristic mHeartRateMeasurementCharacteristics
      = BLECharacteristic(BLEUUID((uint16_t)ESP_GATT_HEART_RATE_MEAS), BLECharacteristic::PROPERTY_NOTIFY);
    unsigned long mCollectionStartTime = 0;
    uint16_t mMinimumDistance = MAX_SENSOR_VALUE;
    uint8_t mValue[4];

    static const BLEUUID SERVICE_UUID;
};

#endif
