#ifndef OBS_BLUETOOTH_DEVICEINFOSERVICE_H
#define OBS_BLUETOOTH_DEVICEINFOSERVICE_H

#include "_IBluetoothService.h"

/*
 * See https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=244369 for spec
 */
class DeviceInfoService : public IBluetoothService {
  public:
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService* getService() override;

  private:
    BLEService *mService = nullptr;
    BLECharacteristic mFirmwareRevisionCharacteristic
      = BLECharacteristic(FIRMWARE_VERSION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
    BLECharacteristic mManufacturerNameCharacteristic
      = BLECharacteristic(MANUFACTURER_NAME_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);

    static const BLEUUID SERVICE_UUID;
    static const BLEUUID FIRMWARE_VERSION_CHARACTERISTIC_UUID;
    static const BLEUUID MANUFACTURER_NAME_CHARACTERISTIC_UUID;
};

#endif
