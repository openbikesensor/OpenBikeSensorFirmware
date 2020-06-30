#include "BluetoothManager.h"

void BluetoothManager::init() {
  BLEDevice::init("OpenBikeSensor");
  pServer = BLEDevice::createServer();
  //pServer->setCallbacks(new CustomBTCallback());

  // Decide here what services should be instantiated
  services.push_back(new DeviceInfoService);
  services.push_back(new DistanceService);
  services.push_back(new HeartRateService);

  // Create the services
  for (auto &service : services) {
    service->setup(pServer);
  }

  // Start the service
  for (auto &service : services) {
    service->getService()->start();
  }
}

void BluetoothManager::activateBluetooth() {
  for (auto &service : services) {
    if (service->shouldAdvertise()) {
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

void BluetoothManager::newSensorValue(float value) {
  for (auto &service : services) {
    service->newSensorValue(value);
  }
}
