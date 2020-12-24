//
// Created by Andreas on 23.12.2020.
//

#ifndef OPENBIKESENSORFIRMWARE_BATTERYSERVICE_H
#define OPENBIKESENSORFIRMWARE_BATTERYSERVICE_H

#include "_IBluetoothService.h"

class BatteryLevelCallback : public BLECharacteristicCallbacks {
  public:
    BatteryLevelCallback(uint8_t *value, uint8_t (*getLevel)()) :
      mValue(value), mGetLevel(getLevel) {};
    void onRead(BLECharacteristic *pCharacteristic) override;

  private:
    uint8_t *mValue;
    uint8_t (*mGetLevel)();

};

class BatteryService : public IBluetoothService {
  public:
    BatteryService(uint8_t (*getLevel)()) : mBatteryLevelCallback(&mBatteryLevelValue, getLevel)  { };
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService* getService() override;

  private:
    BLEService *mService = nullptr;
    BLECharacteristic mBatteryLevelCharacteristic = BLECharacteristic(
        BATTERY_LEVEL_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
    BLEDescriptor mBatteryLevelDescriptor = BLEUUID((uint16_t)0x2901);

    uint8_t mBatteryLevelValue = 42;
    BatteryLevelCallback mBatteryLevelCallback;

    static const std::string BATTERY_LEVEL_DESCRIPTION_TEXT;
    static const BLEUUID BATTERY_SERVICE_UUID;
    static const BLEUUID BATTERY_LEVEL_CHARACTERISTIC_UUID;
};


#endif //OPENBIKESENSORFIRMWARE_BATTERYSERVICE_H
