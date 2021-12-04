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
 *  - https://github.com/esphome/esp-web-tools/issues/135
 *  - provide short documentation (on index.html?)
 *  - refactor as lib?
 *  - smooth display after wifi connect via improve (initWifi)
 */

#include <HardwareSerial.h>
#include <functional>
#include <vector>

/**
 * Utility class to implement the improv protocol.
 * See protocol documentation at https://www.improv-wifi.com/serial/ for
 * the specification of the protocol.
 */
class ObsImprov {
  public:
    enum State : uint8_t {
      READY = 0x02,
      PROVISIONING = 0x03,
      PROVISIONED = 0x04,
    };

    /**
     * Create a new ObsImprov instance, able to handle incoming messages.
     * All callback methods must be expected to be called from "handle()"
     * call.
     *
     * @param initWifi callback method if new wifi data was received via improv.
     *                 must return true if a connection could be established.
     * @param getWifiStatus should return the status of the wifi connection.
     * @param getDeviceUrl if the device wifi is up the url needed to reach
     *                     the device should be returned, a empty string otherwise.
     */
    ObsImprov(std::function<bool(const std::string & ssid, const std::string & password)> initWifi,
              std::function<State()> getWifiStatus,
              std::function<std::string()> getDeviceUrl,
               HardwareSerial* serial = &Serial) :
                mSerial(serial),
                mInitWifi(initWifi),
                mWifiStatus(getWifiStatus),
                mDeviceUrl(getDeviceUrl) { };

    /**
     * Check for new chars on serial.
     */
    void handle();

    /**
     * Consume given char as if it had been read from serial.
     * Can be used if there are several parties interested in serial data.
     * @param c the char to be consumed.
     */
    void handle(char c);

    /**
     * Store detailed device information.
     * Should be called as soon as possible after creating a ObsImprov instance.
     * Empty data is reported upstream if needed earlier.
     */
    void setDeviceInfo(const std::string & firmwareName,
                       const std::string & firmwareVersion,
                       const std::string & hardwareVariant,
                       const std::string & deviceName);

    /**
     * Returns true if any improv message was received.
     * @return true if a improv message was received at any time
     */
    bool isActive();

  private:
    enum Type : uint8_t {
      CURRENT_STATE = 0x01,
      ERROR_STATE = 0x02,
      RPC_COMMAND = 0x03,
      RPC_RESULT = 0x04
    };
    enum Command : uint8_t {
      WIFI_SETTINGS = 0x01,
      GET_CURRENT_STATE = 0x02,
      GET_DEVICE_INFO = 0x03
    };
    enum Error : uint8_t {
      ERROR_NONE = 0x00,
      ERROR_INVALID_RPC = 0x01,
      ERROR_UNKNOWN_RPC = 0x02,
      ERROR_UNABLE_TO_CONNECT = 0x03,
      ERROR_UNKNOWN = 0xFF
    };
    enum Offset : uint8_t {
      TYPE_OFFSET = 0x00,
      LENGTH_OFFSET = 0x01,
      RPC_COMMAND_OFFSET = 0x02,
      RPC_DATA_LENGTH_OFFSET = 0x03,
      RPC_DATA_1_OFFSET = 0x04
    };
    HardwareSerial* mSerial;
    std::vector<uint8_t> mBuffer;
    uint8_t  mHeaderPos = 0;
    std::string mFirmwareName;
    std::string mFirmwareVersion;
    std::string mHardwareVariant;
    std::string mDeviceName;
    static const char *HEADER;
    static const uint8_t HEADER_LENGTH;
    static const char *IMPROV_STARTUP_MESSAGE;
    static const uint8_t IMPROV_STARTUP_MESSAGE_LENGTH;
    const std::function<bool(const std::string & ssid, const std::string & password)> mInitWifi;
    const std::function<State()> mWifiStatus;
    const std::function<std::string()> mDeviceUrl;
    bool mImprovActive = false;
    void sendWifiSuccess(Command cmd = WIFI_SETTINGS) const;
    void sendCurrentState(State state) const;
    void sendErrorState(Error error) const;
    void sendRpcDeviceInformation() const;
    void appendStringAndLength(std::vector<uint8_t> &response, std::string data) const;
    void sendPayload(Stream *stream, std::vector<uint8_t> payload) const;
    bool isCompleteImprovMessage(std::vector<uint8_t> buffer) const;
    bool isValidImprovMessage(std::vector<uint8_t> buffer) const;
    void handleImprovMessage(std::vector<uint8_t> buffer);

};


#endif //OPENBIKESENSORFIRMWARE_OBSIMPROV_H
