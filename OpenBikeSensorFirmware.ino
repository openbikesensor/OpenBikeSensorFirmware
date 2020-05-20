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
#include <EEPROM.h>
#define CIRCULAR_BUFFER_INT_SAFE
#include <CircularBuffer.h>
#include "gps.h"
#include "displays.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "SPIFFS.h"


#include "writer.h"
#include "sensor.h"

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
const char *OBSVersion = "v0.1.1";

// define the number of bytes to store
//#define EEPROM_SIZE 1
#define EEPROM_SIZE 128


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

int timeout = 15000;
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
  displayTest->showLogo(true);
  //displayTest->showGrid(true); // Debug only
  //displayTest->flipScreen(); // TODO: Make this configurable
  //displayTest->invert(); // TODO: Make this configurable
  
  displayTest->showTextOnGrid(2, 0, OBSVersion);

  //return;

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
  sensorManaged1.triggerPin = 15;
  sensorManaged1.echoPin = 4;
  sensorManaged1.sensorLocation = "Lid"; // TODO
  sensorManager->registerSensor(sensorManaged1);

  HCSR04SensorInfo sensorManaged2;
  sensorManaged2.triggerPin = 25;
  sensorManaged2.echoPin = 26;
  sensorManaged2.sensorLocation = "Case"; // TODO
  sensorManager->registerSensor(sensorManaged2);

  sensorManager->setOffsets(config.sensorOffsets);
  sensorManager->setTimeouts();

  //##############################################################
  // GPS
  //##############################################################

  SerialGPS.begin(9600, SERIAL_8N1, 16, 17);
  while (!EEPROM.begin(EEPROM_SIZE)) {
    true;
  }
  // readLastFixFromEEPROM();

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
  // ???
  //##############################################################
  
  // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);


  //##############################################################
  // GPS
  //##############################################################

  displayTest->showTextOnGrid(2, 4, "Wait for GPS");  
  while (gps.satellites.value() < 4)
  {
    Serial.println("Waiting for GPS fix...");
    readGPSData();
    delay(300);
    String satellitesString = String(gps.satellites.value()) + " sats";
    displayTest->showTextOnGrid(2, 5, satellitesString);
    buttonState = digitalRead(PushButton);
    if (buttonState == HIGH)
    {
      Serial.println("Skipped get GPS...");
      displayTest->showTextOnGrid(2, 5, "...skipped");
      break;
    }
  }

  if(gps.satellites.value() == 4) {
    Serial.print("Got GPS Fix: ");
    Serial.println(String(gps.satellites.value()));
    displayTest->showTextOnGrid(2, 5, "Got GPS Fix");
  }
  
  delay(1000); // Added for user experience

  // Clear the display once!
  displayTest->clear(); 
}

/*
   easily read and write EEPROM
*/
template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
  const byte* p = (const byte*)(const void*)&value;
  int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
  return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{
  byte* p = (byte*)(void*)&value;
  int i;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
  return i;
}

void loop() {

  //Serial.println("loop()");
  //delay(1000);
  //return;

  //ArduinoOTA.handle(); 
  DataSet* currentSet = new DataSet;
  uint8_t confirmationSensorID = 1;
  //writeLastFixToEEPROM();
  // Todo: proceed only if gps is valid and updated
  readGPSData();
  currentSet->location = gps.location;
  currentSet->altitude = gps.altitude;
  currentSet->date = gps.date;
  currentSet->time = gps.time;
  currentSet->speed = gps.speed;
  currentSet->course = gps.course;
  sensorManager->reset();

  CurrentTime = millis();
  uint8_t minDistance = MAX_SENSOR_VALUE;
  int measurements = 0;
  if ((CurrentTime - timeOfMinimum ) > 5000)
  {
    minDistanceToConfirm = MAX_SENSOR_VALUE;
  }

  while ((CurrentTime - StartTime) < measureInterval)
  {
    CurrentTime = millis();
    sensorManager->getDistances();

    //displayTest->showValue(minDistance);
    if (sensorManager->sensorValues[confirmationSensorID] < minDistanceToConfirm)
    {
      minDistanceToConfirm = sensorManager->sensorValues[confirmationSensorID];
      timeOfMinimum = millis();
    }
    displayTest->showValues(minDistanceToConfirm,sensorManager->m_sensors[0].minDistance,sensorManager->m_sensors[1].minDistance);
    
    if ((minDistanceToConfirm < MAX_SENSOR_VALUE) && !transmitConfirmedData)
    {
      buttonState = digitalRead(PushButton);
      if (buttonState != lastButtonState)
      {
        if (buttonState == LOW)
        {
          if (!handleBarWidthReset) transmitConfirmedData = true;
          else handleBarWidthReset = false;
        }
        else
        {
          buttonPushedTime = CurrentTime;
        }
      }
      else
      {
        if (buttonState == HIGH)
        {
          //Button state is high for longer than 5 seconds -> reset handlebar width
          //Todo: do it for all connected sensors
          if ((CurrentTime - buttonPushedTime > 5000))
          {
            setHandleBarWidth(minDistanceToConfirm);
            displayTest->showValue(minDistanceToConfirm);
            //displayTest2->showValue(minDistanceToConfirm);
            delay(5000);
            handleBarWidthReset = true;
            transmitConfirmedData = false;
          }
        }
      }
      lastButtonState = buttonState;
    }
    measurements++;
  }
  currentSet->sensorValues = sensorManager->sensorValues;

  //if nothing was detected, write the dataset to file, otherwise write it to the buffer for confirmation
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

  uint8_t buffer[2];
  uint8_t isit8or16bit = 0;
  buffer[0] = isit8or16bit;
  if (transmitConfirmedData)
  {
    displayTest->invert();
    Serial.printf("Trying to transmit Confirmed data \n");
    buffer[1] = minDistanceToConfirm;
    // make sure the minimum distance is saved only once
    using index_t = decltype(dataBuffer)::index_t;
    index_t j;
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
        dataBuffer[i]->confirmed = true;
        Serial.printf("Found confirmed data in buffer \n");
      }
    }

    while (!dataBuffer.isEmpty())
    {
      DataSet* dataset = dataBuffer.shift();
      Serial.printf("Trying to write set to file\n");
      if (writer) writer->writeData(dataset);
      Serial.printf("Wrote set to file\n");
      delete dataset;
    }
    writer->writeDataToSD();
    minDistanceToConfirm = MAX_SENSOR_VALUE;
    transmitConfirmedData = false;
    displayTest->normalDisplay();
  }
  else
  {
    buffer[1] = MAX_SENSOR_VALUE;
  }

  if (dataBuffer.isFull())
  {
    DataSet* dataset = dataBuffer.shift();
    dataset->sensorValues[0] = MAX_SENSOR_VALUE;
    Serial.printf("Buffer full, writing set to file\n");
    if (writer) writer->writeData(dataset);
    Serial.printf("Deleting dataset\n");
    delete dataset;
  }

  if ((writer->getDataLength() > 5000) && !(digitalRead(PushButton))) {
    writer->writeDataToSD();
  }

  unsigned long ElapsedTime = CurrentTime - StartTime;
  StartTime = CurrentTime;
  Serial.write(" Time elapsed: ");
  Serial.print(ElapsedTime);
  Serial.write(" milliseconds\n");
}
