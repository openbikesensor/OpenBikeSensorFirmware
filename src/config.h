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
#include <vector>
#include <FS.h>

enum DisplayOptions {
  DisplaySatellites = 0x01,  // 1
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

// Most values should be moved away from here to the concrete implementation
// which uses the value
struct Config {
  char obsName[32];
  std::vector<uint16_t> sensorOffsets;
  char hostname[64];
  char obsUserID[64];
  uint displayConfig;
  bool bluetooth;
  bool simRaMode;
  int devConfig;
  int privacyConfig;
  int confirmationTimeWindow;
  std::vector<PrivacyArea> privacyAreas;
};

enum DevOptions {
  ShowGrid = 0x01,
  PrintWifiPassword = 0x02
};

class ObsConfig {
  public:
    ObsConfig() : jsonData(4096) {};
    ~ObsConfig() = default;
    bool loadConfig();
    bool loadConfig(File &file);
    bool saveConfig() const;
    void printConfig() const;

    std::vector<int> getIntegersProperty(String const &name) const;
    template<typename T> T getProperty(const String &key) const;
    bool setBitMaskProperty(int profile, const String &key, uint value, bool state);
    uint getBitMaskProperty(int profile, const String &key, uint mask) const;

    bool setProperty(int profile, const String &key, std::string const &value);
    bool setProperty(int profile, const String &key, String const &value);
    template<typename T> bool setProperty(int profile, const String &key, T const &value);
    bool setOffsets(int profile, std::vector<int> const &value);

    bool setPrivacyArea(int profile, int paId, PrivacyArea const &pa);
    bool addPrivacyArea(int profile, PrivacyArea const &pa);
    bool removePrivacyArea(int profile, int paId);
    PrivacyArea getPrivacyArea(int profile, int paId) const;
    int getNumberOfPrivacyAreas(int profile) const;

    int getNumberOfProfiles() const;
    int addProfile();
    bool deleteProfile(int profile);
    bool selectProfile(int profile);
    int getSelectedProfile() const;
    String getProfileName(int profile); // = getProperty(profile, "name")

    void fill(Config &cfg) const;
    /* free memory allocated by json document. */
    void releaseJson();
    String asJsonString() const;
    bool parseJson(const String &json);

    static const String PROPERTY_OBS_NAME;
    static const String PROPERTY_NAME;
    static const String PROPERTY_BLUETOOTH;
    static const String PROPERTY_OFFSET;
    static const String PROPERTY_SIM_RA;
    static const String PROPERTY_WIFI_SSID;
    static const String PROPERTY_WIFI_PASSWORD;
    static const String PROPERTY_PORTAL_TOKEN;
    static const String PROPERTY_PORTAL_URL;
    static const String PROPERTY_GPS_FIX;
    static const String PROPERTY_DISPLAY_CONFIG;
    static const String PROPERTY_CONFIRMATION_TIME_SECONDS;
    static const String PROPERTY_PRIVACY_CONFIG;
    static const String PROPERTY_DEVELOPER;
    static const String PROPERTY_SELECTED_PRESET;
    static const String PROPERTY_PRIVACY_AREA;
    static const String PROPERTY_PA_LAT;
    static const String PROPERTY_PA_LONG;
    static const String PROPERTY_PA_LAT_T;
    static const String PROPERTY_PA_LONG_T;
    static const String PROPERTY_PA_RADIUS;
    static const String PROPERTY_HTTP_PIN;

  private:
    static bool loadJson(JsonDocument &jsonDocument, const String &filename);
    static bool loadJson(JsonDocument &jsonDocument, fs::File &file);
    static bool parseJsonFromString(JsonDocument &jsonDocument, const String &jsonAsString);
    bool loadOldConfig(const String &filename);
    bool loadConfig(const String &filename);
    void parseOldJsonDocument(DynamicJsonDocument &document);
    void makeSureSystemDefaultsAreSet();
    template<typename T> bool ensureSet(JsonObject data, const String &key, T value);
    JsonObject getProfile(int profile);
    JsonObjectConst getProfileConst(int profile) const;
    void resetJson();

    static const String CONFIG_OLD_FILENAME;
    static const String CONFIG_FILENAME;

    DynamicJsonDocument jsonData;
    int selectedProfile = 0;
};

#endif
