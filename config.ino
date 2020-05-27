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

void loadConfiguration(const char *configFilename, Config &config) {
  // Open file for reading
  File file = SPIFFS.open(configFilename);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<1024> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonDocument to the Config
  config.numSensors = doc["numSensors"] | 2;
  for (size_t idx = 0; idx < config.numSensors; ++idx)
  {
    uint8_t offsetTemp;
    String offsetString = "offsetInfo" + String(idx);
    offsetTemp = doc[offsetString] | 35;
    config.sensorOffsets.push_back(offsetTemp);
  }
  strlcpy(config.ssid, doc["ssid"] | "Freifunk", sizeof(config.ssid));
  strlcpy(config.password, doc["password"] | "Freifunk", sizeof(config.password));
  strlcpy(config.obsUserID, doc["obsUserID"] | "5e8f2f43e7e3b3668ca13151", sizeof(config.obsUserID));
  config.displayConfig = doc["displayConfig"] | 0;
  config.port = doc["port"] | 2731;
  strlcpy(config.hostname,                  // <- destination
          doc["hostname"] | "openbikesensor.hlrs.de",  // <- source
          sizeof(config.hostname));         // <- destination's capacity
  config.satsForFix = doc["satsForFix"] | 4;
  config.confirmationTimeWindow = doc["confirmationTimeWindow"] | 5;

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

// Saves the configuration to a file
void saveConfiguration(const char *filename, const Config &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  SPIFFS.remove(filename);

  // Open file for writing
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<256> doc;

  // Set the values in the document
  doc["numSensors"] = config.numSensors;
  for (size_t idx = 0; idx < config.numSensors; ++idx)
  {
    String offsetString = "offsetInfo" + String(idx);
    doc[offsetString] = config.sensorOffsets[idx];
  }
  doc["satsForFix"] = config.satsForFix;
  
  doc["hostname"] = config.hostname;
  doc["port"] = config.port;
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["obsUserID"] = config.obsUserID;
  doc["displayConfig"] = config.displayConfig;
  doc["confirmationTimeWindow"] = config.confirmationTimeWindow;

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
}

// Prints the content of a file to the Serial
void printConfig(Config &config) {

  Serial.println(F("################################"));
  Serial.print(F("numSensors = "));
  Serial.println(String(config.numSensors));

  for (size_t idx = 0; idx < config.numSensors; ++idx)
  {
    String offsetString = "Offset[" + String(idx) + "] = " + config.sensorOffsets[idx];
    Serial.println(offsetString);
  }

  Serial.print(F("satsForFix = "));
  Serial.println(String(config.satsForFix));

  Serial.print(F("displayConfig = "));
  Serial.println(String(config.displayConfig));

  Serial.print(F("confirmationTimeWindow = "));
  Serial.println(String(config.confirmationTimeWindow));

  Serial.print(F("hostname = "));
  Serial.println(String(config.hostname));

  Serial.print(F("port = "));
  Serial.println(String(config.port));

  Serial.print(F("obsUserID = "));
  Serial.println(String(config.obsUserID));

  Serial.print(F("SSID = "));
  Serial.println(String(config.ssid));

  #ifdef dev
    Serial.print(F("password = "));
    Serial.println(String(config.password));
  #endif

  Serial.println(F("################################"));

/*
  doc["displayConfig"] = config.displayConfig;

*/



  /*
  // Open file for reading
  File file = SPIFFS.open(filename);
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    // do not ptrint passwords, please it is bad enough that we save them as plain text anyway...
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
  */
}
