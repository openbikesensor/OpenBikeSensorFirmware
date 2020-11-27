#ifndef OBS_BLUETOOTH_CONNECTIONSERVICE_H
#define OBS_BLUETOOTH_CONNECTIONSERVICE_H

#include <CircularBuffer.h>
#include "_IBluetoothService.h"

#define SERVICE_CONNECTION_UUID "1FE7FAF9-CE63-4236-0002-000000000000"
#define SERVICE_CONNECTION_CHAR_CONNECTED_UUID "1FE7FAF9-CE63-4236-0002-000000000001"

class ConnectionService : public IBluetoothService {
  public:
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService *getService() override;

    void newSensorValues(const uint16_t leftValue, const uint16_t rightValue) override;
    void buttonPressed() override;

  private:
    bool isSensorCovered();

    BLEService *mService;
    BLECharacteristic *mCharacteristic;

    unsigned long mConnectionStartTime;
    unsigned long mSensorCoveredStartTime;
    CircularBuffer<uint8_t, 25> mSensorValues;
};

#endif
