/*
 * Copyright (C) 2019-2021 OpenBikeSensor Contributors
 * Contact: https://openbikesensor.org
 *
 * This file is part of the OpenBikeSensor sensor firmware.
 *
 * The OpenBikeSensor sensor firmware is free software: you can
 * redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * OpenBikeSensor sensor firmware is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the OpenBikeSensor sensor firmware.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

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
