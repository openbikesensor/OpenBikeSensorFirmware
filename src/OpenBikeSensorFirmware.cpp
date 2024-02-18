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
#include <utils/timeutils.h>
#include "OpenBikeSensorFirmware.h"

#include "SPIFFS.h"
#include "displays.h"
#include <rom/rtc.h>

#ifndef BUILD_NUMBER
#define BUILD_NUMBER "local"
#endif
//#define DEVELOP

// --- Global variables ---
// Version only change the "vN.M" part if needed.
const char *OBSVersion = "v0.19" BUILD_NUMBER;

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

DisplayDevice* obsDisplay;
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
unsigned long timeOfMinimum = millis();
unsigned long startTimeMillis = 0;
unsigned long currentTimeMillis = millis();

uint16_t minDistanceToConfirm = MAX_SENSOR_VALUE;
uint16_t minDistanceToConfirmIndex = 0;
bool transmitConfirmedData = false;

CircularBuffer<DataSet*, 10> dataBuffer;

FileWriter* writer;

// Enable dev-mode. Allows to
// - set wifi config
// - prints more detailed log messages to serial (WIFI password)
//#define DEVELOP

int lastMeasurements = 0 ;

uint8_t batteryPercentage();
void serverLoop();
void handleButtonInServerMode();
bool loadConfig(ObsConfig &cfg);
void copyCollectedSensorData(DataSet *set);

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
    obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "Bluetooth ..");
    bluetoothManager = new BluetoothManager;
    bluetoothManager->init(
      cfg.getProperty<String>(ObsConfig::PROPERTY_OBS_NAME),
      config.sensorOffsets[LEFT_SENSOR_ID],
      config.sensorOffsets[RIGHT_SENSOR_ID],
      batteryPercentage,
      trackUniqueIdentifier);
    bluetoothManager->activateBluetooth();
    obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), "Bluetooth up");
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
  obsDisplay = new DisplayDevice(DisplayDevice::detectVariant());

  obsDisplay->showLogo(true);
  obsDisplay->showTextOnGrid(2, obsDisplay->startLine(), OBSVersion);

  voltageMeter = new VoltageMeter; // takes a moment, so do it here
  if (voltageMeter->hasReadings()) {
    obsDisplay->showTextOnGrid(2, obsDisplay->newLine(),
                                "Battery: " + String(voltageMeter->read(), 1) + "V");
    delay(333); // Added for user experience
  }
  if (voltageMeter->isWarningLevel()) {
    obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "LOW BAT");
    obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "WARNING!");
    delay(5000);
  }

  //##############################################################
  // Load, print and save config
  //##############################################################
  ObsConfig cfg; // this one is valid in setup!
  bool configIsNew = loadConfig(cfg);

  //##############################################################
  // Handle SD
  //##############################################################
  int8_t sdCount = 0;
  obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "SD...");
  while (!SD.begin()) {
    sdCount++;
    obsDisplay->showTextOnGrid(2,
                               obsDisplay->currentLine(), "SD... error " + String(sdCount));
    if (button.read() == HIGH || sdCount > 10) {
      break;
    }
    delay(200);
  }

  if (SD.begin()) {
    obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), "SD... OK");
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
  bool triggerServerMode = false;
  if (Serial.peek() == 'I') {
    log_i("IMPROV char detected on serial, will start in server mode.");
    triggerServerMode = true;
  }
  if (configIsNew) {
    log_i("Config was freshly generated, will start in server mode.");
    triggerServerMode = true;
  }

  if (button.read() == HIGH || triggerServerMode) {
    obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "Start Server");
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
  obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "Start GPS...");
  gps.begin();
  if (gps.getValidMessageCount() > 0) {
    obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), "Start GPS OK");
  } else {
    obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), "Start GPS ??");
  }

  //##############################################################
  // Prepare CSV file
  //##############################################################

  obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "CSV file...");
  const String trackUniqueIdentifier = ObsUtils::createTrackUuid();


  if (SD.begin()) {
    writer = new CSVFileWriter;
    writer->setFileName();
    writer->writeHeader(trackUniqueIdentifier);
    obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), "CSV file... OK");
  } else {
    obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), "CSV. skipped");
  }

  gps.handle();
  //##############################################################
  // Temperature Sensor BMP280
  //##############################################################

  BMP280_active = TemperatureValue = bmp280.begin(BMP280_ADDRESS_ALT,BMP280_CHIPID);
  if(BMP280_active == true) TemperatureValue = bmp280.readTemperature();


  gps.handle();

  setupBluetooth(cfg, trackUniqueIdentifier);

  obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "Wait for GPS");
  obsDisplay->newLine();
  gps.handle();
  gps.setStatisticsIntervalInSeconds(1); // get regular updates.

  while (!gps.hasFix(obsDisplay)) {
    currentTimeMillis = millis();
    gps.handle();
    sensorManager->pollDistancesAlternating();
    reportBluetooth();
    gps.showWaitStatus(obsDisplay);
    if (button.read() == HIGH) {
      log_d("Skipped get GPS...");
      obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), "...skipped");
      break;
    }
  }

  // now we have a fix only rate updates, could be set to 0?
  gps.setStatisticsIntervalInSeconds(0);

  gps.enableSbas();
  gps.handle(1100); // Added for user experience
  gps.pollStatistics();
  obsDisplay->clear();
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
      obsDisplay->showTextOnGrid(0, 3, "Press the button for");
      obsDisplay->showTextOnGrid(0, 4, "automatic track upload.");
      obsDisplay->clearProgressBar(5);
    } else if (button.getPreviousStateMillis() > 0 && button.getState() == HIGH) {
      const uint32_t buttonPressedMs = button.getCurrentStateMillis();
      obsDisplay->drawProgressBar(5, buttonPressedMs, LONG_BUTTON_PRESS_TIME_MS);
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
  log_v("loop()");
  //specify which sensors value can be confirmed by pressing the button, should be configurable
  const uint8_t confirmationSensorID = LEFT_SENSOR_ID;
  auto* currentSet = new DataSet;
  uint32_t thisLoopTow;

  if (gps.hasTowTicks()) {
    /* only Timeinfo is reliably set until now! */
    GpsRecord incoming = gps.getIncomingGpsRecord();
    // GPS time is leading
    startTimeMillis = incoming.getCreatedAtMillisTicks();
    thisLoopTow = incoming.getTow();
    currentSet->time = TimeUtils::toTime(incoming.getWeek(), thisLoopTow / 1000);
  } else {
    startTimeMillis = (millis() / 1000) * 1000;
    currentSet->time = time(nullptr);
    thisLoopTow = 0;
  }
  currentSet->millis = startTimeMillis;
  currentSet->batteryLevel = voltageMeter->read();

  lastMeasurements = sensorManager->m_sensors[confirmationSensorID].numberOfTriggers;
  sensorManager->reset(startTimeMillis);

  // if the detected minimum was measured more than 5s ago, it is discarded and cannot be confirmed
  int timeDelta = (int) (currentTimeMillis - timeOfMinimum);
  if (datasetToConfirm != nullptr &&
      timeDelta > (config.confirmationTimeWindow * 1000)) {
    log_d("minimum %dcm unconfirmed - resetting", minDistanceToConfirm);
    minDistanceToConfirm = MAX_SENSOR_VALUE;
    datasetToConfirm = nullptr;
  }

  int loops = 0;
  // do loop until gps clock ticks
  while (gps.currentTowEquals(thisLoopTow)) {
    loops++;
    currentTimeMillis = millis();
    // No GPS Mode:
    if (thisLoopTow == 0 && ((startTimeMillis / 1000) != (currentTimeMillis / 1000)) ) {
      break;
    }
    button.handle(currentTimeMillis);
    gps.handle();
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
      obsDisplay->showValues(
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
    gps.handle();
    reportBluetooth();
    if (button.gotPressed()) { // after button was released, detect long press here
      // immediate user feedback - we start the action
      obsDisplay->highlight();

      transmitConfirmedData = true;
      numButtonReleased++;
      if (datasetToConfirm != nullptr) {
        datasetToConfirm->confirmedDistances.push_back(minDistanceToConfirm);
        datasetToConfirm->confirmedDistancesIndex.push_back(minDistanceToConfirmIndex);
        buttonBluetooth(datasetToConfirm, minDistanceToConfirmIndex);
        datasetToConfirm = nullptr;
      } else { // confirming an overtake without left measure
        currentSet->confirmedDistances.push_back(MAX_SENSOR_VALUE);
        currentSet->confirmedDistancesIndex.push_back(
          sensorManager->getCurrentMeasureIndex());
        buttonBluetooth(currentSet, sensorManager->getCurrentMeasureIndex());
      }
      minDistanceToConfirm = MAX_SENSOR_VALUE; // ready for next confirmation
    }

    if(BMP280_active == true)  TemperatureValue = bmp280.readTemperature();

    gps.handle();
  } // end measureInterval while

  currentSet->gpsRecord = gps.getCurrentGpsRecord();
  currentSet->isInsidePrivacyArea = gps.isInsidePrivacyArea();
  if (currentSet->gpsRecord.getTow() == thisLoopTow) {
    copyCollectedSensorData(currentSet);
    log_d("NEW SET: TOW: %u GPSms: %u, SETms: %u, GPS Time: %s, SET Time: %s, innerLoops: %d, buffer: %d, write time %3ums, loop time: %4ums, measurements: %2d",
          currentSet->gpsRecord.getTow(), currentSet->gpsRecord.getCreatedAtMillisTicks(),
          currentSet->millis,
          TimeUtils::dateTimeToString(TimeUtils::toTime(currentSet->gpsRecord.getWeek(), currentSet->gpsRecord.getTow() / 1000)).c_str(),
          TimeUtils::dateTimeToString(currentSet->time).c_str(),
          loops, dataBuffer.size(), writer->getWriteTimeMillis(),
          currentTimeMillis - startTimeMillis, lastMeasurements);
    dataBuffer.push(currentSet);
  } else {
    // If we lost time somewhere so GPS data is wrong!
    log_e("There is something wrong expected TOW: %d != gps tow: %d",
          thisLoopTow, currentSet->gpsRecord.getTow());
    if (datasetToConfirm == currentSet) {
      datasetToConfirm = nullptr;
      minDistanceToConfirm = MAX_SENSOR_VALUE;
    }
  }

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

  // After confirmation make sure it will be written to SD card directly
  if (transmitConfirmedData ||
    // also write if we are inside a privacy area ...
    (currentSet->isInsidePrivacyArea
    // ... and privacy mode does not require to write all sets
      && (config.privacyConfig & AbsolutePrivacy) || (config.privacyConfig & OverridePrivacy))) {
    // so no confirmed sets might be lost
    if (writer) {
      writer->flush();
    }
    // there might be a confirmed value in the current set and also already a
    // new value to be confirmed flagged in the current set.
    // In this case the dataset is kept until we know if there are further
    // confirmed values in the set to be written.
    if (dataBuffer.isEmpty() || dataBuffer.first()->confirmedDistancesIndex.empty()) {
      log_d("Confirmed data flushed to sd.");
      transmitConfirmedData = false;
    }
  }
  log_d("Time in loop: %lums %d inner loops, %d measures, %s, TOW: %d",
        currentTimeMillis - startTimeMillis, loops, lastMeasurements,
        TimeUtils::timeToString().c_str(), thisLoopTow);
}

void copyCollectedSensorData(DataSet *set) {// Write the minimum values of the while-loop to a set
  for (auto & m_sensor : sensorManager->m_sensors) {
    set->sensorValues.push_back(m_sensor.minDistance);
  }
  set->measurements = sensorManager->lastReadingCount;
  memcpy(&(set->readDurationsRightInMicroseconds),
         &(sensorManager->m_sensors[0].echoDurationMicroseconds), set->measurements * sizeof(int32_t));
  memcpy(&(set->readDurationsLeftInMicroseconds),
         &(sensorManager->m_sensors[1].echoDurationMicroseconds), set->measurements * sizeof(int32_t));
  memcpy(&(set->startOffsetMilliseconds),
         &(sensorManager->startOffsetMilliseconds), set->measurements * sizeof(uint16_t));
}

uint8_t batteryPercentage() {
  uint8_t result = 0;
  if (voltageMeter) {
    result = voltageMeter->readPercentage();
  }
  return result;
}

bool loadConfig(ObsConfig &cfg) {
  bool isNew = false;
  obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "Config... ");

  if (!SPIFFS.begin(true)) {
    log_e("An Error has occurred while mounting SPIFFS");
    obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), "Config... error ");
    obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "Please reboot!");
    while (true) {
      delay(1000);
    }
  }

  if (SD.begin() && SD.exists("/obs.json")) {
    obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), "Init from SD.");
    log_i("Configuration init from SD.");
    delay(1000);
    File f = SD.open("/obs.json");
    if (cfg.loadConfig(f)) {
      cfg.saveConfig();
      obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "done.");
    } else {
      obsDisplay->showTextOnGrid(2, obsDisplay->newLine(), "format error.");
    }
    f.close();
    SD.remove("/obs.json");
    delay(5000);
    isNew = true;
  } else {
    log_d("No configuration init from SD. SD: %d File: %d", SD.begin(), (SD.begin() && SD.exists("/obs.json")));
  }

  log_i("Load cfg");
  if (!cfg.loadConfig()) {
    obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), "Config...RESET");
    isNew = true;
    cfg.saveConfig(); // save the new config - next boot will allow obs mode
  }

  cfg.printConfig();

  // TODO: Select Profile missing here or below!
  // Copy data to static view on cfg!
  cfg.fill(config);

  // Setup display, this is not the right place ;)
  if (config.displayConfig & DisplayInvert) {
    obsDisplay->invert();
  } else {
    obsDisplay->normalDisplay();
  }
  if (config.displayConfig & DisplayFlip) {
    obsDisplay->flipScreen();
  }

  delay(333); // Added for user experience
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "<%02d| - |%02d>",
           config.sensorOffsets[LEFT_SENSOR_ID],
           config.sensorOffsets[RIGHT_SENSOR_ID]);
  obsDisplay->showTextOnGrid(2, obsDisplay->currentLine(), buffer);
  return isNew;
}
