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
    void activateBluetooth();

    /**
     * Stops advertising the bluetooth services. The bluetooth server will not be
     * stopped.
     */
    void deactivateBluetooth();

    /**
     * Disconnects the currently connected device/client.
     */
    void disconnectDevice();

    /**
     * Processes new sensor values by calling each services with the values.
     * @param leftValue sensor value of the left side (MAX_SENSOR_VALUE for no reading)
     * @param rightValues sensor value of the right side (MAX_SENSOR_VALUE for no reading)
     */
    void newSensorValues(uint16_t leftValue, uint16_t rightValue);

    /**
     * Process the current push button state itself. This will call
     * buttonPressed() internally. Alternatively, the method buttonPressed() could
     * be called instead if the button press detection is calculated somewhere
     * else.
     * @param state current button state (LOW or HIGH)
     */
    void processButtonState(int state);

    /**
     * Process the event that the push button was just pressed. This method should
     * only be called if processButtonState() isn't called.
     */
    void buttonPressed();
};

#endif
