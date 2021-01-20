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
#include <SD.h>
#include <HTTPUpdate.h>
#include "globals.h"
#include "alpdata.h"

/* Download http://alp.u-blox.com/current_14d.alp (ssl?) if there is a new one */
// TODO: Error handling!!
// TODO: Feedback while download? How fast?
// TODO: https?
void AlpData::update() {
  String lastModified = loadLastModified();

  File f = SD.open(ALP_DATA_FILE_NAME, FILE_READ);
  if (!f || f.size() < 1024) {
    lastModified = "";
  } else {
    log_e("Existing file seems valid. Size is %d", f.size());
  }
  f.close();
  log_e("Existing file is from %s", lastModified.c_str());

  f = SD.open(ALP_NEW_DATA_FILE_NAME, FILE_WRITE);
  if (f) {
    HTTPClient httpClient;
    httpClient.begin(ALP_DOWNLOAD_URL);
    const char *lastModifiedHeaderName = "Last-Modified";
    const char *headers[] = {lastModifiedHeaderName};
    httpClient.collectHeaders(headers, 1);
    httpClient.setUserAgent(String("openbikesensor.org/") + String(OBSVersion));
    if (!lastModified.isEmpty()) {
      httpClient.addHeader("If-Modified-Since", lastModified);
    }
    int httpCode = httpClient.GET();
    log_e("Size: %d", httpClient.getSize());
    // ??? int expectedSize
    if (httpCode == 200) {
      String newLastModified = httpClient.header(lastModifiedHeaderName);
      int written = httpClient.writeToStream(&f);
      log_e("Written: %d", written);
      if (written < 0) {
        log_e("[HTTP] READ... failed, error: %s", httpClient.errorToString(httpCode).c_str());
      }
      log_e("Read %d bytes - all god! %s", written, newLastModified.c_str());
      f.close();
      SD.remove(ALP_DATA_FILE_NAME);
      delay(100);
      SD.rename(ALP_NEW_DATA_FILE_NAME, ALP_DATA_FILE_NAME);
      saveLastModified(newLastModified);

    } else if (httpCode == 304) {
      log_e("All fine, not modified!");
    } else if (httpCode > 0) {
      log_e("HTTP status: %d: %s", httpCode, httpClient.getString().c_str());
    } else {
      log_e("[HTTP] GET... failed, error: %s", httpClient.errorToString(httpCode).c_str());
    }
    f.close();

  }
}

bool AlpData::available() {
  return SD.exists(LAST_MODIFIED_HEADER_FILE_NAME);
}

void AlpData::saveLastModified(String header) {
  File f = SD.open(LAST_MODIFIED_HEADER_FILE_NAME, FILE_WRITE);
  f.print(header);
  f.close();
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

uint16_t AlpData::fill(uint8_t *data, uint16_t ofs, uint16_t dataSize) {
  if (!mAlpDataFile) {
    mAlpDataFile = SD.open(ALP_DATA_FILE_NAME, FILE_READ);
  }
  mAlpDataFile.seek(ofs);
  int read = mAlpDataFile.read(data, dataSize);
  if (read <= 0) {
    log_e("Failed to read got %d.", read);
    mAlpDataFile.close();
    mAlpDataFile = SD.open(ALP_DATA_FILE_NAME, FILE_READ);
    mAlpDataFile.seek(ofs);
    read = mAlpDataFile.read(data, dataSize);
    log_e("Read again: %d.", read);
  }
// not closing the file saves 10ms per message
// - need to take care when writing!
//  mAlpDataFile.close();
  return read;
}
