#ifndef OBS_GLOBALS_H
#define OBS_GLOBALS_H

#include <Arduino.h>

#include "utils/obsutils.h"
#include "config.h"
#include "displays.h"
#include "sensor.h"
#include "VoltageMeter.h"

// This file should contain declarations of all variables that will be used globally.
// The variables don't have to be set here, but need to be declared.

// Version
extern const char *OBSVersion;

// PINs
extern const int PushButton_PIN;

extern int confirmedMeasurements;
extern int numButtonReleased;

extern int buttonState;

extern const char *configFilename;  // <- SD library uses 8.3 filenames
extern Config config;

// Forward declare classes to build (because there is a cyclic dependency between sensor.h and displays.h)
class SSD1306DisplayDevice;
class HCSR04SensorManager;

extern SSD1306DisplayDevice* displayTest;

extern HCSR04SensorManager* sensorManager;

extern VoltageMeter* voltageMeter;

extern String esp_chipid;

extern const uint32_t MAX_DURATION_MICRO_SEC;
extern const int LEFT_SENSOR_ID;
extern const int RIGHT_SENSOR_ID;

#endif

#define MAX_SENSOR_VALUE 999

#define BLUETOOTH_ACTIVATED
#define BLUETOOTH_SERVICE_CLOSEPASS
#define BLUETOOTH_SERVICE_CONNECTION
#define BLUETOOTH_SERVICE_DEVICEINFO
#define BLUETOOTH_SERVICE_DISTANCE
#define BLUETOOTH_SERVICE_HEARTRATE
#define RADMESSER_S_COMPATIBILITY_MODE
