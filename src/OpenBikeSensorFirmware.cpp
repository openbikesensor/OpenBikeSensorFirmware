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

#include <utils/obsutils.h>
#include "OpenBikeSensorFirmware.h"

#include "SPIFFS.h"

#ifndef BUILD_NUMBER
#define BUILD_NUMBER "local"
#endif
//#define DEVELOP

// --- Global variables ---
// Version only change the "vN.M" part if needed.
const char *OBSVersion = "v0.4" BUILD_NUMBER;

const uint8_t LEFT_SENSOR_ID = 1;
const uint8_t RIGHT_SENSOR_ID = 0;

const uint32_t LONG_BUTTON_PRESS_TIME_MS = 2000;


// PINs
const int PushButton_PIN = 2;
const uint8_t GPS_POWER_PIN = 12;
const uint8_t BatterieVoltage_PIN = 34;

int confirmedMeasurements = 0;
int numButtonReleased = 0;
DataSet *datasetToConfirm = nullptr;

int buttonState = 0;
uint32_t buttonStateChanged = millis();

Config config;

SSD1306DisplayDevice* displayTest;
HCSR04SensorManager* sensorManager;
BluetoothManager* bluetoothManager;
const long BLUETOOTH_INTERVAL_MILLIS = 200;
long lastBluetoothInterval = 0;

float BatteryValue = -1;
float TemperatureValue = -1;


VoltageMeter* voltageMeter;

String esp_chipid;

Adafruit_BMP280 bmp280(&Wire);
bool BMP280_active = false;

// --- Local variables ---
unsigned long measureInterval = 1000;
unsigned long timeOfMinimum = esp_timer_get_time(); //  millis();
unsigned long startTimeMillis = 0;
unsigned long currentTimeMillis = millis();

String text = "";
uint16_t minDistanceToConfirm = MAX_SENSOR_VALUE;
uint16_t minDistanceToConfirmIndex = 0;
bool transmitConfirmedData = false;
int lastButtonState = 0;

CircularBuffer<DataSet*, 10> dataBuffer;

FileWriter* writer;

const uint8_t displayAddress = 0x3c;

// Enable dev-mode. Allows to
// - set wifi config
// - prints more detailed log messages to serial (WIFI password)
//#define DEVELOP

int lastMeasurements = 0 ;

void bluetoothConfirmed(const DataSet *dataSet, uint16_t measureIndex);
uint8_t batteryPercentage();
void serverLoop();
void handleButtonInServerMode();

// The BMP280 can keep up to 3.4MHz I2C speed, so no need for an individual slower speed
void switch_wire_speed_to_VL53(){
	Wire.setClock(400000);
}
void switch_wire_speed_to_SSD1306(){
	Wire.setClock(500000);
}

void setup() {
  Serial.begin(115200);

  // Serial.println("setup()");

  //##############################################################
  // Configure button pin as INPUT
  //##############################################################

  pinMode(PushButton_PIN, INPUT);
  pinMode(BatterieVoltage_PIN, INPUT);
  pinMode(GPS_POWER_PIN, OUTPUT);
  digitalWrite(GPS_POWER_PIN,HIGH);

  //##############################################################
  // Setup display
  //##############################################################
  Wire.begin();
  Wire.beginTransmission(displayAddress);
  byte displayError = Wire.endTransmission();
  if (displayError != 0) {
    Serial.println("Display not found");
  }
  displayTest = new SSD1306DisplayDevice;

  switch_wire_speed_to_SSD1306();

  displayTest->showLogo(true);
  displayTest->showTextOnGrid(2, 0, OBSVersion,DEFAULT_FONT);

  //##############################################################
  // Load, print and save config
  //##############################################################

  displayTest->showTextOnGrid(2, 1, "Config... ",DEFAULT_FONT);

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    displayTest->showTextOnGrid(2, 1, "Config... error ",DEFAULT_FONT);
    return;
  }

  Serial.println(F("Load config"));
  ObsConfig cfg; // this one is valid in setup!
  if (!cfg.loadConfig()) {
    displayTest->showTextOnGrid(2, 1, "Config...RESET",DEFAULT_FONT);
    delay(1000); // resetting config once, wait a moment
  }

  // Dump config file
  log_d("Print config file...");
  cfg.printConfig();

  // TODO: Select Profile missing here or below!
  // Copy data to static view on config!
  cfg.fill(config);

  if(config.devConfig & ShowGrid) {
    displayTest->showGrid(true);
  }
  if (config.displayConfig & DisplayInvert) {
    displayTest->invert();
  } else {
    displayTest->normalDisplay();
  }

  if (config.displayConfig & DisplayFlip) {
    displayTest->flipScreen();
  }

  delay(333); // Added for user experience
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "<%02d| |%02d>",
    config.sensorOffsets[LEFT_SENSOR_ID],
    config.sensorOffsets[RIGHT_SENSOR_ID]);
  displayTest->showTextOnGrid(2, 1, buffer,DEFAULT_FONT);


  //##############################################################
  // GPS
  //##############################################################
  SerialGPS.begin(9600, SERIAL_8N1, 16, 17);

  //##############################################################
  // Handle SD
  //##############################################################
  // Counter, how often the SD card will be read before writing an error on the display
  int8_t sdCount = 5;

  displayTest->showTextOnGrid(2, 2, "SD...",DEFAULT_FONT);
  while (!SD.begin()) {
    if(sdCount > 0) {
      sdCount--;
    } else {
      displayTest->showTextOnGrid(2, 2, "SD... error",DEFAULT_FONT);
      if (config.simRaMode || digitalRead(PushButton_PIN) == HIGH) {
        break;
      }  // FIXME: Stop trying!!?
    }
    Serial.println("Card Mount Failed");
    //delay(100);
  }
  delay(333); // Added for user experience
  if (SD.begin()) {
    Serial.println("Card Mount Succeeded");
    displayTest->showTextOnGrid(2, 2, "SD... ok",DEFAULT_FONT);
  }

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
  // Check, if the button is pressed
  // Enter configuration mode and enable OTA
  //##############################################################

  buttonState = digitalRead(PushButton_PIN);
  if (buttonState == HIGH || (!config.simRaMode && displayError != 0)) {
    displayTest->showTextOnGrid(2, 2, "Start Server",DEFAULT_FONT);
    ESP_ERROR_CHECK_WITHOUT_ABORT(
      esp_bt_mem_release(ESP_BT_MODE_BTDM)); // no bluetooth at all here.

    buttonStateChanged = 0;
    lastButtonState = buttonState;
    delay(200);
    startServer(&cfg);
    OtaInit(esp_chipid);
    while (true) {
      yield();
      serverLoop();
    }
  }
  SPIFFS.end();
  WiFiGenericClass::mode(WIFI_OFF);

  //##############################################################
  // Prepare CSV file
  //##############################################################

  displayTest->showTextOnGrid(2, 3, "CSV file...",DEFAULT_FONT);

  const String trackUniqueIdentifier = ObsUtils::createTrackUuid();

  if (SD.begin()) {
    writer = new CSVFileWriter;
    writer->setFileName();
    writer->writeHeader(trackUniqueIdentifier);
    displayTest->showTextOnGrid(2, 3, "CSV file... ok",DEFAULT_FONT);
    Serial.println("File initialised");
  } else {
    displayTest->showTextOnGrid(2, 3, "CSV. skipped",DEFAULT_FONT);
  }

  //##############################################################
  // GPS
  //##############################################################

  displayTest->showTextOnGrid(2, 4, "Wait for GPS",DEFAULT_FONT);
  Serial.println("Waiting for GPS fix...");
  bool validGPSData = false;
  readGPSData();
  voltageMeter = new VoltageMeter; // takes a moment, so do it here
  readGPSData();

  //##############################################################
  // Temperatur Sensor BMP280
  //##############################################################

  BMP280_active = TemperatureValue = bmp280.begin(BMP280_ADDRESS_ALT,BMP280_CHIPID);
  if(BMP280_active == true) TemperatureValue = bmp280.readTemperature();


  //##############################################################
  // Bluetooth
  //##############################################################
  if (cfg.getProperty<bool>(ObsConfig::PROPERTY_BLUETOOTH)) {
    bluetoothManager = new BluetoothManager;
    bluetoothManager->init(
      cfg.getProperty<String>(ObsConfig::PROPERTY_OBS_NAME),
      config.sensorOffsets[LEFT_SENSOR_ID],
      config.sensorOffsets[RIGHT_SENSOR_ID],
      batteryPercentage,
      trackUniqueIdentifier);
    bluetoothManager->activateBluetooth();
  } else {
    bluetoothManager = nullptr;
    ESP_ERROR_CHECK_WITHOUT_ABORT(
      esp_bt_mem_release(ESP_BT_MODE_BTDM)); // no bluetooth at all here.
  }
  readGPSData();
  int gpsWaitFor = cfg.getProperty<int>(ObsConfig::PROPERTY_GPS_FIX);
  bool first_gps_run = true;
  while (!validGPSData) {
    readGPSData();

    switch (gpsWaitFor) {
      case GPS::FIX_POS:
        validGPSData = gps.sentencesWithFix() > 0;
        if (validGPSData) {
          Serial.println("Got location...");
          displayTest->showTextOnGrid(2, 4, "Got location",DEFAULT_FONT);
          }
        break;
      case GPS::FIX_TIME:
        validGPSData = gps.time.isValid()
          && !(gps.time.second() == 00 && gps.time.minute() == 00 && gps.time.hour() == 00);
        if (validGPSData) {
          Serial.println("Got time...");
          displayTest->showTextOnGrid(2, 4, "Got time",DEFAULT_FONT);
         }
        break;
      case GPS::FIX_NO_WAIT:
        validGPSData = true;
        if (validGPSData) {
          Serial.println("GPS, no wait");
          displayTest->showTextOnGrid(2, 4, "GPS, no wait",DEFAULT_FONT);
          }
        break;
      default:
        validGPSData = gps.satellites.value() >= gpsWaitFor;
        if (validGPSData) {
          Serial.println("Got required number of satellites...");
        }
        break;
    }

    currentTimeMillis = millis();
    if (bluetoothManager
        && lastBluetoothInterval != (currentTimeMillis / BLUETOOTH_INTERVAL_MILLIS)) {
      lastBluetoothInterval = currentTimeMillis / BLUETOOTH_INTERVAL_MILLIS;
      bluetoothManager->newSensorValues(currentTimeMillis, MAX_SENSOR_VALUE, MAX_SENSOR_VALUE);
    }

    delay(50);

    String satellitesString[2];
    if (gps.passedChecksum() == 0) { // could not get any valid char from GPS module
      satellitesString[0] = "OFF?";
    } else if (!gps.time.isValid()
                || (gps.time.second() == 00 && gps.time.minute() == 00 && gps.time.hour() == 00)) {
      satellitesString[0] = "no time";
    } else {
      first_gps_run = false;
      char timeStr[32];
      snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
               gps.time.hour(), gps.time.minute(), gps.time.second());
      satellitesString[0] = String(timeStr);
      satellitesString[1] = String(gps.satellites.value()) + " / " + String(gpsWaitFor) + " sats";
    }

    if(gps.passedChecksum() != 0    //only do this if a communication is there and a valid time is there
        && gps.time.isValid()
        && !(gps.time.second() == 00 && gps.time.minute() == 00 && gps.time.hour() == 00)){

      if(first_gps_run == true){  //This should implement scrolling and only scroll up on the first time
        for(uint8_t i = 0; i<4;i++){
          displayTest->showTextOnGrid(2, i, displayTest->get_gridTextofCell(2,i+1),DEFAULT_FONT);
        }
      }
      displayTest->showTextOnGrid(2, 4, satellitesString[0],DEFAULT_FONT);
      displayTest->showTextOnGrid(2, 5, satellitesString[1],DEFAULT_FONT);
    }else    displayTest->showTextOnGrid(2, 5, satellitesString[0],DEFAULT_FONT);   //if no gps comm or no time is there, just write in the last row

    buttonState = digitalRead(PushButton_PIN);
    if (buttonState == HIGH
      || (config.simRaMode && gps.passedChecksum() == 0) // no module && simRaMode
    ) {
      Serial.println("Skipped get GPS...");
      displayTest->showTextOnGrid(2, 5, "...skipped",DEFAULT_FONT);
      break;
    }
  }

  delay(1000); // Added for user experience

  // Clear the display once!
  displayTest->clear();
}

void serverLoop() {
  readGPSData();
  server.handleClient();
  ArduinoOTA.handle();
  sensorManager->getDistancesNoWait();
  handleButtonInServerMode();
}

void handleButtonInServerMode() {
  buttonState = digitalRead(PushButton_PIN);
  const uint32_t now = millis();
  if (buttonState != lastButtonState) {
    if (buttonState == LOW && !configServerWasConnectedViaHttp()) {
      displayTest->clearProgressBar(5);
      displayTest->showTextOnGrid(0, 3, "Press the button for");
      displayTest->showTextOnGrid(0, 4, "automatic track upload.");
    }
    lastButtonState = buttonState;
    buttonStateChanged = now;
  }
  if (!configServerWasConnectedViaHttp() &&
      buttonState == HIGH && buttonStateChanged != 0) {
    const uint32_t buttonPressedMs = now - buttonStateChanged;
    displayTest->drawProgressBar(5, buttonPressedMs, LONG_BUTTON_PRESS_TIME_MS);
    if (buttonPressedMs > LONG_BUTTON_PRESS_TIME_MS) {
      uploadTracks(false);
    }
  }
}


void loop() {

  Serial.println("loop()");

  auto* currentSet = new DataSet;
  //specify which sensors value can be confirmed by pressing the button, should be configurable
  const uint8_t confirmationSensorID = LEFT_SENSOR_ID;
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
    readGPSData();

    displayTest->showValues(
      sensorManager->m_sensors[LEFT_SENSOR_ID],
      sensorManager->m_sensors[RIGHT_SENSOR_ID],
      minDistanceToConfirm,
      BatteryValue,
      (int16_t) TemperatureValue,
      lastMeasurements,
      currentSet->isInsidePrivacyArea
    );


    if (bluetoothManager
        && lastBluetoothInterval != (currentTimeMillis / BLUETOOTH_INTERVAL_MILLIS)) {
      log_d("Reporting BT: %d/%d Button: %d\n",
                    sensorManager->m_sensors[LEFT_SENSOR_ID].median->median(),
                    sensorManager->m_sensors[RIGHT_SENSOR_ID].median->median(),
                    buttonState);
      lastBluetoothInterval = currentTimeMillis / BLUETOOTH_INTERVAL_MILLIS;
      bluetoothManager->newSensorValues(
        currentTimeMillis,
        sensorManager->m_sensors[LEFT_SENSOR_ID].median->median(),
        sensorManager->m_sensors[RIGHT_SENSOR_ID].median->median());
    }

    buttonState = digitalRead(PushButton_PIN);
    // detect state change
    if (buttonState != lastButtonState) {
      if (buttonState == LOW) { // after button was released, detect long press here
        // immediate user feedback - we start the action
        // invert state might be a bit long - it does not block next confirmation.
        if (config.displayConfig & DisplayInvert) {
          displayTest->normalDisplay();
        } else {
          displayTest->invert();
        }

        transmitConfirmedData = true;
        numButtonReleased++;
        if (datasetToConfirm != nullptr) {
          datasetToConfirm->confirmedDistances.push_back(minDistanceToConfirm);
          datasetToConfirm->confirmedDistancesTimeOffset.push_back(minDistanceToConfirmIndex);
          bluetoothConfirmed(datasetToConfirm, minDistanceToConfirmIndex);
          datasetToConfirm = nullptr;
        } else { // confirming a overtake without left measure
          currentSet->confirmedDistances.push_back(MAX_SENSOR_VALUE);
          currentSet->confirmedDistancesTimeOffset.push_back(
            sensorManager->getCurrentMeasureIndex());
          bluetoothConfirmed(currentSet, sensorManager->getCurrentMeasureIndex());
        }
        minDistanceToConfirm = MAX_SENSOR_VALUE; // ready for next confirmation
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
      timeOfMinimum = currentTimeMillis;
    }
    measurements++;

      // #######################################################
      // Batterievoltage
      // #######################################################

      if (voltageBuffer.available() == 0)
      {
        BatteryValue = (float) movingaverage(&voltageBuffer,&BatterieVoltage_movav,batterie_voltage_read(BatterieVoltage_PIN));
        BatteryValue = (float)get_batterie_percent((uint16_t)BatteryValue);
        currentSet->batteryLevel = BatteryValue;
        }else{
        (float) movingaverage(&voltageBuffer,&BatterieVoltage_movav,batterie_voltage_read(BatterieVoltage_PIN));
        BatteryValue = -1;
      }

       if(BMP280_active == true)  TemperatureValue = bmp280.readTemperature();
  } // end measureInterval while

  // Write the minimum values of the while-loop to a set
  for (auto & m_sensor : sensorManager->m_sensors) {
    currentSet->sensorValues.push_back(m_sensor.minDistance);
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
      writer->append(*currentSet);
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
      if (dataset->confirmedDistances.empty()) {
        if (writer) {
          writer->append(*dataset);
        }
      }
      // write record as many times as we have confirmed values
      for (int i = 0; i < dataset->confirmedDistances.size(); i++) {
        // make sure the distance reported is the one that was confirmed
        dataset->sensorValues[confirmationSensorID] = dataset->confirmedDistances[i];
        dataset->confirmed = dataset->confirmedDistancesTimeOffset[i];
        confirmedMeasurements++;
        if (writer) {
          writer->append(*dataset);
        }
      }
      delete dataset;
    }
    if (writer) {  // "flush"
      writer->flush();
    }
    Serial.printf(">>> flush - reset <<<");
    transmitConfirmedData = false;
    // back to normal display mode
    if (config.displayConfig & DisplayInvert) {
      displayTest->invert();
    } else {
      displayTest->normalDisplay();
    }
  }

  // If the circular buffer is full, write just one set to the writers buffer,
  if (dataBuffer.isFull()) { // TODO: Same code as above
    DataSet* dataset = dataBuffer.shift();
    Serial.printf("data buffer full, writing set to file buffer\n");
    if (writer) {
      writer->append(*dataset);
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

void bluetoothConfirmed(const DataSet *dataSet, uint16_t measureIndex) {
  if (bluetoothManager) {
    uint16_t left = dataSet->readDurationsLeftInMicroseconds[measureIndex];
    if (left >= MAX_DURATION_MICRO_SEC && measureIndex > 0) {
      measureIndex--;
      left = dataSet->readDurationsLeftInMicroseconds[measureIndex];
    }
    uint16_t right = dataSet->readDurationsRightInMicroseconds[measureIndex];
    if (right >= MAX_DURATION_MICRO_SEC && measureIndex > 0) {
      right = dataSet->readDurationsLeftInMicroseconds[measureIndex - 1];
    }
    if (left > MAX_DURATION_MICRO_SEC || left <= 0) {
      left = MAX_SENSOR_VALUE;
    } else {
      left /= MICRO_SEC_TO_CM_DIVIDER;
    }
    if (right > MAX_DURATION_MICRO_SEC || right <= 0) {
      right = MAX_SENSOR_VALUE;
    } else {
      right /= MICRO_SEC_TO_CM_DIVIDER;
    }
    bluetoothManager->newPassEvent(
      dataSet->millis + (uint32_t) dataSet->startOffsetMilliseconds[measureIndex],
      left, right);
  }
}

uint8_t batteryPercentage() {
  uint8_t result = 0;
  if (voltageMeter) {
    result = voltageMeter->readPercentage();
  }
  return result;
}
