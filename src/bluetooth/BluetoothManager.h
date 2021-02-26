#ifndef OBS_BLUETOOTH_BLUETOOTHMANAGER_H
#define OBS_BLUETOOTH_BLUETOOTHMANAGER_H

#include <Arduino.h>
#include <list>

#include "_IBluetoothService.h"
#include "ClosePassService.h"
#include "DeviceInfoService.h"
#include "DistanceService.h"
#include "HeartRateService.h"
#include "BatteryService.h"
#include "ObsService.h"

class BluetoothManager {
  public:
    /**
     * Initializes all defined services and starts the bluetooth server.
     */
    void init(const String &obsName,
              uint16_t leftOffset, uint16_t rightOffset,
              std::function<uint8_t()> batteryPercentage,
              const String &trackId);

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
    void newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue);

    /**
     * Processes new confirmed pass event.
     * @param millis sender millis counter at the time of measurement of the left value
     * @param leftValue sensor value of the left side (MAX_SENSOR_VALUE for no reading)
     * @param rightValues sensor value of the right side (MAX_SENSOR_VALUE for no reading)
     */
    void newPassEvent(uint32_t millis, uint16_t leftValue, uint16_t rightValue);

    /* True if any client is currently connected. */
    bool hasConnectedClients();

  private:
    BLEServer *pServer;
    std::list<IBluetoothService*> services;
};

#endif
