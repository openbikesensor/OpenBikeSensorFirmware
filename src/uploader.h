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

#ifndef UPLOADER_H
#define UPLOADER_H

#include <Arduino.h>
#ifdef HTTP_INSECURE
#include <WiFiClient.h>
#else
#include <WiFiClientSecure.h>
#endif
#include <FS.h>

class Uploader {
  public:
    Uploader(String portalUrl, String userToken);
    /* uploads the named file to the portal,
     * moves it to uploaded directory and
     * returns true if successful, otherwise false */
    bool upload(const String& fileName);
    String getLastStatusMessage() const;
    String getLastLocation() const;

  private:
    const String mPortalUrl;
    const String mPortalUserToken;
#ifdef HTTP_INSECURE
    WiFiClient mWiFiClient;
#else
    WiFiClientSecure mWiFiClient;
#endif
    String mLastLocation = "";
    String mLastStatusMessage = "NO UPLOAD";

    bool uploadFile(fs::File &file);
};
#endif
