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
    /* Strips technical details like extension or '/' from the file name. */
    static String stripCsvFileName(const String &fileName);
    static void setClockByNtp();
    static void setClockByNtpAndWait();
};


#endif //OPENBIKESENSORFIRMWARE_OBSUTILS_H
