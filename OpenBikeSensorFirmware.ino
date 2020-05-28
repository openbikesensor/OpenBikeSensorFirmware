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

#define MAX_SENSOR_VALUE 255

#include "vector.h"
#include <ArduinoJson.h>
#include "config.h"
#include <Wire.h>
#define CIRCULAR_BUFFER_INT_SAFE
#include <CircularBuffer.h>
#include "gps.h"
#include "sensor.h"
#include "displays.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "SPIFFS.h"


#include "writer.h"


#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "configServer.h"

//GPS

//Version
const char *OBSVersion = "v0.1.5";


// PINs
const int PushButton = 2;


// VARs
const int runs = 20;
unsigned long measureInterval = 1000;
unsigned long timeOfLastNotificationAttempt = millis();
unsigned long timeOfMinimum = millis();
unsigned long StartTime = millis();
unsigned long CurrentTime = millis();
unsigned long buttonPushedTime = millis();

//int timeout = 15000; /// ???
bool usingSD = false;
String text = "";
uint8_t minDistanceToConfirm = MAX_SENSOR_VALUE;
uint8_t confirmedMinDistance = MAX_SENSOR_VALUE;
bool transmitConfirmedData = false;
int lastButtonState = 0;
int buttonState = 0;
bool handleBarWidthReset = false;

String filename;

const char *configFilename = "/config.txt";  // <- SD library uses 8.3 filenames
Config config;

CircularBuffer<DataSet*, 10> dataBuffer;
Vector<String> sensorNames;

#define TASK_SERIAL_RATE 1000 // ms

uint32_t nextSerialTaskTs = 0;
uint32_t nextOledTaskTs = 0;

uint8_t handleBarWidth = 0;

SSD1306DisplayDevice* displayTest;

FileWriter* writer;

HCSR04SensorManager* sensorManager;

String esp_chipid;

// Enable dev-mode. Allows to 
// - set wifi config 
// - prints more detailed log messages to serial (WIFI password)
//#define dev

void setup() {
  Serial.begin(115200);

  // Serial.println("setup()");

  //##############################################################
  // Configure button pin as INPUT
  //##############################################################

  pinMode(PushButton, INPUT);

  //##############################################################
  // Setup display
  //##############################################################
  
  displayTest = new SSD1306DisplayDevice;
  #ifdef dev
    //displayTest->showGrid(true);
  #endif
  
  displayTest->showLogo(true);
  displayTest->showTextOnGrid(2, 0, OBSVersion);

  //##############################################################
  // Load, print and save config
  //##############################################################

  displayTest->showTextOnGrid(2, 1, "Config... ");

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
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

  if(config.displayConfig & DisplayInvert) displayTest->invert(); 
  else displayTest->normalDisplay();

  if(config.displayConfig & DisplayFlip) displayTest->flipScreen();
  
  delay(333); // Added for user experience
  displayTest->showTextOnGrid(2, 1, "Config... ok");

  //##############################################################
  // Check, if the button is pressed
  // Enter configuration mode and enable OTA 
  //##############################################################

  buttonState = digitalRead(PushButton);
  if (buttonState == HIGH)
  {
    displayTest->showTextOnGrid(2, 2, "Start Server");
    delay(1000); // Added for user experience

    startServer();
    OtaInit(esp_chipid);

    while (true) {
      server.handleClient();
      delay(1);
      ArduinoOTA.handle();
    }
  }

  //##############################################################
  // Init HCSR04
  //##############################################################

  sensorManager = new HCSR04SensorManager;

  HCSR04SensorInfo sensorManaged1;
  sensorManaged1.triggerPin = (config.displayConfig & DisplaySwapSensors) ? 25 : 15;
  sensorManaged1.echoPin = (config.displayConfig & DisplaySwapSensors) ? 26 : 4;
  sensorManaged1.sensorLocation = "Right"; // TODO
  sensorManager->registerSensor(sensorManaged1);

  HCSR04SensorInfo sensorManaged2;
  sensorManaged2.triggerPin = (config.displayConfig & DisplaySwapSensors) ? 15 : 25;
  sensorManaged2.echoPin = (config.displayConfig & DisplaySwapSensors) ? 4 : 26;
  sensorManaged2.sensorLocation = "Left"; // TODO
  sensorManager->registerSensor(sensorManaged2);

  sensorManager->setOffsets(config.sensorOffsets);
  sensorManager->setTimeouts();

  //##############################################################
  // GPS
  //##############################################################

  SerialGPS.begin(9600, SERIAL_8N1, 16, 17);

  //##############################################################
  // Handle SD
  //##############################################################

  displayTest->showTextOnGrid(2, 2, "SD...");
  while (!SD.begin())
  {
    Serial.println("Card Mount Failed");
    delay(20);
  }
  delay(333); // Added for user experience
  Serial.println("Card Mount Succeeded");
  displayTest->showTextOnGrid(2, 2, "SD... ok");

  //##############################################################
  // Prepare CSV file
  //##############################################################
  
  displayTest->showTextOnGrid(2, 3, "CSV file...");

  writer = new CSVFileWriter;
  writer->setFileName();
  writer->writeHeader();
  Serial.println("File initialised");

  displayTest->showTextOnGrid(2, 3, "CSV file... ok");

  //##############################################################
  // GPS
  //##############################################################

  displayTest->showTextOnGrid(2, 4, "Wait for GPS");  
  Serial.println("Waiting for GPS fix...");
  while (gps.satellites.value() < config.satsForFix)
  {
    readGPSData();
    delay(300);

    String satellitesString = String(gps.satellites.value()) + " / " + String(config.satsForFix) +" sats";
    displayTest->showTextOnGrid(2, 5, satellitesString);

    buttonState = digitalRead(PushButton);
    if (buttonState == HIGH)
    {
      Serial.println("Skipped get GPS...");
      displayTest->showTextOnGrid(2, 5, "...skipped");
      break;
    }
  }

  if(gps.satellites.value() == config.satsForFix) {
    Serial.print("Got GPS Fix: ");
    Serial.println(String(gps.satellites.value()));
    displayTest->showTextOnGrid(2, 5, "Got GPS Fix");
  }
  
  delay(1000); // Added for user experience

  // Clear the display once!
  displayTest->clear(); 
}

void loop() {

  Serial.println("loop()");

  DataSet* currentSet = new DataSet;
  //specify which sensors value can be confirmed by pressing the button, should be configurable
  uint8_t confirmationSensorID = 1; // LEFT !!!
  readGPSData();
  currentSet->location = gps.location;
  currentSet->altitude = gps.altitude;
  currentSet->date = gps.date;
  currentSet->time = gps.time;
  currentSet->speed = gps.speed;
  currentSet->course = gps.course;
  currentSet->isInsidePrivacyArea = isInsidePrivacyArea(currentSet->location);
  sensorManager->reset(false);

  CurrentTime = millis();
  uint8_t minDistance = MAX_SENSOR_VALUE;
  int measurements = 0;

  // if the detected minimum was measured more than 5s ago, it is discarded and cannot be confirmed
  int timeDelta = CurrentTime - timeOfMinimum;
  if ((timeDelta ) > (config.confirmationTimeWindow * 1000))
  {
    Serial.println(">>> CTW reached - reset() <<<");
    minDistanceToConfirm = MAX_SENSOR_VALUE;
    sensorManager->reset(true);
  }

  // do this for the time specified by measureInterval, e.g. 1s
  while ((CurrentTime - StartTime) < measureInterval && !transmitConfirmedData)
  {
    // #######################################################
    // Display
    // #######################################################

    CurrentTime = millis();
    sensorManager->getDistances();

    // Show values on the display
    displayTest->showValues(
      sensorManager->m_sensors[1],
      sensorManager->m_sensors[0]
    );

    // #######################################################
    // Stoarge
    // #######################################################

    // if a new minimum on the selected sensor is detected, the value and the time of detection will be stored
    if (sensorManager->sensorValues[confirmationSensorID] < minDistanceToConfirm)
    {
      minDistanceToConfirm = sensorManager->sensorValues[confirmationSensorID];
      Serial.print("New minDistanceToConfirm=");
      Serial.println(minDistanceToConfirm);
      timeOfMinimum = millis();
    }



    // if there is a sensor value and confirmation was not already triggered
    if (!transmitConfirmedData)
    {
      buttonState = digitalRead(PushButton);
      // detect state change
      if (buttonState != lastButtonState)
      {
        if (buttonState == LOW) //after button was released
        {
          transmitConfirmedData = true;
        }
      }
      lastButtonState = buttonState;
    }
    measurements++;
  }

  // Write the minimum values of the while-loop to a set
  currentSet->sensorValues = sensorManager->sensorValues;

  // if nothing was detected, write the dataset to file, otherwise write it to the buffer for confirmation
  if ((currentSet->sensorValues[confirmationSensorID] == MAX_SENSOR_VALUE) && dataBuffer.isEmpty())
  {
    Serial.write("Empty Buffer, writing directly ");
    if (writer) writer->writeData(currentSet);
    delete currentSet;
  }
  else
  {
    dataBuffer.push(currentSet);
  }

  Serial.write("min. distance: ");
  Serial.print(currentSet->sensorValues[confirmationSensorID]) ;
  Serial.write(" cm,");
  Serial.print(measurements);
  Serial.write(" measurements  \n");

  if (transmitConfirmedData)
  {
    //inverting the display until the data is saved TODO: add control over the time the display will be inverted so the user actually sees it.
    if(config.displayConfig & DisplayInvert) displayTest->normalDisplay(); 
    else displayTest->invert();
    
    Serial.printf("Trying to transmit Confirmed data \n");
    // make sure the minimum distance is saved only once
    using index_t = decltype(dataBuffer)::index_t;
    index_t j;
    //Search for the latest appearence of the confirmed minimum value
    for (index_t i = 0; i < dataBuffer.size(); i++)
    {
      if (dataBuffer[i]->sensorValues[confirmationSensorID] == minDistanceToConfirm)
      {
        j = i;
      }
    }
    for (index_t i = 0; i < dataBuffer.size(); i++)
    {
      if (i == j)
      {
        dataBuffer[i]->confirmed = true; // set the confirmed tag of the set in the buffer containing the minimum
        Serial.printf("Found confirmed data in buffer \n");
      }
    }

    // Empty buffer by writing it, after confirmation it will be written to SD card directly so no confirmed sets will be lost
    while (!dataBuffer.isEmpty())
    {
      DataSet* dataset = dataBuffer.shift();
      Serial.printf("Trying to write set to file\n");
      if (writer) writer->writeData(dataset);
      Serial.printf("Wrote set to file\n");
      delete dataset;
    }
    writer->writeDataToSD();
    
    Serial.printf(">>> writeDataToSD - reset <<<");
    minDistanceToConfirm = MAX_SENSOR_VALUE;
    sensorManager->reset(true);
    
    transmitConfirmedData = false;

    if(config.displayConfig & DisplayInvert) displayTest->invert(); 
    else displayTest->normalDisplay();

    // Lets clear the display again, so on the next cycle, it will be recreated
    displayTest->clear();
  }

  // If the circular buffer is full, write just one set to the writers buffer, will fail when the confirmation timeout is larger than measureInterval * dataBuffer.size() 
  if (dataBuffer.isFull())
  {
    DataSet* dataset = dataBuffer.shift();
    dataset->sensorValues[0] = MAX_SENSOR_VALUE;
    Serial.printf("Buffer full, writing set to file\n");
    if (writer) writer->writeData(dataset);
    delete dataset;
  }

  // write data to sd every now and then, hardcoded value assuming it will write every minute or so
  if ((writer->getDataLength() > 5000) && !(digitalRead(PushButton))) {
    writer->writeDataToSD();
  }

  unsigned long ElapsedTime = CurrentTime - StartTime;
  StartTime = CurrentTime;
  Serial.write(" Time elapsed: ");
  Serial.print(ElapsedTime);
  Serial.write(" milliseconds\n");
}
