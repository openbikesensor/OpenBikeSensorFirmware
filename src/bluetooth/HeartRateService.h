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

#ifndef OBS_BLUETOOTH_HEARTRATESERVICE_H
#define OBS_BLUETOOTH_HEARTRATESERVICE_H

#include "_IBluetoothService.h"

class HeartRateService : public IBluetoothService {
  public:
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService* getService() override;
    void newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue) override;

  private:
    BLEService *mService = nullptr;
    BLECharacteristic mHeartRateMeasurementCharacteristics
      = BLECharacteristic(BLEUUID((uint16_t)ESP_GATT_HEART_RATE_MEAS), BLECharacteristic::PROPERTY_NOTIFY);
    unsigned long mCollectionStartTime = 0;
    uint16_t mMinimumDistance = MAX_SENSOR_VALUE;
    uint8_t mValue[4];

    static const BLEUUID SERVICE_UUID;
};

#endif
