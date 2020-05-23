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

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

void configAction() {
  
  String offsetS1 = server.arg("offsetS1");
  String offsetS2 = server.arg("offsetS2");
  String satsForFix = server.arg("satsForFix");
  String displayGPS = server.arg("displayGPS");
  String displayBoth = server.arg("displayBoth");
  String displayVELO = server.arg("displayVELO");
  String swapSensors = server.arg("swapSensors");
  String obsUserID = server.arg("obsUserID");
  String hostname = server.arg("hostname");
  
  //String confirmation = server.arg("confirmation");
  
   /* displayTest->clear();
    displayTest->drawString(64, 36, "displayConfig");
    displayTest->drawString(64, 48, displayGPS);*/
  if(displayGPS == "on")
    config.displayConfig |= DisplaySatelites;
  else
    config.displayConfig &= ~DisplaySatelites;

  if(displayBoth == "on")
    config.displayConfig |= DisplayBoth;
  else
    config.displayConfig &= ~DisplayBoth;

  if(displayVELO == "on")
    config.displayConfig |= DisplayVelocity;
  else
    config.displayConfig &= ~DisplayVelocity;

  if(swapSensors == "on")
    config.swapSensors = 1;
  else
    config.swapSensors = 0;
    
  strlcpy(config.hostname,hostname.c_str(),sizeof(config.hostname));
  strlcpy(config.obsUserID,obsUserID.c_str(),sizeof(config.obsUserID));
    
  config.sensorOffsets[0] = atoi(offsetS1.c_str());
  config.sensorOffsets[1] = atoi(offsetS2.c_str());
  config.satsForFix = atoi(satsForFix.c_str());
  config.satsForFix = atoi(satsForFix.c_str());

  Serial.print("Offset Sensor 1:");
  Serial.println(config.sensorOffsets[0]);

  Serial.print("Offset Sensor 2:");
  Serial.println(config.sensorOffsets[1]);

  //Serial.print("confirmation:");
  //Serial.println(confirmation);

  // Print and safe config
  Serial.println(F("Print config file..."));
  printConfig(config);
  Serial.println(F("Saving configuration..."));
  saveConfiguration(configFilename, config);

  String s = "<meta http-equiv='refresh' content='0; url=/'><a href='/'> Go Back </a>";
  server.send(200, "text/html", s); //Send web page
}

void wifiAction() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  Serial.print("ssid:");
  Serial.println(ssid);

  #ifdef dev
    Serial.print(F("pass = "));
    Serial.println(pass);
  #endif

  // Write always both data, thus the WIFI config can be overwritten
  strlcpy(config.ssid,ssid.c_str(),sizeof(config.ssid));
  strlcpy(config.password,pass.c_str(),sizeof(config.password));

  // Print and safe config
  Serial.println(F("Print config file..."));
  printConfig(config);
  Serial.println(F("Saving configuration..."));
  saveConfiguration(configFilename, config);

  String s = "<meta http-equiv='refresh' content='0; url=/'><a href='/'> Go Back </a>";
  server.send(200, "text/html", s);
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

  displayTest->showTextOnGrid(0, 2, "AP:");
  displayTest->showTextOnGrid(1, 2, "");
  displayTest->showTextOnGrid(0, 3, APName.c_str());

  
  WiFi.softAPConfig(apIP, apIP, netMsk);
  if (SoftAccOK)
  {
    /* Setup the DNS server redirecting all the domains to the apIP */
    //dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    //dnsServer.start(DNS_PORT, "*", apIP);

    Serial.println(F("AP successful."));

    displayTest->showTextOnGrid(0, 4, "Pass:");
    displayTest->showTextOnGrid(1, 4, APPassword);
    
    displayTest->showTextOnGrid(0, 5, "IP:");
    displayTest->showTextOnGrid(1, 5, "172.20.0.1");
  } 
  else
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

  displayTest->clear();
  displayTest->showTextOnGrid(0, 0, "Ver.:");
  displayTest->showTextOnGrid(1, 0, OBSVersion);

  displayTest->showTextOnGrid(0, 1, "SSID:");
  displayTest->showTextOnGrid(1, 1, config.ssid);


  // Connect to WiFi network
  Serial.println("Trying to connect to");
  Serial.println(config.ssid);
  WiFi.begin(config.ssid, config.password);
  Serial.println("ChipID  ");
  Serial.println(esp_chipid.c_str());
  // Wait for connection
  uint16_t startTime = millis();
  uint16_t timeout = 5000;
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

    displayTest->showTextOnGrid(0, 2, "IP:");
    displayTest->showTextOnGrid(1, 2, WiFi.localIP().toString().c_str());
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


  // #############################################
  // Handle web pages
  // #############################################

  // ### Reboot ###

  server.on("/reboot", HTTP_GET, []() {

    String html = rebootIndex;
    // Header
    html.replace("{action}", "");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Reboot");

    server.send(200, "text/html", html);

    delay(100);
    ESP.restart();
  });

  // ### Index ###
  
  server.on("/", HTTP_GET, []() {
    String html = navigationIndex;
    // Header
    html.replace("{action}", "");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Navigation");
    
    server.send(200, "text/html", html);
  });

  // ### Wifi ###

  server.on("/wifi_action", wifiAction);

  server.on("/wifi", HTTP_GET, []() {
    String html = wifiSettingsIndex;
    // Header
    html.replace("{action}", "/wifi_action");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Wifi Config");
    // Form data
    html.replace("{ssid}", config.ssid);
    
    server.send(200, "text/html", html);
  });

  // ### Config ###

  server.on("/config_action", configAction);

  server.on("/config", HTTP_GET, []() {
    String html = configIndex;
    // Header
    html.replace("{action}", "/config_action");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Config");
    // Form data
    html.replace("{offset1}", String(config.sensorOffsets[0]));
    html.replace("{offset2}", String(config.sensorOffsets[1]));
    html.replace("{satsForFix}", String(config.satsForFix));
    html.replace("{hostname}", String(config.hostname));
    html.replace("{userId}", String(config.obsUserID));

    bool displayGPS = config.displayConfig & DisplaySatelites;
    bool displayBoth = config.displayConfig & DisplayBoth;
    bool displayVelo = config.displayConfig & DisplayVelocity;
    bool swapSensors = config.swapSensors;

    html.replace("{displayGPS}", displayGPS ? "checked" : "");
    html.replace("{displayBoth}", displayBoth ? "checked" : "");
    html.replace("{displayVELO}", displayVelo ? "checked" : "");
    html.replace("{swapSensors}", swapSensors ? "checked" : "");
    
    server.send(200, "text/html", html);
  });

  // ### Update Firmware ###

  server.on("/update", HTTP_GET, []() {
    String html = uploadIndex;
    // Header
    html.replace("{action}", ""); // Handled by XHR
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Update Firmware");
    
    server.send(200, "text/html", html);
  });

  // Handling uploading firmware file
  server.on("/update", HTTP_POST, []() {
    Serial.println("Send response...");
    if(Update.hasError()) {
      server.send(500, "text/plain", "Update fails!");
    } else {
      server.send(200, "text/plain", "Update successful!");
      delay(250);
      ESP.restart();      
    }
  }, []() {
    //Serial.println('Update Firmware...');
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

  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("Server Ready");
}
