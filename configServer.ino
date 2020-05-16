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

// Based on https://lastminuteengineers.com/esp32-ota-web-updater-arduino-ide/
// The information provided on the LastMinuteEngineers.com may be used, copied,
// remix, transform, build upon the material and distributed for any purposes
// only if provided appropriate credit to the author and link to the original article.

void handleForm() {
  String offsetS1 = server.arg("offsetS1");
  String offsetS2 = server.arg("offsetS2");
  String displayGPS = server.arg("displayGPS");
  String obsUserID = server.arg("obsUserID");
  String hostname = server.arg("hostname");
  //String confirmation = server.arg("confirmation");
  
   /* displayTest->clear();
    displayTest->drawString(64, 36, "displayConfig");
    displayTest->drawString(64, 48, displayGPS);*/
  if(displayGPS == "on")
    config.displayConfig |= 0x01;
  else
    config.displayConfig &= ~0x01;
    
  if(hostname.length()>0)
      strlcpy(config.hostname,hostname.c_str(),sizeof(config.hostname));
  if(obsUserID.length()>0)
      strlcpy(config.obsUserID,obsUserID.c_str(),sizeof(config.obsUserID));
  
    
  config.sensorOffsets[0] = atoi(offsetS1.c_str());
  config.sensorOffsets[1] = atoi(offsetS2.c_str());

  Serial.print("Offset Sensor 1:");
  Serial.println(config.sensorOffsets[0]);

  Serial.print("Offset Sensor 2:");
  Serial.println(config.sensorOffsets[1]);

  //Serial.print("confirmation:");
  //Serial.println(confirmation);

  // Create configuration file
  Serial.println(F("Saving configuration..."));
  saveConfiguration(configFilename, config);

  String s = "<meta http-equiv='refresh' content='0; url=/navigationIndex'><a href='/'> Go Back </a>";
  server.send(200, "text/html", s); //Send web page
}

void createConfigPage()
{
  String configPage = configIndexPrefix;
  configPage += "Offset S1<input name=offsetS1 placeholder='Offset Sensor 1' value=";
  if(config.sensorOffsets.size()>0)
  {
    configPage += String(config.sensorOffsets[0]);
  }
  configPage += "> ";
  configPage += "Offset S2<input name=offsetS2 placeholder='Offset Sensor 2' value=";
  if(config.sensorOffsets.size()>1)
  {
    configPage += String(config.sensorOffsets[1]);
  }
  configPage += "> ";
  // unused so far 
  // configPage +="<p>Which sensor values should be confirmed?.</p>"
  //"<input type=radio id=sensor1 name=confirmation value=0><label for=lid>Sensor 1</label><br>"
  //"<input type=radio id=sensor2 name=confirmation value=1><label for=case>Sensor 2</label><br>";
  
  configPage +="Upload Host<input name='hostname' placeholder='hostname' value=" + String(config.hostname) +" >";
  configPage +="Upload UserID<input name='obsUserID' placeholder='API ID' value=" + String(config.obsUserID) +" >";
  bool DisplayGPS = config.displayConfig & 0x01;
  if(DisplayGPS)
    configPage +="Display Satelites<input type='checkbox' name=displayGPS  checked=checked>";
  else
    configPage +="Display Satelites<input type='checkbox' name=displayGPS>";
  configPage+=configIndexPostfix;
  server.send(200, "text/html", configPage);
}

void handleReboot() {
  ESP.restart();
}

void wifiAction() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  Serial.print("ssid:");
  Serial.println(ssid);
  strlcpy(config.ssid,ssid.c_str(),sizeof(config.ssid));
  if(pass.length()>0)
      strlcpy(config.password,pass.c_str(),sizeof(config.password));
  
  // Create configuration file
  Serial.println(F("Saving configuration..."));
  saveConfiguration(configFilename, config);


  String s = "<meta http-equiv='refresh' content='0; url=/navigationIndex'><a href='/'> Go Back </a>";
  server.send(200, "text/html", s); //Send web page
}

bool CreateWifiSoftAP(String chipID)
{
  bool SoftAccOK;
  WiFi.disconnect();
  Serial.print(F("Initalize SoftAP "));
  String APName = "OpenBikeSensor-" + chipID;
  String APPassword = "12345678";
  SoftAccOK  =  WiFi.softAP(APName.c_str(), APPassword.c_str()); // Passwortlänge mindestens 8 Zeichen !
  delay(2000); // Without delay I've seen the IP address blank
  /* Soft AP network parameters */
  IPAddress apIP(172, 20, 0, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  if (SoftAccOK)
  {
    /* Setup the DNS server redirecting all the domains to the apIP */
    //dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    //dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println(F("successful."));
    displayTest->drawString(0, 30, APName.c_str());
    String passwordString = "Password: " + APPassword;
    displayTest->drawString(0, 40, passwordString.c_str());
    String connectString = "open page:172.20.0.1";
    displayTest->drawString(0, 50, connectString.c_str());
  } else
  {
    Serial.println(F("Soft AP Error."));
    Serial.println(APName.c_str());
    Serial.println(APPassword.c_str());
  }
  return SoftAccOK;
}

void startServer() {
#if defined(ESP32)
  uint64_t chipid_num;
  chipid_num = ESP.getEfuseMac();
  esp_chipid = String((uint16_t)(chipid_num >> 32), HEX);
  esp_chipid += String((uint32_t)chipid_num, HEX);
#endif
  // Connect to WiFi network
  Serial.println("Trying to connect to");
  Serial.println(config.ssid);
  WiFi.begin(config.ssid, config.password);
  Serial.println("ChipID  ");
  Serial.println(esp_chipid.c_str());
  // Wait for connection
  uint16_t startTime = millis();
  uint16_t timeout = 12000;
  Serial.printf("Timeout %u\n", timeout);
  Serial.printf("startTime %u\n", startTime);
  while ((WiFi.status() != WL_CONNECTED) && (( millis() - startTime) <= timeout)) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    CreateWifiSoftAP(esp_chipid);
  }
  else
  {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(config.ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    displayTest->drawString(0, 30, "connected to");
    displayTest->drawString(0, 40, config.ssid);
    displayTest->drawString(0, 50, WiFi.localIP().toString().c_str());
  }

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://openbikesensor.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", navigationIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  server.on("/navigationIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", navigationIndex);
  });
  server.on("/configIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    createConfigPage();
  });
  server.on("/wifiSettingsIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", wifiSettingsIndex);
  });
  server.on("/wifi_action_page", wifiAction);
  server.on("/action_page", handleForm);
  server.on("/reboot", handleReboot);

  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
  Serial.print("Ready");
}
