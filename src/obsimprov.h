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

#ifndef OPENBIKESENSORFIRMWARE_OBSIMPROV_H
#define OPENBIKESENSORFIRMWARE_OBSIMPROV_H

/* TODO:
 *  - is the original improv lib of use?
 *  - not clear if we need to respond early in the boot phase already. I saw different results
 *    (waiting after update or not offering a set wi-fi
 *  - cleanup, refine log levels
 *  - remove debug logging
 *  - provide short documentation (on index.html?)
 *  - still some `Improv Wi-Fi Serial not detected` in console.
 *  - refactor as lib?
 *  - smooth display after wifi connect via improve (initWifi)
 *  - do we still need the freifunk default? -> No
 *  - serial is there a race condition when writing? (sendPayload)
 */

#include <HardwareSerial.h>
#include <improv.h>
#include <functional>


class ObsImprov {
  public:
    ObsImprov(std::function<std::string(const std::string & ssid, const std::string & password)> initWifi,
              std::function<improv::State()> getWifiStatus,
               HardwareSerial* serial = &Serial) : mSerial(serial), mInitWifi(initWifi), mWifiStatus(getWifiStatus) { };
    void handle();
    void handle(char c);
    void setDeviceInfo(const std::string firmwareName,
                       const std::string firmwareVersion,
                       const std::string hardwareVariant,
                       const std::string deviceName);
    static const char *HEADER;
    static const uint8_t HEADER_LENGTH;
    static const char *IMPROV_STARTUP_MESSAGE;
    static const uint8_t IMPROV_STARTUP_MESSAGE_LENGTH;

  private:
    HardwareSerial* mSerial;
    std::vector<uint8_t> mBuffer;
    uint8_t  mHeaderPos = 0;
    std::string mFirmwareName;
    std::string mFirmwareVersion;
    std::string mHardwareVariant;
    std::string mDeviceName;
    std::string mUrl;
    const std::function<std::string(const std::string & ssid, const std::string & password)> mInitWifi;
    const std::function<improv::State()> mWifiStatus;

    void sendWifiSuccess() const;
};


#endif //OPENBIKESENSORFIRMWARE_OBSIMPROV_H
