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

#ifndef OPENBIKESENSORFIRMWARE_TIMEUTILS_H
#define OPENBIKESENSORFIRMWARE_TIMEUTILS_H

#include <Arduino.h>

class TimeUtils {
  public:
    static const time_t PAST_TIME;
    static String dateTimeToString(time_t timeIn = 0);
    static String timeToString(time_t theTime =0);
    static String dateTimeToHttpHeaderString(time_t theTime);
    static void setClockByNtp(const char *ntpServer = nullptr);
    static void setClockByNtpAndWait(const char *ntpServer = nullptr);
    static void setClockByGps(uint32_t iTow, int32_t fTow, int16_t week, int8_t leapS = 0);
    static bool systemTimeIsSet();
    static int16_t getLeapSecondsGps(time_t gps);
    static int16_t getLeapSecondsUtc(time_t gps);
    static time_t gpsDayToTime(uint16_t week, uint16_t dayOfWeek);
    static time_t toTime(uint16_t week, uint32_t weekTime);
    static uint32_t utcTimeToTimeOfWeek(time_t t);
    static uint16_t utcTimeToWeekNumber(time_t t);

  private:
    static const char *WEEK_DAYS[7];
    static const char *MONTHS[12];
    static const uint32_t GPS_EPOCH_OFFSET;
    static const uint32_t SECONDS_PER_DAY;
    static const uint32_t SECONDS_PER_WEEK;
    static const char *weekDayToString(uint8_t wDay);
    static const char *monthToString(uint8_t mon);
};


#endif //OPENBIKESENSORFIRMWARE_TIMEUTILS_H
