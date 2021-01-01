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
#include <esp_wifi.h>
#include "BLEServer.h"
#include "obsutils.h"

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
