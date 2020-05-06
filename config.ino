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

void setHandleBarWidth(int width) {
  handleBarWidth = width;
  timeout = 15000 + (int)(handleBarWidth * 29.1 * 2);
  EEPROM.write(0, handleBarWidth);
  EEPROM.commit();
}

void loadConfiguration(const char *configFilename, Config &config) {
  // Open file for reading
  File file = SD.open(configFilename);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonDocument to the Config
  config.numSensors = doc["numSensors"] | 2;
  for (size_t idx = 0; idx < config.numSensors; ++idx)
  {
    uint8_t offsetTemp;
    String offsetString = "offsetInfo"+String(idx);
    offsetTemp = doc[offsetString] | 35;
    config.sensorOffsets.push_back(offsetTemp);
  }
  strlcpy(config.ssid, doc["ssid"] | "Freifunk", sizeof(config.ssid));
  strlcpy(config.password, doc["password"] | "Freifunk", sizeof(config.password));
  config.port = doc["port"] | 2731;
  strlcpy(config.hostname,                  // <- destination
          doc["hostname"] | "example.com",  // <- source
          sizeof(config.hostname));         // <- destination's capacity

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

// Saves the configuration to a file
void saveConfiguration(const char *filename, const Config &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  SD.remove(filename);

  // Open file for writing
  File file = SD.open(filename, FILE_WRITE);
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

  }
  doc["hostname"] = config.hostname;
  doc["port"] = config.port;
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
}

// Prints the content of a file to the Serial
void printFile(const char *filename) {
  // Open file for reading
  File file = SD.open(filename);
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
}
