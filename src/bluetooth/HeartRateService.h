#ifndef OBS_BLUETOOTH_HEARTRATESERVICE_H
#define OBS_BLUETOOTH_HEARTRATESERVICE_H

#include "globals.h"
#include <CircularBuffer.h>
#include "_IBluetoothService.h"

#define SERVICE_HEARTRATE_UUID "0000180D-0000-1000-8000-00805F9B34FB"
#define SERVICE_HEARTRATE_DESCRIPTOR_UUID "00002902-0000-1000-8000-00805f9b34fb"
#define SERVICE_HEARTRATE_CHAR_HEARTRATE_UUID "00002a37-0000-1000-8000-00805f9b34fb"
#define SERVICE_HEARTRATE_CHAR_SENSORLOCATION_UUID "00002a38-0000-1000-8000-00805f9b34fb"
#define SERVICE_HEARTRATE_CHAR_SENSORLOCATION_VALUE 1

class HeartRateService : public IBluetoothService {
  public:
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService* getService() override;
    void newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue) override;

  private:
    BLEService *mService = nullptr;
    BLECharacteristic *mCharacteristic = nullptr;

    unsigned long mCollectionStartTime = 0;
    uint16_t mMinimumDistance = MAX_SENSOR_VALUE;
};

#endif
