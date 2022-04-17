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

#include <utils/timeutils.h>
#include "writer.h"
#include "utils/file.h"

const String CSVFileWriter::EXTENSION = ".obsdata.csv";

int FileWriter::getTrackNumber() {
  File numberFile = SD.open("/tracknumber.txt","r");
  int trackNumber = numberFile.readString().toInt();
  numberFile.close();
  return trackNumber;
}

void FileWriter::storeTrackNumber(int trackNumber) {
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
      SD.rename(mFileName.c_str(), newName.c_str())) {
      mFileName = newName;
      mFinalFileName = true;
    }
  }
}

uint16_t FileWriter::getBufferLength() const {
  return mBuffer.length();
}

bool FileWriter::appendString(const String &s) {
  bool stored = false;
  if (getBufferLength() < 11000) { // do not add data if our buffer is full already. We loose data here!
    mBuffer.concat(s);
    stored = true;
  }
  if (getBufferLength() > 10000 && !(digitalRead(PUSHBUTTON_PIN))) {
    flush();
  }
  if (!stored && getBufferLength() < 11000) { // do not add data if our buffer is full already. We loose data here!
    mBuffer.concat(s);
    stored = true;
  }
#ifdef DEVELOP
  if (!stored) {
    Serial.printf("File buffer overflow, not allowed to write - "
                  "will skip, memory is at %dk, buffer at %u.\n",
                  ESP.getFreeHeap() / 1024, getBufferLength());
  }
#endif
  return stored;
}

bool FileWriter::flush() {
  bool result;
#ifdef DEVELOP
  Serial.printf("Writing to concrete file.\n");
#endif
  const auto start = millis();
  result = FileUtil::appendFile(SD, mFileName.c_str(), mBuffer.c_str() );
  mBuffer.clear();
  mWriteTimeMillis = millis() - start;
  if (!mFinalFileName) {
    correctFilename();
  }
#ifdef DEVELOP
  Serial.printf("Writing to concrete file done took %lums.\n", mWriteTimeMillis);
#endif
  return result;
}

unsigned long FileWriter::getWriteTimeMillis() const {
  return mWriteTimeMillis;
}

bool CSVFileWriter::writeHeader(String trackId) {
  String header;
  header += "OBSDataFormat=2&";
  header += "OBSFirmwareVersion=" + String(OBSVersion) + "&";
  header += "DeviceId=" + String((uint16_t)(ESP.getEfuseMac() >> 32), 16) + "&";
  header += "DataPerMeasurement=3&";
  header += "MaximumMeasurementsPerLine=" + String(MAX_NUMBER_MEASUREMENTS_PER_INTERVAL) + "&";
  header += "OffsetLeft=" + String(config.sensorOffsets[LEFT_SENSOR_ID]) + "&";
  header += "OffsetRight=" + String(config.sensorOffsets[RIGHT_SENSOR_ID]) + "&";
  header += "NumberOfDefinedPrivacyAreas=" + String((int) config.privacyAreas.size()) + "&";
  header += "TrackId=" + trackId + "&";
  header += "PrivacyLevelApplied=";

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
    header += "OverridePrivacy";
  }
  if (privacyString.isEmpty()) {
    privacyString += "NoPrivacy";
  }
  header += privacyString + "&";
  header += "MaximumValidFlightTimeMicroseconds=" + String(MAX_DURATION_MICRO_SEC) + "&";
  header += "BluetoothEnabled=" + String(config.bluetooth) + "&";
  header += "PresetId=default&";
  header += "TimeZone=GPS&";
  header += "DistanceSensorsUsed=HC-SR04/JSN-SR04T\n";

  header += "Date;Time;Millis;Comment;Latitude;Longitude;Altitude;"
    "Course;Speed;HDOP;Satellites;BatteryLevel;Left;Right;Confirmed;Marked;Invalid;"
    "InsidePrivacyArea;Factor;Measurements";
  for (uint16_t idx = 1; idx <= MAX_NUMBER_MEASUREMENTS_PER_INTERVAL; ++idx) {
    String number = String(idx);
    header += ";Tms" + number;
    header += ";Lus" + number;
    header += ";Rus" + number;
  }
  header += "\n";
  return appendString(header);
}

bool CSVFileWriter::append(DataSet &set) {
  String csv;
  /*
    AbsolutePrivacy : When inside privacy area, the writer does noting, unless overriding is selected and the current set is confirmed
    NoPosition : When inside privacy area, the writer will replace latitude and longitude with NaNs
    NoPrivacy : Privacy areas are ignored, but the value "insidePrivacyArea" will be 1 inside
    OverridePrivacy : When selected, a full set is written, when a value was confirmed, even inside the privacy area
  */
  if (set.isInsidePrivacyArea
    && ((config.privacyConfig & AbsolutePrivacy) || ((config.privacyConfig & OverridePrivacy) && !set.confirmed))) {
    return true;
  }

  time_t theTime;
  if (set.gpsRecord.getTow() != 0 && set.gpsRecord.getWeek() != 0) {
    theTime = TimeUtils::toTime(set.gpsRecord.getWeek(), set.gpsRecord.getTow() / 1000);
    // TODO: Force adjust filename if week changes and  set.gpsRecord.getTow() is not small via mFinalFileName = false;
  } else {
    theTime = set.time;
  }
  tm time;
  localtime_r(&(theTime), &time);
  // localtime_r(&(set.time), &time);
  char date[32];
  snprintf(date, sizeof(date),
    "%02d.%02d.%04d;%02d:%02d:%02d;%u;",
    time.tm_mday, time.tm_mon + 1, time.tm_year + 1900,
    time.tm_hour, time.tm_min, time.tm_sec, set.millis);

  csv += date;
  csv += set.comment;

// FIXME #ifdef DEVELOP
  if (time.tm_sec == 0) {
    csv += "DEV: GPSMessages: " + String(gps.getValidMessageCount())
           + " GPS crc errors: " + String(gps.getMessagesWithFailedCrcCount());
  } else if (time.tm_sec == 1) {
    csv += "DEV: Mem: "
           + String(ESP.getFreeHeap() / 1024) + "k Buffer: "
           + String(getBufferLength() / 1024) + "k last write time: "
           + String(getWriteTimeMillis());
  } else if (time.tm_sec == 2) {
    csv += "DEV: Mem min free: "
           + String(ESP.getMinFreeHeap() / 1024) + "k";
  } else if (time.tm_sec == 3) {
    csv += "DEV: GPS lastNoiseLevel: ";
    csv += gps.getLastNoiseLevel();
  } else if (time.tm_sec == 4) {
    csv += "DEV: GPS baud: ";
    csv += gps.getBaudRate();
  } else if (time.tm_sec == 5) {
    csv += "DEV: GPS alp bytes: ";
    csv += gps.getNumberOfAlpBytesSent();
  } else if (time.tm_sec == 6) {
    csv += "DEV: Left Sensor no : ";
    csv += sensorManager->getNoSignalReadings(LEFT_SENSOR_ID);
  } else if (time.tm_sec == 7) {
    csv += "DEV: Right Sensor no : ";
    csv += sensorManager->getNoSignalReadings(RIGHT_SENSOR_ID);
  } else if (time.tm_sec == 8) {
    csv += "DEV: Left last delay till start : ";
    csv += sensorManager->getLastDelayTillStartUs(LEFT_SENSOR_ID);
  } else if (time.tm_sec == 9) {
    csv += "DEV: Right last delay till start : ";
    csv += sensorManager->getLastDelayTillStartUs(RIGHT_SENSOR_ID);
  } else if (time.tm_sec == 10) {
    csv += "DEV: Left min echo : ";
    csv += sensorManager->getMinDurationUs(LEFT_SENSOR_ID);
  } else if (time.tm_sec == 11) {
    csv += "DEV: Right min echo : ";
    csv += sensorManager->getMinDurationUs(RIGHT_SENSOR_ID);
  } else if (time.tm_sec == 12) {
    csv += "DEV: Left max echo : ";
    csv += sensorManager->getMaxDurationUs(LEFT_SENSOR_ID);
  } else if (time.tm_sec == 13) {
    csv += "DEV: Right max echo : ";
    csv += sensorManager->getMaxDurationUs(RIGHT_SENSOR_ID);
  } else if (time.tm_sec == 14) {
    csv += "DEV: Left low after measure: ";
    csv += sensorManager->getNumberOfLowAfterMeasurement(LEFT_SENSOR_ID);
  } else if (time.tm_sec == 15) {
    csv += "DEV: Right low after measure : ";
    csv += sensorManager->getNumberOfLowAfterMeasurement(RIGHT_SENSOR_ID);
  } else if (time.tm_sec == 16) {
    csv += "DEV: Left long measurement : ";
    csv += sensorManager->getNumberOfToLongMeasurement(LEFT_SENSOR_ID);
  } else if (time.tm_sec == 17) {
    csv += "DEV: Right long measurement : ";
    csv += sensorManager->getNumberOfToLongMeasurement(RIGHT_SENSOR_ID);
  } else if (time.tm_sec == 18) {
    csv += "DEV: Left interrupt adjusted : ";
    csv += sensorManager->getNumberOfInterruptAdjustments(LEFT_SENSOR_ID);
  } else if (time.tm_sec == 19) {
    csv += "DEV: Right interrupt adjusted : ";
    csv += sensorManager->getNumberOfInterruptAdjustments(RIGHT_SENSOR_ID);
  } else if (time.tm_sec >= 20 && time.tm_sec < 40) {
    String msg = gps.getMessage(time.tm_sec - 20);
    if (!msg.isEmpty()) {
      csv += "DEV: GPS: ";
      csv += ObsUtils::encodeForCsvField(msg);
    }
  } else if (time.tm_sec == 40) {
    csv += "DBG GPS Time: " +
      TimeUtils::dateTimeToString(TimeUtils::toTime(set.gpsRecord.getWeek(), set.gpsRecord.getTow() / 1000));
  }
// #endif
  csv += ";";

  if ((!set.gpsRecord.hasValidFix()) ||
      ((config.privacyConfig & NoPosition) && set.isInsidePrivacyArea
    && !((config.privacyConfig & OverridePrivacy) && set.confirmed))) {
    csv += ";;;;;";
  } else {
    csv += set.gpsRecord.getLatString() + ";";
    csv += set.gpsRecord.getLongString() + ";";
    csv += set.gpsRecord.getAltitudeMetersString() + ";";
    csv += set.gpsRecord.getCourseString() + ";";
    csv += set.gpsRecord.getSpeedKmHString() + ";";
  }
  csv += set.gpsRecord.getHdopString() + ";";

  csv += String(set.gpsRecord.getSatellitesUsed()) + ";";
  csv += String(set.batteryLevel, 2) + ";";
  if (set.sensorValues[LEFT_SENSOR_ID] < MAX_SENSOR_VALUE) {
    csv += String(set.sensorValues[LEFT_SENSOR_ID]);
  }
  csv += ";";
  if (set.sensorValues[RIGHT_SENSOR_ID] < MAX_SENSOR_VALUE) {
    csv += String(set.sensorValues[RIGHT_SENSOR_ID]);
  }
  csv += ";";
  csv += String(set.confirmed) + ";";
  csv += String(set.marked) + ";";
  csv += String(set.invalidMeasurement) + ";";
  csv += String(set.isInsidePrivacyArea) + ";";
  csv += String(set.factor) + ";";
  csv += String(set.measurements);

  for (size_t idx = 0; idx < set.measurements; ++idx) {
    csv += ";" + String(set.startOffsetMilliseconds[idx]) + ";";
    if (set.readDurationsLeftInMicroseconds[idx] > 0) {
      csv += String(set.readDurationsLeftInMicroseconds[idx]) + ";";
    } else {
      csv += ";";
    }
    if (set.readDurationsRightInMicroseconds[idx] > 0) {
      csv += String(set.readDurationsRightInMicroseconds[idx]);
    }
  }
  for (size_t idx = set.measurements; idx < MAX_NUMBER_MEASUREMENTS_PER_INTERVAL; ++idx) {
    csv += ";;;";
  }
  csv += "\n";
  return appendString(csv);
}
