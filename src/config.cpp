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

#include <langinfo.h>
#include "config.h"
#include "gps.h"
#include "SPIFFS.h"

/* TODO:
 *  - PROPERTY_OBS_NAME should be reset if it has the form "OpenBikeSensor-????"
 *    to have the device Id 8might be was restored from a backup of a different
 *    OBS unit.
 *
 */

const int MAX_CONFIG_FILE_SIZE = 4096;

const String ObsConfig::PROPERTY_OBS_NAME = String("obsName");
const String ObsConfig::PROPERTY_NAME = String("name");
const String ObsConfig::PROPERTY_BLUETOOTH = String("bluetooth");
const String ObsConfig::PROPERTY_OFFSET = String("offset");
const String ObsConfig::PROPERTY_SIM_RA = String("simRa");
const String ObsConfig::PROPERTY_WIFI_SSID = String("wifiSsid");
const String ObsConfig::PROPERTY_WIFI_PASSWORD = String("wifiPassword");
const String ObsConfig::PROPERTY_PORTAL_URL = String("portalUrl");
const String ObsConfig::PROPERTY_PORTAL_TOKEN = String("portalToken");
const String ObsConfig::PROPERTY_GPS_FIX = String("gpsFix");
const String ObsConfig::PROPERTY_DISPLAY_CONFIG = String("displayConfig");
const String ObsConfig::PROPERTY_CONFIRMATION_TIME_SECONDS = String("confirmationTimeSeconds");
const String ObsConfig::PROPERTY_PRIVACY_CONFIG = String("privacyConfig");
const String ObsConfig::PROPERTY_DEVELOPER = String("devConfig");
const String ObsConfig::PROPERTY_SELECTED_PRESET = String("selectedPreset");
const String ObsConfig::PROPERTY_PRIVACY_AREA = String("privacyArea");
const String ObsConfig::PROPERTY_PA_LAT = String("lat");
const String ObsConfig::PROPERTY_PA_LONG = String("long");
const String ObsConfig::PROPERTY_PA_LAT_T = String("latT");
const String ObsConfig::PROPERTY_PA_LONG_T = String("longT");
const String ObsConfig::PROPERTY_PA_RADIUS = String("radius");
const String ObsConfig::PROPERTY_HTTP_PIN = String("httpPin");

// Filenames 8.3 here!
const String OLD_CONFIG_FILENAME = "/config.txt";
const String ObsConfig::CONFIG_OLD_FILENAME = "/cfg.old";
const String ObsConfig::CONFIG_FILENAME = "/cfg.obs";

bool ObsConfig::loadConfig() {
  bool loaded = false;

  jsonData.clear();
  if (SPIFFS.exists(CONFIG_FILENAME)) {
    loaded = loadConfig(CONFIG_FILENAME);
  }
  // problem with the current config, use the last config
  if (!loaded && SPIFFS.exists(CONFIG_OLD_FILENAME)) {
    loaded = loadConfig(CONFIG_OLD_FILENAME);
  }
  // still no luck, do we have a previous version file?
  if (!loaded && SPIFFS.exists(OLD_CONFIG_FILENAME)) {
    loaded = loadOldConfig(OLD_CONFIG_FILENAME);
  }
  makeSureSystemDefaultsAreSet();
  return loaded;
}

bool ObsConfig::saveConfig() const {
  log_d("Saving config to SPIFFS %s free space %luk", CONFIG_FILENAME.c_str(), SPIFFS.usedBytes() / 1024);
  SPIFFS.remove(CONFIG_OLD_FILENAME);
  SPIFFS.rename(CONFIG_FILENAME, CONFIG_OLD_FILENAME);
  bool result;
  File file = SPIFFS.open(CONFIG_FILENAME, FILE_WRITE);
  auto size = serializeJson(jsonData, file);
  file.close();
  if (size == 0) {
    log_i("Failed to write configuration 0 bytes written.");
    result = false;
  } else {
    result = true;
  }
  printConfig();
  log_d("Configuration saved %ld bytes - remaining %luk.",
        size, SPIFFS.usedBytes() / 1024);
  // delete old config format file if result == true
  // TODO: Activate once released
  //  if (result) {
  //   SPIFFS.remove(OLD_CONFIG_FILENAME);
  //  }
  return result;
}

String ObsConfig::asJsonString() const {
  String result;
  serializeJson(jsonData, result);
  return result;
}

void ObsConfig::resetJson() {
  jsonData.clear();
  jsonData.createNestedArray("obs");
  if (jsonData["obs"].size() == 0) {
    jsonData["obs"].createNestedObject();
  }
}

void ObsConfig::makeSureSystemDefaultsAreSet() {
  if (!jsonData.containsKey("obs") || !jsonData["obs"].is<JsonArray>()) {
    jsonData.createNestedArray("obs");
  }
  if (jsonData["obs"].size() == 0) {
    jsonData["obs"].createNestedObject();
  }
  JsonObject data = jsonData["obs"][0];
  ensureSet(data, PROPERTY_OBS_NAME, "OpenBikeSensor-" + String((uint16_t)(ESP.getEfuseMac() >> 32), 16));
  ensureSet(data, PROPERTY_NAME, "default");
  ensureSet(data, PROPERTY_SIM_RA, false);
  ensureSet(data, PROPERTY_BLUETOOTH, false);
  data[PROPERTY_OFFSET][0] = data[PROPERTY_OFFSET][0] | 35;
  data[PROPERTY_OFFSET][1] = data[PROPERTY_OFFSET][1] | 35;
  if (ensureSet(data, PROPERTY_WIFI_SSID, "Freifunk")) {
    data[PROPERTY_WIFI_PASSWORD] = "Freifunk";
  }
  ensureSet(data, PROPERTY_PORTAL_URL, "https://openbikesensor.hlrs.de");
  ensureSet(data, PROPERTY_HTTP_PIN, ""); // we choose and save a new default when we need it

  const String &token = getProperty<const char*>(PROPERTY_PORTAL_TOKEN);
  if (token && token.equals("5e8f2f43e7e3b3668ca13151")) {
    log_e("Ignored old default API-KEY.");
    setProperty(0, PROPERTY_PORTAL_TOKEN, "");
  }
  ensureSet(data, PROPERTY_PORTAL_TOKEN, "");
  ensureSet(data, PROPERTY_GPS_FIX, (int) Gps::WaitFor::FIX_POS);
  ensureSet(data, PROPERTY_DISPLAY_CONFIG, DisplaySimple);
  if (getProperty<int>(PROPERTY_DISPLAY_CONFIG) == 0) {
    data[PROPERTY_DISPLAY_CONFIG] = DisplaySimple;
  }
  ensureSet(data, PROPERTY_PRIVACY_CONFIG, AbsolutePrivacy);
  if (getProperty<int>(PROPERTY_PRIVACY_CONFIG) == 0) {
    data[PROPERTY_PRIVACY_CONFIG] = AbsolutePrivacy;
  }
  ensureSet(data, PROPERTY_CONFIRMATION_TIME_SECONDS, 5);
  if (getProperty<int>(PROPERTY_CONFIRMATION_TIME_SECONDS) == 0) {
    data[PROPERTY_CONFIRMATION_TIME_SECONDS] = 5;
  }
  ensureSet(data, PROPERTY_DEVELOPER, 0);
  ensureSet(data, PROPERTY_SELECTED_PRESET, 0);
  if (!data.containsKey(PROPERTY_PRIVACY_AREA)) {
    data.createNestedArray(PROPERTY_PRIVACY_AREA);
  }
}

template< typename T>
bool ObsConfig::ensureSet(JsonObject data, const String &key, T value) {
  bool result = !data.containsKey(key) || data[key].isNull() || data[key].as<String>().isEmpty();
  if (result) {
    log_d("Value for %s not found, setting to default.", key.c_str());
    log_d("Testing for null gives %d", data[key] == nullptr);
    data[key] = value;
  }
  return result;
}

bool ObsConfig::setBitMaskProperty(int profile, String const &key, uint const value, bool state) {
  const uint currentValue = jsonData["obs"][profile][key];
  uint newValue;
  if (state) {
    newValue = (currentValue | value);
  } else {
    newValue = (currentValue & ~value);
  }
  jsonData["obs"][profile][key] = newValue;
  return currentValue != newValue;
}

uint ObsConfig::getBitMaskProperty(int profile, String const &key, uint const value) const {
  return ((uint) jsonData["obs"][profile][key]) & value;
}

template< typename T>
bool ObsConfig::setProperty(int profile, String const &key, T const &value) {
  // auto oldValue = jsonData["obs"][profile][key];
  jsonData["obs"][profile][key] = value;
  return true; // value == oldValue;
}

bool ObsConfig::setProperty(int profile, String const &key, String const &value) {
  auto oldValue = jsonData["obs"][profile][key];
  jsonData["obs"][profile][key] = value;
  return value == oldValue;
}

bool ObsConfig::setProperty(int profile, String const &key, std::string const &value) {
  auto oldValue = jsonData["obs"][profile][key];
  jsonData["obs"][profile][key] = value;
  return value == oldValue;
}

bool ObsConfig::setOffsets(int profile, const std::vector<int> &offsets) {
  bool changed = false;
  auto jOffsets = jsonData["obs"][profile].createNestedArray(PROPERTY_OFFSET);
  for (int i = 0; i < offsets.size(); i++) {
    changed = changed | (offsets[i] != jOffsets[i]);
    jOffsets[i] = offsets[i];
  }
  return changed;
}

bool ObsConfig::removePrivacyArea(int profile, int paId) {
  getProfile(profile)[PROPERTY_PRIVACY_AREA].remove(paId);
  return true;
}

bool ObsConfig::addPrivacyArea(int profile, const PrivacyArea &pa) {
  auto data = getProfile(profile);
  return setPrivacyArea(profile, data[PROPERTY_PRIVACY_AREA].size(), pa);
}

bool ObsConfig::setPrivacyArea(int profile, int paId, const PrivacyArea &pa) {
  auto paEntry = getProfile(profile)[PROPERTY_PRIVACY_AREA][paId];
  paEntry[PROPERTY_PA_LAT] = pa.latitude;
  paEntry[PROPERTY_PA_LONG] = pa.longitude;
  paEntry[PROPERTY_PA_LAT_T] = pa.transformedLatitude;
  paEntry[PROPERTY_PA_LONG_T] = pa.transformedLongitude;
  paEntry[PROPERTY_PA_RADIUS] = pa.radius;
  return true;
}

PrivacyArea ObsConfig::getPrivacyArea(int profile, int i) const {
  auto data = getProfileConst(profile)[PROPERTY_PRIVACY_AREA][i];
  PrivacyArea pa;
  pa.longitude = data[PROPERTY_PA_LONG];
  pa.latitude = data[PROPERTY_PA_LAT];
  pa.transformedLongitude = data[PROPERTY_PA_LONG_T];
  pa.transformedLatitude = data[PROPERTY_PA_LAT_T];
  pa.radius = data[PROPERTY_PA_RADIUS];
  return pa;
}

int ObsConfig::getNumberOfPrivacyAreas(int profile) const {
  return getProfileConst(profile)[PROPERTY_PRIVACY_AREA].size();
}

JsonObject ObsConfig::getProfile(int profile) {
  return jsonData["obs"][profile];
}

JsonObjectConst ObsConfig::getProfileConst(int profile) const {
  return jsonData["obs"][profile];
}

void ObsConfig::printConfig() const {
#ifdef DEVELOP
  Serial.printf(
    "Dumping current configuration, current selected profile is %d:\n",
    selectedProfile);
  serializeJsonPretty(jsonData, Serial);
  Serial.println();
#endif
}

bool ObsConfig::loadOldConfig(const String &filename) {
  log_d("Loading old config json from %s.", filename.c_str());
  DynamicJsonDocument doc(MAX_CONFIG_FILE_SIZE);
  bool success = loadJson(doc, filename);
  if (success) {
    parseOldJsonDocument(doc);
  }
  return success;
}

bool ObsConfig::loadConfig(const String &filename) {
  log_d("Loading config json from %s.", filename.c_str());
  return loadJson(jsonData, filename);
}

bool ObsConfig::loadConfig(File &file) {
  log_d("Loading config json from %s.", file.name());
  return loadJson(jsonData, file);
}

bool ObsConfig::loadJson(JsonDocument &jsonDocument, const String &filename) {
  File file = SPIFFS.open(filename);
  bool success = loadJson(jsonDocument, file);
  file.close();
  return success;
}

bool ObsConfig::loadJson(JsonDocument &jsonDocument, File &file) {
  bool success = false;
  jsonDocument.clear();
  DeserializationError error = deserializeJson(jsonDocument, file);
  if (error) {
    log_w("Failed to read file %s, using default configuration got %s.\n",
          file.name(), error.c_str());
  } else {
    success = true;
  }
#ifdef DEVELOP
  log_d("Config found in file '%s':", file.name());
  serializeJsonPretty(jsonDocument, Serial);
  log_d("------------------------------------------");
#endif
  file.close();
  return success;
}

bool ObsConfig::parseJsonFromString(JsonDocument &jsonDocument, const String &jsonAsString) {
  bool success = false;
  jsonDocument.clear();
  DeserializationError error = deserializeJson(jsonDocument, jsonAsString);
  if (error) {
    log_w("Failed to parse %s, got %s.\n", jsonAsString.c_str(), error.c_str());
  } else {
    success = true;
  }
#ifdef DEVELOP
  log_i("Json data:");
  serializeJsonPretty(jsonDocument, Serial);
  log_i("------------------------------------------");
#endif
  return success;
}

void ObsConfig::fill(Config &cfg) const {
  strlcpy(cfg.obsName, getProperty<const char*>(PROPERTY_OBS_NAME), sizeof(cfg.obsName));
  cfg.sensorOffsets.clear();
  for (int i : getIntegersProperty(PROPERTY_OFFSET)) {
    cfg.sensorOffsets.push_back(i);
  }
  strlcpy(cfg.obsUserID, getProperty<const char*>(PROPERTY_PORTAL_TOKEN), sizeof(cfg.obsUserID));
  strlcpy(cfg.hostname, getProperty<const char*>(PROPERTY_PORTAL_URL), sizeof(cfg.hostname));
  cfg.displayConfig = getProperty<uint>(PROPERTY_DISPLAY_CONFIG);
  cfg.confirmationTimeWindow = getProperty<int>(PROPERTY_CONFIRMATION_TIME_SECONDS);
  cfg.privacyConfig = getProperty<int>(PROPERTY_PRIVACY_CONFIG);
  cfg.bluetooth = getProperty<bool>(PROPERTY_BLUETOOTH);
  cfg.simRaMode = getProperty<bool>(PROPERTY_SIM_RA);
  cfg.devConfig = getProperty<int>(PROPERTY_DEVELOPER);
  cfg.privacyAreas.clear();
  if (selectedProfile != 0) { // not sure if we ever support PAs per profile.
    for (int i = 0; i < jsonData["obs"][selectedProfile][PROPERTY_PRIVACY_AREA].size(); i++) {
      cfg.privacyAreas.push_back(getPrivacyArea(selectedProfile, i));
    }
  }
  for (int i = 0; i < jsonData["obs"][0][PROPERTY_PRIVACY_AREA].size(); i++) {
    cfg.privacyAreas.push_back(getPrivacyArea(0, i));
  }
}

bool ObsConfig::parseJson(const String &json) {
  bool result = false;
  resetJson();
  // parse also old backups - to be removed in v0.5 or later
  if (json.indexOf("\"obs\"") == -1) {
    log_d("Old format detected.");
    DynamicJsonDocument doc(MAX_CONFIG_FILE_SIZE);
    if (parseJsonFromString(doc, json)) {
      parseOldJsonDocument(doc);
      result = true;
    }
  } else {
    log_d("New format detected.");
    result = parseJsonFromString(jsonData, json);
  }
  makeSureSystemDefaultsAreSet();
  return result;
}

void ObsConfig::parseOldJsonDocument(DynamicJsonDocument &doc) {
  int numSensors = doc["numSensors"] | 2;
  std::vector<int> offsets;
  for (int idx = 0; idx < numSensors; ++idx) {
    int offsetTemp = doc["offsetInfo" + String(idx)] | 35;
    offsets.push_back(offsetTemp);
  }
  setOffsets(0, offsets);
  setProperty(0, PROPERTY_WIFI_SSID, doc["ssid"] | String("Freifunk"));
  setProperty(0, PROPERTY_WIFI_PASSWORD, doc["password"] | String("Freifunk"));
  setProperty(0, PROPERTY_PORTAL_TOKEN, doc["obsUserID"] | String("5e8f2f43e7e3b3668ca13151"));
  setProperty(0, PROPERTY_DISPLAY_CONFIG, doc["displayConfig"] | DisplaySimple);

  uint16_t gpsConfig = doc["GPSConfig"];
  if (gpsConfig & GPSOptions::ValidTime) {
    setProperty(0, PROPERTY_GPS_FIX, (int) Gps::WaitFor::FIX_TIME);
  } else if (gpsConfig & GPSOptions::ValidLocation) {
    setProperty(0, PROPERTY_GPS_FIX, (int) Gps::WaitFor::FIX_POS);
  } else if (gpsConfig & GPSOptions::NumberSatellites) {
    setProperty(0, PROPERTY_GPS_FIX, doc["satsForFix"] | 4);
  } else {
    setProperty(0, PROPERTY_GPS_FIX, (int) Gps::WaitFor::FIX_NO_WAIT);
  }
  setProperty(0, PROPERTY_CONFIRMATION_TIME_SECONDS, doc["confirmationTimeWindow"] | 5);
  setProperty(0, PROPERTY_PRIVACY_CONFIG, doc["privacyConfig"] | AbsolutePrivacy);
  setProperty(0, PROPERTY_BLUETOOTH, doc["bluetooth"] | false);
  setProperty(0, PROPERTY_SIM_RA, doc["simRaMode"] | false);
  setProperty(0, PROPERTY_DEVELOPER, doc["devConfig"] | 0);

  int numPrivacyAreas = doc["numPrivacyAreas"] | 0;
  // Append new values to the privacy-vector
  for (int idx = 0; idx < numPrivacyAreas || doc.containsKey("privacyLatitude" + String(idx)); ++idx) {
    PrivacyArea privacyAreaTemp;
    privacyAreaTemp.latitude = doc["privacyLatitude" + String(idx)] | 51.0;
    privacyAreaTemp.longitude = doc["privacyLongitude" + String(idx)] | 9.0;
    privacyAreaTemp.transformedLatitude = doc["privacyTransformedLatitude" + String(idx)] | 51.0;
    privacyAreaTemp.transformedLongitude = doc["privacyTransformedLongitude" + String(idx)] | 9.0;
    privacyAreaTemp.radius = doc["privacyRadius" + String(idx)] | 500.0;
    setPrivacyArea(0, idx, privacyAreaTemp);
  }

  // after parseOld
  log_d("After parse old:");
  printConfig();
}

template<typename T> T ObsConfig::getProperty(const String &key) const {
  T result;
  if (jsonData["obs"][selectedProfile].containsKey(key)) {
    result = jsonData["obs"][selectedProfile][key].as<T>();
  } else {
    result = jsonData["obs"][0][key].as<T>();
  }
  return result;
}

template<> String ObsConfig::getProperty(const String &key) const {
  String result;
  if (jsonData["obs"][selectedProfile].containsKey(key)) {
    result = jsonData["obs"][selectedProfile][key].as<String>();
  } else {
    result = jsonData["obs"][0][key].as<String>();
  }
  return result;
}

std::vector<int> ObsConfig::getIntegersProperty(const String &key) const {
  JsonArrayConst ints;
  if (jsonData["obs"][selectedProfile].containsKey(key)) {
    ints = jsonData["obs"][selectedProfile][key];
  } else {
    ints = jsonData["obs"][0][key];
  }
  std::vector<int> result;
  for (int i : ints) {
    result.push_back(i);
  }
  return result;
}

void ObsConfig::releaseJson() {
  jsonData.clear(); // should we just shrink?
}
