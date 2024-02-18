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

#ifndef OPENBIKESENSORFIRMWARE_ALPDATA_H
#define OPENBIKESENSORFIRMWARE_ALPDATA_H

#include <SD.h>
#include "displays.h"

static const char *const LAST_MODIFIED_HEADER_FILE_NAME = "/current_14d.hdr";

static const char *const ALP_DATA_FILE_NAME = "/current_14d.alp";
static const char *const ALP_NEW_DATA_FILE_NAME = "/current_14d.new";
static const char *const AID_INI_DATA_FILE_NAME = "/aid_ini.ubx";

static const int32_t ALP_DATA_MIN_FILE_SIZE = 80000; // CHECK!

static const char *const ALP_DOWNLOAD_URL = "https://alp.u-blox.com/current_14d.alp";

class AlpData {
  public:
    static void update(DisplayDevice *display);
    static bool available();
    static void saveMessage(const uint8_t *data, size_t size);
    static size_t loadMessage(uint8_t *data, size_t size);

    uint16_t fill(uint8_t *data, size_t ofs, uint16_t dataSize);
    void save(const uint8_t *data, size_t offset, int length);

  private:
    static void saveLastModified(const String &header);
    static String loadLastModified();
    static void displayHttpClientError(DisplayDevice *display, int httpError);
    File mAlpDataFile;

};


#endif //OPENBIKESENSORFIRMWARE_ALPDATA_H
