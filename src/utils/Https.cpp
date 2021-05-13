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

#include "Https.h"
#include <SPIFFS.h>
#include <FS.h>

using namespace httpsserver;

// "20190101000000"
static std::string toCertDate(time_t theTime) {
  char date[32];
  tm timeStruct;
  localtime_r(&theTime, &timeStruct);
  snprintf(date, sizeof(date),
           "%04d%02d%02d%02d%02d%02d",
           timeStruct.tm_year + 1900, timeStruct.tm_mon + 1, timeStruct.tm_mday,
           timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec);
  return std::string(date);
}



/* Load or create cert.
 * Based on https://github.com/fhessel/esp32_https_server/blob/de1876cf6fe717cf236ad6603a97e88f22e38d62/examples/REST-API/REST-API.ino#L219
 */
SSLCert *Https::getCertificate() {
  // Try to open key and cert file to see if they exist
  File keyFile = SPIFFS.open("/key.der");
  File certFile = SPIFFS.open("/cert.der");

  // If now, create them
  if (!keyFile || !certFile || keyFile.size()==0 || certFile.size()==0) {
    log_i("No certificate found in SPIFFS, generating a new one for you.");
    log_i("If you face a Guru Meditation, give the script another try (or two...).");
    log_i("This may take up to a minute, so please stand by :)");

    SSLCert * newCert = new SSLCert();
    // The part after the CN= is the domain that this certificate will match, in this
    // case, it's esp32.local.
    // However, as the certificate is self-signed, your browser won't trust the server
    // anyway.

    std::string fromDate;
    std::string toDate;
    time_t now = time(nullptr);
    if (now > (2020 - 1970) * 365 * 24 * 60 * 60) {
      fromDate = toCertDate(now);
      toDate = toCertDate(now + 365 * 24 * 60 * 60);
    } else {
      fromDate = "20210513090700";
      toDate = "20301232235959";
    }

    int res = createSelfSignedCert(*newCert,
                                   KEYSIZE_1024,
                                   "CN=obs.local,O=openbikesensor.org,C=DE",
                                   fromDate,
                                   toDate);
    if (res == 0) {
      // We now have a certificate. We store it on the SPIFFS to restore it on next boot.

      bool failure = false;
      // Private key
      keyFile = SPIFFS.open("/key.der", FILE_WRITE);
      if (!keyFile || !keyFile.write(newCert->getPKData(), newCert->getPKLength())) {
        log_e("Could not write /key.der");
        failure = true;
      }
      if (keyFile) keyFile.close();

      // Certificate
      certFile = SPIFFS.open("/cert.der", FILE_WRITE);
      if (!certFile || !certFile.write(newCert->getCertData(), newCert->getCertLength())) {
        log_e("Could not write /cert.der");
        failure = true;
      }
      if (certFile) certFile.close();

      if (failure) {
        log_e("Certificate could not be stored permanently, generating new certificate on reboot...");
      }

      return newCert;

    } else {
      // Certificate generation failed. Inform the user.
      log_e("An error occured during certificate generation.");
      log_e("Error code is 0x%04x", res);
      log_e("You may have a look at SSLCert.h to find the reason for this error.");
      return nullptr;
    }

  } else {
    log_i("Reading certificate from SPIFFS.");

    // The files exist, so we can create a certificate based on them
    size_t keySize = keyFile.size();
    size_t certSize = certFile.size();

    uint8_t * keyBuffer = new uint8_t[keySize];
    if (keyBuffer == nullptr) {
      log_e("Not enough memory to load privat key");
      return nullptr;
    }
    uint8_t * certBuffer = new uint8_t[certSize];
    if (certBuffer == nullptr) {
      delete[] keyBuffer;
      log_e("Not enough memory to load certificate");
      return nullptr;
    }
    keyFile.read(keyBuffer, keySize);
    certFile.read(certBuffer, certSize);

    // Close the files
    keyFile.close();
    certFile.close();
    log_i("Read %u bytes of certificate and %u bytes of key from SPIFFS", certSize, keySize);
    return new SSLCert(certBuffer, certSize, keyBuffer, keySize);
  }
}

bool Https::removeCertificate() {
  SPIFFS.remove("/key.der");
  SPIFFS.remove("/cert.der");
  return !SPIFFS.exists("/key.der") && !SPIFFS.exists("/cert.der");
}
