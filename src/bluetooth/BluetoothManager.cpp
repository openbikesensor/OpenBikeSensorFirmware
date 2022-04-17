/*
 * Copyright (C) 2019-2021 OpenBikeSensor Contributors
 * Contact: https://openbikesensor.org
 *
 * This file is part of the OpenBikeSensor firmware.
 *
 * The OpenBikeSensor firmware is free software: you can
 * redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * OpenBikeSensor firmware is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the OpenBikeSensor firmware.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include "BluetoothManager.h"

const uint32_t BluetoothManager::HIGH_ADVERTISEMENT_TIME_MS = 60 * 1000;

void BluetoothManager::init(
  const String &obsName,
  const uint16_t leftOffset, const uint16_t rightOffset,
  std::function<uint8_t()> batteryPercentage,
  const String &trackId) {

  deviceConnected = false;
  ESP_ERROR_CHECK_WITHOUT_ABORT(
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
  BLEDevice::init(obsName.c_str());
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(this);

  services.push_back(new DeviceInfoService);
  services.push_back(new HeartRateService);
  services.push_back(new BatteryService(batteryPercentage));
  services.push_back(new ObsService(leftOffset, rightOffset, trackId));

  for (auto &service : services) {
    service->setup(pServer);
  }
  for (auto &service : services) {
    service->getService()->start();
  }
  lastDisconnected = millis();
}

void BluetoothManager::activateBluetooth() {
  auto adv = pServer->getAdvertising();

  // only the heard rate service && OBS service use advertisement
  // if I add one more service the 0x12 info is not sent.
  for (auto &service : services) {
    if (service->shouldAdvertise()) {
      adv->addServiceUUID(service->getService()->getUUID());
    }
  }
  setFastAdvertising();

//  Save some bytes in the advertising payload
//  https://specificationrefs.bluetooth.com/assigned-values/Appearance%20Values.pdf
//  adv->setAppearance(1152); // Generic: Cycling

// causes 0x12 Slave Connection Interval Range to be sent
  adv->setMinPreferred(0x06);  // Apple?
  adv->setMaxPreferred(0x12);

  BLEDevice::startAdvertising();
}

void BluetoothManager::deactivateBluetooth() const {
  pServer->getAdvertising()->stop();
}

void BluetoothManager::disconnectDevice() const {
  pServer->disconnect(pServer->getConnId());
}

void BluetoothManager::newSensorValues(const uint32_t millis, const uint16_t leftValues, const uint16_t rightValues) {
  if (deviceConnected) {
    for (auto &service : services) {
      service->newSensorValues(millis, leftValues, rightValues);
    }
  } else if (lastDisconnected + HIGH_ADVERTISEMENT_TIME_MS < millis) {
    setSlowAdvertising();
  }
}

void BluetoothManager::newPassEvent(const uint32_t millis, const uint16_t leftValue, const uint16_t rightValue) {
  log_i("BLE new pass event at %d, left: %dcm, right: %dcm", millis, leftValue, rightValue);
  if (deviceConnected) {
    for (auto &service : services) {
      service->newPassEvent(millis, leftValue, rightValue);
    }
  }
}

bool BluetoothManager::hasConnectedClients() {
  return pServer->getConnectedCount() != 0;
}

void BluetoothManager::onConnect(BLEServer* pServer) {
  log_i("BTLE connected!");
  deviceConnected = true;
};

void BluetoothManager::onDisconnect(BLEServer* pServer) {
  log_i("BTLE disconnected!");
  lastDisconnected = millis();
  deviceConnected = false;
  setFastAdvertising();
  pServer->startAdvertising();
}

void BluetoothManager::setFastAdvertising() {
  auto adv = pServer->getAdvertising();
  adv->setMinInterval(48  /*  0.625 msec =  30ms */);
  adv->setMaxInterval(80  /*  0.625 msec =  50ms */);
  fastAdvertising = true;
}

void BluetoothManager::setSlowAdvertising() {
  if (fastAdvertising) {
    log_i("Decreasing BTLE advertisement interval.");
    auto adv = pServer->getAdvertising();
    adv->stop();
    adv->setMinInterval(800   /*  0.625 msec = 500ms */);
    adv->setMaxInterval(1280  /*  0.625 msec = 800ms */);
    adv->start();
    fastAdvertising = false;
  }
}
