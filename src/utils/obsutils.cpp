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
#include "obsutils.h"
#include <esp_wifi.h>
#include "writer.h"
#include "BLEServer.h"

static const int BYTES_PER_KB = 1024;
static const int BYTES_PER_MB = 1024 * 1024;

const time_t ObsUtils::PAST_TIME = 30 * 365 * 24 * 60 * 60;

String ObsUtils::createTrackUuid() {
  uint8_t data[16];
  esp_fill_random(data, 16);
  return String(BLEUUID(data, 16, false).toString().c_str());
}

String ObsUtils::dateTimeToString(time_t theTime) {
  char date[32];
  if (theTime == 0) {
    theTime = time(nullptr);
  }
  tm timeStruct;
  localtime_r(&theTime, &timeStruct);
  snprintf(date, sizeof(date),
           "%02d.%02d.%04dT%02d:%02d:%02dZ",
           timeStruct.tm_mday, timeStruct.tm_mon + 1, timeStruct.tm_year + 1900,
           timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec);
  return String(date);
}

String ObsUtils::timeToString(time_t theTime) {
  char date[32];
  if (theTime == 0) {
    theTime = time(nullptr);
  }
  tm timeStruct;
  localtime_r(&theTime, &timeStruct);
  snprintf(date, sizeof(date),
           "%02d:%02d:%02dZ",
           timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec);
  return String(date);
}

String ObsUtils::dateTimeToHttpHeaderString(time_t theTime) {
  char date[32];
  if (theTime == 0) {
    theTime = time(nullptr);
  }
  tm timeStruct;
  localtime_r(&theTime, &timeStruct);
  snprintf(date, sizeof(date),
           "%s, %02d %s %04d %02d:%02d:%02d GMT",
           weekDayToString(timeStruct.tm_wday),
           timeStruct.tm_mday,
           monthToString(timeStruct.tm_mon),
           timeStruct.tm_year + 1900,
           timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec);
  return String(date);
}

const char *ObsUtils::WEEK_DAYS[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
const char *ObsUtils::MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

const char* ObsUtils::weekDayToString(uint8_t wDay) {
  if (wDay > 6) {
    return "???";
  } else {
    return WEEK_DAYS[wDay];
  }
}

const char* ObsUtils::monthToString(uint8_t mon) {
  if (mon > 11) {
    return "???";
  } else {
    return MONTHS[mon];
  }
}

String ObsUtils::stripCsvFileName(const String &fileName) {
  String userPrintableFilename = fileName.substring(fileName.lastIndexOf("/") + 1);
  if (userPrintableFilename.endsWith(CSVFileWriter::EXTENSION)) {
    userPrintableFilename
      = userPrintableFilename.substring(
      0, userPrintableFilename.length() - CSVFileWriter::EXTENSION.length());
  }
  return userPrintableFilename;
}

void ObsUtils::setClockByNtp(const char* ntpServer) {
  if (!systemTimeIsSet()) {
    if (ntpServer) {
      log_d("Got additional NTP Server %s.", ntpServer);
      configTime(
        0, 0, ntpServer,
        "rustime01.rus.uni-stuttgart.de", "pool.ntp.org");
    } else {
      configTime(
        0, 0,
        "rustime01.rus.uni-stuttgart.de", "pool.ntp.org");
    }
  }
}

void ObsUtils::setClockByNtpAndWait(const char* ntpServer) {
  setClockByNtp(ntpServer);

  log_d("Waiting for NTP time sync: ");
  while (!systemTimeIsSet()) {
    delay(500);
    log_v(".");
    yield();
  }
  log_d("NTP time set got %s.", ObsUtils::dateTimeToString(time(nullptr)).c_str());
}

bool ObsUtils::systemTimeIsSet() {
  time_t now = time(nullptr);
  log_v("NTP time %s.", ObsUtils::dateTimeToString(now).c_str());
  return now > 1609681614;
}

String ObsUtils::encodeForXmlAttribute(const String &text) {
  String result(text);
  result.replace("&", "&amp;");
  result.replace("<", "&lt;");
  result.replace(">", "&gt;");
  result.replace("'", "&#39;");
  result.replace("\"", "&#34;");
  return result;
}

String ObsUtils::encodeForXmlText(const String &text) {
  String result(text);
  result.replace("&", "&amp;");
  result.replace("<", "&lt;");
  return result;
}

String ObsUtils::encodeForCsvField(const String &field) {
  String result(field);
  result.replace('&', '_');
  result.replace('\n', ' ');
  result.replace('\r', ' ');
  return result;
}

String ObsUtils::toScaledByteString(uint32_t size) {
  String result;
  if (size <= BYTES_PER_KB * 10) {
    result = String(size) + "b";
  } else if (size <= BYTES_PER_MB * 10) {
    result = String(size / BYTES_PER_KB) + "kb";
  } else {
    result = String(size / BYTES_PER_MB) + "mb";
  }
  return result;
}

void ObsUtils::logHexDump(const uint8_t *buffer, uint16_t length) {
  String debug;
  char buf[16];
  for (int i = 0; i < length; i++) {
    if (i % 16 == 0) {
      snprintf(buf, 8, "\n%04X  ", i);
      debug += buf;
    }
    snprintf(buf, 8, "%02X ", buffer[i]);
    debug += buf;
  }
  log_e("%s", debug.c_str());
}

