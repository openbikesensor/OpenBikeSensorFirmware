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

#include "vector.h"
#include <ArduinoJson.h>
#include "config.h"
#include <Wire.h>
#include <EEPROM.h>
#define CIRCULAR_BUFFER_INT_SAFE
#include <CircularBuffer.h>
#include "displays.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "SPIFFS.h"

#include "gps.h"

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
const char *OBSVersion = "v0.1.0";

// define the number of bytes to store
#define EEPROM_SIZE 1
#define EEPROM_SIZE 128

#define MAX_SENSOR_VALUE 255

// PINs
const int triggerPin = 15;
const int echoPin = 4;

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
bool deviceConnected = false;
/*
  class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }

  };

  class MyWriterCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      handleBarWidth = atoi(value.c_str());
      timeout = 15000 + (int)(handleBarWidth * 29.1 * 2);
      EEPROM.write(0, handleBarWidth);
      EEPROM.commit();
    }
  };
*/
DisplayDevice* displayTest;
//DisplayDevice* displayTest2;

FileWriter* writer;

//DistanceSensor* sensor1;
//DistanceSensor* sensor2;
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

HCSR04SensorManager* sensorManager;

String esp_chipid;

void setup() {
  Serial.begin(115200);

  displayTest = new SSD1306DisplayDevice;
  displayTest->drawString(64, 0, OBSVersion);

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Should load default config if run for the first time
  Serial.println(F("Loading configuration..."));
  loadConfiguration(configFilename, config);

  // Dump config file
  Serial.println(F("Print config file..."));
  printFile(configFilename);

  //enter configuration mode and enable OTA if button is pressed,
  buttonState = digitalRead(PushButton);
  if (buttonState == HIGH)
  {
    displayTest->clear();
    startServer();
    OtaInit(esp_chipid);
    displayTest->drawString(0, 0, "OpenBikeSensor");
    displayTest->drawString(0, 12, OBSVersion);

    while (true) {
      server.handleClient();
      delay(1);
      ArduinoOTA.handle();
    }
  }

  sensorManager = new HCSR04SensorManager;

  HCSR04SensorInfo sensorManaged1;
  sensorManaged1.sensorLocation = "Lid";
  sensorManager->registerSensor(sensorManaged1);

  HCSR04SensorInfo sensorManaged2;
  sensorManaged2.triggerPin = 25;
  sensorManaged2.echoPin = 26;
  sensorManaged2.sensorLocation = "Case";
  sensorManager->registerSensor(sensorManaged2);

  sensorManager->setOffsets(config.sensorOffsets);
  sensorManager->setTimeouts();

  //GPS
  SerialGPS.begin(9600, SERIAL_8N1, 16, 17);
  while (!EEPROM.begin(EEPROM_SIZE)) {
    true;
  }

  // readLastFixFromEEPROM();
  displayTest->drawString(64, 12, "Mounting SD");
  while (!SD.begin())
  {
    Serial.println("Card Mount Failed");
    delay(20);
  }

  {
    Serial.println("Card Mount Succeeded");
    displayTest->drawString(64, 24, "...success");

    writer = new CSVFileWriter;
    writer->setFileName();
    writer->writeHeader();
  }
  Serial.println("File initialised");

  // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);

  // PIN-Modes
  pinMode(PushButton, INPUT);

  while (gps.satellites.value() < 4)
  {
    readGPSData();
    delay(1000);
    Serial.println("Waiting for GPS fix... \n");
    //ToDo: clear line
    //displayTest->clearRectangle(64,36,64,12);
    displayTest->drawString(64, 36, "Wait for GPS");
    String satellitesString = String(gps.satellites.value()) + "satellites";
  }
  Serial.println("Got GPS Fix  \n");
  displayTest->drawString(64, 48, "Got GPS Fix");
  //heartRateBLEInit();
  Serial.println("Waiting a client connection to notify...");
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
    //sensor1->getMinDistance(minDistance);
    sensorManager->getDistances();


    //displayTest->showValue(minDistance);
    if (sensorManager->sensorValues[confirmationSensorID] < minDistanceToConfirm)
    {
      minDistanceToConfirm = sensorManager->sensorValues[confirmationSensorID];
      timeOfMinimum = millis();
    }
    displayTest->showValue(minDistanceToConfirm);
    //displayTest->showValue(gps.satellites.value());
    //displayTest2->showValue(minDistanceToConfirm);

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

  if (deviceConnected) {
    Serial.printf("*** NOTIFY: %d ***\n", buffer[1] );
    //heartRateBLENotify(buffer);
  }

  unsigned long ElapsedTime = CurrentTime - StartTime;
  StartTime = CurrentTime;
  Serial.write(" Time elapsed: ");
  Serial.print(ElapsedTime);
  Serial.write(" milliseconds\n");
}
