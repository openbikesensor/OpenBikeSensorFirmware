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

Gps gps;

const long BLUETOOTH_INTERVAL_MILLIS = 250;
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
void loadConfig(ObsConfig &cfg);

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
  displayTest->showTextOnGrid(2, displayTest->startLine(), OBSVersion);

  voltageMeter = new VoltageMeter; // takes a moment, so do it here
  if (voltageMeter->hasReadings()) {
    displayTest->showTextOnGrid(2, displayTest->newLine(),
                                "Battery: " + String(voltageMeter->read(), 1) + "V");
    delay(333); // Added for user experience
  }
  if (voltageMeter->isWarningLevel()) {
    displayTest->showTextOnGrid(2, displayTest->newLine(), "LOW BAT");
    displayTest->showTextOnGrid(2, displayTest->newLine(), "WARNING!");
    delay(5000);
  }

  //##############################################################
  // Load, print and save config
  //##############################################################
  ObsConfig cfg; // this one is valid in setup!
  loadConfig(cfg);

  gps.begin();

  //##############################################################
  // Handle SD
  //##############################################################
  int8_t sdCount = 0;
  displayTest->showTextOnGrid(2, displayTest->newLine(), "SD...");
  while (!SD.begin()) {
    sdCount++;
    displayTest->showTextOnGrid(2,
      displayTest->currentLine(), "SD... error " + String(sdCount));
    if (config.simRaMode || digitalRead(PushButton_PIN) == HIGH || sdCount > 10) {
      break;
    }
    delay(200);
  }

  if (SD.begin()) {
    displayTest->showTextOnGrid(2, displayTest->currentLine(), "SD... ok");
  }
  delay(333); // Added for user experience

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
    displayTest->showTextOnGrid(2, displayTest->newLine(), "Start Server");
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

  displayTest->showTextOnGrid(2, displayTest->newLine(), "CSV file...");

  const String trackUniqueIdentifier = ObsUtils::createTrackUuid();

  if (SD.begin()) {
    writer = new CSVFileWriter;
    writer->setFileName();
    writer->writeHeader(trackUniqueIdentifier);
    displayTest->showTextOnGrid(2, displayTest->currentLine(), "CSV file... ok");
  } else {
    displayTest->showTextOnGrid(2, displayTest->currentLine(), "CSV. skipped");
  }

  //##############################################################
  // Temperatur Sensor BMP280
  //##############################################################

  BMP280_active = TemperatureValue = bmp280.begin(BMP280_ADDRESS_ALT,BMP280_CHIPID);
  if(BMP280_active == true) TemperatureValue = bmp280.readTemperature();


  //##############################################################
  // Bluetooth
  //##############################################################
  if (cfg.getProperty<bool>(ObsConfig::PROPERTY_BLUETOOTH)) {
    displayTest->showTextOnGrid(2, displayTest->newLine(), "Bluetooth ..");
    bluetoothManager = new BluetoothManager;
    bluetoothManager->init(
      cfg.getProperty<String>(ObsConfig::PROPERTY_OBS_NAME),
      config.sensorOffsets[LEFT_SENSOR_ID],
      config.sensorOffsets[RIGHT_SENSOR_ID],
      batteryPercentage,
      trackUniqueIdentifier);
    bluetoothManager->activateBluetooth();
    displayTest->showTextOnGrid(2, displayTest->currentLine(), "Bluetooth up");
  } else {
    bluetoothManager = nullptr;
    ESP_ERROR_CHECK_WITHOUT_ABORT(
      esp_bt_mem_release(ESP_BT_MODE_BTDM)); // no bluetooth at all here.
  }

  displayTest->showTextOnGrid(2, displayTest->newLine(), "Wait for GPS");
  displayTest->newLine();
  gps.handle();
  int gpsWaitFor = cfg.getProperty<int>(ObsConfig::PROPERTY_GPS_FIX);
  while (!gps.hasState(gpsWaitFor, displayTest)) {
    currentTimeMillis = millis();
    gps.handle();
    if (bluetoothManager && bluetoothManager->hasConnectedClients()
        && lastBluetoothInterval != (currentTimeMillis / BLUETOOTH_INTERVAL_MILLIS)) {
      lastBluetoothInterval = currentTimeMillis / BLUETOOTH_INTERVAL_MILLIS;
      bluetoothManager->newSensorValues(currentTimeMillis, MAX_SENSOR_VALUE, MAX_SENSOR_VALUE);
    }
    delay(50);
    gps.showWaitStatus(displayTest);

    buttonState = digitalRead(PushButton_PIN);
    if (buttonState == HIGH
        || (config.simRaMode && !gps.moduleIsAlive()) // no module && simRaMode
      ) {
      log_d("Skipped get GPS...");
      displayTest->showTextOnGrid(2, displayTest->currentLine(), "...skipped");
      break;
    }
  }

  delay(1000); // Added for user experience

  displayTest->clear();
}


void serverLoop() {
  gps.handle();
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
  gps.handle(); // needs <=1ms

  currentTimeMillis = millis();
  if (startTimeMillis == 0) {
    startTimeMillis = (currentTimeMillis / measureInterval) * measureInterval;
  }
  currentSet->time = gps.currentTime();
  currentSet->millis = currentTimeMillis;
  currentSet->location = gps.location;
  currentSet->altitude = gps.altitude;
  currentSet->course = gps.course;
  currentSet->speed = gps.speed;
  currentSet->hdop = gps.hdop;
  currentSet->validSatellites = gps.getValidSatellites();
  currentSet->batteryLevel = voltageMeter->read();
  currentSet->isInsidePrivacyArea = gps.isInsidePrivacyArea();

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
    gps.handle();

    displayTest->showValues(
      sensorManager->m_sensors[LEFT_SENSOR_ID],
      sensorManager->m_sensors[RIGHT_SENSOR_ID],
      minDistanceToConfirm,
      BatteryValue,
      (int16_t) TemperatureValue,
      lastMeasurements,
      currentSet->isInsidePrivacyArea,
      gps.getSpeed(),
      gps.getValidSatellites()
    );


    if (bluetoothManager && bluetoothManager->hasConnectedClients()
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

#ifdef DEVELOP
  Serial.write("min. distance: ");
  Serial.print(currentSet->sensorValues[confirmationSensorID]) ;
  Serial.write(" cm,");
  Serial.print(measurements);
  Serial.write(" measurements  \n");
#endif

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
  if (bluetoothManager && bluetoothManager->hasConnectedClients()) {
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

void loadConfig(ObsConfig &cfg) {
  displayTest->showTextOnGrid(2, displayTest->newLine(), "Config... ");

  if (!SPIFFS.begin(true)) {
    log_e("An Error has occurred while mounting SPIFFS");
    displayTest->showTextOnGrid(2, displayTest->currentLine(), "Config... error ");
    displayTest->showTextOnGrid(2, displayTest->newLine(), "Please reboot!");
    while (true) {
      delay(1000);
    }
  }

  if (SD.begin() && SD.exists("/obs.json")) {
    displayTest->showTextOnGrid(2, displayTest->currentLine(), "Init from SD.");
    log_i("Configuration init from SD.");
    delay(1000);
    File f = SD.open("/obs.json");
    if (cfg.loadConfig(f)) {
      cfg.saveConfig();
      displayTest->showTextOnGrid(2, displayTest->newLine(), "done.");
    } else {
      displayTest->showTextOnGrid(2, displayTest->newLine(), "format error.");
    }
    f.close();
    SD.remove("/obs.json");
    delay(5000);
  } else {
    log_d("No configuration init from SD. SD: %d File: %d", SD.begin(), (SD.begin() && SD.exists("/obs.json")));
  }

  log_i("Load cfg");
  if (!cfg.loadConfig()) {
    displayTest->showTextOnGrid(2, displayTest->currentLine(), "Config...RESET");
    delay(5000); // resetting cfg once, wait a moment
  }

  cfg.printConfig();

  // TODO: Select Profile missing here or below!
  // Copy data to static view on cfg!
  cfg.fill(config);

  // Setup display, this is not the right place ;)
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
  snprintf(buffer, sizeof(buffer), "<%02d| - |%02d>",
           config.sensorOffsets[LEFT_SENSOR_ID],
           config.sensorOffsets[RIGHT_SENSOR_ID]);
  displayTest->showTextOnGrid(2, displayTest->currentLine(), buffer);
}
