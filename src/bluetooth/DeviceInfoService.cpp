#include "DeviceInfoService.h"

const BLEUUID DeviceInfoService::SERVICE_UUID = BLEUUID((uint16_t)0x180a);
const BLEUUID DeviceInfoService::FIRMWARE_VERSION_CHARACTERISTIC_UUID = BLEUUID((uint16_t)0x2a26);
const BLEUUID DeviceInfoService::MANUFACTURER_NAME_CHARACTERISTIC_UUID = BLEUUID((uint16_t)0x2a29);


void DeviceInfoService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_UUID);
  mService->addCharacteristic(&mFirmwareRevisionCharacteristic);
  mFirmwareRevisionCharacteristic.setValue(OBSVersion);
  mService->addCharacteristic(&mManufacturerNameCharacteristic);
  mManufacturerNameCharacteristic.setValue(std::string("openbikesensor.org"));
}

bool DeviceInfoService::shouldAdvertise() {
  return false;
}

BLEService* DeviceInfoService::getService() {
  return mService;
}
