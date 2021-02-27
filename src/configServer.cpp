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
#include "OpenBikeSensorFirmware.h"
#include <uploader.h>
#include <utils/alpdata.h>
#include "SPIFFS.h"

static const char *const HTML_ENTITY_FAILED_CROSS = "&#x274C;";
static const char *const HTML_ENTITY_OK_MARK = "&#x2705;";


const char* host = "openbikesensor";

ObsConfig *theObsConfig;

WebServer server(80);
String json_buffer;

/* Style */
// TODO
//  - Fix CSS Style for mobile && desktop
//  - a vs. button
//  - back navigation after save
String style =
  "<style>"
  "#file-input,input, button {width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px;}"
  "input, button, a.back {background:#f1f1f1;border:0;padding:0;text-align:center;}"
  "body {background:#3498db;font-family:sans-serif;font-size:12px;color:#777}"
  "#file-input {padding:0 5px;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
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
  "ul.directory-listing {list-style: none; text-align: left; padding: 0; margin: 0; line-height: 1.5;}"
  "li.directory a {text-decoration: none; font-weight: bold;}"
  "li.file a {text-decoration: none;}"
  "</style>";

const String previous = "<a href=\"javascript:history.back()\" class='previous'>&#8249;</a>";

String header =
  "<!DOCTYPE html>\n"
  "<html lang='en'><head><meta charset='utf-8'/><title>{title}</title>" + style +
  "<link rel='icon' href='data:;base64,iVBORw0KGgo=' />"
  "<script>"
  "window.onload = function() {"
  "  if (window.location.pathname == '/') {"
  "    document.querySelectorAll('.previous')[0].style.display = 'none';"
  "  } else {"
  "    document.querySelectorAll('.previous')[0].style.display = '';"
  "  }"
  "}"
  "</script></head><body>"
  ""
  "<form action='{action}'>"
  "<h1><a href='/'>OpenBikeSensor</a></h1>"
  "<h2>{subtitle}</h2>"
  "<p>Firmware version: {version}</p>"
  + previous;

const String footer = "</form></body></html>";

// #########################################
// Upload form
// #########################################

String xhrUpload =   "<input type='file' name='upload' id='file' accept='{accept}'>"
  "<label id='file-input' for='file'>Choose file...</label>"
  "<input id='btn' type='submit' class=btn value='Upload'>"
  "<br><br>"
  "<div id='prg'></div>"
  "<br><div id='prgbar'><div id='bar'></div></div><br>" // </form>"
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
  "if (fileName == '') { alert('No file chosen'); return; }"
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
  "var per = Math.round((evt.loaded * 100) / evt.total);"
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
  "<input type=button onclick=\"window.location.href='/upload'\" class=btn value='Upload Tracks'>"
  "<h3>Settings</h3>"
  "<input type=button onclick=\"window.location.href='/settings/general'\" class=btn value='General'>"
  "<input type=button onclick=\"window.location.href='/settings/privacy'\" class=btn value='Privacy Zones'>"
  "<input type=button onclick=\"window.location.href='/settings/wifi'\" class=btn value='Wifi'>"
  "<input type=button onclick=\"window.location.href='/settings/backup'\" class=btn value='Backup &amp; Restore'>"
  "<h3>Maintenance</h3>"
  "<input type=button onclick=\"window.location.href='/update'\" class=btn value='Update Firmware'>"
  "<input type=button onclick=\"window.location.href='/sd'\" class=btn value='Show SD Card Contents'>"
  "<input type=button onclick=\"window.location.href='/about'\" class=btn value='About'>"
  "<input type=button onclick=\"window.location.href='/reboot'\" class=btn value='Reboot'>"
  "{dev}";

// #########################################
// Development
// #########################################

String development =
  "<h3>Development</h3>"
  "<input type=button onclick=\"window.location.href='/settings/development'\" class=btn value='Development'>";

// #########################################
// Reboot
// #########################################

String rebootIndex =
  "<div>Device reboots now.</div>";

// #########################################
// Wifi
// #########################################

String wifiSettingsIndex =
  "<script>"
  "function resetPassword() { document.getElementById('pass').value = ''; }"
  "</script>"
  "<h3>Settings</h3>"
  "SSID"
  "<input name=ssid placeholder='ssid' value='{ssid}'>"
  "Password"
  "<input id=pass name=pass placeholder='password' type='Password' value='{password}' onclick='resetPassword()'>"
  "<input type=submit class=btn value=Save>";

// #########################################
// Backup and Restore
// #########################################

String backupIndex =
  "<p>This backups and restores the device configuration incl. the Basic Config, Privacy Zones and Wifi Settings.</p>"
  "<h3>Backup</h3>"
  "<input type='button' onclick=\"window.location.href='/settings/backup.json'\" class=btn value='Download' />"
  "<h3>Restore</h3>";

// #########################################
// Development Index
// #########################################

String devIndex =
  "<h3>Display</h3>"
  "Show Grid<br><input type='checkbox' name='showGrid' {showGrid}>"
  "Print WLAN password to serial<br><input type='checkbox' name='printWifiPassword' {printWifiPassword}>"
  "<input type=submit class=btn value=Save>"
  "<hr>";

// #########################################
// Config
// #########################################

String configIndex =
  "<h3>Sensor</h3>"
  "Offset Sensor Left<input name='offsetS1' placeholder='Offset Sensor Left' value='{offset1}'>"
  "<hr>"
  "Offset Sensor Right<input name='offsetS2' placeholder='Offset Sensor Right' value='{offset2}'>"
  "<hr>"
  "Swap Sensors (Left &#8660; Right)<input type='checkbox' name='displaySwapSensors' {displaySwapSensors}>"
  ""
  "<h3>GPS</h3>"
  "<label for='gpsFix'>GPS to wait for</label> "
  "<select id='gpsFix' name='gpsFix'>"
  "<option value='-2' {fixPos}>position</option>"
  "<option value='-1' {fixTime}>time only</option>"
  "<option value='0' {fixNoWait}>no wait</option>"
  "</select>"
  ""
  "<h3>Generic Display</h3>"
  "Invert<br>(black &#8660; white)<input type='checkbox' name='displayInvert' {displayInvert}>"
  "<hr>"
  "Flip<br>(upside down &#8661;)<input type='checkbox' name='displayFlip' {displayFlip}>"
  ""
  "<h3>Measurement Display</h3>"
  "Confirmation Time Window<br>(time in seconds to confirm until a new measurement starts)<input name='confirmationTimeWindow' placeholder='Seconds' value='{confirmationTimeWindow}'>"
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
  "Show raw details for distance sensors <input type='checkbox' name='displayDistanceDetail' {displayDistanceDetail}>"
  "<small>Displays raw, unfiltered distance sensor reading in cm (L=left/R=right) "
  "and reading-cycles per second (F) in the form <code>LLL|FF|RRR</code> in the 2nd display line.</small>"
  "<h3>Privacy Options</h3>"
  "<label for='absolutePrivacy'>Dont record at all in privacy areas</label>"
  "<input type='radio' id='absolutePrivacy' name='privacyOptions' value='absolutePrivacy' {absolutePrivacy}>"
  "<hr>"
  "<label for='noPosition'>Dont record position in privacy areas</label>"
  "<input type='radio' id='noPosition' name='privacyOptions' value='noPosition' {noPosition}>"
  "<hr>"
  "<label for='noPrivacy'>Record even in privacy areas</label>"
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
  "<input type=submit class=btn value=Save>";

// #########################################
// Upload
// #########################################

/* Server Index Page */
const String uploadIndex = "<h3>Update</h3>";

// #########################################
// Privacy
// #########################################

String privacyIndexPostfix =
  "<input type=submit onclick=\"window.location.href='/'\" class=btn value=Save>"
  "<input type=button onclick=\"window.location.href='/settings/privacy/makeCurrentLocationPrivate'\" class=btn value='Make current location private'>";


String makeCurrentLocationPrivateIndex =
  "<div>Making current location private, waiting for fix. Press device button to cancel.</div>";

// #########################################

bool configServerWasConnectedViaHttpFlag = false;

void tryWiFiConnect(const ObsConfig *obsConfig);
uint16_t countFilesInRoot();
String ensureSdIsAvailable();
void moveToUploaded(const String &fileName);

bool configServerWasConnectedViaHttp() {
  return configServerWasConnectedViaHttpFlag;
}

void touchConfigServerHttp() {
  configServerWasConnectedViaHttpFlag = true;
}

String createPage(const String& content, const String& additionalContent = "") {
  configServerWasConnectedViaHttpFlag = true;
  String result;
  result += header;
  result += content;
  result += additionalContent;
  result += footer;

  return result;
}

String replaceDefault(String html, const String& subTitle, const String& action = "#") {
  configServerWasConnectedViaHttpFlag = true;
  html.replace("{title}",
               theObsConfig->getProperty<String>(ObsConfig::PROPERTY_OBS_NAME) + " - " + subTitle);
  html.replace("{version}", OBSVersion);
  html.replace("{subtitle}", subTitle);
  html.replace("{action}", action);
  displayTest->clear();
  displayTest->showTextOnGrid(0, 0,
                              theObsConfig->getProperty<String>(ObsConfig::PROPERTY_OBS_NAME));
  displayTest->showTextOnGrid(0, 1, "IP:");
  String ip;
  if (WiFiGenericClass::getMode() == WIFI_MODE_STA) {
    ip = WiFi.localIP().toString();
  } else {
    ip = WiFi.softAPIP().toString();
  }
  displayTest->showTextOnGrid(1, 1, ip);
  displayTest->showTextOnGrid(0, 2, "Menu");
  displayTest->showTextOnGrid(1, 2, subTitle);
  return html;
}

void handleUpload() {
  uploadTracks(true);
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

// #########################################
void devAction() {
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DEVELOPER, ShowGrid,
    server.arg("showGrid") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DEVELOPER, PrintWifiPassword,
                                   server.arg("printWifiPassword") == "on");

  theObsConfig->saveConfig();
  String s = "<meta http-equiv='refresh' content='0; url=/settings/development'><a href='/settings/development'>Go Back</a>";
  server.send(200, "text/html", s); //Send web page
}

void configAction() {
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplaySatellites,
                                   server.arg("displayGPS") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayVelocity,
                                   server.arg("displayVELO") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplaySimple,
                                   server.arg("displaySimple") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayLeft,
                                   server.arg("displayLeft") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayRight,
                                   server.arg("displayRight") == "on");
  // NOT a display setting!?
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplaySwapSensors,
                                   server.arg("displaySwapSensors") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayInvert,
                                   server.arg("displayInvert") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayFlip,
                                   server.arg("displayFlip") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayNumConfirmed,
                                   server.arg("displayNumConfirmed") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayDistanceDetail,
                                   server.arg("displayDistanceDetail") == "on");
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_CONFIRMATION_TIME_SECONDS,
                            atoi(server.arg("confirmationTimeWindow").c_str()));
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_BLUETOOTH,
                            (bool) (server.arg("bluetooth") == "on"));
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_SIM_RA,
                            (bool) (server.arg("simRaMode") == "on"));
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_PORTAL_TOKEN,
                            server.arg("obsUserID"));
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_PORTAL_URL,
                            server.arg("hostname"));

  std::vector<int> offsets;
  offsets.push_back(atoi(server.arg("offsetS2").c_str()));
  offsets.push_back(atoi(server.arg("offsetS1").c_str()));
  theObsConfig->setOffsets(0, offsets);

  const String gpsFix = server.arg("gpsFix");
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_GPS_FIX,
                            atoi(server.arg("gpsFix").c_str()));

  // TODO: cleanup
  const String privacyOptions = server.arg("privacyOptions");
  const String overridePrivacy = server.arg("overridePrivacy");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_PRIVACY_CONFIG, AbsolutePrivacy,
                                   privacyOptions == "absolutePrivacy");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_PRIVACY_CONFIG, NoPosition,
                                   privacyOptions == "noPosition");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_PRIVACY_CONFIG, NoPrivacy,
                                   privacyOptions == "noPrivacy");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_PRIVACY_CONFIG, OverridePrivacy,
                                   overridePrivacy == "on");

  theObsConfig->saveConfig();

  String s = "<meta http-equiv='refresh' content='0; url=/settings/general'><a href='/settings/general'>Go Back</a>";
  server.send(200, "text/html", s); //Send web page
}

void wifiAction() {
  const String ssid = server.arg("ssid");
  const String password = server.arg("pass");
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_WIFI_SSID, ssid);
  if(strcmp(password.c_str(), "******") != 0) {
    theObsConfig->setProperty(0, ObsConfig::PROPERTY_WIFI_PASSWORD, password);
  }
  theObsConfig->saveConfig();

  String s = "<meta http-equiv='refresh' content='0; url=/settings/wifi'><a href='/settings/wifi'>Go Back</a>";
  server.send(200, "text/html", s);
}

String keyValue(const String& key, const String& value, const String& suffix = "") {
  return "<b>" + key + ":</b> " + value + suffix + "<br />";
}

String keyValue(const String& key, const uint32_t value, const String& suffix = "") {
  return keyValue(key, String(value), suffix);
}

String keyValue(const String& key, const int32_t value, const String& suffix = "") {
  return keyValue(key, String(value), suffix);
}

String keyValue(const String& key, const uint64_t value, const String& suffix = "") {
  // is long this sufficient?
  return keyValue(key, String((unsigned long) value), suffix);
}

void aboutPage() {
  gps.pollStatistics(); // takes ~100ms!

  String page;

  page += "<h3>ESP32</h3>"; // SPDIFF
  page += keyValue("Heap size", ObsUtils::toScaledByteString(ESP.getHeapSize()));
  page += keyValue("Free heap", ObsUtils::toScaledByteString(ESP.getFreeHeap()));
  page += keyValue("Min. free heap", ObsUtils::toScaledByteString(ESP.getMinFreeHeap()));
  String chipId = String((uint32_t) ESP.getEfuseMac(), HEX) + String((uint32_t) (ESP.getEfuseMac() >> 32), HEX);
  chipId.toUpperCase();
  page += keyValue("Chip id", chipId);
  page += keyValue("IDF Version", esp_get_idf_version());

  page += keyValue("App size", ObsUtils::toScaledByteString(ESP.getSketchSize()));
  page += keyValue("App space", ObsUtils::toScaledByteString(ESP.getFreeSketchSpace()));
  page += keyValue("Flash speed", ESP.getFlashChipSpeed() / 1000 / 1000, "MHz");
  page += keyValue("App 'DEVELOP'",
#ifdef DEVELOP
                   "true"
#else
                        "false"
#endif
  );
#ifdef CONFIG_LOG_DEFAULT_LEVEL
  page += keyValue("Log default level", String(CONFIG_LOG_DEFAULT_LEVEL));
#endif
#ifdef CORE_DEBUG_LEVEL
  page += keyValue("Core debug level", String(CORE_DEBUG_LEVEL));
#endif

  esp_chip_info_t ci;
  esp_chip_info(&ci);
  page += keyValue("Cores", String(ci.cores));
  page += keyValue("CPU frequency", ESP.getCpuFreqMHz(), "MHz");

  page += keyValue("SPIFFS size", ObsUtils::toScaledByteString(SPIFFS.totalBytes()));
  page += keyValue("SPIFFS used", ObsUtils::toScaledByteString(SPIFFS.usedBytes()));

  String files;
  auto dir = SPIFFS.open("/");
  auto file = dir.openNextFile();
  while(file) {
    files += "<br />";
    files += file.name();
    files += " ";
    files += ObsUtils::toScaledByteString(file.size());
    files += " ";
    files += ObsUtils::dateTimeToString(file.getLastWrite());
    file.close();
    file = dir.openNextFile();
  }
  dir.close();
  page += keyValue("SPIFFS files", files);
  page += keyValue("System date time", ObsUtils::dateTimeToString(file.getLastWrite()));
  page += keyValue("System millis", String(millis()));

  if (voltageMeter) {
    page += keyValue("Battery voltage", String(voltageMeter->read(), 2), "V");
  }

  page += "<h3>SD Card</h3>";

  page += keyValue("SD card size", ObsUtils::toScaledByteString(SD.cardSize()));

  String sdCardType;
  switch (SD.cardType()) {
    case CARD_NONE: sdCardType = "NONE"; break;
    case CARD_MMC:  sdCardType = "MMC"; break;
    case CARD_SD:   sdCardType = "SD"; break;
    case CARD_SDHC: sdCardType = "SDHC"; break;
    default:        sdCardType = "UNKNOWN"; break;
  }

  page += keyValue("SD card type", sdCardType);
  page += keyValue("SD fs size", ObsUtils::toScaledByteString(SD.totalBytes()));
  page += keyValue("SD fs used", ObsUtils::toScaledByteString(SD.usedBytes()));

  page += "<h3>TOF Sensors</h3>";
  page += keyValue("Left Sensor raw", sensorManager->getRawMedianDistance(LEFT_SENSOR_ID), "cm");
  page += keyValue("Left Sensor max duration", sensorManager->getMaxDurationUs(LEFT_SENSOR_ID), "&#xB5;s");
  page += keyValue("Left Sensor min duration", sensorManager->getMinDurationUs(LEFT_SENSOR_ID), "&#xB5;s");
  page += keyValue("Left Sensor last start delay", sensorManager->getLastDelayTillStartUs(LEFT_SENSOR_ID), "&#xB5;s");
  page += keyValue("Right Sensor raw", sensorManager->getRawMedianDistance(RIGHT_SENSOR_ID), "cm");
  page += keyValue("Right Sensor max duration", sensorManager->getMaxDurationUs(RIGHT_SENSOR_ID), "&#xB5;s");
  page += keyValue("Right Sensor min duration", sensorManager->getMinDurationUs(RIGHT_SENSOR_ID), "&#xB5;s");
  page += keyValue("Right Sensor last start delay", sensorManager->getLastDelayTillStartUs(RIGHT_SENSOR_ID), "&#xB5;s");

  page += "<h3>GPS</h3>";
  page += keyValue("GPS valid checksum", gps.getValidMessageCount());
  page += keyValue("GPS failed checksum", gps.getMessagesWithFailedCrcCount());
  page += keyValue("GPS hdop", gps.getCurrentGpsRecord().getHdopString());
  page += keyValue("GPS fix", String(gps.getCurrentGpsRecord().getFixStatus(), 16));
  page += keyValue("GPS fix flags", String(gps.getCurrentGpsRecord().getFixStatusFlags(), 16));
  page += keyValue("GPS satellites", gps.getValidSatellites());
  page += keyValue("GPS uptime", gps.getUptime(), "ms");
  page += keyValue("GPS noise level", gps.getLastNoiseLevel());
  page += keyValue("GPS baud rate", gps.getBaudRate());
  page += keyValue("GPS ALP bytes", gps.getNumberOfAlpBytesSent());
  page += keyValue("GPS messages", gps.getMessagesHtml());

  page += "<h3>Display / Button</h3>";
  page += keyValue("Button State", digitalRead(PushButton_PIN));
  page += keyValue("Display i2c last error", Wire.lastError());
  page += keyValue("Display i2c speed", Wire.getClock() / 1000, "KHz");
  page += keyValue("Display i2c timeout", Wire.getTimeOut(), "ms");

  page = createPage(page);
  page = replaceDefault(page, "About");
  server.send(200, "text/html", page);
}

void privacyAction() {

  String latitude = server.arg("newlatitude");
  latitude.replace(",", ".");
  String longitude = server.arg("newlongitude");
  longitude.replace(",", ".");
  String radius = server.arg("newradius");

  if ( (latitude != "") && (longitude != "") && (radius != "") ) {
    Serial.println(F("Valid privacyArea!"));
    theObsConfig->addPrivacyArea(0,
      Gps::newPrivacyArea(atof(latitude.c_str()), atof(longitude.c_str()), atoi(radius.c_str())));
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
  SoftAccOK  =  WiFi.softAP(APName.c_str(), APPassword.c_str()); // Passwortlänge mindestens 8 Zeichen !
  delay(2000); // Without delay I've seen the IP address blank
  /* Soft AP network parameters */
  IPAddress apIP(172, 20, 0, 1);
  IPAddress netMsk(255, 255, 255, 0);

  displayTest->showTextOnGrid(0, 1, "AP:");
  displayTest->showTextOnGrid(1, 1, "");
  displayTest->showTextOnGrid(0, 2, APName.c_str());


  WiFi.softAPConfig(apIP, apIP, netMsk);
  if (SoftAccOK) {
    /* Setup the DNS server redirecting all the domains to the apIP */
    //dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    //dnsServer.start(DNS_PORT, "*", apIP);

    Serial.println(F("AP successful."));

    displayTest->showTextOnGrid(0, 3, "Pass:");
    displayTest->showTextOnGrid(1, 3, APPassword);

    displayTest->showTextOnGrid(0, 4, "IP:");
    displayTest->showTextOnGrid(1, 4, WiFi.softAPIP().toString());
  } else {
    Serial.println(F("Soft AP Error."));
    Serial.println(APName.c_str());
    Serial.println(APPassword.c_str());
  }
  return SoftAccOK;
}

void startServer(ObsConfig *obsConfig) {
  theObsConfig = obsConfig;

  uint64_t chipid_num;
  chipid_num = ESP.getEfuseMac();
  esp_chipid = String((uint16_t)(chipid_num >> 32), HEX);
  esp_chipid += String((uint32_t)chipid_num, HEX);

  displayTest->clear();
  displayTest->showTextOnGrid(0, 0, "Ver.:");
  displayTest->showTextOnGrid(1, 0, OBSVersion);

  displayTest->showTextOnGrid(0, 1, "SSID:");
  displayTest->showTextOnGrid(1, 1,
                              theObsConfig->getProperty<String>(ObsConfig::PROPERTY_WIFI_SSID));

  tryWiFiConnect(obsConfig);

  if (WiFiClass::status() != WL_CONNECTED) {
    CreateWifiSoftAP(esp_chipid);
    touchConfigServerHttp(); // side effect do not allow track upload via button
  } else {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    displayTest->showTextOnGrid(0, 2, "IP:");
    displayTest->showTextOnGrid(1, 2, WiFi.localIP().toString().c_str());
  }

  /*use mdns for host name resolution*/
  if (WiFiGenericClass::getMode() != WIFI_MODE_STA) {
    if (!MDNS.begin(host)) { //http://openbikesensor.local
      log_e("Error setting up MDNS responder for %s!", host);
    } else {
      log_i("mDNS responder started for %s!", host);
    }
  }
  /*return index page which is stored in serverIndex */

  ObsUtils::setClockByNtp(WiFi.gatewayIP().toString().c_str());
  if (!voltageMeter) {
    voltageMeter = new VoltageMeter();
  }

  if (SD.begin() && WiFiClass::status() == WL_CONNECTED) {
    AlpData::update(displayTest);
  }


  // #############################################
  // Handle web pages
  // #############################################

  // ### Reboot ###

  server.on("/reboot", HTTP_GET, []() {

    String html = createPage(rebootIndex);
    html = replaceDefault(html, "Reboot");
    server.send(200, "text/html", html);
    delay(100);
    ESP.restart();
  });

  server.on("/upload", handleUpload);

  // ###############################################################
  // ### Index ###
  // ###############################################################

  server.on("/", HTTP_GET, []() {
    String html = createPage(navigationIndex);
    html = replaceDefault(html, "Navigation");
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
    String html = createPage(backupIndex, xhrUpload);
    html = replaceDefault(html, "Backup & Restore");
    html.replace("{method}", "/settings/restore");
    html.replace("{accept}", ".json");
    server.send(200, "text/html", html);
  });

  server.on("/settings/backup.json", HTTP_GET, []() {
    const String fileName
      = String(theObsConfig->getProperty<String>(ObsConfig::PROPERTY_OBS_NAME)) + "-" + OBSVersion;
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + fileName + ".json\"");
    log_d("Sending config for backup %s:", fileName.c_str());
    theObsConfig->printConfig();
    server.send(200, "application/json", theObsConfig->asJsonString());
  });

  server.on("/settings/restore", HTTP_POST, []() {
    server.send(200, "text/plain", "Restore successful!");
  }, []() {
    //Serial.println("Recover Config...");
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Recover: %s\n", upload.filename.c_str());
      json_buffer = "";
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      // Convert all uint8_t elements until currentSize to String
      for(int i = 0; i<upload.currentSize; i++) {
        json_buffer += (char) upload.buf[i];
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      theObsConfig->parseJson(json_buffer);
      theObsConfig->fill(config); // OK here??
      theObsConfig->printConfig();
      log_d("Saving configuration...");
      theObsConfig->saveConfig();
    }
  });

  // ###############################################################
  // ### Wifi ###
  // ###############################################################

  server.on("/settings/wifi/action", wifiAction);

  server.on("/settings/wifi", HTTP_GET, []() {
    String html = createPage(wifiSettingsIndex);
    html = replaceDefault(html, "WiFi", "/settings/wifi/action");

    // Form data
    html.replace("{ssid}", theObsConfig->getProperty<String>(ObsConfig::PROPERTY_WIFI_SSID));
    if(theObsConfig->getProperty<String>(ObsConfig::PROPERTY_WIFI_SSID).length() > 0) {
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
    String html = createPage(devIndex);
    html = replaceDefault(html, "Development Setting", "/settings/development/action");

    // SHOWGRID
    bool showGrid = theObsConfig->getBitMaskProperty(
      0, ObsConfig::PROPERTY_DEVELOPER, ShowGrid);
    html.replace("{showGrid}", showGrid ? "checked" : "");

    bool printWifiPassword = theObsConfig->getBitMaskProperty(
      0, ObsConfig::PROPERTY_DEVELOPER, PrintWifiPassword);
    html.replace("{printWifiPassword}", printWifiPassword ? "checked" : "");

    server.send(200, "text/html", html);
  });

#endif

  // ###############################################################
  // ### Config ###
  // ###############################################################

  server.on("/settings/general/action", configAction);

  server.on("/settings/general", HTTP_GET, []() {
    String html = createPage(configIndex);
    html = replaceDefault(html, "General", "/settings/general/action");

    // Form data
    const std::vector<int> offsets
      = theObsConfig->getIntegersProperty(ObsConfig::PROPERTY_OFFSET);
    html.replace("{offset1}", String(offsets[LEFT_SENSOR_ID]));
    html.replace("{offset2}", String(offsets[RIGHT_SENSOR_ID]));
    html.replace("{hostname}",
                 theObsConfig->getProperty<String>(ObsConfig::PROPERTY_PORTAL_URL));
    html.replace("{userId}",
                theObsConfig->getProperty<String>(ObsConfig::PROPERTY_PORTAL_TOKEN));
    html.replace("{confirmationTimeWindow}",
                 String(theObsConfig->getProperty<String>(ObsConfig::PROPERTY_CONFIRMATION_TIME_SECONDS)));

    const uint displayConfig = (uint) theObsConfig->getProperty<uint>(
      ObsConfig::PROPERTY_DISPLAY_CONFIG);
    bool displaySimple = displayConfig & DisplaySimple;
    bool displayGPS = displayConfig & DisplaySatellites;
    bool displayLeft = displayConfig & DisplayLeft;
    bool displayRight = displayConfig & DisplayRight;
    bool displayVelo = displayConfig & DisplayVelocity;
    bool displaySwapSensors = displayConfig & DisplaySwapSensors;
    bool displayInvert = displayConfig & DisplayInvert;
    bool displayFlip = displayConfig & DisplayFlip;
    bool displayNumConfirmed = displayConfig & DisplayNumConfirmed;
    bool displayDistanceDetail = displayConfig & DisplayDistanceDetail;

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

    html.replace("{bluetooth}",
                 theObsConfig->getProperty<bool>(ObsConfig::PROPERTY_BLUETOOTH) ? "checked" : "");
    html.replace("{simRaMode}",
                 theObsConfig->getProperty<bool>(ObsConfig::PROPERTY_SIM_RA) ? "checked" : "");

    int gpsFix = theObsConfig->getProperty<int>(ObsConfig::PROPERTY_GPS_FIX);
    html.replace("{fixPos}", gpsFix == (int) Gps::WaitFor::FIX_POS || gpsFix > 0 ? "selected" : "");
    html.replace("{fixTime}", gpsFix == (int) Gps::WaitFor::FIX_TIME ? "selected" : "");
    html.replace("{fixNoWait}", gpsFix == (int) Gps::WaitFor::FIX_NO_WAIT ? "selected" : "");

    const uint privacyConfig = (uint) theObsConfig->getProperty<int>(
      ObsConfig::PROPERTY_PRIVACY_CONFIG);
    const bool absolutePrivacy = privacyConfig & AbsolutePrivacy;
    const bool noPosition = privacyConfig & NoPosition;
    const bool noPrivacy = privacyConfig & NoPrivacy;
    const bool overridePrivacy = privacyConfig & OverridePrivacy;

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
    String html = createPage(uploadIndex, xhrUpload);
    html = replaceDefault(html, "Update Firmware");
    html.replace("{method}", "/update");
    html.replace("{accept}", ".bin");

    server.send(200, "text/html", html);
  });

  // Handling uploading firmware file
  server.on("/update", HTTP_POST, []() {
    if (Update.hasError()) {
      server.send(500, "text/plain",
        "Update failed! Note: update from v0.2.x to v0.3 or newer needs to be done once with USB cable!");
      displayTest->showTextOnGrid(0, 3, Update.errorString());
      sensorManager->attachInterrupts();
    } else {
      server.send(200, "text/plain", "Update successful! Device reboots now!");
      displayTest->showTextOnGrid(0, 3, "Success rebooting...");
      delay(250);
      ESP.restart();
    }
  }, []() {
    sensorManager->detachInterrupts();
    HTTPUpload &upload = server.upload();
    Update.onProgress([](size_t pos, size_t all) {
      displayTest->drawProgressBar(4, pos, all);
    });
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin()) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %ul\nRebooting...\n", upload.totalSize);
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

    String html = createPage(makeCurrentLocationPrivateIndex);
    html = replaceDefault(html, "MakeLocationPrivate");

    server.send(200, "text/html", html);

    bool validGPSData = false;
    buttonState = digitalRead(PushButton_PIN);
    while (!validGPSData && (buttonState == LOW)) {
      log_d("GPSData not valid");
      buttonState = digitalRead(PushButton_PIN);
      gps.handle();
      validGPSData = gps.getCurrentGpsRecord().hasValidFix();
      if (validGPSData) {
        log_d("GPSData valid");
        // FIXME: Not used?
        Gps::newPrivacyArea(gps.getCurrentGpsRecord().getLatitude(),
                            gps.getCurrentGpsRecord().getLongitude(), 500);
      }
      delay(300);
    }

    // #77 - 200 cannot be send twice via HTTP
    //String s = "<meta http-equiv='refresh' content='0; url=/settings/privacy'><a href='/settings/privacy'>Go Back</a>";
    //server.send(200, "text/html", s); //Send web page

  });

  server.on("/settings/privacy", HTTP_GET, []() {
    String privacyPage;
    for (int idx = 0; idx < theObsConfig->getNumberOfPrivacyAreas(0); ++idx) {
      auto pa = theObsConfig->getPrivacyArea(0, idx);
      privacyPage += "<h3>Privacy Area #" + String(idx + 1) + "</h3>";
      privacyPage += "Latitude <input name=latitude" + String(idx)
        + " placeholder='latitude' value='" + String(pa.latitude, 7) + "' disabled />";
      privacyPage += "Longitude <input name='longitude" + String(idx)
        + "' placeholder='longitude' value='" + String(pa.longitude, 7) + "' disabled />";
      privacyPage += "Radius (m) <input name='radius" + String(idx)
        + "' placeholder='radius' value='" + String(pa.radius) + "' disabled />";
      privacyPage += "<a class='deletePrivacyArea' href='/privacy_delete?erase=" + String(idx) + "'>&#x2716;</a>";
    }

    privacyPage += "<h3>New Privacy Area  <a href='javascript:window.location.reload()'>&#8635;</a></h3>";
    gps.handle();
    bool validGPSData = gps.getCurrentGpsRecord().hasValidFix();
    if (validGPSData) {
      privacyPage += "Latitude<input name='newlatitude' value='" + gps.getCurrentGpsRecord().getLatString() + "' />";
      privacyPage += "Longitude<input name='newlongitude' value='" + gps.getCurrentGpsRecord().getLongString() + "' />";
    } else {
      privacyPage += "Latitude<input name='newlatitude' placeholder='48.12345' />";
      privacyPage += "Longitude<input name='newlongitude' placeholder='9.12345' />";
    }
    privacyPage += "Radius (m)<input name='newradius' placeholder='radius' value='500' />";

    String html = createPage(privacyPage, privacyIndexPostfix);
    html = replaceDefault(html, "Privacy Zones", "/privacy_action");
    server.send(200, "text/html", html);
  });

  server.on("/privacy_delete", HTTP_GET, []() {

    String erase = server.arg("erase");
    if (erase != "") {
      theObsConfig->removePrivacyArea(0, atoi(erase.c_str()));
      theObsConfig->saveConfig();
    }

    String s = "<meta http-equiv='refresh' content='0; url=/settings/privacy'><a href='/settings/privacy'>Go Back</a>";
    server.send(200, "text/html", s);
  });

  server.on("/about", aboutPage);

  server.on("/privacy_action", privacyAction);

  // ###############################################################
  // SD card file systen access
  // ###############################################################

  server.on("/sd", []() {
    String path = "/";
    if (server.hasArg("path")) {
      path = server.arg("path");
    }

    File file = SDFileSystem.open(path);

    if (!file) {
      server.send(404, "text/plain", "File not found.");
      return;
    }

    if (file.isDirectory()) {
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server.send(200, "text/html");

      String html = header;
      html = replaceDefault(html, "SD Card Contents " + String(file.name()));
      html += "<ul class=\"directory-listing\">";
      server.sendContent(html);
      html.clear();
      displayTest->showTextOnGrid(0, 3, "Path:");
      displayTest->showTextOnGrid(1, 3, path);

      // Iterate over directories
      File child = file.openNextFile();
      uint16_t counter = 0;
      while (child) {
        displayTest->drawWaitBar(5, counter++);

        String fileName = String(child.name());
        fileName = fileName.substring(int (fileName.lastIndexOf("/") + 1));
        bool isDirectory = child.isDirectory();
        html +=
          ("<li class=\""
          + String(isDirectory ? "directory" : "file")
          + "\"><a href=\"/sd?path="
          + String(child.name())
          + "\">"
          + String(isDirectory ? "&#x1F4C1;" : "&#x1F4C4;")
          + fileName
          + String(isDirectory ? "/" : "")
          + "</a></li>");

        child.close();
        child = file.openNextFile();

        if (html.length() >= (HTTP_UPLOAD_BUFLEN - 80)) {
          server.sendContent(html);
          html.clear();
        }
      }
      file.close();
      html += "</ul>";
      html += footer;
      server.sendContent(html);
      displayTest->clearProgressBar(5);
      return;
    }

    String dataType;
    if (path.endsWith(".htm")) {
      dataType = "text/html";
    } else if (path.endsWith(".css")) {
      dataType = "text/css";
    } else if (path.endsWith(".js")) {
      dataType = "application/javascript";
    } else if (path.endsWith(".png")) {
      dataType = "image/png";
    } else if (path.endsWith(".gif")) {
      dataType = "image/gif";
    } else if (path.endsWith(".jpg")) {
      dataType = "image/jpeg";
    } else if (path.endsWith(".ico")) {
      dataType = "image/x-icon";
    } else if (path.endsWith(".xml")) {
      dataType = "text/xml";
    } else if (path.endsWith(".pdf")) {
      dataType = "application/pdf";
    } else if (path.endsWith(".zip")) {
      dataType = "application/zip";
    } else if (path.endsWith(".csv")) {
      dataType = "text/csv";
    } else {
      // arbitrary data
      dataType = "application/octet-stream";
    }

    String fileName = String(file.name());
    fileName = fileName.substring((int) (fileName.lastIndexOf("/") + 1));
    server.sendHeader("Content-Disposition", String("attachment; filename=\"") + fileName + String("\""));
    server.streamFile(file, dataType);
    file.close();
  });

  // ###############################################################
  // Default, send 404
  // ###############################################################

  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("Server Ready");

}

/* Upload tracks found on SD card to the portal server.
 * This method also takes care to give appropriate feedback
 * to the user about the progress. If httpRequest is true
 * a html response page is created. Also the progress can be
 * seen on the display if connected.
 */
void uploadTracks(bool httpRequest) {
  const String &portalToken
    = theObsConfig->getProperty<String>(ObsConfig::PROPERTY_PORTAL_TOKEN);
  Uploader uploader(
    theObsConfig->getProperty<String>(ObsConfig::PROPERTY_PORTAL_URL), portalToken);

  configServerWasConnectedViaHttpFlag = true;
  SDFileSystem.mkdir("/uploaded");

  String sdErrorMessage = ensureSdIsAvailable();
  if (sdErrorMessage != "") {
    if (httpRequest) {
      server.send(500, "text/plain", sdErrorMessage);
    }
    displayTest->showTextOnGrid(0, 3, "Error:");
    displayTest->showTextOnGrid(0, 4, sdErrorMessage);
    return;
  }

  String html = replaceDefault(header, "Upload Tracks");
  html += "<h3>Uploading tracks...</h3>";
  html += "<div>";
  if (httpRequest) {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html");
    server.sendContent(html);
  }
  html.clear();

  if (portalToken.isEmpty()) {
    if (httpRequest) {
      html += "</div><h3>API Key not set</h3/>";
      html += "See <a href='https://www.openbikesensor.org/benutzer-anleitung/konfiguration.html'>";
      html += "configuration</a>.</div>";
      html += "<input type=button onclick=\"window.location.href='/'\" class='btn' value='OK' />";
      html += footer;
      server.sendContent(html);
    }
    displayTest->showTextOnGrid(0, 3, "Error:");
    displayTest->showTextOnGrid(0, 4, "API Key not set");
    return;
  }

  const uint16_t numberOfFiles = countFilesInRoot();

  uint16_t currentFileIndex = 0;
  uint16_t okCount = 0;
  uint16_t failedCount = 0;
  File root = SDFileSystem.open("/");
  File file = root.openNextFile();
  while (file) {
    const String fileName(file.name());
    log_d("Upload file: %s", fileName.c_str());
    if (!file.isDirectory()
        && fileName.endsWith(CSVFileWriter::EXTENSION)) {
      const String friendlyFileName = ObsUtils::stripCsvFileName(fileName);
      currentFileIndex++;

      displayTest->showTextOnGrid(0, 4, friendlyFileName);
      displayTest->drawProgressBar(3, currentFileIndex, numberOfFiles);
      if (httpRequest) {
        server.sendContent(friendlyFileName);
      }
      const boolean uploaded = uploader.upload(file.name());
      file.close();
      if (uploaded) {
        moveToUploaded(fileName);
        html += "<a href='" + ObsUtils::encodeForXmlAttribute(uploader.getLastLocation())
          + "' title='" + ObsUtils::encodeForXmlAttribute(uploader.getLastStatusMessage())
          + "' target='_blank'>" + HTML_ENTITY_OK_MARK  + "</a>";
        okCount++;
      } else {
        html += "<a href='#' title='" + ObsUtils::encodeForXmlAttribute(uploader.getLastStatusMessage())
          + "'>" + HTML_ENTITY_FAILED_CROSS + "</a>";
        failedCount++;
      }
      if (httpRequest) {
        html += "<br />\n";
        server.sendContent(html);
      }
      html.clear();
      displayTest->clearProgressBar(5);
    } else {
      file.close();
    }
    file = root.openNextFile();
  }
  root.close();

  displayTest->clearProgressBar(3);
  displayTest->showTextOnGrid(0, 4, "");
  displayTest->showTextOnGrid(1, 3, "Upload done.");
  displayTest->showTextOnGrid(0, 5, "OK:");
  displayTest->showTextOnGrid(1, 5, String(okCount));
  displayTest->showTextOnGrid(2, 5, "Failed:");
  displayTest->showTextOnGrid(3, 5, String(failedCount));
  if (httpRequest) {
    html += "</div><h3>...all files done</h3/>";
    html += keyValue("OK", okCount);
    html += keyValue("Failed", failedCount);
    html += "<input type=button onclick=\"window.location.href='/'\" class='btn' value='OK' />";
    html += footer;
    server.sendContent(html);
  }
}

void moveToUploaded(const String &fileName) {
  int i = 0;
  while (!SDFileSystem.rename(
    fileName, String("/uploaded") + fileName + (i == 0 ? "" : String(i)))) {
    i++;
    if (i > 100) {
      break;
    }
  }
}

String ensureSdIsAvailable() {
  String result = "";
  File root = SDFileSystem.open("/");
  if (!root) {
    result = "Failed to open SD directory";
  } else if (!root.isDirectory()) {
    result = "SD root is not a directory?";
  }
  root.close();
  return result;
}

uint16_t countFilesInRoot() {
  uint16_t numberOfFiles = 0;
  File root = SDFileSystem.open("/");
  File file = root.openNextFile("r");
  while (file) {
    if (!file.isDirectory()
        && String(file.name()).endsWith(CSVFileWriter::EXTENSION)) {
      numberOfFiles++;
      displayTest->drawWaitBar(4, numberOfFiles);
    }
    file = root.openNextFile();
  }
  root.close();
  displayTest->clearProgressBar(4);
  return numberOfFiles;
}


void tryWiFiConnect(const ObsConfig *obsConfig) {
  if (!WiFiGenericClass::mode(WIFI_MODE_STA)) {
    log_e("Failed to enable WiFi station mode.");
  }
  const char* hostname
    = obsConfig->getProperty<const char *>(ObsConfig::PROPERTY_OBS_NAME);
  if (!WiFi.setHostname(hostname)) {
    log_e("Failed to set hostname to %s.", hostname);
  }

  const auto startTime = millis();
  const uint16_t timeout = 10000;
  // Connect to WiFi network
  while ((WiFiClass::status() != WL_CONNECTED) && (( millis() - startTime) <= timeout)) {
    log_d("Trying to connect to %s",
      theObsConfig->getProperty<const char *>(ObsConfig::PROPERTY_WIFI_SSID));
    wl_status_t status = WiFi.begin(
      theObsConfig->getProperty<const char *>(ObsConfig::PROPERTY_WIFI_SSID),
      theObsConfig->getProperty<const char *>(ObsConfig::PROPERTY_WIFI_PASSWORD));
    log_d("WiFi status after begin is %d", status);
    status = static_cast<wl_status_t>(WiFi.waitForConnectResult());
    log_d("WiFi status after wait is %d", status);
    if (status >= WL_CONNECT_FAILED) {
      log_d("WiFi resetting connection for retry.");
      WiFi.disconnect(true, true);
    } else if (status == WL_NO_SSID_AVAIL){
      log_d("WiFi SSID not found - try rescan.");
      WiFi.scanNetworks(false);
    }
    delay(250);
  }
}
