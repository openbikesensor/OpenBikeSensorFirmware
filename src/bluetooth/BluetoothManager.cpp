#include "BluetoothManager.h"

void BluetoothManager::init() {

  ESP_ERROR_CHECK_WITHOUT_ABORT(
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
  char deviceName[32];
  snprintf(deviceName, sizeof(deviceName), "OpenBikeSensor-%04X", (uint16_t)(ESP.getEfuseMac() >> 32));
  BLEDevice::init(deviceName);
  pServer = BLEDevice::createServer();

  services.push_back(new HeartRateService);
//  services.push_back(new DeviceInfoService);
  services.push_back(new DistanceService);
  services.push_back(new ConnectionService);
  services.push_back(new ClosePassService);
  services.push_back(new ObsService);

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
  // Discarding values if they are more recent than 50 ms
  if (millis - lastValueTimestamp < 50) {
    return;
  }
  lastValueTimestamp = millis;

  for (auto &service : services) {
    service->newSensorValues(millis, leftValues, rightValues);
  }
}

void BluetoothManager::newPassEvent(const uint32_t millis, const uint16_t leftValue, const uint16_t rightValue) {
  for (auto &service : services) {
    service->newPassEvent(millis, leftValue, rightValue);
  }
}
