#ifndef OBS_BLUETOOTH_BLUETOOTHMANAGER_H
#define OBS_BLUETOOTH_BLUETOOTHMANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEDescriptor.h>
#include <list>

#include "_IBluetoothService.h"
#include "ClosePassService.h"
#include "ConnectionService.h"
#include "DeviceInfoService.h"
#include "DistanceService.h"
#include "HeartRateService.h"
#include "ObsService.h"

class BluetoothManager {
  public:
    /**
     * Initializes all defined services (using #ifdef's) and starts the bluetooth
     * server.
     */
    void init();

    /**
     * Starts advertising all services that internally implement shouldAdvertise()
     * with `true`. The bluetooth server needs to be started before this method.
     */
    void activateBluetooth() const;

    /**
     * Stops advertising the bluetooth services. The bluetooth server will not be
     * stopped.
     */
    void deactivateBluetooth() const;

    /**
     * Disconnects the currently connected device/client.
     */
    void disconnectDevice() const;

    /**
     * Processes new sensor values by calling each services with the values.
     * @param millis sender millis counter at the time of measurement of the left value
     * @param leftValue sensor value of the left side (MAX_SENSOR_VALUE for no reading)
     * @param rightValues sensor value of the right side (MAX_SENSOR_VALUE for no reading)
     */
    void newSensorValues(const uint32_t millis, uint16_t leftValue, uint16_t rightValue);

    /**
     * Processes new confirmed pass event.
     * @param millis sender millis counter at the time of measurement of the left value
     * @param leftValue sensor value of the left side (MAX_SENSOR_VALUE for no reading)
     * @param rightValues sensor value of the right side (MAX_SENSOR_VALUE for no reading)
     */
    void newPassEvent(const uint32_t millis, const uint16_t leftValue, const uint16_t rightValue);

  private:
    BLEServer *pServer;
    std::list<IBluetoothService*> services;
    unsigned long lastValueTimestamp = millis();
};

#endif
