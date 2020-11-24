/*
  Copyright (C) 2019 Zweirat
  Contact: https://openbikesensor.org

  This file is part of the OpenBikeSensor project.

  The OpenBikeSensor sensor firmware is free software: you can redistribute
  it and/or modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  The OpenBikeSensor sensor firmware is distributed in the hope that it will
  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
  Public License for more details.

  You should have received a copy of the GNU General Public License along with
  the OpenBikeSensor sensor firmware.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OBS_CONFIG_H
#define OBS_CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

#include "vector.h"

enum DisplayOptions {
  DisplaySatelites = 0x01,  // 1
  DisplayVelocity = 0x02,   // 2
  DisplayLeft = 0x04,       // 4
  DisplayRight = 0x08,      // 8
  DisplaySimple = 0x10,     // 16
  DisplaySwapSensors = 0x20, // 32
  DisplayInvert = 0x40,     // 64
  DisplayFlip = 0x80,       // 128
  DisplayNumConfirmed = 0x100, //256
  DisplayDistanceDetail = 0x200 // 512
};

enum GPSOptions {
  ValidLocation = 0x01, //1
  ValidTime = 0x02, //2
  NumberSatellites = 0x04 //4
};

enum PrivacyOptions {
  AbsolutePrivacy = 0x01, //1
  NoPosition = 0x02, //2
  NoPrivacy = 0x04, //4
  OverridePrivacy = 0x08 //8
};

struct PrivacyArea {
  double latitude;
  double longitude;
  double transformedLatitude;
  double transformedLongitude;
  double radius;
};

struct Config {
  uint8_t numSensors;
  Vector<uint16_t> sensorOffsets;
  char ssid[32];
  char password[64];
  char hostname[64];
  char obsUserID[64];
  uint displayConfig;
  boolean bluetooth;
  boolean simRaMode;
#ifdef DEVELOP
  int devConfig;
#endif
  int GPSConfig;
  int privacyConfig;
  int satsForFix;
  int port;
  int confirmationTimeWindow;
  uint8_t numPrivacyAreas;
  Vector<PrivacyArea> privacyAreas;
};

#ifdef DEVELOP
enum DevOptions {
  ShowGrid = 0x01,
  PrintWifiPassword = 0x02
};
#endif

void printConfig(Config &config);

// SPIFFS
void loadConfiguration(const char *configFilename, Config &config);

void saveConfiguration(const char *filename, const Config &config);

String configToJson(Config &config);

void jsonToConfig(String json, Config &config);

#endif
