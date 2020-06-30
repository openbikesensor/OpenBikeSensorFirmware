#ifndef OBS_BLUETOOTH_BLUETOOTHMANAGER_H
#define OBS_BLUETOOTH_BLUETOOTHMANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEDescriptor.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <list>

#include "_IBluetoothService.h"
#include "DeviceInfoService.h"
#include "DistanceService.h"
#include "HeartRateService.h"

class BluetoothManager {
public:
  void init();
  void activateBluetooth();
  void deactivateBluetooth();
  void disconnectDevice();
  void newSensorValue(float value);

private:
  std::list<IBluetoothService*> services;
  BLEServer *pServer;
};

#endif