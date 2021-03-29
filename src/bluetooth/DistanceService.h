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

#ifndef OBS_BLUETOOTH_DISTANCESERVICE_H
#define OBS_BLUETOOTH_DISTANCESERVICE_H

#include "_IBluetoothService.h"

#define SERVICE_DISTANCE_UUID "1FE7FAF9-CE63-4236-0001-000000000000"
#define SERVICE_DISTANCE_CHAR_50MS_UUID "1FE7FAF9-CE63-4236-0001-000000000001"

class DistanceService : public IBluetoothService {
  public:
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService* getService() override;
    void newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue) override;

  private:
    BLEService *mService = nullptr;
    BLECharacteristic *mCharacteristic = nullptr;
};

#endif
