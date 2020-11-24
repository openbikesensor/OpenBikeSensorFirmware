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

const char *host = "openbikesensor";
WebServer server(80);
String json_buffer;

/* Style */
String style =
  "<style>"
  "#file-input,input, button {width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px;}"
  "input, button, a.back {background:#f1f1f1;border:0;padding:0 15px;text-align:center;}"
  "body {background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
  "#file-input {padding:0 5;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
  "#bar,#prgbar {background-color:#f1f1f1;border-radius:10px}"
  "#bar {background-color:#3498db;width:0%;height:10px}"
  "form {background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
  ".btn {background:#3498db;color:#fff;cursor:pointer}"
  "h1,h2, h3 {padding:0;margin:0;}"
  "h3 {padding:10px 0;margin-top:10px;margin-bottom:10px;border-top:3px solid #3498db;border-bottom:3px solid #3498db;}"
  "h1 a {color:#777}"
  "h2 {margin-top:5px}"
  "hr { border-top:1px solid #CCC;margin-left:10px;margin-right:10px;}"
  ".deletePrivacyArea, a.back {color: black; text-decoration: none; font-size: x-large;}"
  ".deletePrivacyArea:hover {color: red;}"
  "a.previous {text-decoration: none; display: inline-block; padding: 8px 16px;background-color: #f1f1f1; color: black;border-radius: 50%; font-family: Verdana, sans-serif; font-size: 18px}"
  "a.previous:hover {background-color: #ddd; color: black;}"
  "</style>";

String previous = "<a href='/' class='previous'>&#8249;</a>";

String header =
  "<script>"
  "window.onload = function() {"
  "  console.log(window.location.pathname);"
  "  if (window.location.pathname == '/') {"
  "    console.log('Hide previous');"
  "    document.querySelectorAll('.previous')[0].style.display = 'none';"
  "  } else {"
  "    console.log('Show previous');"
  "    document.querySelectorAll('.previous')[0].style.display = '';"
  "  }"
  "}"
  "</script>"
  ""
  "<form action='{action}'>"
  "<h1><a href='/'>OpenBikeSensor</a></h1>"
  "<h2>{subtitle}</h2>"
  "<p>Firmware version: {version}</p>"
  + previous;

String footer = "</form>"
                + style;

// #########################################
// Upload form
// #########################################

String xhrUpload = "<input type='file' name='upload' id='file' accept='{accept}'>"
                   "<label id='file-input' for='file'>Choose file...</label>"
                   "<input id='btn' type='submit' class=btn value='Upload'>"
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
                   "xhr.open( 'POST', '{method}', true );"
                   "xhr.onreadystatechange = function(s) {"
                   "console.log(xhr.responseText);"
                   "if (xhr.readyState == 4 && xhr.status == 200) {"
                   "document.getElementById('prg').innerHTML = xhr.responseText;"
                   "} else if (xhr.readyState == 4 && xhr.status == 500) {"
                   "document.getElementById('prg').innerHTML = 'Upload error:' + xhr.responseText;"
                   "} else {"
                   "document.getElementById('prg').innerHTML = 'Unknown error';"
                   "}"
                   "};"
                   "xhr.upload.addEventListener('progress', function(evt) {"
                   "if (evt.lengthComputable) {"
                   "var per = Math.round(evt.loaded / evt.total * 100);"
                   "if(per == 100) document.getElementById('prg').innerHTML = 'Updating...';"
                   "else document.getElementById('prg').innerHTML = 'Upload progress: ' + per + '%';"
                   "document.getElementById('bar').style.width = per + '%';"
                   "}"
                   "}, false);"
                   "xhr.send( data );"
                   "});" // btn click
                   ""
                   "</script>";

// #########################################
// Navigation
// #########################################

String navigationIndex =
  header +
  "<h3>Settings</h3>"
  "<input type=button onclick=window.location.href='/settings/general' class=btn value='General'>"
  "<input type=button onclick=window.location.href='/settings/privacy' class=btn value='Privacy Zones'>"
  "<input type=button onclick=window.location.href='/settings/wifi' class=btn value='Wifi'>"
  "<input type=button onclick=window.location.href='/settings/backup' class=btn value='Backup &amp; Restore'>"
  "<h3>Maintenance</h3>"
  "<input type=button onclick=window.location.href='/update' class=btn value='Update Firmware'>"
  "<input type=button onclick=window.location.href='/upload' class=btn value='Upload Tracks'>"
  "<input type=button onclick=window.location.href='/reboot' class=btn value='Reboot'>"
  "{dev}"
  + footer;

// #########################################
// Development
// #########################################

String development =
  "<h3>Development</h3>"
  "<input type=button onclick=window.location.href='/settings/development' class=btn value='Development'>"
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
  "<script>"
  "function resetPassword() { document.getElementById('pass').value = ''; }"
  "</script>"
  "<h3>Settings</h3>"
  "SSID"
  "<input name=ssid placeholder='ssid' value='{ssid}'>"
  "Password"
  "<input id=pass name=pass placeholder='password' type='Password' value='{password}' onclick='resetPassword()'>"
  "<input type=submit class=btn value=Save>"
  + footer;

// #########################################
// Backup and Restore
// #########################################

String backupIndex =
  header +
  "<p>This backups and restores the device configuration incl. the Basic Config, Privacy Zones and Wifi Settings.</p>"
  "<h3>Backup</h3>"
  "<a href='/settings/backup.json'><button type='button' class='btn'>Download</button></a>"
  "<h3>Restore</h3>"
  + xhrUpload
  + footer;

// #########################################
// Development Index
// #########################################

String devIndex =
  header +
  "<h3>Display</h3>"
  "Show Grid<br><input type='checkbox' name='showGrid' {showGrid}>"
  "Print WLAN password to serial<br><input type='checkbox' name='printWifiPassword' {printWifiPassword}>"
  "<input type=submit class=btn value=Save>"
  "<hr>"
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
  "<hr>"
  "Show nerd details for distance sensors <input type='checkbox' name='displayDistanceDetail' {displayDistanceDetail}>"
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
  "<h3>Operation</h3>"
  "Enable Bluetooth <input type='checkbox' name='bluetooth' {bluetooth}>"
  "<hr>"
  "SimRa Mode <input type='checkbox' name='simRaMode' {simRaMode}>"
  "<input type=submit class=btn value=Save>"
  + footer;

// #########################################
// Upload
// #########################################

/* Server Index Page */
String uploadIndex =
  header +
  "<h3>Update</h3>" +
  xhrUpload +
  footer;

// #########################################
// Privacy
// #########################################

String privacyIndexPrefix =
  header;

String privacyIndexPostfix =
  "<input type=submit onclick=window.location.href='/' class=btn value=Save>"
  "<input type=button onclick=window.location.href='/settings/privacy/makeCurrentLocationPrivate' class=btn value='Make current location private'>"
  "</form>"
  + style;


String makeCurrentLocationPrivateIndex =
  header +
  "<div>Making current location private, waiting for fix. Press device button to cancel.</div>"
  + footer;

// #########################################

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

// #########################################

#ifdef DEVELOP

void devAction() {

  String showGrid = server.arg("showGrid");
  if (showGrid == "on")
    config.devConfig |= ShowGrid;
  else
    config.devConfig &= ~ShowGrid;

  String printWifiPassword = server.arg("printWifiPassword");
  if (printWifiPassword == "on")
    config.devConfig |= PrintWifiPassword;
  else
    config.devConfig &= ~PrintWifiPassword;

  // Print and safe config
  Serial.println(F("Print config file..."));
  printConfig(config);
  Serial.println(F("Saving configuration..."));
  saveConfiguration(configFilename, config);

  String s = "<meta http-equiv='refresh' content='0; url=/settings/development'><a href='/settings/development'>Go Back</a>";
  server.send(200, "text/html", s); //Send web page
}

#endif

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
  String displayDistanceDetail = server.arg("displayDistanceDetail") == "on" ? "on" : "off";
  String bluetooth = server.arg("bluetooth") == "on" ? "on" : "off";
  String simRaMode = server.arg("simRaMode") == "on" ? "on" : "off";

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

  if (displayDistanceDetail == "on")
    config.displayConfig |= DisplayDistanceDetail;
  else
    config.displayConfig &= ~DisplayDistanceDetail;

  config.bluetooth = (bluetooth == "on");
  config.simRaMode = (simRaMode == "on");

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

  config.sensorOffsets[LEFT_SENSOR_ID] = atoi(offsetS1.c_str());
  config.sensorOffsets[RIGHT_SENSOR_ID] = atoi(offsetS2.c_str());
  config.satsForFix = atoi(satsForFix.c_str());
  config.confirmationTimeWindow = atoi(confirmationTimeWindow.c_str());

  strlcpy(config.hostname, hostname.c_str(), sizeof(config.hostname));
  strlcpy(config.obsUserID, obsUserID.c_str(), sizeof(config.obsUserID));

  // Print and safe config
  Serial.println(F("Print config file..."));
  printConfig(config);
  Serial.println(F("Saving configuration..."));
  saveConfiguration(configFilename, config);

  String s = "<meta http-equiv='refresh' content='0; url=/settings/general'><a href='/settings/general'>Go Back</a>";
  server.send(200, "text/html", s); //Send web page
}

void wifiAction() {
  String ssid = server.arg("ssid");
  String password = server.arg("pass");

  Serial.print("ssid:");
  Serial.println(ssid);

#ifdef DEVELOP
  if (config.devConfig & PrintWifiPassword) {
    Serial.print(F("password = "));
    Serial.println(password);
  }
#endif

  // Write always both data, thus the WIFI config can be overwritten
  strlcpy(config.ssid, ssid.c_str(), sizeof(config.ssid));
  if (strcmp(password.c_str(), "******") != 0) {
    strlcpy(config.password, password.c_str(), sizeof(config.password));
  }

  // Print and safe config
  Serial.println(F("Print config file..."));
  printConfig(config);
  Serial.println(F("Saving configuration..."));
  saveConfiguration(configFilename, config);

  String s = "<meta http-equiv='refresh' content='0; url=/settings/wifi'><a href='/settings/wifi'>Go Back</a>";
  server.send(200, "text/html", s);
}

void privacyAction() {

  String latitude = server.arg("newlatitude");
  latitude.replace(",", ".");
  String longitude = server.arg("newlongitude");
  longitude.replace(",", ".");
  String radius = server.arg("newradius");

  if ((latitude != "") && (longitude != "") && (radius != "")) {
    Serial.println(F("Valid privacyArea!"));
    addNewPrivacyArea(atof(latitude.c_str()), atof(longitude.c_str()), atoi(radius.c_str()));
  }

  String s = "<meta http-equiv='refresh' content='0; url=/settings/privacy'><a href='/settings/privacy'>Go Back</a>";
  server.send(200, "text/html", s);
}

bool CreateWifiSoftAP(String chipID) {
  bool SoftAccOK;
  WiFi.disconnect();
  Serial.print(F("Initalize SoftAP "));
  String APName = "OpenBikeSensor-" + chipID;
  String APPassword = "12345678";
  SoftAccOK = WiFi.softAP(APName.c_str(), APPassword.c_str()); // Passwortlänge mindestens 8 Zeichen !
  delay(2000); // Without delay I've seen the IP address blank
  /* Soft AP network parameters */
  IPAddress apIP(172, 20, 0, 1);
  IPAddress netMsk(255, 255, 255, 0);

  displayTest->showTextOnGrid(0, 2, "AP:");
  displayTest->showTextOnGrid(1, 2, "");
  displayTest->showTextOnGrid(0, 3, APName.c_str());


  WiFi.softAPConfig(apIP, apIP, netMsk);
  if (SoftAccOK) {
    /* Setup the DNS server redirecting all the domains to the apIP */
    //dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    //dnsServer.start(DNS_PORT, "*", apIP);

    Serial.println(F("AP successful."));

    displayTest->showTextOnGrid(0, 4, "Pass:");
    displayTest->showTextOnGrid(1, 4, APPassword);

    displayTest->showTextOnGrid(0, 5, "IP:");
    displayTest->showTextOnGrid(1, 5, "172.20.0.1");
  } else {
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
  esp_chipid = String((uint16_t) (chipid_num >> 32), HEX);
  esp_chipid += String((uint32_t) chipid_num, HEX);
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
  while ((WiFi.status() != WL_CONNECTED) && ((millis() - startTime) <= timeout)) {
    delay(1000);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    CreateWifiSoftAP(esp_chipid);
  } else {
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

    String html = header + "<div>";

    html.replace("{action}", "");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Upload Tracks");

    File root = SDFileSystem.open("/");
    if (!root) {
      Serial.println("Failed to open directory");
      return;
    }
    if (!root.isDirectory()) {
      Serial.println("Not a directory");
      return;
    }

    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        if (uploader::instance()->upload(file.name())) {

          SDFileSystem.mkdir("/uploaded");
          SDFileSystem.rename(file.name(), String("/uploaded") + file.name());
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

  // ###############################################################
  // ### Index ###
  // ###############################################################

  server.on("/", HTTP_GET, []() {
    String html = navigationIndex;

    // Header
    html.replace("{action}", "");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Navigation");
#ifdef DEVELOP
    html.replace("{dev}", development);
#else
    html.replace("{dev}", "");
#endif

    server.send(200, "text/html", html);
  });

  // ###############################################################
  // ### Backup ###
  // ###############################################################

  server.on("/settings/backup", HTTP_GET, []() {
    String html = backupIndex;
    // Header
    html.replace("{action}", "");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Backup & Restore");
    html.replace("{method}", "/settings/restore");
    html.replace("{accept}", ".json");

    server.send(200, "text/html", html);
  });

  server.on("/settings/backup.json", HTTP_GET, []() {
    String json = configToJson(config);
    String version = (String) OBSVersion;

    // Add Header https://stackoverflow.com/a/11545741/605890
    server.sendHeader("Content-disposition", "attachment; filename=OpenBikeSensorConfig-" + version + ".json", false);
    server.sendHeader("Content-type", "application/json", false);
    server.send(200, "text/html", json);
  });

  // Handling uploading firmware file
  server.on("/settings/restore", HTTP_POST, []() {
    Serial.println("Send response...");
    server.send(200, "text/plain", "Restore successful!");
  }, []() {
    //Serial.println("Recover Config...");
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Recover: %s\n", upload.filename.c_str());
      json_buffer = "";
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      // Convert all uint8_t elements until currentSize to String
      for (int i = 0; i < upload.currentSize; i++) {
        json_buffer += (char) upload.buf[i];
      }

    } else if (upload.status == UPLOAD_FILE_END) {
      jsonToConfig(json_buffer, config);

      Serial.println(F("Print config file..."));
      printConfig(config);

      Serial.println(F("Saving configuration..."));
      saveConfiguration(configFilename, config);
    }
  });

  // ###############################################################
  // ### Wifi ###
  // ###############################################################

  server.on("/settings/wifi/action", wifiAction);

  server.on("/settings/wifi", HTTP_GET, []() {
    String html = wifiSettingsIndex;
    // Header
    html.replace("{action}", "/settings/wifi/action");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Wifi");
    // Form data
    html.replace("{ssid}", config.ssid);
    if (sizeof(config.password) > 0) {
      html.replace("{password}", "******");
    } else {
      html.replace("{password}", "");
    }

    server.send(200, "text/html", html);
  });

  // ###############################################################
  // ### Development Options ###
  // ###############################################################

#ifdef DEVELOP
  server.on("/settings/development/action", devAction);

  server.on("/settings/development", HTTP_GET, []() {
    String html = devIndex;
    // Header
    html.replace("{action}", "/settings/development/action"); // Handled by XHR
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Development Settings");

    // SHOWGRID
    bool showGrid = config.devConfig & ShowGrid;
    html.replace("{showGrid}", showGrid ? "checked" : "");

    bool printWifiPassword = config.devConfig & PrintWifiPassword;
    html.replace("{printWifiPassword}", printWifiPassword ? "checked" : "");

    server.send(200, "text/html", html);
  });

#endif

  // ###############################################################
  // ### Config ###
  // ###############################################################

  server.on("/settings/general/action", configAction);

  server.on("/settings/general", HTTP_GET, []() {
    String html = configIndex;
    // Header
    html.replace("{action}", "/settings/general/action");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "General");
    // Form data
    html.replace("{offset1}", String(config.sensorOffsets[LEFT_SENSOR_ID]));
    html.replace("{offset2}", String(config.sensorOffsets[RIGHT_SENSOR_ID]));
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
    bool displayDistanceDetail = config.displayConfig & DisplayDistanceDetail;

    html.replace("{displaySimple}", displaySimple ? "checked" : "");
    html.replace("{displayGPS}", displayGPS ? "checked" : "");
    html.replace("{displayVELO}", displayVelo ? "checked" : "");
    html.replace("{displayLeft}", displayLeft ? "checked" : "");
    html.replace("{displayRight}", displayRight ? "checked" : "");
    html.replace("{displaySwapSensors}", displaySwapSensors ? "checked" : "");
    html.replace("{displayInvert}", displayInvert ? "checked" : "");
    html.replace("{displayFlip}", displayFlip ? "checked" : "");
    html.replace("{displayNumConfirmed}", displayNumConfirmed ? "checked" : "");
    html.replace("{displayDistanceDetail}", displayDistanceDetail ? "checked" : "");
    html.replace("{bluetooth}", config.bluetooth ? "checked" : "");
    html.replace("{simRaMode}", config.simRaMode ? "checked" : "");

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

  // ###############################################################
  // ### Update Firmware ###
  // ###############################################################

  server.on("/update", HTTP_GET, []() {
    String html = uploadIndex;
    // Header
    html.replace("{action}", ""); // Handled by XHR
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Update Firmware");
    html.replace("{method}", "/update");
    html.replace("{accept}", ".bin");

    server.send(200, "text/html", html);
  });

  // Handling uploading firmware file
  server.on("/update", HTTP_POST, []() {
    Serial.println("Send response...");
    if (Update.hasError()) {
      server.send(500, "text/plain",
                  "Update failed! Note: update from v0.2.x to v0.3 or newer needs to be done once with USB cable!");
    } else {
      server.send(200, "text/plain", "Update successful! Device reboots now!");
      delay(250);
      ESP.restart();
    }
  }, []() {
    //Serial.println('Update Firmware...');
    HTTPUpload &upload = server.upload();
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

  // ###############################################################
  // ### Privacy ###
  // ###############################################################

  // Make current location private
  server.on("/settings/privacy/makeCurrentLocationPrivate", HTTP_GET, []() {

    String html = makeCurrentLocationPrivateIndex;
    // Header
    html.replace("{action}", "");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "MakeLocationPrivate");

    server.send(200, "text/html", html);

    bool validGPSData = false;
    buttonState = digitalRead(PushButton);
    while (!validGPSData && (buttonState == LOW)) {
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

    // #77 - 200 cannot be send twice via HTTP
    //String s = "<meta http-equiv='refresh' content='0; url=/settings/privacy'><a href='/settings/privacy'>Go Back</a>";
    //server.send(200, "text/html", s); //Send web page

  });

  server.on("/settings/privacy", HTTP_GET, []() {

    String html = privacyIndexPrefix;

    // Header
    html.replace("{action}", "/privacy_action");
    html.replace("{version}", OBSVersion);
    html.replace("{subtitle}", "Privacy Zones");

    String privacyPage = html;


    for (size_t idx = 0; idx < config.numPrivacyAreas; ++idx) {
      privacyPage += "<h3>Privacy Area #" + String(idx) + "</h3>";
      privacyPage += "Latitude <input name=latitude" + String(idx) + " placeholder='latitude' value='" +
                     String(config.privacyAreas[idx].latitude, 7) + "'disabled>";
      privacyPage += "Longitude <input name=longitude" + String(idx) + "placeholder='longitude' value='" +
                     String(config.privacyAreas[idx].longitude, 7) + "'disabled>";
      privacyPage += "Radius (m) <input name=radius" + String(idx) + "placeholder='radius' value='" +
                     String(config.privacyAreas[idx].radius) + "'disabled>";
      privacyPage += "<a class=\"deletePrivacyArea\" href=\"/privacy_delete?erase=" + String(idx) + "\">&#x2716;</a>";
    }

    privacyPage += "<h3>New Privacy Area</h3>";

    readGPSData();
    bool validGPSData = gps.location.isValid();
    if (validGPSData) {
      privacyPage += "Latitude<input name=newlatitude value='" + String(gps.location.lat(), 7) + "'>";
      privacyPage += "Longitude<input name=newlongitude value='" + String(gps.location.lng(), 7) + "'>";
    } else {
      privacyPage += "Latitude<input name=newlatitude placeholder='48.12345'>";
      privacyPage += "Longitude<input name=newlongitude placeholder='9.12345'>";
    }
    privacyPage += "Radius (m)<input name=newradius placeholder='radius' value='500'>";

    privacyPage += privacyIndexPostfix;
    server.send(200, "text/html", privacyPage);
  });

  server.on("/privacy_delete", HTTP_GET, []() {

    String erase = server.arg("erase");
    if (erase != "") {
      int idx = atoi(erase.c_str());
      Serial.print(F("Erase idx="));
      Serial.println(idx);

      // Erase is broken, see #104
      //if (idx < config.privacyAreas.size())
      //{
      //  config.privacyAreas.erase(idx);
      //}
      // DEBUG #104 !!!
      //Serial.print(F("VECTOR SIZE:"));
      //Serial.println(config.privacyAreas.size());

      // Workaround for #104: Create a new vector and add existing elements
      Vector<PrivacyArea> privacyAreas;
      for (int i = 0; i < config.privacyAreas.size(); i++) {
        if (i != idx) {
          privacyAreas.push_back(config.privacyAreas[i]);
        }
      }
      config.privacyAreas = privacyAreas;
      // Workaround end

      config.numPrivacyAreas = config.privacyAreas.size();
    }

    // Print and save configuration
    Serial.println(F("Print config file..."));
    printConfig(config); // This crashes the ESP
    Serial.println(F("Saving configuration..."));
    saveConfiguration(configFilename, config);

    String s = "<meta http-equiv='refresh' content='0; url=/settings/privacy'><a href='/settings/privacy'>Go Back</a>";
    server.send(200, "text/html", s);
  });

  server.on("/privacy_action", privacyAction);

  // ###############################################################
  // Default, send 404
  // ###############################################################

  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("Server Ready");

}
