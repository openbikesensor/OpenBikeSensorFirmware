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

#include "timeutils.h"

#include <time.h>
#include <sys/time.h>


/* Flag character used when time was set according to GPS Time */
static const char TIMEZONE_GPS = ' ';
/* Flag character used when time was set according to UTC Time */
static const char TIMEZONE_UTC = 'Z';
/* Flag character used when time was not set */
static const char TIMEZONE_UNKNOWN = 'X';

// remember last timezone used, need to do it manually because GPS is not a
// TimeZone in sense of linux supported time zones.
static char timeZone = TIMEZONE_UNKNOWN;


const uint32_t TimeUtils::SECONDS_PER_DAY = 60L * 60 * 24;
const uint32_t TimeUtils::SECONDS_PER_WEEK = SECONDS_PER_DAY * 7;
// use difftime() ?
/* GPS Start time is 1980-01-06T00:00:00Z  */
const uint32_t TimeUtils::GPS_EPOCH_OFFSET = 315964800L;


const time_t TimeUtils::PAST_TIME = 30 * 365 * 24 * 60 * 60;
const char *TimeUtils::WEEK_DAYS[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
const char *TimeUtils::MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


String TimeUtils::dateTimeToString(time_t timeIn) {
  char date[32];
  time_t theTime;
  if (timeIn == 0) {
    theTime = time(nullptr);
  } else {
    theTime = timeIn;
  }
  tm timeStruct;
  localtime_r(&theTime, &timeStruct);
  snprintf(date, sizeof(date),
           "%04d-%02d-%02dT%02d:%02d:%02d",
           timeStruct.tm_year + 1900, timeStruct.tm_mon + 1, timeStruct.tm_mday,
           timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec);
  String result(date);
  if (timeIn == 0 && timeZone == TIMEZONE_UTC) {
    result += TIMEZONE_UTC;
  }
  return result;
}

String TimeUtils::timeToString(time_t timeIn) {
  char date[32];
  time_t theTime;
  if (timeIn == 0) {
    theTime = time(nullptr);
  } else {
    theTime = timeIn;
  }
  if (theTime == 0) {
    theTime = time(nullptr);
  }
  tm timeStruct;
  localtime_r(&theTime, &timeStruct);
  snprintf(date, sizeof(date),
           "%02d:%02d:%02d",
           timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec);
  String result(date);
  if (timeIn == 0 && timeZone == TIMEZONE_UTC) {
    result += TIMEZONE_UTC;
  }
  return result;
}

String TimeUtils::dateTimeToHttpHeaderString(time_t theTime) {
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

const char* TimeUtils::weekDayToString(uint8_t wDay) {
  if (wDay > 6) {
    return "???";
  } else {
    return WEEK_DAYS[wDay];
  }
}

const char* TimeUtils::monthToString(uint8_t mon) {
  if (mon > 11) {
    return "???";
  } else {
    return MONTHS[mon];
  }
}

void TimeUtils::setClockByNtp(const char* ntpServer) {
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
  timeZone = TIMEZONE_UTC;
}

void TimeUtils::setClockByGps(uint32_t iTow, int32_t fTow, int16_t week, int8_t leapS) {
  const time_t gpsTime = toTime(week, iTow / 1000);
  if (timeZone == TIMEZONE_UTC && systemTimeIsSet()) {
    log_i("Ignore new GPS time (%s), already set via NTP (%s).",
          dateTimeToString(gpsTime).c_str(),
          dateTimeToString().c_str());
    return;
  }
  const int32_t gpsTimeUsec = ((iTow % 1000) * 1000) + (fTow / 1000);
  const struct timeval now = {
    .tv_sec = gpsTime,
    .tv_usec = gpsTimeUsec
  };
  settimeofday(&now, nullptr);
  timeZone = TIMEZONE_GPS;
}

void TimeUtils::setClockByNtpAndWait(const char* ntpServer) {
  setClockByNtp(ntpServer);

  log_i("Waiting for NTP time sync: ");
  while (!systemTimeIsSet()) {
    delay(500);
    log_i(".");
    yield();
  }
  timeZone = TIMEZONE_UTC;
  log_i("NTP time set got %s.", dateTimeToString(time(nullptr)).c_str());
}

bool TimeUtils::systemTimeIsSet() {
  time_t now = time(nullptr);
  log_v("time %s.", dateTimeToString(now).c_str());
  return now > 1609681614;
}

/* Determine the number of leap seconds to be considered at the given GPS
 * time.
 * TODO: Make this more fancy leverage AID_HUI info that can be stored on
 *    SD or hardcode dates if announced at
 *    https://www.ietf.org/timezones/data/leap-seconds.list
 */
int16_t TimeUtils::getLeapSecondsGps(time_t gps) {
  return 18;
}

/* Determine the number of leap seconds to be considered at the given UTC
 * time.
 */
int16_t TimeUtils::getLeapSecondsUtc(time_t utc) {
  return 18;
}

time_t TimeUtils::gpsDayToTime(uint16_t week, uint16_t dayOfWeek) {
  return (time_t) GPS_EPOCH_OFFSET + (SECONDS_PER_WEEK * week) + (SECONDS_PER_DAY * dayOfWeek);
}

time_t TimeUtils::toTime(uint16_t week, uint32_t weekTime) {
  return (time_t) GPS_EPOCH_OFFSET + (SECONDS_PER_WEEK * week) + weekTime;
}

uint32_t TimeUtils::utcTimeToTimeOfWeek(time_t t) {
  return (t - GPS_EPOCH_OFFSET - getLeapSecondsGps(t)) % SECONDS_PER_WEEK;
}

uint16_t TimeUtils::utcTimeToWeekNumber(time_t t) {
  return (t - GPS_EPOCH_OFFSET - getLeapSecondsGps(t)) / SECONDS_PER_WEEK;
}