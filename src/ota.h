#ifndef OBS_OTA_H
#define OBS_OTA_H

#include <Arduino.h>
#include <ArduinoOTA.h>
#include "displays.h"

void OtaInit(String esp_chipid, DisplayDevice* display);

#endif
