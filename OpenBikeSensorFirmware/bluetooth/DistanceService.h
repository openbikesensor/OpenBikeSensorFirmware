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

    void newSensorValues(uint16_t leftValue, uint16_t rightValue) override;
    void buttonPressed() override;

  private:
    BLEService *mService = nullptr;
    BLECharacteristic *mCharacteristic = nullptr;
};

#endif
