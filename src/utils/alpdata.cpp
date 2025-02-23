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

#include <SD.h>
#include <HTTPUpdate.h>
#include "globals.h"
#include "alpdata.h"
#include "timeutils.h"

extern const uint8_t x509_crt_bundle_start[] asm("_binary_src_truststore_x509_crt_bundle_start");

/* Download http://alp.u-blox.com/current_14d.alp (ssl?) if there is a new one
 * Takes 5 seconds to update the data.
*/
void AlpData::update(DisplayDevice *display) {
  String lastModified = loadLastModified();

  File f = SD.open(ALP_DATA_FILE_NAME, FILE_READ);
  const time_t lastWrite = f.getLastWrite();
  if (!f || f.size() < ALP_DATA_MIN_FILE_SIZE ) {
    lastModified = "";
  } else if (lastWrite > TimeUtils::PAST_TIME &&
    time(nullptr) - lastWrite < 4 * 24 * 60 * 60) {
    log_d("File still current %s",
          TimeUtils::dateTimeToString(lastWrite).c_str());
    log_d("Now: %s",
          TimeUtils::dateTimeToString(time(nullptr)).c_str());
    log_d("Next Update: %s",
          TimeUtils::dateTimeToString(lastWrite + 4 * 24 * 60 * 60).c_str());
    f.close();
    display->showTextOnGrid(0, 5, "ALP not checked.");
    return;
  }
  f.close();
  log_d("Existing file last write %s", TimeUtils::dateTimeToString(f.getLastWrite()).c_str());
  log_d("Existing file is from %s", lastModified.c_str());
  display->showTextOnGrid(0, 5, "ALP data...");

  WiFiClientSecure wiFiClient;
  wiFiClient.setCACertBundle(x509_crt_bundle_start);
  HTTPClient httpClient;
  httpClient.begin(wiFiClient, ALP_DOWNLOAD_URL);
  const char *lastModifiedHeaderName = "Last-Modified";
  const char *headers[] = {lastModifiedHeaderName};
  httpClient.collectHeaders(headers, 1);
  // be polite - tell the server who we are
  httpClient.setUserAgent(String("openbikesensor.org/") + String(OBSVersion));
  if (!lastModified.isEmpty()) {
    httpClient.addHeader("If-Modified-Since", lastModified);
  }
  int httpCode = httpClient.GET();
  log_i("Size: %d", httpClient.getSize());
  if (httpCode == 200) {
    String newLastModified = httpClient.header(lastModifiedHeaderName);
    File newFile = SD.open(ALP_NEW_DATA_FILE_NAME, FILE_WRITE);
    const int written = httpClient.writeToStream(&newFile);
    newFile.close();
    log_e("Written: %d", written);
    if (written < ALP_DATA_MIN_FILE_SIZE) {
      displayHttpClientError(display, written);
    } else {
      log_e("Read %d bytes - all good! %s", written, newLastModified.c_str());
      SD.remove(ALP_DATA_FILE_NAME);
      SD.rename(ALP_NEW_DATA_FILE_NAME, ALP_DATA_FILE_NAME);
      saveLastModified(newLastModified);
      display->showTextOnGrid(0, 5, "ALP data updated!");
    }
  } else if (httpCode == 304) { // Not-Modified
    display->showTextOnGrid(0, 5, "ALP data was up to date.");
    log_i("All fine, not modified!");
  } else if (httpCode > 0) {
    display->showTextOnGrid(0,4, String("ALP data failed ") + String(httpCode).c_str());
    if (httpClient.getSize() < 200) {
      display->showTextOnGrid(0, 5, httpClient.getString().c_str());
    }
  } else {
    displayHttpClientError(display, httpCode);
  }
  httpClient.end();
}

void AlpData::displayHttpClientError(DisplayDevice *display, int httpError) {
  display->showTextOnGrid(0, 4, String("ALP data failed ") + String(httpError).c_str());
  String errorString = HTTPClient::errorToString(httpError);
  display->showTextOnGrid(0, 5, errorString.c_str());
  log_e("[HTTP] GET... failed, error %d: %s", httpError, errorString.c_str());
}

bool AlpData::available() {
  return SD.exists(LAST_MODIFIED_HEADER_FILE_NAME);
}

void AlpData::saveLastModified(const String &header) {
  File f = SD.open(LAST_MODIFIED_HEADER_FILE_NAME, FILE_WRITE);
  if (f) {
    f.print(header);
    f.close();
  }
}

String AlpData::loadLastModified() {
  File f = SD.open(LAST_MODIFIED_HEADER_FILE_NAME, FILE_READ);
  String lastModified = String();
  if (f) {
    lastModified = f.readString();
  }
  f.close();
  return lastModified;
}

uint16_t AlpData::fill(uint8_t *data, size_t ofs, uint16_t dataSize) {
  if (!mAlpDataFile) {
    mAlpDataFile = SD.open(ALP_DATA_FILE_NAME, FILE_READ);
  }
  int read = -1;
  if (mAlpDataFile.seek(ofs)) {
    read = mAlpDataFile.read(data, dataSize);
  }
  if (read <= 0) {
    log_e("Failed to read got %d.", read);
    mAlpDataFile.close();
    mAlpDataFile = SD.open(ALP_DATA_FILE_NAME, FILE_READ);
    if (mAlpDataFile.seek(ofs)) {
      read = mAlpDataFile.read(data, dataSize);
    }
    log_e("Read again: %d.", read);
  }
// not closing the file saves 10ms per message
// - need to take care when writing!
  return read;
}

/* Used to save AidIni data. */
void AlpData::saveMessage(const uint8_t *data, size_t size) {
  File f = SD.open(AID_INI_DATA_FILE_NAME, FILE_WRITE);
  if (f) {
    size_t written = f.write(data, size);
    f.close();
    if (written != size) {
      log_e("Written only %d of %d bytes", written, size);
    } else {
      log_d("Written %d bytes", written);
    }
  }
}

/* Used to save AidIni data. */
size_t AlpData::loadMessage(uint8_t *data, size_t size) {
  size_t result = 0;
  File f = SD.open(AID_INI_DATA_FILE_NAME, FILE_READ);
  if (f) {
    result = f.read(data, size);
    f.close();
    log_d("Read %d bytes", result);
    SD.remove(AID_INI_DATA_FILE_NAME);
  }
  return result;
}

void AlpData::save(const uint8_t *data, size_t offset, int length) {
#ifdef RANDOM_ACCESS_FILE_AVAILAVLE
  // this is currently not possible to seek and modify data within an existing
  // file, we can only append :(
  if (!mAlpDataFile) {
    mAlpDataFile = SD.open(ALP_DATA_FILE_NAME, FILE_APPEND);
  }
  int written = -99;
  if (mAlpDataFile.seek(offset)) {
    written = mAlpDataFile.write(data, length);
  }
  if (written <= 0) {
    log_e("Failed to write got %d.", written);
    mAlpDataFile.close();
    mAlpDataFile = SD.open(ALP_DATA_FILE_NAME, FILE_APPEND);
    if (mAlpDataFile.seek(offset)) {
      written = mAlpDataFile.write(data, length);
    }
    log_e("Write again: %d.", written);
  }
#endif
}
