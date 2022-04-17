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

#include "obsutils.h"
#include <esp_wifi.h>
#include "writer.h"
#include "BLEServer.h"

static const uint32_t BYTES_PER_KIB = 1 << 10;
static const uint32_t BYTES_PER_MIB = 1 << 20;
static const uint32_t BYTES_PER_GIB = 1 << 30;

String ObsUtils::createTrackUuid() {
  uint8_t data[16];
  esp_fill_random(data, 16);
  return String(BLEUUID(data, 16, false).toString().c_str());
}

String ObsUtils::stripCsvFileName(const String &fileName) {
  String userPrintableFilename = fileName.substring(fileName.lastIndexOf("/") + 1);
  if (userPrintableFilename.endsWith(CSVFileWriter::EXTENSION)) {
    userPrintableFilename
      = userPrintableFilename.substring(
      0, userPrintableFilename.length() - CSVFileWriter::EXTENSION.length());
  }
  return userPrintableFilename;
}

String ObsUtils::encodeForXmlAttribute(const String &text) {
  String result(text);
  result.replace("&", "&amp;");
  result.replace("<", "&lt;");
  result.replace(">", "&gt;");
  result.replace("'", "&#39;");
  result.replace("\"", "&#34;");
  return result;
}

String ObsUtils::encodeForXmlText(const String &text) {
  String result(text);
  result.replace("&", "&amp;");
  result.replace("<", "&lt;");
  return result;
}

String ObsUtils::encodeForCsvField(const String &field) {
  String result(field);
  result.replace(';', '_');
  result.replace('\n', ' ');
  result.replace('\r', ' ');
  return result;
}

// Poor man....
String ObsUtils::encodeForUrl(const String &url) {
  String result(url);
  result.replace("%", "%25");
  result.replace("+", "%2B");
  result.replace(" ", "+");
  result.replace("\"", "%22");
  result.replace("\'", "%27");
  result.replace("=", "%3D");
  return result;
}

String ObsUtils::toScaledByteString(uint64_t size) {
  String result;
  if (size <= BYTES_PER_KIB * 10) {
    result = String((uint32_t) size) + "B";
  } else if (size <= BYTES_PER_MIB * 10) {
    result = String((uint32_t) (size >> 10)) + "KiB";
  } else if (size <= BYTES_PER_GIB * 10) {
    result = String((uint32_t) (size >> 20)) + "MiB";
  } else {
    result = String((uint32_t) (size >> 30)) + "GiB";
  }
  return result;
}

void ObsUtils::logHexDump(const uint8_t *buffer, uint16_t length) {
  ESP_LOG_BUFFER_HEXDUMP(__FILE__, buffer, length, ESP_LOG_WARN);
}

String ObsUtils::sha256ToString(byte *sha256) {
  const int HASH_LEN = 32;
  char hash_print[HASH_LEN * 2 + 1];
  hash_print[HASH_LEN * 2] = 0;
  for (int i = 0; i < HASH_LEN; ++i) {
    snprintf(&hash_print[i * 2], 3, "%02x", sha256[i]);
  }
  return String(hash_print);
}
