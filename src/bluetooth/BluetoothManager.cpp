#include "BluetoothManager.h"

void BluetoothManager::init(
  const String &obsName,
  const uint16_t leftOffset, const uint16_t rightOffset,
  std::function<uint8_t()> batteryPercentage,
  const String &trackId) {

  ESP_ERROR_CHECK_WITHOUT_ABORT(
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
  BLEDevice::init(obsName.c_str());
  pServer = BLEDevice::createServer();

  services.push_back(new DeviceInfoService);
  services.push_back(new HeartRateService);
  services.push_back(new BatteryService(batteryPercentage));
  services.push_back(new DistanceService);
  services.push_back(new ClosePassService);
  services.push_back(new ObsService(leftOffset, rightOffset, trackId));

  for (auto &service : services) {
    service->setup(pServer);
  }
  for (auto &service : services) {
    service->getService()->start();
  }
}

void BluetoothManager::activateBluetooth() const {
  for (auto &service : services) {
    if (service->shouldAdvertise()) {
      pServer->getAdvertising()->addServiceUUID(service->getService()->getUUID());
    }
  }
  pServer->getAdvertising()->start();
}

void BluetoothManager::deactivateBluetooth() const {
  pServer->getAdvertising()->stop();
}

void BluetoothManager::disconnectDevice() const {
  pServer->disconnect(pServer->getConnId());
}

void BluetoothManager::newSensorValues(const uint32_t millis, const uint16_t leftValues, const uint16_t rightValues) {
  if (hasConnectedClients()) {
    for (auto &service : services) {
      service->newSensorValues(millis, leftValues, rightValues);
    }
  }
}

void BluetoothManager::newPassEvent(const uint32_t millis, const uint16_t leftValue, const uint16_t rightValue) {
  if (hasConnectedClients()) {
    for (auto &service : services) {
      service->newPassEvent(millis, leftValue, rightValue);
    }
  }
}

bool BluetoothManager::hasConnectedClients() {
  return pServer->getConnectedCount() != 0;
}
