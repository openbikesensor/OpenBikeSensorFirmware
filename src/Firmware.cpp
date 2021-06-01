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
#include "Arduino.h"
#include "Firmware.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SD.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include "utils/cacerts.h"

static const String FLASH_APP_FILENAME("/sdflash/app.bin");
static const String OPEN_BIKE_SENSOR_FLASH_APP_PROJECT_NAME("OpenBikeSensorFlash");
static const size_t APP_PARTITION_SIZE = 0x380000; // read from part?
static const int SHA256_HASH_LEN = 32;

// todo: error handling
void Firmware::downloadToSd(String url, String filename) {
  WiFiClientSecure client;
  client.setCACert(trustedRootCACertificates);
  HTTPClient http;
  http.setUserAgent(mUserAgent);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (http.begin(client, url)) {
    int status = http.GET();
    log_i("Opening %s got %d.", url.c_str(), status);
    log_i("http size: %d. ", http.getSize());
    log_i("Will access Stream:, free heap %dkb", ESP.getFreeHeap() / 1024);
    if (status == 200) {
      File newFile = SD.open(filename, FILE_WRITE);
      int written = http.writeToStream(&newFile);
      newFile.close();
      log_i("Got %d bytes", written);
    }
  }
  http.end();
}

bool Firmware::downloadToFlash(String url,
                               std::function<void(uint32_t pos, uint32_t size)> progress) {
  bool success = false;
  WiFiClientSecure client;
  client.setCACert(trustedRootCACertificates);
  HTTPClient http;
  http.setUserAgent(mUserAgent);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (http.begin(client, url)) {
    progress(0, 999);
    int status = http.GET();
    log_i("Opening %s got %d.", url.c_str(), status);
    size_t size = http.getSize();
    log_i("http size: %d. ", size);
    log_i("Will access Stream:, free heap %dkb", ESP.getFreeHeap() / 1024);
    if (status == 200) {
      Update.begin();
      Update.onProgress([progress, size](size_t pos, size_t all) {
        progress(pos, size);
      });

      Stream& stream = http.getStream();
      byte buffer[256];
      size_t read;
      size_t written = 0;
      while ((read = stream.readBytes(&buffer[0], sizeof(buffer))) > 0) {
        Update.write(buffer, read);
        written += read;
      }
      log_i("Got %d bytes", written);
    }
  }
  http.end();
  if (Update.end(true)) { //true to set the size to the current progress
    mLastMessage = "Success";

    // Suppress the ota Firmware switch!
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_set_boot_partition(running);
    success = true;
  } else {
    mLastMessage = Update.errorString();
    log_e("Update: %s", mLastMessage.c_str());
  }
  return success;
}

String Firmware::getLastMessage() {
  return mLastMessage;
};

const esp_partition_t* Firmware::findEspFlashAppPartition() {
  const esp_partition_t *part
    = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);

  esp_app_desc_t app_desc;
  esp_err_t ret = ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ota_get_partition_description(part, &app_desc));

  if (ret != ESP_OK || OPEN_BIKE_SENSOR_FLASH_APP_PROJECT_NAME != app_desc.project_name) {
    part
      = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1,
                                 nullptr);
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ota_get_partition_description(part, &app_desc));
  }

  if (ret == ESP_OK && OPEN_BIKE_SENSOR_FLASH_APP_PROJECT_NAME == app_desc.project_name) {
    return part;
  } else {
    return nullptr;
  }
}

String Firmware::getFlashAppVersion() {
  const esp_partition_t *part = findEspFlashAppPartition();
  esp_app_desc_t app_desc;
  esp_err_t ret = ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ota_get_partition_description(part, &app_desc));
  String version;
  if (ret == ESP_OK) {
    version = app_desc.version;
  }
  return version;
}

static void calculateSha256(File &f, uint8_t *shaResult, int32_t bytes) {
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);

  const uint16_t bufferSize = 8192;
  uint8_t *buffer = static_cast<uint8_t *>(malloc(bufferSize));
  uint32_t toRead = bufferSize;
  uint32_t read;
  uint32_t readFromFile = 0;
  while ((read = f.read(buffer, toRead)) > 0) {
    mbedtls_md_update(&ctx, buffer, read);
    readFromFile += read;
    if (readFromFile + toRead > bytes) {
      toRead = bytes - readFromFile;
    }
  }
  free(buffer);
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);
}

String Firmware::checkSdFirmware() {
  String error;
  File f = SD.open(FLASH_APP_FILENAME, FILE_READ);
  if (!f) {
    error = "Failed to find '" + FLASH_APP_FILENAME + "' on sd card.";
    return error;
  }
  const int32_t fileSize = f.size();
  if (fileSize > APP_PARTITION_SIZE) {
    error = "Firmware to flash is to large. ";
  } else {
    uint8_t shaResult[SHA256_HASH_LEN];
    calculateSha256(f, shaResult, fileSize - SHA256_HASH_LEN);

    for (int i = 0; i < SHA256_HASH_LEN; i++) {
      int c = f.read();
      if (shaResult[i] != c) {
        error = "Checksum mismatch.";
        break;
      }
    }
  }
  f.close();
  return error;
}

bool Firmware::switchToFlashApp() {
  bool success = false;
  const esp_partition_t *part = findEspFlashAppPartition();
  if (part) {
    success = (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ota_set_boot_partition(part)) == ESP_OK);
  }
  return success;
}
