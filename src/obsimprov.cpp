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

#include "obsimprov.h"

const char *ObsImprov::HEADER = "IMPROV\01";
const uint8_t ObsImprov::HEADER_LENGTH = 7;

static const uint8_t SEPARATOR = 0x0A;

void ObsImprov::setDeviceInfo(const std::string &firmwareName,
                              const std::string &firmwareVersion,
                              const std::string &hardwareVariant,
                              const std::string &deviceName) {
  mFirmwareName = firmwareName;
  mFirmwareVersion = firmwareVersion;
  mHardwareVariant = hardwareVariant;
  mDeviceName = deviceName;
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
      log_w("unexpected header char read(0x%02x)", c);
      mHeaderPos = (c == HEADER[0]) ? 1 : 0;
    }
  } else {
    mBuffer.push_back(c);
    if (isCompleteImprovMessage(mBuffer)) {
      if (isValidImprovMessage(mBuffer)) {
        mImprovActive = true;
        handleImprovMessage(mBuffer);
        if (mSerial->peek() == SEPARATOR) {
          mSerial->read();
        }
      } else {
        log_w("ignoring invalid message.");
        ESP_LOG_BUFFER_HEXDUMP(__FILE__, mBuffer.data(), mBuffer.size(), ESP_LOG_WARN);
      }
      mHeaderPos = 0;
      mBuffer.clear();
    } else if (mBuffer.size() > 256) {
      log_w("Garbage data on serial, ignoring.");
      mHeaderPos = 0;
      mBuffer.clear();
    }
  }
}

bool ObsImprov::isActive() const {
  return mImprovActive;
}

void ObsImprov::handleImprovMessage(std::vector<uint8_t> buffer) {
  if (buffer[static_cast<int>(Offset::TYPE)] == static_cast<int>(Type::RPC_COMMAND)) {
    log_d("received RPC command 0x%02x", buffer[2]);
    switch (static_cast<Command>(buffer[static_cast<int>(Offset::RPC_COMMAND)])) {
      case Command::WIFI_SETTINGS:
        handleRpcWifiSettings(buffer);
        break;
      case Command::GET_CURRENT_STATE:
        handleRpcGetCurrentState();
        break;
      case Command::GET_DEVICE_INFO:
        handleRpcGetDeviceInfo();
        break;
      default:
        log_w("Unsupported improv rpc command 0x%02x ignored.",
              buffer[static_cast<int>(Offset::RPC_COMMAND)]);
    }
  } else {
    log_w("Unsupported improv message type 0x%02x ignored.",
          buffer[static_cast<int>(Offset::TYPE)]);
  }
}

void ObsImprov::handleRpcGetCurrentState() const {
  State state = mWifiStatus();
  log_i("get current state -> 0x%02x", state);
  sendCurrentState(state);
  if (state == State::PROVISIONED) {
    sendWifiSuccess(Command::GET_CURRENT_STATE);
  }
}

void ObsImprov::handleRpcWifiSettings(std::vector<uint8_t> &buffer) const {
  const std::string ssid((char *) &buffer.data()[5], (size_t) buffer.data()[4]);
  const std::string password((char *) &buffer.data()[6 + buffer[4]], (size_t) buffer.data()[5 + buffer[4]]);
  log_i("received wifi ssid: %s.", ssid.c_str());
  if (mInitWifi(ssid, password)) {
    sendCurrentState(State::PROVISIONED);
    sendWifiSuccess();
  } else {
    sendErrorState(Error::UNABLE_TO_CONNECT);
  }
}

void ObsImprov::handleRpcGetDeviceInfo() const {
  log_i("get device info.");
  std::vector<uint8_t> response;
  response.push_back(static_cast<int>(Type::RPC_RESULT));
  response.push_back(0x00 /* LENGTH, to be set */);
  response.push_back(static_cast<int>(Command::GET_DEVICE_INFO));
  response.push_back(0x00 /* LENGTH payload to be set */);
  appendStringAndLength(response, mFirmwareName);
  appendStringAndLength(response, mFirmwareVersion);
  appendStringAndLength(response, mHardwareVariant);
  appendStringAndLength(response, mDeviceName);
  response[static_cast<int>(Offset::LENGTH)] = response.size() - 2;
  response[static_cast<int>(Offset::RPC_DATA_LENGTH)] = response.size() - 4;
  sendPayload(mSerial, response);
}

void ObsImprov::sendCurrentState(State state) const {
  std::vector<uint8_t> response;
  response.push_back(static_cast<int>(Type::CURRENT_STATE));
  response.push_back(0x01 /* LENGTH */);
  response.push_back(static_cast<int>(state));
  sendPayload(mSerial, response);
}

void ObsImprov::sendErrorState(Error error) const {
  std::vector<uint8_t> response;
  response.push_back(static_cast<int>(Type::ERROR_STATE));
  response.push_back(0x01 /* LENGTH */);
  response.push_back(static_cast<int>(error));
  sendPayload(mSerial, response);
}

void ObsImprov::sendWifiSuccess(Command cmd) const {
  std::vector<uint8_t> response;
  response.push_back(static_cast<int>(Type::RPC_RESULT));
  response.push_back(0x00 /* */); // LENGTH
  response.push_back(static_cast<int>(cmd));
  response.push_back(0x00 /* */); // LENGTH
  appendStringAndLength(response, mDeviceUrl());
  response[static_cast<int>(Offset::LENGTH)] = response.size() - 2;
  response[static_cast<int>(Offset::RPC_DATA_LENGTH)] = response.size() - 4;
  sendPayload(mSerial, response);
}

void ObsImprov::appendStringAndLength(std::vector<uint8_t> &response, std::string data) const {
  response.push_back(data.size());
  response.insert(response.end(), data.begin(), data.end());
  log_d("insert '%s'", data.c_str());
}

void ObsImprov::sendPayload(Stream *stream, std::vector<uint8_t> payload) const {
  stream->flush();

  std::vector<uint8_t> out;
  for (int i = 0; i < ObsImprov::HEADER_LENGTH; i++) {
    out.push_back((uint8_t) ObsImprov::HEADER[i]);
  }
  out.insert(out.end(), payload.begin(), payload.end());

  uint8_t calculated_checksum = 0;
  for (uint8_t byte: out) {
    calculated_checksum += byte;
  }
  out.push_back(calculated_checksum);
  out.push_back(SEPARATOR);

  stream->write(out.data(), out.size());
  stream->flush();
  ESP_LOG_BUFFER_HEXDUMP(__FILE__ "->send", out.data(), out.size(), ESP_LOG_INFO);
}

bool ObsImprov::isCompleteImprovMessage(std::vector<uint8_t> buffer) const {
  if (buffer.size() <= 2) {
    return false;
  }
  if (buffer.size() < (3 + buffer[1])) {
    return false;
  }
  return true;
}

bool ObsImprov::isValidImprovMessage(std::vector<uint8_t> buffer) const {
  log_d("received message of type: 0x%02x", buffer[0]);
  ESP_LOG_BUFFER_HEXDUMP(__FILE__ "<-receive", buffer.data(), buffer.size(), ESP_LOG_INFO);
  int dataLength = buffer[1];
  uint8_t calculated_checksum = 0xDE; /* from header */
  for (int pos = 0; pos < dataLength + 2; pos++) {
    calculated_checksum += buffer[pos];
  }
  if (calculated_checksum == buffer[dataLength + 2]) {
    log_d("Correct checksum 0x%02x", calculated_checksum);
  } else {
    log_w("Wrong checksum 0x%02x != 0x%02x", calculated_checksum, buffer[dataLength + 2]);
  }
  return calculated_checksum == buffer[dataLength + 2];
}
