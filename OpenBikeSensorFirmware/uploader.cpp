/*
  Copyright (C) 2020 HLRS, Uni-Stuttgart
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

#include "uploader.h"

#include "config.h"
#include "globals.h"
#include "utils/multipart.h"

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <SD.h>
#include <FS.h>

// Telekom rootCA certificate
const char *rootCACertificate =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIDwzCCAqugAwIBAgIBATANBgkqhkiG9w0BAQsFADCBgjELMAkGA1UEBhMCREUx\n"
  "KzApBgNVBAoMIlQtU3lzdGVtcyBFbnRlcnByaXNlIFNlcnZpY2VzIEdtYkgxHzAd\n"
  "BgNVBAsMFlQtU3lzdGVtcyBUcnVzdCBDZW50ZXIxJTAjBgNVBAMMHFQtVGVsZVNl\n"
  "YyBHbG9iYWxSb290IENsYXNzIDIwHhcNMDgxMDAxMTA0MDE0WhcNMzMxMDAxMjM1\n"
  "OTU5WjCBgjELMAkGA1UEBhMCREUxKzApBgNVBAoMIlQtU3lzdGVtcyBFbnRlcnBy\n"
  "aXNlIFNlcnZpY2VzIEdtYkgxHzAdBgNVBAsMFlQtU3lzdGVtcyBUcnVzdCBDZW50\n"
  "ZXIxJTAjBgNVBAMMHFQtVGVsZVNlYyBHbG9iYWxSb290IENsYXNzIDIwggEiMA0G\n"
  "CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCqX9obX+hzkeXaXPSi5kfl82hVYAUd\n"
  "AqSzm1nzHoqvNK38DcLZSBnuaY/JIPwhqgcZ7bBcrGXHX+0CfHt8LRvWurmAwhiC\n"
  "FoT6ZrAIxlQjgeTNuUk/9k9uN0goOA/FvudocP05l03Sx5iRUKrERLMjfTlH6VJi\n"
  "1hKTXrcxlkIF+3anHqP1wvzpesVsqXFP6st4vGCvx9702cu+fjOlbpSD8DT6Iavq\n"
  "jnKgP6TeMFvvhk1qlVtDRKgQFRzlAVfFmPHmBiiRqiDFt1MmUUOyCxGVWOHAD3bZ\n"
  "wI18gfNycJ5v/hqO2V81xrJvNHy+SE/iWjnX2J14np+GPgNeGYtEotXHAgMBAAGj\n"
  "QjBAMA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/BAQDAgEGMB0GA1UdDgQWBBS/\n"
  "WSA2AHmgoCJrjNXyYdK4LMuCSjANBgkqhkiG9w0BAQsFAAOCAQEAMQOiYQsfdOhy\n"
  "NsZt+U2e+iKo4YFWz827n+qrkRk4r6p8FU3ztqONpfSO9kSpp+ghla0+AGIWiPAC\n"
  "uvxhI+YzmzB6azZie60EI4RYZeLbK4rnJVM3YlNfvNoBYimipidx5joifsFvHZVw\n"
  "IEoHNN/q/xWA5brXethbdXwFeilHfkCoMRN3zUA7tFFHei4R40cR3p1m0IvVVGb6\n"
  "g1XqfMIpiRvpb7PO4gWEyS8+eIVibslfwXhjdFjASBgMmTnrpMwatXlajRWc2BQN\n"
  "9noHV8cigwUtPJslJj0Ys6lDfMjIq2SPDqO/nBudMNva0Bkuqjzx+zOAduTNrRlP\n"
  "BSeOE6Fuwg==\n"
  "-----END CERTIFICATE-----\n";

// Not sure if WiFiClientSecure checks the validity date of the certificate.
// Setting clock just to be sure...
void uploader::setClock() {
  configTime(0, 0, "rustime01.rus.uni-stuttgart.de", "time.nist.gov");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
  Serial.print(F("Waiting for NTP time sync done: "));
}

uploader *uploader::inst = nullptr;

uploader::uploader() {
  setClock();

  Serial.print("WiFiClientSecure\n");
  client = new WiFiClientSecure;
  Serial.print("WiFiClientSecure2\n");
  if (client) {
    client->setCACert(rootCACertificate);
  } else {
    Serial.println("Unable to create client");
  }
}

/* Upload data to "The Portal".
 *
 * File data can be sent nearly directly since the only thing we have
 * to escape inside the JSON string is the newline, we do not use other
 * characters that need escaping in the CSV (for now) (" & \)
 *
 */
bool uploader::upload(const String& fileName) {
  int number=0;
  if(fileName.substring(0,7)!="/sensor") {
    Serial.printf(("not sending " + fileName + "\n").c_str());
    return false;
  }
  Serial.printf(("sending " + fileName + "\n").c_str());
  sscanf(fileName.c_str(),"/sensorData%d",&number);

  File csvFile = SD.open(fileName.c_str(), "r");
  if (csvFile) {
    HTTPClient https;
    https.setUserAgent(String("OBS/") + String(OBSVersion));
    boolean res = false;

    // USE CONGIG !!res = https.begin(*client, "http://192.168.98.51:3000/api/tracks");
    res = https.begin(*client, "https://openbikesensor.hlrs.de/api/tracks");
    https.addHeader("Authorization", "OBSUserId " + String(config.obsUserID));
    https.addHeader("Content-Type", "application/json");

    if (res) { // HTTPS
      MultipartStream mp(&https);
      MultipartDataString title("title", "AutoUpload " + String(number));
      mp.add(title);
      MultipartDataString description("description", "Uploaded with OpenBikeSensor " + String(OBSVersion));
      mp.add(description);
      MultipartDataStream data("body", fileName, &csvFile, "text/csv");
      mp.add(data);
      mp.last();
      int httpCode = https.sendRequest("POST", &mp, mp.predictSize());
      if (httpCode != 200) {
        Serial.printf("[HTTPS] POST... code: %d\n", httpCode);
        String payload = https.getString();
        Serial.println(payload);
        return false;
      }
      https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
      return false;
    }
  } else {
    Serial.printf(("file " + fileName + " not found\n").c_str());
    return false;
  }
  return true;
}

void uploader::destroy() {
  delete client;
}
