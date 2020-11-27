#ifndef OBS_BLUETOOTH_HEARTRATESERVICE_H
#define OBS_BLUETOOTH_HEARTRATESERVICE_H

#include <CircularBuffer.h>
#include "_IBluetoothService.h"

#define SERVICE_HEARTRATE_UUID "0000180D-0000-1000-8000-00805F9B34FB"
#define SERVICE_HEARTRATE_DESCRIPTOR_UUID "00002902-0000-1000-8000-00805f9b34fb"
#define SERVICE_HEARTRATE_CHAR_HEARTRATE_UUID "00002a37-0000-1000-8000-00805f9b34fb"
#define SERVICE_HEARTRATE_CHAR_SENSORLOCATION_UUID "00002a38-0000-1000-8000-00805f9b34fb"
#define SERVICE_HEARTRATE_CHAR_SENSORLOCATION_VALUE 1 // UINT16 Values

class HeartRateService : public IBluetoothService {
  public:
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService* getService() override;
    void newSensorValues(const std::list<uint16_t>& leftValues, const std::list<uint16_t>& rightValues) override;
    void buttonPressed() override;

  private:
    BLEService *mService;
    BLECharacteristic *mCharacteristic;

    unsigned long mCollectionStartTime;
    uint16_t mMinimumDistance = 999;
};

#endif
