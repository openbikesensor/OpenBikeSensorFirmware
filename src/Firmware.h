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
#ifndef OPENBIKESENSORFIRMWARE_FIRMWARE_H
#define OPENBIKESENSORFIRMWARE_FIRMWARE_H


#include <functional>

class Firmware {
  public:
    explicit Firmware(String userAgent) : mUserAgent(userAgent) {};
    void downloadToSd(String url, String filename);
    bool downloadToFlash(String url, std::function<void(uint32_t, uint32_t)> progress);
    String getLastMessage();

    static String getFlashAppVersion();
    static String checkSdFirmware();
    static bool switchToFlashApp();

  private:
    String mLastMessage;
    String mUserAgent;
    static const esp_partition_t *findEspFlashAppPartition();
};


#endif //OPENBIKESENSORFIRMWARE_FIRMWARE_H
