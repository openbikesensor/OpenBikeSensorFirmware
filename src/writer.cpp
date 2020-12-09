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

#include "writer.h"

const String GPXFileWriter::EXTENSION = ".gpx";
const String CSVFileWriter::EXTENSION = ".obsdata.csv";

void FileWriter::listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void FileWriter::createDir(fs::FS &fs, const char * path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void FileWriter::removeDir(fs::FS &fs, const char * path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void FileWriter::readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void FileWriter::writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void FileWriter::appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

bool FileWriter::renameFile(fs::FS &fs, const char * path1, const char * path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  return fs.rename(path1, path2);
}

void FileWriter::deleteFile(fs::FS &fs, const char * path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

int FileWriter::getTrackNumber() const {
  File numberFile = SD.open("/tracknumber.txt","r");
  int trackNumber = numberFile.readString().toInt();
  numberFile.close();
  return trackNumber;
}

void FileWriter::storeTrackNumber(int trackNumber) const {
  File numberFile = SD.open("/tracknumber.txt","w");
  numberFile.println(trackNumber);
  numberFile.close();
}

void FileWriter::setFileName() {
  int number = getTrackNumber();
  String base_filename = "/sensorData";
  mFileName = base_filename + String(number) + mFileExtension;
  while (SD.exists(mFileName)) {
    number++;
    mFileName = base_filename + String(number) + mFileExtension;
  }
  storeTrackNumber(number);
}

void FileWriter::correctFilename() {
  tm startTm;
  auto start = time(nullptr);
  unsigned long passedSeconds = (millis() - mStartedMillis) / 1000L;
  start -= passedSeconds;
  localtime_r(&start, &startTm);
  if (startTm.tm_year + 1900 > 2019) {
    char name[32];
    snprintf(name, sizeof (name), "/%04d-%02d-%02dT%02d.%02d.%02d-%4x",
             startTm.tm_year + 1900, startTm.tm_mon + 1, startTm.tm_mday,
             startTm.tm_hour, startTm.tm_min, startTm.tm_sec,
             (uint16_t)(ESP.getEfuseMac() >> 32));
    const String newName = name + mFileExtension;
    if(!SD.exists(mFileName.c_str()) || // already written
      renameFile(SD, mFileName.c_str(), newName.c_str())) {
      mFileName = newName;
      mFinalFileName = true;
    }
  }
}

uint16_t FileWriter::getBufferLength() const {
  return mBuffer.length();
}

void FileWriter::writeDataToSD() {
  const auto start = millis();
  appendFile(SD, mFileName.c_str(), mBuffer.c_str() );
  mBuffer.clear();
  mWriteTimeMillis = millis() - start;
  if (!mFinalFileName) {
    correctFilename();
  }
}

void FileWriter::writeDataBuffered(DataSet* set) {
  if (getBufferLength() > 10000 && !(digitalRead(PushButton))) {
#ifdef DEVELOP
    Serial.printf("File buffer full, writing to file\n");
#endif
    writeDataToSD();
  }

  if (getBufferLength() < 11000) { // do not add data if our buffer is full already. We loose data here!
    writeData(set);
  } else {
    // ALERT!! - display?
#ifdef DEVELOP
    Serial.printf("File buffer overflow, not allowed to write - will skip\n");
#endif
  }
}

String FileWriter::getFileName() {
  return mFileName;
}

unsigned long FileWriter::getWriteTimeMillis() {
  return mWriteTimeMillis;
}

void CSVFileWriter::writeHeader() {
  String headerString;
  headerString += "OBSDataFormat=2&";
  headerString += "OBSFirmwareVersion=" + String(OBSVersion) + "&";
  headerString += "DeviceId=" + String((uint16_t)(ESP.getEfuseMac() >> 32), 16) + "&";
  headerString += "DataPerMeasurement=3&";
  headerString += "MaximumMeasurementsPerLine=" + String(MAX_NUMBER_MEASUREMENTS_PER_INTERVAL) + "&";
  headerString += "OffsetLeft=" + String(config.sensorOffsets[LEFT_SENSOR_ID]) + "&";
  headerString += "OffsetRight=" + String(config.sensorOffsets[RIGHT_SENSOR_ID]) + "&";
  headerString += "NumberOfDefinedPrivacyAreas=" + String((int) config.privacyAreas.size()) + "&";
  headerString += "PrivacyLevelApplied=";

  String privacyString;
  if (config.privacyConfig & AbsolutePrivacy) {
    privacyString += "AbsolutePrivacy";
  }
  if (config.privacyConfig & NoPosition) {
    if (!privacyString.isEmpty()) {
      privacyString += "|";
    }
    privacyString += "NoPosition";
  }
  if (config.privacyConfig & NoPrivacy) {
    if (!privacyString.isEmpty()) {
      privacyString += "|";
    }
    privacyString += "NoPrivacy";
  }
  if (config.privacyConfig & OverridePrivacy) {
    if (!privacyString.isEmpty()) {
      privacyString += "|";
    }
    headerString += "OverridePrivacy";
  }
  if (privacyString.isEmpty()) {
    privacyString += "NoPrivacy";
  }
  headerString += privacyString + "&";
  headerString += "MaximumValidFlightTimeMicroseconds=" + String(MAX_DURATION_MICRO_SEC) + "&";
  headerString += "BluetoothEnabled=" + String(config.bluetooth) + "&";
  headerString += "PresetId=default&";
  headerString += "DistanceSensorsUsed=HC-SR04/JSN-SR04T\n";

  headerString += "Date;Time;Millis;Comment;Latitude;Longitude;Altitude;"
    "Course;Speed;HDOP;Satellites;BatteryLevel;Left;Right;Confirmed;Marked;Invalid;"
    "InsidePrivacyArea;Factor;Measurements";
  for (uint16_t idx = 1; idx <= MAX_NUMBER_MEASUREMENTS_PER_INTERVAL; ++idx) {
    String number = String(idx);
    headerString += ";Tms" + number;
    headerString += ";Lus" + number;
    headerString += ";Rus" + number;
  }
  headerString += "\n";
  mBuffer += headerString;
}

void CSVFileWriter::writeData(DataSet* set) {
  /*
    AbsolutePrivacy : When inside privacy area, the writer does noting, unless overriding is selected and the current set is confirmed
    NoPosition : When inside privacy area, the writer will replace latitude and longitude with NaNs
    NoPrivacy : Privacy areas are ignored, but the value "insidePrivacyArea" will be 1 inside
    OverridePrivacy : When selected, a full set is written, when a value was confirmed, even inside the privacy area
  */
  if (set->isInsidePrivacyArea
    && ((config.privacyConfig & AbsolutePrivacy) || ((config.privacyConfig & OverridePrivacy) && !set->confirmed))) {
    return;
  }

  tm time;
  localtime_r(&set->time, &time);
  char date[32];
  snprintf(date, sizeof(date),
    "%02d.%02d.%04d;%02d:%02d:%02d;%u;",
    time.tm_mday, time.tm_mon + 1, time.tm_year + 1900,
    time.tm_hour, time.tm_min, time.tm_sec, set->millis);

  mBuffer += date;
  mBuffer += set->comment;

#ifdef DEVELOP
  if (time.tm_sec == 0) {
    mBuffer += "DEVELOP:  GPSMessages: " + String(gps.passedChecksum())
               + " GPS crc errors: " + String(gps.failedChecksum());
  } else if (time.tm_sec == 1) {
    mBuffer += "DEVELOP: Mem: "
               + String(ESP.getFreeHeap() / 1024) + "k Buffer: "
               + String(getBufferLength() / 1024) + "k last write time: "
               + String(getWriteTimeMillis());
  } else if (time.tm_sec == 2) {
    mBuffer += "DEVELOP: Mem min free: "
      + String(ESP.getMinFreeHeap() / 1024) + "k";
  }
#endif
  mBuffer += ";";

  if ((!set->location.isValid()) ||
      ((config.privacyConfig & NoPosition) && set->isInsidePrivacyArea
    && !((config.privacyConfig & OverridePrivacy) && set->confirmed))) {
    mBuffer += ";;;;;";
  } else {
    mBuffer += String(set->location.lat(), 6) + ";";
    mBuffer += String(set->location.lng(), 6) + ";";
    mBuffer += String(set->altitude.meters(), 1) + ";";
    if (set->course.isValid()) {
      mBuffer += String(set->course.deg(), 2);
    }
    mBuffer += ";";
    if (set->speed.isValid()) {
      mBuffer += String(set->speed.kmph(), 2);
    }
    mBuffer += ";";
  }
  if (set->hdop.isValid()) {
    mBuffer += String(set->hdop.hdop(), 2);
  }
  mBuffer += ";";
  mBuffer += String(set->validSatellites) + ";";
  mBuffer += String(set->batteryLevel, 2) + ";";
  if (set->sensorValues[LEFT_SENSOR_ID] < MAX_SENSOR_VALUE) {
    mBuffer += String(set->sensorValues[LEFT_SENSOR_ID]);
  }
  mBuffer += ";";
  if (set->sensorValues[RIGHT_SENSOR_ID] < MAX_SENSOR_VALUE) {
    mBuffer += String(set->sensorValues[RIGHT_SENSOR_ID]);
  }
  mBuffer += ";";
  mBuffer += String(set->confirmed) + ";";
  mBuffer += String(set->marked) + ";";
  mBuffer += String(set->invalidMeasurement) + ";";
  mBuffer += String(set->isInsidePrivacyArea) + ";";
  mBuffer += String(set->factor) + ";";
  mBuffer += String(set->measurements);

  for (size_t idx = 0; idx < set->measurements; ++idx) {
    mBuffer += ";" + String(set->startOffsetMilliseconds[idx]) + ";";
    if (set->readDurationsLeftInMicroseconds[idx] > 0) {
      mBuffer += String(set->readDurationsLeftInMicroseconds[idx]) + ";";
    } else {
      mBuffer += ";";
    }
    if (set->readDurationsRightInMicroseconds[idx] > 0) {
      mBuffer += String(set->readDurationsRightInMicroseconds[idx]);
    }
  }
  for (size_t idx = set->measurements; idx < MAX_NUMBER_MEASUREMENTS_PER_INTERVAL; ++idx) {
    mBuffer += ";;;";
  }
  mBuffer += "\n";
}


void GPXFileWriter::writeHeader() {
  String headerString;
  headerString += F(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<gpx version=\"1.0\">\n"
      "\t<trk><trkseg>\n");
  this->appendFile(SD, getFileName().c_str(), headerString.c_str() );
}

void GPXFileWriter::writeData(DataSet* set) {
  String dataString;

  dataString += "\t\t<trkpt ";
  dataString += "lat=\"";
  String latitudeString = String(set->location.lat(), 6);
  dataString += latitudeString;
  dataString += "\" lon=\"";
  String longitudeString = String(set->location.lng(), 6);
  dataString += longitudeString;
  dataString += "\">";

  char dateTimeString[25];
  struct tm time;
  localtime_r(&set->time, &time);
  snprintf(dateTimeString, sizeof(dateTimeString),
    "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
    time.tm_year + 1900, time.tm_mon + 1, time.tm_mday,
    time.tm_hour, time.tm_min, time.tm_sec, 0);
  dataString += F("<time>");
  dataString += dateTimeString;
  dataString += F("</time>");

  dataString += F("<ele>");
  dataString += String(set->altitude.meters(), 2);
  dataString += F("</ele>");

  dataString += F("</trkpt>\n");

  this->appendFile(SD, getFileName().c_str(), dataString.c_str() );
}
