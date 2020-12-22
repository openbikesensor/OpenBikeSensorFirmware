#include "BluetoothManager.h"

void BluetoothManager::init() {

  ESP_ERROR_CHECK_WITHOUT_ABORT(
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
  char deviceName[32];
  snprintf(deviceName, sizeof(deviceName), "OpenBikeSensor-%04X", (uint16_t)(ESP.getEfuseMac() >> 32));
  BLEDevice::init(deviceName);
  pServer = BLEDevice::createServer();
  //pServer->setCallbacks(new CustomBTCallback());

  // Decide here what services should be instantiated
#ifdef BLUETOOTH_SERVICE_CLOSEPASS
  services.push_back(new ClosePassService);
#endif
#ifdef BLUETOOTH_SERVICE_CONNECTION
  services.push_back(new ConnectionService);
#endif
#ifdef BLUETOOTH_SERVICE_DEVICEINFO
  services.push_back(new DeviceInfoService);
#endif
#ifdef BLUETOOTH_SERVICE_DISTANCE
  services.push_back(new DistanceService);
#endif
#ifdef BLUETOOTH_SERVICE_HEARTRATE
  services.push_back(new HeartRateService);
#endif

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

/**
 * Triggers the buttonPressed() method if the button is pressed.
 * After a button press, we wait for a time window of 300ms in which no button
 * presses occur. Afterwards, new button presses will result in triggering
 * buttonPressed() again.
 *
 * The method is implemented in this way because the `state` can be erroneously
 * set to `LOW` even if the button is still pressed. This implementation fixes
 * that problem.
 * @param state current state of the push button (LOW or HIGH)
 */
void BluetoothManager::processButtonState(int state) {
  if (state == HIGH) {
    if (!buttonWasPressed) {
      buttonPressed();
    }

    buttonWasPressed = true;
    buttonPressTimestamp = millis();
  }

  if (buttonWasPressed && (millis() - buttonPressTimestamp) >= 300) {
    buttonWasPressed = false;
    buttonPressTimestamp = 0;
  }
}

void BluetoothManager::buttonPressed() const {
  for (auto &service : services) {
    service->buttonPressed();
  }
}
