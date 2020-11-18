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

#include "OpenBikeSensorFirmware.h"

#ifndef BUILD_NUMBER
#define BUILD_NUMBER "local"
#endif

// --- Global variables ---
// Version only change the "vN.M" part if needed.
const char *OBSVersion = "v0.3" BUILD_NUMBER;

const int LEFT_SENSOR_ID = 1;
const int RIGHT_SENSOR_ID = 0;



// PINs
const int PushButton = 2;
const uint8_t GPS_POWER = 12;

int confirmedMeasurements = 0;
int numButtonReleased = 0;
DataSet *datasetToConfirm = nullptr;

int buttonState = 0;

const char *configFilename = "/config.txt";  // <- SD library uses 8.3 filenames
Config config;

SSD1306DisplayDevice* displayTest;
HCSR04SensorManager* sensorManager;
BluetoothManager* bluetoothManager;
VoltageMeter* voltageMeter;

String esp_chipid;

// --- Local variables ---
unsigned long measureInterval = 1000;
unsigned long timeOfMinimum = esp_timer_get_time(); //  millis();
unsigned long startTimeMillis = 0;
unsigned long currentTimeMillis = millis();

bool usingSD = false;
String text = "";
uint16_t minDistanceToConfirm = MAX_SENSOR_VALUE;
uint16_t minDistanceToConfirmIndex = 0;
bool transmitConfirmedData = false;
int lastButtonState = 0;

String filename;

CircularBuffer<DataSet*, 10> dataBuffer;

FileWriter* writer;

const uint8_t displayAddress = 0x3c;

// Enable dev-mode. Allows to
// - set wifi config
// - prints more detailed log messages to serial (WIFI password)
//#define DEVELOP

int lastMeasurements = 0 ;

void setup() {
  Serial.begin(115200);

  // Serial.println("setup()");

  //##############################################################
  // Configure button pin as INPUT
  //##############################################################

  pinMode(PushButton, INPUT);
  pinMode(GPS_POWER, OUTPUT);
  digitalWrite(GPS_POWER,HIGH);

  //##############################################################
  // Setup display
  //##############################################################
  Wire.begin();
  Wire.beginTransmission(displayAddress);
  byte displayError = Wire.endTransmission();
  if (displayError != 0)
  {
    Serial.println("Display not found");
  }
  displayTest = new SSD1306DisplayDevice;

  displayTest->showLogo(true);
  displayTest->showTextOnGrid(2, 0, OBSVersion);

  //##############################################################
  // Load, print and save config
  //##############################################################

  displayTest->showTextOnGrid(2, 1, "Config... ");

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    displayTest->showTextOnGrid(2, 1, "Config... error ");
    return;
  }
  // Should load default config if run for the first time
  Serial.println(F("Load config"));
  loadConfiguration(configFilename, config);

  // Dump config file
  Serial.println(F("Print config file..."));
  printConfig(config);

  // Save the config. This ensures, that new options exist as well
  saveConfiguration(configFilename, config);

#ifdef DEVELOP
  if(config.devConfig & ShowGrid) {
    displayTest->showGrid(true);
  }
#endif

  if (config.displayConfig & DisplayInvert) displayTest->invert();
  else displayTest->normalDisplay();

  if (config.displayConfig & DisplayFlip) displayTest->flipScreen();

  delay(333); // Added for user experience
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "<%02d| |%02d>",
           config.sensorOffsets[LEFT_SENSOR_ID],
           config.sensorOffsets[RIGHT_SENSOR_ID]);
  displayTest->showTextOnGrid(2, 1, buffer);


  //##############################################################
  // GPS
  //##############################################################

  SerialGPS.begin(9600, SERIAL_8N1, 16, 17);
  // default buffer is 256, would not be to bad to go back to the default in need of memory see gps.cpp
  SerialGPS.setRxBufferSize(512);

  //##############################################################
  // Handle SD
  //##############################################################

  // Counter, how often the SD card will be read before writing an error on the display
  int8_t sdCount = 5;

  displayTest->showTextOnGrid(2, 2, "SD...");
  while (!SD.begin())
  {
    if(sdCount > 0) {
      sdCount--;
    } else {
      displayTest->showTextOnGrid(2, 2, "SD... error");
      if (config.simRaMode || digitalRead(PushButton) == HIGH) {
        break;
      }
    }
    Serial.println("Card Mount Failed");
    //delay(100);
  }
  delay(333); // Added for user experience
  if (SD.begin()) {
    Serial.println("Card Mount Succeeded");
    displayTest->showTextOnGrid(2, 2, "SD... ok");
  }

  //##############################################################
  // Check, if the button is pressed
  // Enter configuration mode and enable OTA
  //##############################################################

  buttonState = digitalRead(PushButton);
  if (buttonState == HIGH || (!config.simRaMode && displayError != 0))
  {
    displayTest->showTextOnGrid(2, 2, "Start Server");
    esp_bt_mem_release(ESP_BT_MODE_BTDM); // no bluetooth at all here.

    delay(1000); // Added for user experience

    startServer();
    OtaInit(esp_chipid);

    while (true) {
      server.handleClient();
      delay(1);
      ArduinoOTA.handle();
    }
  }
  SPIFFS.end();
  WiFiGenericClass::mode(WIFI_MODE_NULL);

  //##############################################################
  // Init HCSR04
  //##############################################################

  sensorManager = new HCSR04SensorManager;

  HCSR04SensorInfo sensorManaged1;
  sensorManaged1.triggerPin = (config.displayConfig & DisplaySwapSensors) ? 25 : 15;
  sensorManaged1.echoPin = (config.displayConfig & DisplaySwapSensors) ? 26 : 4;
  sensorManaged1.sensorLocation = (char*) "Right"; // TODO
  sensorManager->registerSensor(sensorManaged1);

  HCSR04SensorInfo sensorManaged2;
  sensorManaged2.triggerPin = (config.displayConfig & DisplaySwapSensors) ? 15 : 25;
  sensorManaged2.echoPin = (config.displayConfig & DisplaySwapSensors) ? 4 : 26;
  sensorManaged2.sensorLocation = (char*) "Left"; // TODO
  sensorManager->registerSensor(sensorManaged2);

  sensorManager->setOffsets(config.sensorOffsets);

  sensorManager->setPrimarySensor(LEFT_SENSOR_ID);

  //##############################################################
  // Prepare CSV file
  //##############################################################

  displayTest->showTextOnGrid(2, 3, "CSV file...");

  if (SD.begin()) {
    writer = new CSVFileWriter;
    writer->setFileName();
    writer->writeHeader();
    displayTest->showTextOnGrid(2, 3, "CSV file... ok");
    Serial.println("File initialised");
  } else {
    displayTest->showTextOnGrid(2, 3, "CSV. skipped");
  }

  //##############################################################
  // GPS
  //##############################################################

  displayTest->showTextOnGrid(2, 4, "Wait for GPS");
  Serial.println("Waiting for GPS fix...");
  bool validGPSData = false;
  readGPSData();

  voltageMeter = new VoltageMeter; // takes a moment, so do it here

  delay(300);
  while (!validGPSData)
  {
    Serial.println("readGPSData()");
    readGPSData();

    switch (config.GPSConfig)
    {
      case ValidLocation: {
          validGPSData = gps.location.isValid();
          if (validGPSData)
            Serial.println("Got location...");
          break;
        }
      case ValidTime: {
          validGPSData = gps.time.isValid();
          if (validGPSData)
            Serial.println("Got time...");
          break;
        }
      case NumberSatellites: {
          validGPSData = gps.satellites.value() >= config.satsForFix;
          if (validGPSData)
            Serial.println("Got required number of satellites...");
          break;
        }
      default: {
          validGPSData = false;
        }

    }
    delay(300);

    String satellitesString;
    if (gps.passedChecksum() == 0) { // could not get any valid char from GPS module
      satellitesString = "OFF?";
    } else if (!gps.time.isValid()) {
      satellitesString = "no time";
    } else {
      satellitesString = String(gps.satellites.value()) + " / " + String(config.satsForFix) + " sats";
    }
    displayTest->showTextOnGrid(2, 5, satellitesString);

    buttonState = digitalRead(PushButton);
    if (buttonState == HIGH
      || (config.simRaMode && gps.passedChecksum() == 0) // no module && simRaMode
      )
    {
      Serial.println("Skipped get GPS...");
      displayTest->showTextOnGrid(2, 5, "...skipped");
      break;
    }
  }

  if (gps.satellites.value() == config.satsForFix) {
    Serial.print("Got GPS Fix: ");
    Serial.println(String(gps.satellites.value()));
    displayTest->showTextOnGrid(2, 5, "Got GPS Fix");
  }

  //##############################################################
  // Bluetooth
  //##############################################################
  if (config.bluetooth) {
    bluetoothManager->init();
    bluetoothManager->activateBluetooth();
  } else {
    esp_bt_mem_release(ESP_BT_MODE_BTDM); // no bluetooth at all here.
  }

  delay(1000); // Added for user experience

  // Clear the display once!
  displayTest->clear();
}

void loop() {

  Serial.println("loop()");

  auto* currentSet = new DataSet;
  //specify which sensors value can be confirmed by pressing the button, should be configurable
  uint8_t confirmationSensorID = LEFT_SENSOR_ID;
  readGPSData(); // needs <=1ms

  currentTimeMillis = millis();
  if (startTimeMillis == 0) {
    startTimeMillis = (currentTimeMillis / measureInterval) * measureInterval;
  }
  currentSet->time = currentTime();
  currentSet->millis = currentTimeMillis;
  currentSet->location = gps.location;
  currentSet->altitude = gps.altitude;
  currentSet->course = gps.course;
  currentSet->speed = gps.speed;
  currentSet->hdop = gps.hdop;
  currentSet->validSatellites = gps.satellites.isValid() ? (uint8_t) gps.satellites.value() : 0;
  currentSet->batteryLevel = voltageMeter->read();
  currentSet->isInsidePrivacyArea = isInsidePrivacyArea(currentSet->location);

  sensorManager->reset();

  int measurements = 0;

  // if the detected minimum was measured more than 5s ago, it is discarded and cannot be confirmed
  int timeDelta = (int) (currentTimeMillis - timeOfMinimum);
  if ((timeDelta ) > (config.confirmationTimeWindow * 1000)) {
    Serial.println(">>> CTW reached - reset() <<<");
    minDistanceToConfirm = MAX_SENSOR_VALUE;
    datasetToConfirm = nullptr;
  }

  // do this for the time specified by measureInterval, e.g. 1s
  while ((currentTimeMillis - startTimeMillis) < measureInterval) {

    currentTimeMillis = millis();
    sensorManager->getDistances();

    displayTest->showValues(
      sensorManager->m_sensors[LEFT_SENSOR_ID],
      sensorManager->m_sensors[RIGHT_SENSOR_ID],
      minDistanceToConfirm,
      lastMeasurements,
      currentSet->isInsidePrivacyArea
    );

    if (config.bluetooth) {
      auto leftValues = std::list<uint16_t>();
      auto rightValues = std::list<uint16_t>();
      leftValues.push_back(sensorManager->m_sensors[LEFT_SENSOR_ID].rawDistance);
      rightValues.push_back(sensorManager->m_sensors[RIGHT_SENSOR_ID].rawDistance);
      bluetoothManager->newSensorValues(leftValues, rightValues);
      bluetoothManager->processButtonState(digitalRead(PushButton));
    }

    // if there is a sensor value and confirmation was not already triggered
    if (minDistanceToConfirm != MAX_SENSOR_VALUE) {
      buttonState = digitalRead(PushButton);
      // detect state change
      if (buttonState != lastButtonState) {
        if (buttonState == LOW) { // after button was released
          // immediate user feedback - we start the action
          // invert state might be a bit long - it does not block next confirmation.
          if (config.displayConfig & DisplayInvert) displayTest->normalDisplay();
          else displayTest->invert();

          transmitConfirmedData = true;
          numButtonReleased++;
          if (datasetToConfirm != nullptr) {
            datasetToConfirm->confirmedDistances.push_back(minDistanceToConfirm);
            datasetToConfirm->confirmedDistancesTimeOffset.push_back(minDistanceToConfirmIndex);
            datasetToConfirm = nullptr;
          }
          minDistanceToConfirm = MAX_SENSOR_VALUE; // ready for next confirmation
        }
      }
      lastButtonState = buttonState;
    }

    // if a new minimum on the selected sensor is detected, the value and the time of detection will be stored
    if (sensorManager->sensorValues[confirmationSensorID] > 0
        && sensorManager->sensorValues[confirmationSensorID] < minDistanceToConfirm) {
      minDistanceToConfirm = sensorManager->sensorValues[confirmationSensorID];
      minDistanceToConfirmIndex = sensorManager->getCurrentMeasureIndex();
      // if there was no measurement of this sensor for this index, it is the
      // one before. This happens with fast confirmations.
      if (sensorManager->m_sensors[confirmationSensorID].echoDurationMicroseconds[minDistanceToConfirm - 1] <= 0) {
        minDistanceToConfirmIndex--;
      }
      datasetToConfirm = currentSet;
      Serial.print("New minDistanceToConfirm=");
      Serial.println(minDistanceToConfirm);
      timeOfMinimum = currentTimeMillis;
    }
    measurements++;
  } // end measureInterval while

  // Write the minimum values of the while-loop to a set
  for (size_t idx = 0; idx < sensorManager->m_sensors.size(); ++idx) {
    currentSet->sensorValues.push_back(sensorManager->m_sensors[idx].minDistance);
  }
  currentSet->measurements = sensorManager->lastReadingCount;
  memcpy(&(currentSet->readDurationsRightInMicroseconds),
         &(sensorManager->m_sensors[0].echoDurationMicroseconds), currentSet->measurements * sizeof(int32_t));
  memcpy(&(currentSet->readDurationsLeftInMicroseconds),
         &(sensorManager->m_sensors[1].echoDurationMicroseconds), currentSet->measurements * sizeof(int32_t));
  memcpy(&(currentSet->startOffsetMilliseconds),
         &(sensorManager->startOffsetMilliseconds), currentSet->measurements * sizeof(uint16_t));

  // if nothing was detected, write the dataset to file, otherwise write it to the buffer for confirmation
  if (!transmitConfirmedData
      && currentSet->sensorValues[confirmationSensorID] == MAX_SENSOR_VALUE
      && dataBuffer.isEmpty()) {
    Serial.write("Empty Buffer, writing directly ");
    if (writer) {
      writer->writeDataBuffered(currentSet);
    }
    delete currentSet;
  } else {
    dataBuffer.push(currentSet);
  }

#ifdef DEVELOP
  Serial.write("min. distance: ");
  Serial.print(currentSet->sensorValues[confirmationSensorID]) ;
  Serial.write(" cm,");
  Serial.print(measurements);
  Serial.write(" measurements  \n");
#endif

  lastMeasurements = measurements;

  if (transmitConfirmedData) {
    // Empty buffer by writing it, after confirmation it will be written to SD card directly so no confirmed sets will be lost
    while (!dataBuffer.isEmpty() && dataBuffer.first() != datasetToConfirm) {
      DataSet* dataset = dataBuffer.shift();
      if (dataset->confirmedDistances.size() == 0) {
        if (writer) {
          writer->writeDataBuffered(dataset);
        }
      }
      // write record as many times as we have confirmed values
      for (int i = 0; i < dataset->confirmedDistances.size(); i++) {
        // make sure the distance reported is the one that was confirmed
        dataset->sensorValues[confirmationSensorID] = dataset->confirmedDistances[i];
        dataset->confirmed = dataset->confirmedDistancesTimeOffset[i];
        confirmedMeasurements++;
        if (writer) {
          writer->writeDataBuffered(dataset);
        }
      }
      delete dataset;
    }
    if (writer) {  // "flush"
      writer->writeDataToSD();
    }
    Serial.printf(">>> writeDataToSD - reset <<<");
    transmitConfirmedData = false;
    // back to normal display mode
    if (config.displayConfig & DisplayInvert) displayTest->invert();
    else displayTest->normalDisplay();
  }

  // If the circular buffer is full, write just one set to the writers buffer,
  if (dataBuffer.isFull()) { // TODO: Same code as above
    DataSet* dataset = dataBuffer.shift();
    Serial.printf("data buffer full, writing set to file buffer\n");
    if (writer) {
      writer->writeDataBuffered(dataset);
    }
    // we are about to delete the to be confirmed dataset, so take care for this.
    if (datasetToConfirm == dataset) {
      datasetToConfirm = nullptr;
      minDistanceToConfirm = MAX_SENSOR_VALUE;
    }
    delete dataset;
  }

  Serial.printf("Time elapsed %lu milliseconds\n", currentTimeMillis - startTimeMillis);
  // synchronize to full measureIntervals
  startTimeMillis = (currentTimeMillis / measureInterval) * measureInterval;
}
