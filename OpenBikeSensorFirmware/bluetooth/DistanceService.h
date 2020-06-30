#ifndef OBS_BLUETOOTH_DISTANCESERVICE_H
#define OBS_BLUETOOTH_DISTANCESERVICE_H

#include "_IBluetoothService.h"

#define SERVICE_DISTANCE_UUID "1FE7FAF9-CE63-4236-0001-000000000000"
#define SERVICE_DISTANCE_CHAR_50MS_UUID "1FE7FAF9-CE63-4236-0001-000000000001"

class DistanceService : public IBluetoothService {
public:
  void setup(BLEServer *pServer) override;
  virtual bool hasService() override;
  virtual bool shouldAdvertise() override;
  virtual BLEService* getService() override;

  void newSensorValue(float value) override;
  void deviceConnected() override {};
  void deviceDisconnected() override {};

private:
  BLEService *mService;
  BLECharacteristic *mCharacteristic;
};

#endif
