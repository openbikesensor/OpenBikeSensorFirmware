#ifndef OBS_GLOBALS_H
#define OBS_GLOBALS_H

#include <Arduino.h>

// Forward declare classes to build (because there is a cyclic dependency between sensor.h and displays.h)
class SSD1306DisplayDevice;
class HCSR04SensorManager;


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
extern uint32_t buttonStateChanged;

extern Config config;


extern SSD1306DisplayDevice* displayTest;

extern HCSR04SensorManager* sensorManager;

extern VoltageMeter* voltageMeter;

class Gps;
extern Gps gps;

extern const uint32_t MAX_DURATION_MICRO_SEC;
extern const uint8_t LEFT_SENSOR_ID;
extern const uint8_t RIGHT_SENSOR_ID;
extern const uint16_t MAX_SENSOR_VALUE;

#endif
