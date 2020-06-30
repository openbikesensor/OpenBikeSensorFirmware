#include "BluetoothManager.h"

void BluetoothManager::init() {
  BLEDevice::init("OpenBikeSensor");
  pServer = BLEDevice::createServer();
  //pServer->setCallbacks(new CustomBTCallback());

  services.push_back(new DistanceService);
  services.push_back(new HeartRateService);

  // Create the services
  for (auto &service : services) {
    service->setup(pServer);
  }

  // Start the service
  for (auto &service : services) {
    if (service->hasService()) {
      service->getService()->start();
    }
  }
}

void BluetoothManager::activateBluetooth() {
  for (auto &service : services) {
    if (service->hasService() && service->shouldAdvertise()) {
      pServer->getAdvertising()->addServiceUUID(service->getService()->getUUID());
    }
  }

  pServer->getAdvertising()->start();
}

void BluetoothManager::deactivateBluetooth() {
  pServer->getAdvertising()->stop();
}

void BluetoothManager::disconnectDevice() {
  pServer->disconnect(pServer->getConnId());
}

BLEService *BluetoothManager::createDeviceInfoService() {
  BLEService *service = pServer->createService(SERVICE_DEVICE_UUID);

  BLECharacteristic *pSystemIDCharcateristic = service->createCharacteristic(SERVICE_DEVICE_CHAR_SYSTEMID_UUID,
                                                                             BLECharacteristic::PROPERTY_READ);
  uint8_t systemIDValueHex[SERVICE_DEVICE_CHAR_SYSTEMID_VALUE_HEX_LEN] = SERVICE_DEVICE_CHAR_SYSTEMID_VALUE_HEX;
  pSystemIDCharcateristic->setValue(systemIDValueHex, SERVICE_DEVICE_CHAR_SYSTEMID_VALUE_HEX_LEN);

  BLECharacteristic *pModelNumberStringCharacteristic = service->createCharacteristic(
    SERVICE_DEVICE_CHAR_MODELNUMBER_STRING_UUID, BLECharacteristic::PROPERTY_READ);
  pModelNumberStringCharacteristic->setValue(SERVICE_DEVICE_CHAR_MODELNUMBER_STRING_VALUE);

  BLECharacteristic *pSerialNumberStringCharacteristic = service->createCharacteristic(
    SERVICE_DEVICE_CHAR_SERIALNUMBER_STRING_UUID, BLECharacteristic::PROPERTY_READ);
  pSerialNumberStringCharacteristic->setValue(SERVICE_DEVICE_CHAR_SERIALNUMBER_STRING_VALUE);

  BLECharacteristic *pFirmwareRevisionCharacteristic = service->createCharacteristic(
    SERVICE_DEVICE_CHAR_FIRMWAREREVISON_UUID, BLECharacteristic::PROPERTY_READ);
  pFirmwareRevisionCharacteristic->setValue(SERVICE_DEVICE_CHAR_FIRMWAREREVISON_VALUE);

  BLECharacteristic *pHardwareRevisionCharacteristic = service->createCharacteristic(
    SERVICE_DEVICE_CHAR_HARDWAREREVISION_UUID, BLECharacteristic::PROPERTY_READ);
  pHardwareRevisionCharacteristic->setValue(SERVICE_DEVICE_CHAR_HARDWAREREVISION_VALUE);

  BLECharacteristic *pSoftwareRevisionCharacteristic = service->createCharacteristic(
    SERVICE_DEVICE_CHAR_SOFTWAREREVISION_UUID, BLECharacteristic::PROPERTY_READ);
  pSoftwareRevisionCharacteristic->setValue(SERVICE_DEVICE_CHAR_SOFTWAREREVISION_VALUE);

  BLECharacteristic *pManufacturerNameCharacteristic = service->createCharacteristic(
    SERVICE_DEVICE_CHAR_MANUFACTURERNAME_STRING_UUID, BLECharacteristic::PROPERTY_READ);
  pManufacturerNameCharacteristic->setValue(SERVICE_DEVICE_CHAR_MANUFACTURERNAME_STRING_VALUE);

  return service;
}

