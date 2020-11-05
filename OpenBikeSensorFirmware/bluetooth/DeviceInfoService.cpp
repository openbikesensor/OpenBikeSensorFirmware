#include "DeviceInfoService.h"

void DeviceInfoService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_DEVICE_UUID);

  BLECharacteristic *pSystemIDCharcateristic = mService->createCharacteristic(SERVICE_DEVICE_CHAR_SYSTEMID_UUID, BLECharacteristic::PROPERTY_READ);
  uint8_t systemIDValueHex[SERVICE_DEVICE_CHAR_SYSTEMID_VALUE_HEX_LEN] = SERVICE_DEVICE_CHAR_SYSTEMID_VALUE_HEX;
  pSystemIDCharcateristic->setValue(systemIDValueHex, SERVICE_DEVICE_CHAR_SYSTEMID_VALUE_HEX_LEN);

  BLECharacteristic *pModelNumberStringCharacteristic = mService->createCharacteristic(SERVICE_DEVICE_CHAR_MODELNUMBER_STRING_UUID, BLECharacteristic::PROPERTY_READ);
  pModelNumberStringCharacteristic->setValue(SERVICE_DEVICE_CHAR_MODELNUMBER_STRING_VALUE);

  BLECharacteristic *pSerialNumberStringCharacteristic = mService->createCharacteristic(SERVICE_DEVICE_CHAR_SERIALNUMBER_STRING_UUID, BLECharacteristic::PROPERTY_READ);
  pSerialNumberStringCharacteristic->setValue(SERVICE_DEVICE_CHAR_SERIALNUMBER_STRING_VALUE);

  BLECharacteristic *pFirmwareRevisionCharacteristic = mService->createCharacteristic(SERVICE_DEVICE_CHAR_FIRMWAREREVISON_UUID, BLECharacteristic::PROPERTY_READ);
  pFirmwareRevisionCharacteristic->setValue(SERVICE_DEVICE_CHAR_FIRMWAREREVISON_VALUE);

  BLECharacteristic *pHardwareRevisionCharacteristic = mService->createCharacteristic(SERVICE_DEVICE_CHAR_HARDWAREREVISION_UUID, BLECharacteristic::PROPERTY_READ);
  pHardwareRevisionCharacteristic->setValue(SERVICE_DEVICE_CHAR_HARDWAREREVISION_VALUE);

  BLECharacteristic *pSoftwareRevisionCharacteristic = mService->createCharacteristic(SERVICE_DEVICE_CHAR_SOFTWAREREVISION_UUID, BLECharacteristic::PROPERTY_READ);
  pSoftwareRevisionCharacteristic->setValue(SERVICE_DEVICE_CHAR_SOFTWAREREVISION_VALUE);

  BLECharacteristic *pManufacturerNameCharacteristic = mService->createCharacteristic(SERVICE_DEVICE_CHAR_MANUFACTURERNAME_STRING_UUID, BLECharacteristic::PROPERTY_READ);
  pManufacturerNameCharacteristic->setValue(SERVICE_DEVICE_CHAR_MANUFACTURERNAME_STRING_VALUE);
}

bool DeviceInfoService::shouldAdvertise() {
  return false;
}

BLEService* DeviceInfoService::getService() {
  return mService;
}

void DeviceInfoService::newSensorValues(const std::list<uint16_t>& leftValues, const std::list<uint16_t>& rightValues) {
}

void DeviceInfoService::buttonPressed() {
}