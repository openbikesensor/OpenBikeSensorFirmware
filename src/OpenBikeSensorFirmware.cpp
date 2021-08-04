/*
 * Copyright (C) 2019-2021 OpenBikeSensor Contributors
 * Contact: https://openbikesensor.org
 *
 * This file is part of the OpenBikeSensor firmware.
 *
 * The OpenBikeSensor firmware is free software: you can
 * redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * OpenBikeSensor firmware is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the OpenBikeSensor firmware.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include <utils/obsutils.h>
#include <utils/button.h>
#include "OpenBikeSensorFirmware.h"

#include "SPIFFS.h"

#ifndef BUILD_NUMBER
#define BUILD_NUMBER "local"
#endif
//#define DEVELOP

// --- Global variables ---
// Version only change the "vN.M" part if needed.
const char *OBSVersion = "v0.8" BUILD_NUMBER;

const uint8_t LEFT_SENSOR_ID = 1;
const uint8_t RIGHT_SENSOR_ID = 0;

const uint32_t LONG_BUTTON_PRESS_TIME_MS = 2000;


// PINs
const int PUSHBUTTON_PIN = 2;
const uint8_t GPS_POWER_PIN = 12;
const uint8_t BatterieVoltage_PIN = 34;

int confirmedMeasurements = 0;
int numButtonReleased = 0;
DataSet *datasetToConfirm = nullptr;

Button button(PUSHBUTTON_PIN);

Config config;

SSD1306DisplayDevice* displayTest;
HCSR04SensorManager* sensorManager;
static BluetoothManager* bluetoothManager;

Gps gps;

static const long BLUETOOTH_INTERVAL_MILLIS = 100;
static long lastBluetoothInterval = 0;

static const long DISPLAY_INTERVAL_MILLIS = 25;
static long lastDisplayInterval = 0;

float TemperatureValue = -1;


VoltageMeter* voltageMeter;

Adafruit_BMP280 bmp280(&Wire);
bool BMP280_active = false;

// --- Local variables ---
unsigned long measureInterval = 1000;
unsigned long timeOfMinimum = esp_timer_get_time(); //  millis();
unsigned long startTimeMillis = 0;
unsigned long currentTimeMillis = millis();

uint16_t minDistanceToConfirm = MAX_SENSOR_VALUE;
uint16_t minDistanceToConfirmIndex = 0;
bool transmitConfirmedData = false;

CircularBuffer<DataSet*, 10> dataBuffer;

FileWriter* writer;

const uint8_t displayAddress = 0x3c;

// Enable dev-mode. Allows to
// - set wifi config
// - prints more detailed log messages to serial (WIFI password)
//#define DEVELOP

int lastMeasurements = 0 ;

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

void setupSensors() {
  sensorManager = new HCSR04SensorManager;

  HCSR04SensorInfo sensorManaged1;
  sensorManaged1.triggerPin = (config.displayConfig & DisplaySwapSensors) ? 25 : 15;
  sensorManaged1.echoPin = (config.displayConfig & DisplaySwapSensors) ? 26 : 4;
  sensorManaged1.sensorLocation = (char*) "Right"; // TODO
  sensorManager->registerSensor(sensorManaged1, 0);

  HCSR04SensorInfo sensorManaged2;
  sensorManaged2.triggerPin = (config.displayConfig & DisplaySwapSensors) ? 15 : 25;
  sensorManaged2.echoPin = (config.displayConfig & DisplaySwapSensors) ? 4 : 26;
  sensorManaged2.sensorLocation = (char*) "Left"; // TODO
  sensorManager->registerSensor(sensorManaged2, 1);

  sensorManager->setOffsets(config.sensorOffsets);

  sensorManager->setPrimarySensor(LEFT_SENSOR_ID);
}

static void setupBluetooth(const ObsConfig &cfg, const String &trackUniqueIdentifier) {
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
}

static void reportBluetooth() {
  const uint32_t currentInterval = currentTimeMillis / BLUETOOTH_INTERVAL_MILLIS;
  if (bluetoothManager && lastBluetoothInterval != currentInterval) {
    log_d("Reporting BT: %d/%d cm",
          sensorManager->m_sensors[LEFT_SENSOR_ID].median->median(),
          sensorManager->m_sensors[RIGHT_SENSOR_ID].median->median());
    lastBluetoothInterval = currentInterval;
    bluetoothManager->newSensorValues(
      currentTimeMillis,
      sensorManager->m_sensors[LEFT_SENSOR_ID].median->median(),
      sensorManager->m_sensors[RIGHT_SENSOR_ID].median->median());
  }
}

static void buttonBluetooth(const DataSet *dataSet, uint16_t measureIndex) {
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
    if (left > MAX_DURATION_MICRO_SEC || left == 0) {
      left = MAX_SENSOR_VALUE;
    } else {
      left /= MICRO_SEC_TO_CM_DIVIDER;
    }
    if (right > MAX_DURATION_MICRO_SEC || right == 0) {
      right = MAX_SENSOR_VALUE;
    } else {
      right /= MICRO_SEC_TO_CM_DIVIDER;
    }
    bluetoothManager->newPassEvent(
      dataSet->millis + (uint32_t) dataSet->startOffsetMilliseconds[measureIndex],
      left, right);
  }
}

void setup() {
  Serial.begin(115200);

  log_i("openbikesensor.org - OBS/%s", OBSVersion);

  //##############################################################
  // Configure button pin as INPUT
  //##############################################################

  pinMode(PUSHBUTTON_PIN, INPUT);
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

  //##############################################################
  // Handle SD
  //##############################################################
  int8_t sdCount = 0;
  displayTest->showTextOnGrid(2, displayTest->newLine(), "SD...");
  while (!SD.begin()) {
    sdCount++;
    displayTest->showTextOnGrid(2,
      displayTest->currentLine(), "SD... error " + String(sdCount));
    if (config.simRaMode || button.read() == HIGH || sdCount > 10) {
      break;
    }
    delay(200);
  }

  if (SD.begin()) {
    displayTest->showTextOnGrid(2, displayTest->currentLine(), "SD... OK");
  }
  delay(333); // Added for user experience

  //##############################################################
  // Init HCSR04
  //##############################################################

  setupSensors();

  //##############################################################
  // Check, if the button is pressed
  // Enter configuration mode and enable OTA
  //##############################################################

  if (button.read() == HIGH || (!config.simRaMode && displayError != 0)) {
    displayTest->showTextOnGrid(2, displayTest->newLine(), "Start Server");
    ESP_ERROR_CHECK_WITHOUT_ABORT(
      esp_bt_mem_release(ESP_BT_MODE_BTDM)); // no bluetooth at all here.

    delay(200);
    startServer(&cfg);
    gps.begin();
    gps.setStatisticsIntervalInSeconds(2); // ??
    while (true) {
      yield();
      serverLoop();
    }
  }
  SPIFFS.end();
  WiFiGenericClass::mode(WIFI_OFF);
  gps.begin();

  //##############################################################
  // Prepare CSV file
  //##############################################################

  displayTest->showTextOnGrid(2, displayTest->newLine(), "CSV file...");
  const String trackUniqueIdentifier = ObsUtils::createTrackUuid();


  if (SD.begin()) {
    writer = new CSVFileWriter;
    writer->setFileName();
    writer->writeHeader(trackUniqueIdentifier);
    displayTest->showTextOnGrid(2, displayTest->currentLine(), "CSV file... OK");
  } else {
    displayTest->showTextOnGrid(2, displayTest->currentLine(), "CSV. skipped");
  }

  gps.handle();
  //##############################################################
  // Temperature Sensor BMP280
  //##############################################################

  BMP280_active = TemperatureValue = bmp280.begin(BMP280_ADDRESS_ALT,BMP280_CHIPID);
  if(BMP280_active == true) TemperatureValue = bmp280.readTemperature();


  gps.handle();

  setupBluetooth(cfg, trackUniqueIdentifier);

  displayTest->showTextOnGrid(2, displayTest->newLine(), "Wait for GPS");
  displayTest->newLine();
  gps.handle();
  gps.setStatisticsIntervalInSeconds(1); // get regular updates.

  while (!gps.hasFix(displayTest)) {
    currentTimeMillis = millis();
    gps.handle();
    sensorManager->pollDistancesAlternating();
    reportBluetooth();
    gps.showWaitStatus(displayTest);
    if (button.read() == HIGH
        || (config.simRaMode && !gps.moduleIsAlive()) // no module && simRaMode
      ) {
      log_d("Skipped get GPS...");
      displayTest->showTextOnGrid(2, displayTest->currentLine(), "...skipped");
      break;
    }
  }

  // now we have a fix only rate updates, could be set to 0?
  gps.setStatisticsIntervalInSeconds(0);

  gps.enableSbas();
  gps.handle(1100); // Added for user experience
  gps.pollStatistics();
  displayTest->clear();
}

void serverLoop() {
  gps.handle();
  configServerHandle();
  sensorManager->pollDistancesAlternating();
  handleButtonInServerMode();
}

void handleButtonInServerMode() {
  button.handle();
  if (!configServerWasConnectedViaHttp()) {
    if (button.gotPressed()) {
      displayTest->clearProgressBar(5);
      displayTest->showTextOnGrid(0, 3, "Press the button for");
      displayTest->showTextOnGrid(0, 4, "automatic track upload.");
    } else if (button.getPreviousStateMillis() > 0 && button.getState() == HIGH) {
      const uint32_t buttonPressedMs = button.getCurrentStateMillis();
      displayTest->drawProgressBar(5, buttonPressedMs, LONG_BUTTON_PRESS_TIME_MS);
      if (buttonPressedMs > LONG_BUTTON_PRESS_TIME_MS) {
        uploadTracks();
      }
    }
  }
}

void writeDataset(const uint8_t confirmationSensorID, DataSet *dataset) {
  if (dataset->confirmedDistances.empty()) {
    if (writer) {
      writer->append(*dataset);
    }
  }
  // write record as many times as we have confirmed values
  for (int i = 0; i < dataset->confirmedDistances.size(); i++) {
    // make sure the distance reported is the one that was confirmed
    dataset->sensorValues[confirmationSensorID] = dataset->confirmedDistances[i];
    dataset->confirmed = 1 + dataset->confirmedDistancesIndex[i];
    confirmedMeasurements++;
    if (writer) {
      writer->append(*dataset);
    }
  }
}

void loop() {
  log_i("loop()");

  auto* currentSet = new DataSet;
  //specify which sensors value can be confirmed by pressing the button, should be configurable
  const uint8_t confirmationSensorID = LEFT_SENSOR_ID;
  gps.handle(); // needs <=1ms

  currentTimeMillis = millis();
  if (startTimeMillis == 0) {
    startTimeMillis = (currentTimeMillis / measureInterval) * measureInterval;
  }
  currentSet->time = time(nullptr);
  currentSet->millis = currentTimeMillis;
  currentSet->batteryLevel = voltageMeter->read();
  currentSet->isInsidePrivacyArea = gps.isInsidePrivacyArea();
  currentSet->gpsRecord = gps.getCurrentGpsRecord();

  lastMeasurements = sensorManager->m_sensors[confirmationSensorID].numberOfTriggers;
  sensorManager->reset();

  // if the detected minimum was measured more than 5s ago, it is discarded and cannot be confirmed
  int timeDelta = (int) (currentTimeMillis - timeOfMinimum);
  if (datasetToConfirm != nullptr &&
      timeDelta > (config.confirmationTimeWindow * 1000)) {
    log_i("minimum %dcm unconfirmed - resetting", minDistanceToConfirm);
    minDistanceToConfirm = MAX_SENSOR_VALUE;
    datasetToConfirm = nullptr;
  }

  int loops = 0;
  // do this for the time specified by measureInterval, e.g. 1s
  while ((currentTimeMillis - startTimeMillis) < measureInterval) {
    loops++;

    currentTimeMillis = millis();
    button.handle(currentTimeMillis);
    if (sensorManager->pollDistancesAlternating()) {
      // if a new minimum on the selected sensor is detected, the value and the time of detection will be stored
      const uint16_t reading = sensorManager->sensorValues[confirmationSensorID];
      if (reading > 0 && reading < minDistanceToConfirm) {
        minDistanceToConfirm = reading;
        minDistanceToConfirmIndex = sensorManager->getCurrentMeasureIndex();
        // if there was no measurement of this sensor for this index, it is the
        // one before. This happens with fast confirmations.
        while (minDistanceToConfirmIndex > 0
               && sensorManager->m_sensors[confirmationSensorID].echoDurationMicroseconds[minDistanceToConfirmIndex] <= 0) {
            minDistanceToConfirmIndex--;
        }
        datasetToConfirm = currentSet;
        timeOfMinimum = currentTimeMillis;
      }
    }
    gps.handle();

    if (lastDisplayInterval != (currentTimeMillis / DISPLAY_INTERVAL_MILLIS)) {
      lastDisplayInterval = currentTimeMillis / DISPLAY_INTERVAL_MILLIS;
      displayTest->showValues(
        sensorManager->m_sensors[LEFT_SENSOR_ID],
        sensorManager->m_sensors[RIGHT_SENSOR_ID],
        minDistanceToConfirm,
        voltageMeter->readPercentage(),
        (int16_t) TemperatureValue,
        lastMeasurements,
        currentSet->isInsidePrivacyArea,
        gps.getSpeed(),
        gps.getValidSatellites()
      );
    }

    reportBluetooth();
    if (button.gotPressed()) { // after button was released, detect long press here
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
        datasetToConfirm->confirmedDistancesIndex.push_back(minDistanceToConfirmIndex);
        buttonBluetooth(datasetToConfirm, minDistanceToConfirmIndex);
        datasetToConfirm = nullptr;
      } else { // confirming a overtake without left measure
        currentSet->confirmedDistances.push_back(MAX_SENSOR_VALUE);
        currentSet->confirmedDistancesIndex.push_back(
          sensorManager->getCurrentMeasureIndex());
        buttonBluetooth(currentSet, sensorManager->getCurrentMeasureIndex());
      }
      minDistanceToConfirm = MAX_SENSOR_VALUE; // ready for next confirmation
    }

    if(BMP280_active == true)  TemperatureValue = bmp280.readTemperature();

    yield(); //
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
  log_i("min. distance: %dcm, loops %d",
        currentSet->sensorValues[confirmationSensorID],
        loops);
#endif

  dataBuffer.push(currentSet);
  // convert all data that does not wait for confirmation.
  while ((!dataBuffer.isEmpty() && dataBuffer.first() != datasetToConfirm) || dataBuffer.isFull()) {
    DataSet* dataset = dataBuffer.shift();
    writeDataset(confirmationSensorID, dataset);
    // if we are about to delete the to be confirmed dataset, take care for this.
    if (datasetToConfirm == dataset) {
      datasetToConfirm = nullptr;
      minDistanceToConfirm = MAX_SENSOR_VALUE;
    }
    delete dataset;
  }

  if (transmitConfirmedData) {
    // After confirmation make sure it will be written to SD card directly
    // so no confirmed sets might be lost
    if (writer) {
      writer->flush();
    }
    // there might be a confirmed value in the current set and also already a
    // new value to be confirmed flagged in the current set.
    // In this case the dataset is kept until we know if there are further
    // confirmed values in the set to be written.
    if (dataBuffer.isEmpty() || dataBuffer.first()->confirmedDistancesIndex.empty()) {
      log_i("Confirmed data flushed to sd.");
      transmitConfirmedData = false;
    }
    // back to normal display mode
    if (config.displayConfig & DisplayInvert) {
      displayTest->invert();
    } else {
      displayTest->normalDisplay();
    }
  }
  log_d("Time in loop: %lums %d inner loops, %d measures, %s , %d",
        currentTimeMillis - startTimeMillis, loops, lastMeasurements,
        TimeUtils::timeToString().c_str(), millis());
  // synchronize to full measureIntervals
  startTimeMillis = (currentTimeMillis / measureInterval) * measureInterval;
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
