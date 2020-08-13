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

#include "configServer.h"
#include <SD.h>
#include <FS.h>
#include <uploader.h>

const char* host = "openbikesensor";

WebServer server(80);

/* Style */
String style =
  "<style>"
  "#file-input,input {width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px;}"
  "input {background:#f1f1f1;border:0;padding:0 15px;text-align:center;}"
  "body {background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
  "#file-input {padding:0 5;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
  "#bar,#prgbar {background-color:#f1f1f1;border-radius:10px}"
  "#bar {background-color:#3498db;width:0%;height:10px}"
  "form {background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
  ".btn {background:#3498db;color:#fff;cursor:pointer}"
  "h1,h2, h3 {padding:0;margin:0;}"
  "h3 {padding:10px 0;margin-top:10px;margin-bottom:10px;border-top:3px solid #3498db;border-bottom:3px solid #3498db;}"
  "h1 a {color:#777}"
  "hr { border-top:1px solid #CCC;margin-left:10px;margin-right:10px;}"
  "</style>";

String header =
  "<form action='{action}'>"
  "<h1><a href='/'>OpenBikeSensor</a></h1>"
  "<h2>{subtitle}</h2>"
  "<p>Firmware version: {version}</p>";

String footer = "</form>" + style;

// #########################################
// Navigation
// #########################################

String navigationIndex =
  header +
  "<input type=button onclick=window.location.href='/config' class=btn value='Config'>"
  "<input type=button onclick=window.location.href='/privacy' class=btn value='Privacy Zones'>"
  "<input type=button onclick=window.location.href='/wifi' class=btn value='Wifi Settings'>"
  "<input type=button onclick=window.location.href='/update' class=btn value='Update Firmware'>"
  "<input type=button onclick=window.location.href='/reboot' class=btn value='Reboot'>"
  "<input type=button onclick=window.location.href='/upload' class=btn value='Upload'>"
  + footer;

// #########################################
// Reboot
// #########################################

String rebootIndex =
  header +
  "<div>Device reboots now.</div>"
  + footer;

// #########################################
// Wifi
// #########################################

String wifiSettingsIndex =
  header +
  "<input name=ssid placeholder='ssid' value='{ssid}'>"
  "<input name=pass placeholder='password' type='Password'>"
  "<input type=submit class=btn value=Save>"
  + footer;

// #########################################
// Config
// #########################################

String configIndex =
  header +
  "<h3>Sensor</h3>"
  "Offset Sensor Left<input name='offsetS1' placeholder='Offset Sensor Left' value='{offset1}'>"
  "<hr>"
  "Offset Sensor Right<input name='offsetS2' placeholder='Offset Sensor Right' value='{offset2}'>"
  "<hr>"
  "Swap Sensors (Left &#8660; Right)<input type='checkbox' name='displaySwapSensors' {displaySwapSensors}>"
  ""
  "<h3>GPS</h3>"
  "<label for='location'>Valid Location</label>"
  "<input type='radio' id='location' name='validGPS' value='validLocation' {validLocation}>"
  "<hr>"
  "<label for='female'>Valid Time</label>"
  "<input type='radio' id='time' name='validGPS' value='validTime' {validTime}> "
  "<hr>"
  "<label for='other'>Number of satellites</label> "
  "<input type='radio' id='numsatellites' name='validGPS' value='numberSatellites' {numberSatellites}>"
  "<hr>"
  "Number of Satellites for fix<input name='satsForFix' placeholder='Number of Satellites for fix' value='{satsForFix}'>"
  ""
  "<h3>Generic Display</h3>"
  "Invert<br>(black &#8660; white)<input type='checkbox' name='displayInvert' {displayInvert}>"
  "<hr>"
  "Flip<br>(upside down &#8661;)<input type='checkbox' name='displayFlip' {displayFlip}>"
  ""
  "<h3>Measurement Display</h3>"
  "Confirmation Time Window<br>(time in seconds to confirm until a new measurement starts)<input name='confirmationTimeWindow' placeholder='Milliseconds' value='{confirmationTimeWindow}'>"
  "<hr>"
  "Simple Mode<br>(all measurement display options below are ignored)<input type='checkbox' name='displaySimple' {displaySimple}>"
  "<hr>"
  "Show Left Measurement<input type='checkbox' name='displayLeft' {displayLeft}>"
  "<hr>"
  "Show Right Measurement<input type='checkbox' name='displayRight' {displayRight}>"
  "<hr>"
  "Show Satellites<input type='checkbox' name='displayGPS' {displayGPS}>"
  "<hr>"
  "Show Velocity<input type='checkbox' name='displayVELO' {displayVELO}>"
  "<hr>"
  "Show Confirmation Stats<input type='checkbox' name='displayNumConfirmed' {displayNumConfirmed}>"
  ""
  "<h3>Privacy Options</h3>"
  "<label for='location'>Dont record at all in privacy areas</label>"
  "<input type='radio' id='absolutePrivacy' name='privacyOptions' value='absolutePrivacy' {absolutePrivacy}>"
  "<hr>"
  "<label for='location'>Dont record position in privacy areas</label>"
  "<input type='radio' id='noPosition' name='privacyOptions' value='noPosition' {noPosition}>"
  "<hr>"
  "<label for='location'>Record even in privacy areas</label>"
  "<input type='radio' id='noPrivacy' name='privacyOptions' value='noPrivacy' {noPrivacy}>"
  "<hr>"
  "Override Privacy when Pushing the Button<input type='checkbox' name='overridePrivacy' {overridePrivacy}>"
  "<h3>Upload User Data</h3>"
  "<input name='hostname' placeholder='hostname' value='{hostname}'>"
  "<hr>"
  "<input name='obsUserID' placeholder='API ID' value='{userId}' >"
  "<input type=submit class=btn value=Save>"
  + footer;

// #########################################
// Upload
// #########################################

/* Server Index Page */
String uploadIndex =
  header +
  "<input type='file' name='update' id='file'>"
  "<label id='file-input' for='file'>Choose file...</label>"
  "<input id='btn' type='submit' class=btn value='Update'>"
  "<br><br>"
  "<div id='prg'></div>"
  "<br><div id='prgbar'><div id='bar'></div></div><br></form>"
  "<script>"
  ""
  "function hide(x) { x.style.display = 'none'; }"
  "function show(x) { x.style.display = 'block'; }"
  ""
  "hide(document.getElementById('file'));"
  "hide(document.getElementById('prgbar'));"
  "hide(document.getElementById('prg'));"
  ""
  "var fileName = '';"
  "document.getElementById('file').addEventListener('change', function(e){"
  "fileNameParts = e.target.value.split('\\\\');"
  "fileName = fileNameParts[fileNameParts.length-1];"
  "console.log(fileName);"
  "document.getElementById('file-input').innerHTML = fileName;"
  "});"
  ""
  "document.getElementById('btn').addEventListener('click', function(e){"
  "e.preventDefault();"
  "if (fileName == '') { alert('No file choosen'); return; }"
  "console.log('Start upload...');"
  ""
  "var form = document.getElementsByTagName('form')[0];"
  "var data = new FormData(form);"
  "console.log(data);"
  //https://developer.mozilla.org/en-US/docs/Web/API/FormData/values
  "for (var v of data.values()) { console.log(v); }"
  ""
  "hide(document.getElementById('file-input'));"
  "hide(document.getElementById('btn'));"
  "show(document.getElementById('prgbar'));"
  "show(document.getElementById('prg'));"
  ""
  "var xhr = new XMLHttpRequest();"
  "xhr.open( 'POST', '/update', true );"
  "xhr.onreadystatechange = function(s) {"
  "console.log(xhr.responseText);"
  "if (xhr.readyState == 4 && xhr.status == 200) {"
  "document.getElementById('prg').innerHTML = 'Device reboots now!';"
  "} else if (xhr.readyState == 4 && xhr.status == 500) {"
  "document.getElementById('prg').innerHTML = 'Update error:' + xhr.responseText;"
  "} else {"
  "document.getElementById('prg').innerHTML = 'Uknown error';"
  "}"
  "};"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = Math.round(evt.loaded / evt.total * 100);"
  "if(per == 100) document.getElementById('prg').innerHTML = 'Flashing...';"
  "else document.getElementById('prg').innerHTML = 'Upload progress: ' + per + '%';"
  "document.getElementById('bar').style.width = per + '%';"
  "}"
  "}, false);"
  "xhr.send( data );"
  "});" // btn click
  ""
  "</script>" + footer;

// #########################################
// Privacy
// #########################################
String privacyIndexPrefix =
  "<form name=privacyForm action=/privacy_action>"
  "<h1>OpenBikeSensor Privacy Settings</h1>";


String privacyIndexPostfix =
  "<input type=submit onclick=window.location.href='/' class=btn value=Save>"
  "<input type=button onclick=window.location.href='/makecurrentlocationprivate' class=btn value='Make current location private'>"
  "</form>"
  + style;


String makeCurrentLocationPrivateIndex =
  header +
  "<div>Making current location private, waiting for fix. Press device button to cancel.</div>"
  + footer;

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

void configAction() {

  String offsetS1 = server.arg("offsetS1");
  String offsetS2 = server.arg("offsetS2");
  String satsForFix = server.arg("satsForFix");
  String confirmationTimeWindow = server.arg("confirmationTimeWindow");

  String displaySimple = server.arg("displaySimple") == "on" ? "on" : "off";
  String displayGPS = server.arg("displayGPS") == "on" ? "on" : "off";
  String displayVELO = server.arg("displayVELO") == "on" ? "on" : "off";
  String displayLeft = server.arg("displayLeft") == "on" ? "on" : "off";
  String displayRight = server.arg("displayRight") == "on" ? "on" : "off";
  String displaySwapSensors = server.arg("displaySwapSensors") == "on" ? "on" : "off";
  String displayInvert = server.arg("displayInvert") == "on" ? "on" : "off";
  String displayFlip = server.arg("displayFlip") == "on" ? "on" : "off";
  String displayNumConfirmed = server.arg("displayNumConfirmed") == "on" ? "on" : "off";

  String validGPS = server.arg("validGPS");

  String privacyOptions = server.arg("privacyOptions");
  String overridePrivacy = server.arg("overridePrivacy");

  String obsUserID = server.arg("obsUserID");
  String hostname = server.arg("hostname");

  //String confirmation = server.arg("confirmation");

  /* displayTest->clear();
    displayTest->drawString(64, 36, "displayConfig");
    displayTest->drawString(64, 48, displayGPS);*/
  if (displayGPS == "on")
    config.displayConfig |= DisplaySatelites;
  else
    config.displayConfig &= ~DisplaySatelites;

  if (displaySimple == "on")
    config.displayConfig |= DisplaySimple;
  else
    config.displayConfig &= ~DisplaySimple;

  if (displayVELO == "on")
    config.displayConfig |= DisplayVelocity;
  else
    config.displayConfig &= ~DisplayVelocity;

  if (displayLeft == "on")
    config.displayConfig |= DisplayLeft;
  else
    config.displayConfig &= ~DisplayLeft;

  if (displayRight == "on")
    config.displayConfig |= DisplayRight;
  else
    config.displayConfig &= ~DisplayRight;

  if (displaySwapSensors == "on")
    config.displayConfig |= DisplaySwapSensors;
  else
    config.displayConfig &= ~DisplaySwapSensors;

  if (displayInvert == "on")
    config.displayConfig |= DisplayInvert;
  else
    config.displayConfig &= ~DisplayInvert;

  if (displayFlip == "on")
    config.displayConfig |= DisplayFlip;
  else
    config.displayConfig &= ~DisplayFlip;

  if (displayNumConfirmed == "on")
    config.displayConfig |= DisplayNumConfirmed;
  else
    config.displayConfig &= ~DisplayNumConfirmed;

  if (validGPS == "validLocation")
    config.GPSConfig = ValidLocation;

  if (validGPS == "validTime")
    config.GPSConfig = ValidTime;

  if (validGPS == "numberSatellites")
    config.GPSConfig = NumberSatellites;

  if (privacyOptions == "absolutePrivacy")
    config.privacyConfig |= AbsolutePrivacy;
  else
    config.privacyConfig &= ~AbsolutePrivacy;

  if (privacyOptions == "noPosition")
    config.privacyConfig |= NoPosition;
  else
    config.privacyConfig &= ~NoPosition;

  if (privacyOptions == "noPrivacy")
    config.privacyConfig |= NoPrivacy;
  else
    config.privacyConfig &= ~NoPrivacy;

  if (privacyOptions == "noPrivacy")
    config.privacyConfig |= NoPrivacy;
  else
    config.privacyConfig &= ~NoPrivacy;

  if (overridePrivacy == "on")
    config.privacyConfig |= OverridePrivacy;
  else
    config.privacyConfig &= ~OverridePrivacy;

  config.sensorOffsets[0] = atoi(offsetS1.c_str());
  config.sensorOffsets[1] = atoi(offsetS2.c_str());
  config.satsForFix = atoi(satsForFix.c_str());
  config.confirmationTimeWindow = atoi(confirmationTimeWindow.c_str());

  strlcpy(config.hostname, hostname.c_str(), sizeof(config.hostname));
  strlcpy(config.obsUserID, obsUserID.c_str(), sizeof(config.obsUserID));

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
  strlcpy(config.ssid, ssid.c_str(), sizeof(config.ssid));
  strlcpy(config.password, pass.c_str(), sizeof(config.password));

  // Print and safe config
  Serial.println(F("Print config file..."));
  printConfig(config);
  Serial.println(F("Saving configuration..."));
  saveConfiguration(configFilename, config);

  String s = "<meta http-equiv='refresh' content='0; url=/'><a href='/'> Go Back </a>";
  server.send(200, "text/html", s);
}

void privacyAction() {
  String latitude = server.arg("newlatitude");
  latitude.replace(",", ".");
  String longitude = server.arg("newlongitude");
  longitude.replace(",", ".");
  String radius = server.arg("newradius");
  String erase = server.arg("erase");

  if (erase != "")
  {
    if (atoi(erase.c_str()) < config.privacyAreas.size())
    {
      config.privacyAreas.erase(atoi(erase.c_str()));
    }
    config.numPrivacyAreas = config.privacyAreas.size();
  }

  if ( (latitude != "") && (longitude != "") && (radius != ""))
  {
    Serial.println(F("Valid privacyArea!"));
    addNewPrivacyArea(atof(latitude.c_str()), atof(longitude.c_str()), atoi(radius.c_str()));
    /*PrivacyArea newPrivacyArea;
      newPrivacyArea.latitude = atof(latitude.c_str());
      newPrivacyArea.longitude = atof(longitude.c_str());
      newPrivacyArea.radius = atoi(radius.c_str());
      randomOffset(newPrivacyArea);

      config.privacyAreas.push_back(newPrivacyArea);
      config.numPrivacyAreas = config.privacyAreas.size();
      Serial.println(F("Print config file..."));
      printConfig(config);
      Serial.println(F("Saving configuration..."));
      saveConfiguration(configFilename, config);
    */
  }

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
  uint16_t timeout = 10000;
  Serial.printf("Timeout %u\n", timeout);
  Serial.printf("startTime %u\n", startTime);
  while ((WiFi.status() != WL_CONNECTED) && (( millis() - startTime) <= timeout)) {
    delay(1000);
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

  server.on("/upload", HTTP_GET, []() {

    String html = header+"<div>";

    html.replace("{action}", "");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Upload");

    File root = SDFileSystem.open("/");
    if (!root)
    {
      Serial.println("Failed to open directory");
      return;
    }
    if (!root.isDirectory())
    {
      Serial.println("Not a directory");
      return;
    }

    File file = root.openNextFile();
    while (file)
    {
      if (!file.isDirectory())
      {
        if(uploader::instance()->upload(file.name()))
        {

        SDFileSystem.mkdir("/uploaded");
        SDFileSystem.rename(file.name(),String("/uploaded")+file.name());
        html += String(file.name());
        html += "<br>";
        }
      }
      file = root.openNextFile();
    }
    root.close();

    html += "</div>" + footer;

    server.send(200, "text/html", html);
  });

  // ### Make current location private ###
  server.on("/makecurrentlocationprivate", HTTP_GET, []() {

    String html = makeCurrentLocationPrivateIndex;
    // Header
    html.replace("{action}", "");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "MakeLocationPrivate");

    server.send(200, "text/html", html);
    bool validGPSData = false;
    buttonState = digitalRead(PushButton);
    while (!validGPSData && (buttonState == LOW))
    {
      Serial.println("GPSData not valid");
      buttonState = digitalRead(PushButton);
      readGPSData();
      validGPSData = gps.location.isValid();
      if (validGPSData) {
        Serial.println("GPSData valid");
        addNewPrivacyArea(gps.location.lat(), gps.location.lng(), 500);
      }
      delay(300);
    }

    String s = "<meta http-equiv='refresh' content='0; url=/'><a href='/'> Go Back </a>";
    server.send(200, "text/html", s); //Send web page

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
    html.replace("{confirmationTimeWindow}", String(config.confirmationTimeWindow));

    bool displaySimple = config.displayConfig & DisplaySimple;
    bool displayGPS = config.displayConfig & DisplaySatelites;
    bool displayLeft = config.displayConfig & DisplayLeft;
    bool displayRight = config.displayConfig & DisplayRight;
    bool displayVelo = config.displayConfig & DisplayVelocity;
    bool displaySwapSensors = config.displayConfig & DisplaySwapSensors;
    bool displayInvert = config.displayConfig & DisplayInvert;
    bool displayFlip = config.displayConfig & DisplayFlip;
    bool displayNumConfirmed = config.displayConfig & DisplayNumConfirmed;

    html.replace("{displaySimple}", displaySimple ? "checked" : "");
    html.replace("{displayGPS}", displayGPS ? "checked" : "");
    html.replace("{displayVELO}", displayVelo ? "checked" : "");
    html.replace("{displayLeft}", displayLeft ? "checked" : "");
    html.replace("{displayRight}", displayRight ? "checked" : "");
    html.replace("{displaySwapSensors}", displaySwapSensors ? "checked" : "");
    html.replace("{displayInvert}", displayInvert ? "checked" : "");
    html.replace("{displayFlip}", displayFlip ? "checked" : "");
    html.replace("{displayNumConfirmed}", displayNumConfirmed ? "checked" : "");

    bool validLocation = config.GPSConfig & ValidLocation;
    bool validTime = config.GPSConfig & ValidTime;
    bool numberSatellites = config.GPSConfig & NumberSatellites;

    html.replace("{validLocation}", validLocation ? "checked" : "");
    html.replace("{validTime}", validTime ? "checked" : "");
    html.replace("{numberSatellites}", numberSatellites ? "checked" : "");

    bool absolutePrivacy = config.privacyConfig & AbsolutePrivacy;
    bool noPosition = config.privacyConfig & NoPosition;
    bool noPrivacy = config.privacyConfig & NoPrivacy;
    bool overridePrivacy = config.privacyConfig & OverridePrivacy;

    html.replace("{absolutePrivacy}", absolutePrivacy ? "checked" : "");
    html.replace("{noPosition}", noPosition ? "checked" : "");
    html.replace("{noPrivacy}", noPrivacy ? "checked" : "");
    html.replace("{overridePrivacy}", overridePrivacy ? "checked" : "");

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
    if (Update.hasError()) {
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

  // ### Privacy ###

  server.on("/privacy", HTTP_GET, []() {
    createPrivacyPage();
  });

  server.on("/privacy_action", privacyAction);

  server.begin();
  Serial.println("Server Ready");

}
void createPrivacyPage()
{
  String privacyPage = privacyIndexPrefix;

  for (size_t idx = 0; idx < config.numPrivacyAreas; ++idx)
  {
    privacyPage += "Latitude " + String(idx) + "<input name=latitude" + String(idx) + " placeholder='latitude' value='" + String(config.privacyAreas[idx].latitude, 7) + "'disabled>";
    privacyPage += "Longitude " + String(idx) + "<input name=longitude" + String(idx) + "placeholder='longitude' value='" + String(config.privacyAreas[idx].longitude, 7) + "'disabled>";
    privacyPage += "Radius " + String(idx) + " (m)" + "<input name=radius" + String(idx) + "placeholder='radius' value='" + String(config.privacyAreas[idx].radius) + "'disabled>";
  }
  readGPSData();
  bool validGPSData = gps.location.isValid();
  if (validGPSData) {
    privacyPage += "New Latitude<input name=newlatitude value='" + String(gps.location.lat(), 7) + "'>";
    privacyPage += "New Longitude<input name=newlongitude value='" + String(gps.location.lng(), 7) + "'>";
  }
  else
  {
    privacyPage += "New Latitude<input name=newlatitude placeholder='48.12345'>";
    privacyPage += "New Longitude<input name=newlongitude placeholder='9.12345'>";
  }
  privacyPage += "New Radius (m)<input name=newradius placeholder='radius' value='500'>";

  privacyPage += "Erase Area<input name=erase placeholder='0'>";

  privacyPage += privacyIndexPostfix;
  server.send(200, "text/html", privacyPage);
}
