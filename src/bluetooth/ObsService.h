/*
  Copyright (C) 2019 Zweirat
  Contact: https://openbikesensor.org

  This file is part of the OpenBikeSensor project.

  The OpenBikeSensor sensor firmware is free software: you can redistribute
  it and/or modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  The OpenBikeSensor sensor firmware is distributed in the hope that it will
  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
  Public License for more details.

  You should have received a copy of the GNU General Public License along with
  the OpenBikeSensor sensor firmware.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef OPENBIKESENSORFIRMWARE_OBSSERVICE_H
#define OPENBIKESENSORFIRMWARE_OBSSERVICE_H

#include "_IBluetoothService.h"


class ObsTimeServiceCallback : public BLECharacteristicCallbacks {
  public:
    explicit ObsTimeServiceCallback(uint32_t *value) : mValue(value) {};
    void onRead(BLECharacteristic *pCharacteristic) override;

  private:
    uint32_t *mValue;

};


class ObsService : public IBluetoothService {
  public:
    ObsService(uint16_t leftOffset, uint16_t rightOffset, const String &trackId);
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService* getService() override;
    void newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue) override;
    void newPassEvent(uint32_t millis, uint16_t leftValue, uint16_t rightValue) override;

  private:
    BLEService *mService = nullptr;
    BLECharacteristic mTimeCharacteristic
      = BLECharacteristic(OBS_TIME_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
    uint32_t mTimerValue = 0;
    BLEDescriptor mTimeDescriptor = BLEUUID((uint16_t)ESP_GATT_UUID_CHAR_DESCRIPTION);
    ObsTimeServiceCallback mTimeCharacteristicsCallback = ObsTimeServiceCallback(&mTimerValue);

    BLECharacteristic mDistanceCharacteristic
      = BLECharacteristic(OBS_DISTANCE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    uint8_t mDistanceValue[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    BLEDescriptor mDistanceDescriptor = BLEDescriptor(BLEUUID((uint16_t)0x2901));

    BLECharacteristic mButtonCharacteristic
      = BLECharacteristic(OBS_BUTTON_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    uint8_t mButtonValue[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    BLEDescriptor mButtonDescriptor = BLEDescriptor(BLEUUID((uint16_t)ESP_GATT_UUID_CHAR_DESCRIPTION));

    BLECharacteristic mOffsetCharacteristic
      = BLECharacteristic(OBS_OFFSET_CHARACTERISTIC_UUID,BLECharacteristic::PROPERTY_READ);
    uint8_t mOffsetValue[4] = {0, 0, 0, 0};
    BLEDescriptor mOffsetDescriptor = BLEDescriptor(BLEUUID((uint16_t)ESP_GATT_UUID_CHAR_DESCRIPTION));

    BLECharacteristic mTrackIdCharacteristic
      = BLECharacteristic(OBS_TRACK_ID_CHARACTERISTIC_UUID,BLECharacteristic::PROPERTY_READ);
    uint8_t mTrackIdValue[36];
    BLEDescriptor mTrackIdDescriptor = BLEDescriptor(BLEUUID((uint16_t)ESP_GATT_UUID_CHAR_DESCRIPTION));

    static const std::string TIME_DESCRIPTION_TEXT;
    static const std::string DISTANCE_DESCRIPTION_TEXT;
    static const std::string BUTTON_DESCRIPTION_TEXT;
    static const std::string OFFSET_DESCRIPTION_TEXT;
    static const std::string TRACK_ID_DESCRIPTION_TEXT;
    static const BLEUUID OBS_SERVICE_UUID;
    static const BLEUUID OBS_TIME_CHARACTERISTIC_UUID;
    static const BLEUUID OBS_DISTANCE_CHARACTERISTIC_UUID;
    static const BLEUUID OBS_BUTTON_CHARACTERISTIC_UUID;
    static const BLEUUID OBS_OFFSET_CHARACTERISTIC_UUID;
    static const BLEUUID OBS_TRACK_ID_CHARACTERISTIC_UUID;
};

#endif
