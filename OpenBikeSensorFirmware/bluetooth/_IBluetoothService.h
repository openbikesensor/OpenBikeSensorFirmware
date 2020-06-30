#ifndef OBS_BLUETOOTH_IBLUETOOTHSERVICE_H
#define OBS_BLUETOOTH_IBLUETOOTHSERVICE_H

#include <Arduino.h>
#include <BLEService.h>

class IBluetoothService {
public:
  virtual void setup(BLEServer *pServer) = 0;
  virtual bool shouldAdvertise() = 0;
  virtual BLEService* getService() = 0;

  virtual void newSensorValue(uint8_t value) = 0;
};

#endif
