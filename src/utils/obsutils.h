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
#ifndef OPENBIKESENSORFIRMWARE_OBSUTILS_H
#define OPENBIKESENSORFIRMWARE_OBSUTILS_H

#include <Arduino.h>

/**
 * Class for internal - static only methods.
 */
class ObsUtils {
  public:
    static String createTrackUuid();
    static String dateTimeToString(time_t time = 0);
    static String timeToString(time_t theTime =0);
    /* Strips technical details like extension or '/' from the file name. */
    static String stripCsvFileName(const String &fileName);
    static String encodeForXmlAttribute(const String & text);
    static String encodeForXmlText(const String &text);
    static String encodeForCsvField(const String &field);
    static String encodeForUrl(const String &url);
    static void setClockByNtp(const char *ntpServer = nullptr);
    static void setClockByNtpAndWait(const char *ntpServer = nullptr);
    static bool systemTimeIsSet();
    static String toScaledByteString(uint32_t size);
    static String dateTimeToHttpHeaderString(time_t theTime);
    static void logHexDump(const uint8_t *buffer, uint16_t length);
    static const time_t PAST_TIME;

  private:
    static const char *weekDayToString(uint8_t wDay);
    static const char *monthToString(uint8_t mon);
    static const char *WEEK_DAYS[7];
    static const char *MONTHS[12];

};


#endif //OPENBIKESENSORFIRMWARE_OBSUTILS_H
