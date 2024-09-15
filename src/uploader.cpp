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

#include "uploader.h"

#include "globals.h"
#include "utils/multipart.h"
#include "utils/timeutils.h"
#include "writer.h"

// https://docs.platformio.org/en/latest/platforms/espressif32.html#embedding-binary-data
extern const uint8_t x509_crt_bundle_start[] asm("_binary_src_truststore_x509_crt_bundle_start");

static char const *const HTTP_LOCATION_HEADER = "location";

Uploader::Uploader(String portalUrl, String userToken) :
    mPortalUrl(std::move(portalUrl)),
    mPortalUserToken(std::move(userToken)) {
  TimeUtils::setClockByNtpAndWait();
  mWiFiClient.setCACertBundle(x509_crt_bundle_start);
}

/* Upload file as track data to "The Portal" as multipart form data.
 */
bool Uploader::upload(const String& fileName) {
  bool success = false;
  if(fileName.substring(0,7) != "/sensor"
      && !fileName.endsWith(CSVFileWriter::EXTENSION)) {
    log_e("Not sending %s wrong extension.", fileName.c_str());
    mLastStatusMessage = "Not sending " + fileName + " wrong extension.";
  } else {
    log_d("Sending '%s'.", fileName.c_str());
    File csvFile = SD.open(fileName.c_str(), "r");
    if (csvFile) {
      success = uploadFile(csvFile);
      csvFile.close();
    } else {
      log_e("file %s not found", fileName.c_str());
      mLastStatusMessage = "File " + fileName + " not found!";
    }
  }
  return success;
}

bool Uploader::uploadFile(File &file) {
  bool success = false;
  HTTPClient https;
  https.setTimeout(30 * 1000); // give the api some time
  https.setUserAgent(String("OBS/") + String(OBSVersion));

  const char* headers[] = { HTTP_LOCATION_HEADER };
  https.collectHeaders(headers, 1);
  if (https.begin(mWiFiClient, mPortalUrl + "/api/tracks")) { // HTTPS
    https.addHeader("Authorization", "OBSUserId " + mPortalUserToken);
    https.addHeader("Content-Type", "application/json");
    const String fileName = file.name();
    const String displayFileName = ObsUtils::stripCsvFileName(fileName);
    MultipartStream mp(&https);
    MultipartDataString title("title", "AutoUpload " + displayFileName);
    mp.add(title);
    MultipartDataString description("description", "Uploaded with OpenBikeSensor " + String(OBSVersion));
    mp.add(description);
    MultipartDataStream data("body", fileName, &file, "text/csv");
    mp.add(data);
    mp.last();
    const size_t contentLength = mp.predictSize();
    mp.setProgressListener([contentLength](size_t pos) {
      obsDisplay->drawProgressBar(5, pos, contentLength);
    });

    int httpCode = https.sendRequest("POST", &mp, contentLength);

    mLastStatusMessage = String(httpCode) + ": ";
    if (httpCode < 0) {
      mLastStatusMessage += HTTPClient::errorToString(httpCode);
    // This might be a large body, could be any wrong web url configured!
    } else if (https.getSize() < 1024) {
      mLastStatusMessage += https.getString();
    } else {
      mLastStatusMessage += "Length: " + String(https.getSize());
    }
    mLastLocation = https.header(HTTP_LOCATION_HEADER);
    if (httpCode == 200 || httpCode == 201) {
      log_v("HTTP OK %s", mLastStatusMessage.c_str());
      success = true;
    } else {
      log_e("HTTP Error %s", mLastStatusMessage.c_str());
      https.end();
    }
  } else {
    mLastStatusMessage = "[HTTPS] begin to " + mPortalUrl + " failed.";
    log_e("HTTPS error %s", mLastStatusMessage.c_str());
    https.end();
  }
  return success;
}

String Uploader::getLastStatusMessage() const {
  return mLastStatusMessage;
}

String Uploader::getLastLocation() const {
  return mLastLocation;
}
