#include "BluetoothManager.h"

BLEServer *pServer;
std::list<IBluetoothService*> services;

unsigned long lastValueTimestamp = millis();
boolean buttonWasPressed = false;
unsigned long buttonPressTimestamp = -1;

void BluetoothManager::init() {
  BLEDevice::init("OpenBikeSensor");
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
  digitalWrite(13, HIGH);
}

void BluetoothManager::deactivateBluetooth() {
  pServer->getAdvertising()->stop();
}

void BluetoothManager::disconnectDevice() {
  pServer->disconnect(pServer->getConnId());
}

void BluetoothManager::newSensorValues(const std::list<uint8_t>& leftValues, const std::list<uint8_t>& rightValues) {
  // Discarding values if they are more recent than 50 ms
  if (millis() - lastValueTimestamp < 50) return;
  lastValueTimestamp = millis();

  for (auto &service : services) {
    service->newSensorValues(leftValues, rightValues);
  }
}

/**
 * Triggers the buttonPressed() method if the button is pressed.
 * After a button press, we wait for a time window of 300ms in which no button
 * presses occur. Afterwards, new button presses will result in triggering
 * buttonPressed() again.
 *
 * The method is implemented in this way because the `state` can be erroneously
 * set to `LOW` even if the button is still pressed. This implemention fixes
 * that problem.
 * @param state current state of the push button (LOW or HIGH)
 */
void BluetoothManager::processButtonState(int state) {
  if (state == HIGH) {
    if (!buttonWasPressed) buttonPressed();

    buttonWasPressed = true;
    buttonPressTimestamp = millis();
  }

  if (buttonWasPressed && (millis() - buttonPressTimestamp) >= 300) {
    buttonWasPressed = false;
    buttonPressTimestamp = -1;
  }
}

void BluetoothManager::buttonPressed() {
  for (auto &service : services) {
    service->buttonPressed();
  }
}
