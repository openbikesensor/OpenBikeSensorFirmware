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

#include <utils/obsutils.h>
#include "obsimprov.h"

using namespace improv;

const char *ObsImprov::HEADER = "IMPROV\01";
const uint8_t ObsImprov::HEADER_LENGTH = 7;
const char *ObsImprov::IMPROV_STARTUP_MESSAGE = "IMPROV\x01\x01\x01\x01\xE1\x0A";
const uint8_t ObsImprov::IMPROV_STARTUP_MESSAGE_LENGTH = 12;

static const uint8_t SEPARATOR = 0x0A;

void ObsImprov::sendHello(Stream *serial) {
  if (serial) {
    serial->flush();
    serial->write(IMPROV_STARTUP_MESSAGE, IMPROV_STARTUP_MESSAGE_LENGTH);
  }
}

static void sendPayload(Stream * stream, std::vector<uint8_t> payload) {
  stream->flush();

  std::vector<uint8_t> out;
  for (int i = 0; i < ObsImprov::HEADER_LENGTH; i++) {
    out.push_back((uint8_t) ObsImprov::HEADER[i]);
  }
  out.insert(out.end(), payload.begin(), payload.end());

  uint8_t calculated_checksum = 0;
  for (uint8_t byte : out) {
    calculated_checksum += byte;
  }
  out.push_back(calculated_checksum);
  out.push_back(SEPARATOR);

  stream->write(out.data(), out.size());
  stream->flush();
  ObsUtils::logHexDump(out.data(), out.size());
}


static std::vector<uint8_t> build_current_state(State state, bool add_checksum = true) {
  std::vector<uint8_t> out;
  out.push_back(0x01 /* CURRENT_STATE */); // TYPE
  out.push_back(state);
  if (add_checksum) {
    uint32_t calculated_checksum = 0;

    for (uint8_t byte : out) {
      calculated_checksum += byte;
    }
    out.push_back(calculated_checksum);
  }
  return out;
}

static std::vector<uint8_t> build_rpc_response(Command command, State payload, bool add_checksum = true) {
  std::vector<uint8_t> out;
  out.push_back(0x04 /* RPC_RESULT */); // TYPE
  out.push_back(command); // RPC command
  out.push_back(payload);
  if (add_checksum) {
    uint32_t calculated_checksum = 0;

    for (uint8_t byte : out) {
      calculated_checksum += byte;
    }
    out.push_back(calculated_checksum);
  }
  return out;
}

static void appendStringAndLength(std::vector<uint8_t> & response, std::string data) {
  response.push_back(data.size());
  response.insert(response.end(), data.begin(), data.end());
  log_d("insert '%s'", data.c_str());
}

void ObsImprov::handle() {
 while (mSerial->available() > 0) {
   handle(mSerial->read());
 }
}
void ObsImprov::handle(char c) {
  log_d("in improv read(0x%02x) %d/%d ", c, mHeaderPos, HEADER_LENGTH);
  if (mHeaderPos < HEADER_LENGTH) {
    if (c == HEADER[mHeaderPos]) {
      mHeaderPos++;
    } else {
      log_w("unexpected IMPROV Header read(0x%02x)", c);
      mHeaderPos = (c == HEADER[0]) ? 1 : 0;
    }
  } else {
    mBuffer.push_back(c);
      ImprovCommand cmd = parse_improv_data(mBuffer);
      if (cmd.command != UNKNOWN) {
        log_w("received message of type: 0x%02x", mBuffer[0]);
        ObsUtils::logHexDump(mBuffer.data(), mBuffer.size());

        if (mSerial->peek() == SEPARATOR) {
          log_d("Skipping from serial: 0x%02x", mSerial->read());
        }

        switch (mBuffer[0]) { // type
          case 0x03: // RPC Command
            log_i("received RPC command 0x%02x", mBuffer[2]);

            switch (mBuffer[2]) {
              case WIFI_SETTINGS: {
                const std::string ssid((char*)&mBuffer.data()[5], (size_t) mBuffer.data()[4]);
                const std::string password((char*)&mBuffer.data()[6 + mBuffer[4]], (size_t) mBuffer.data()[5 + mBuffer[4]]);
                log_i("wifi settings ssid: %s.", ssid.c_str());
                if (mInitWifi(ssid, password)) {
                  std::vector<uint8_t> response;
                  response.push_back(0x01 /* TYPE_CURRENT_STATE */); // TYPE
                  response.push_back(0x01 /* */); // LENGTH
                  response.push_back(improv::STATE_PROVISIONED);
                  sendPayload(mSerial, response);
                  sendWifiSuccess();
                } else {
                  std::vector<uint8_t> response;
                  response.push_back(0x02 /* TYPE_ERROR */); // TYPE
                  response.push_back(0x01 /* */); // LENGTH
                  response.push_back(ERROR_UNABLE_TO_CONNECT);
                  sendPayload(mSerial, response);
                }
                break;
              }
              case GET_CURRENT_STATE: {
                State state = mWifiStatus();
                log_i("get current state -> 0x%02x", state);

                std::vector<uint8_t> response;
                response.push_back(0x01 /* TYPE_CURRENT_STATE */); // TYPE
                response.push_back(0x01 /* */); // LENGTH
                response.push_back(state);
                sendPayload(mSerial, response);
                if (state == improv::STATE_PROVISIONED) {
                  sendWifiSuccess(GET_CURRENT_STATE);
                }
                break;
              }
              case GET_DEVICE_INFO: {
                log_i("get device info.");
                std::vector<uint8_t> response;
                response.push_back(0x04 /* RPC Result */); // TYPE
                response.push_back(0x00 /* */); // LENGTH
                response.push_back(0x03 /* Response to GET_DEVICE_INFO */); // TYPE
                response.push_back(0x00 /* */); // LENGTH
                appendStringAndLength(response, mFirmwareName);
                appendStringAndLength(response, mFirmwareVersion);
                appendStringAndLength(response, mHardwareVariant);
                appendStringAndLength(response, mDeviceName);
                response[1] = response.size() - 2;
                response[3] = response.size() - 4;
                sendPayload(mSerial, response);
                // OTHER?
                break;
              }
              default: {
                log_w("Unsupported improv rpc command 0x%02x ignored.", mBuffer[2]);
              }
            }
            mBuffer.clear();
            mHeaderPos = 0;
            break;
          case BAD_CHECKSUM: {
            log_w("Error decoding Improv payload");
            mBuffer.clear();
            break;
          }
          default: {
            log_w("Unsupported improv message type 0x%02x ignored.", mBuffer[2]);
            mBuffer.clear();
          }
      }
    }
    if (mBuffer.size() > 256) {
      log_w("Garbage data on serial, ignoring.");
      mBuffer.clear();
    }
  }
}

void ObsImprov::sendWifiSuccess(Command cmd) const {
  std::vector<uint8_t> response;
  response.push_back(0x04 /* RPC Result */); // TYPE
  response.push_back(0x00 /* */); // LENGTH
  response.push_back(cmd); // TYPE
  response.push_back(0x00 /* */); // LENGTH
  appendStringAndLength(response, mDeviceUrl());
  response[1] = response.size() - 2;
  response[3] = response.size() - 4;
  sendPayload(mSerial, response);
}

void ObsImprov::setDeviceInfo(const std::string & firmwareName,
                   const std::string & firmwareVersion,
                   const std::string & hardwareVariant,
                   const std::string & deviceName) {
  mFirmwareName = firmwareName;
  mFirmwareVersion = firmwareVersion;
  mHardwareVariant = hardwareVariant;
  mDeviceName = deviceName;
}
