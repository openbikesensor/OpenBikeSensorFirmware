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

// Based on https://lastminuteengineers.com/esp32-ota-web-updater-arduino-ide/
// The information provided on the LastMinuteEngineers.com may be used, copied,
// remix, transform, build upon the material and distributed for any purposes
// only if provided appropriate credit to the author and link to the original article.

#include <configServer.h>
#include <OpenBikeSensorFirmware.h>
#include <uploader.h>
#include <HTTPURLEncodedBodyParser.hpp>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <DNSServer.h>
#include "SPIFFS.h"
#include "HTTPMultipartBodyParser.hpp"
#include "Firmware.h"
#include "utils/https.h"
#include "utils/timeutils.h"
#include "obsimprov.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_wifi.h>
#include <esp_arduino_version.h>

using namespace httpsserver;

static const char *const HTML_ENTITY_FAILED_CROSS = "&#x274C;";
static const char *const HTML_ENTITY_OK_MARK = "&#x2705;";
static const char *const HTML_ENTITY_WASTEBASKET = "&#x1f5d1;";
static const char *const HTTP_GET = "GET";
static const char *const HTTP_POST = "POST";

static const size_t HTTP_UPLOAD_BUFLEN = 1024; // TODO: refine

static ObsConfig *theObsConfig;
static HTTPSServer * server;
static HTTPServer * insecureServer;
static SSLCert * serverSslCert;
static String OBS_ID;
static String OBS_ID_SHORT;
static DNSServer *dnsServer = nullptr;
static ObsImprov *obsImprov = nullptr;
static WiFiMulti wifiMulti;

// TODO
//  - Fix CSS Style for mobile && desktop
//  - a vs. button
//  - back navigation after save
static const char* const header =
  "<!DOCTYPE html>\n"
  "<html lang='en'><head><meta charset='utf-8'/><title>{title}</title><meta name='viewport' content='width=device-width,initial-scale=1.0' />"
// STYLE
    "<style>"
    "#file-input,input, button {width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px;}"
    ".small {height:12px;width:12px;margin:2px}"
    "input, button, a.back {background:#f1f1f1;border:0;padding:0;text-align:center;}"
    "body {background:#3498db;font-family:'Open Sans',sans-serif;font-size:12px;color:#777}"
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
    "a.previous {text-decoration: none; display: inline-block; padding: 8px 16px;background-color: #f1f1f1; color: black;border-radius: 50%; font-family: 'Open Sans', sans-serif; font-size: 18px}"
    "a.previous:hover {background-color: #ddd; color: black;}"
    "ul.directory-listing {list-style: none; text-align: left; padding: 0; margin: 0; line-height: 1.5;}"
    "li.directory a {text-decoration: none; font-weight: bold;}"
    "li.file a {text-decoration: none;}"
    "</style>"

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
  "<form action='{action}' method='POST'>"
  "<h1><a href='/'>OpenBikeSensor</a></h1>"
  "<h2>{subtitle}</h2>"
  "<p>Firmware version: {version}</p>"
  "<a href=\"javascript:history.back()\" class='previous'>&#8249;</a>";

static const char* const footer = "</form></body></html>";

// #########################################
// Upload form
// #########################################

static const char* const xhrUpload =
  "<input type='file' name='upload' id='file' accept='{accept}'>"
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

static const char* const navigationIndex =
  "<input type=button onclick=\"window.location.href='/upload'\" class=btn value='Upload Tracks'>"
  "<h3>Settings</h3>"
  "<input type=button onclick=\"window.location.href='/settings/general'\" class=btn value='General'>"
  "<input type=button onclick=\"window.location.href='/settings/privacy'\" class=btn value='Privacy Zones'>"
  "<input type=button onclick=\"window.location.href='/settings/wifi'\" class=btn value='Wifi'>"
  "<input type=button onclick=\"window.location.href='/settings/backup'\" class=btn value='Backup &amp; Restore'>"
  "<input type=button onclick=\"window.location.href='/settings/security'\" class=btn value='Security'>"
  "<h3>Maintenance</h3>"
  "<input type=button onclick=\"window.location.href='/updatesd'\" class=btn value='Update Firmware'>"
  "<input type=button onclick=\"window.location.href='/updateFlash'\" class=btn value='Update Flash App'>"
  "<input type=button onclick=\"window.location.href='/sd'\" class=btn value='Show SD Card Contents'>"
  "<input type=button onclick=\"window.location.href='/about'\" class=btn value='About'>"
  "<input type=button onclick=\"window.location.href='/delete'\" class=btn value='Delete'>"
  "<input type=button onclick=\"window.location.href='/reboot'\" class=btn value='Reboot'>";

static const char* const httpsRedirect =
  "<h3>HTTPS</h3>"
  "You need to access the obs via secure https. If not done already, you also need to "
  "accept the self signed cert from the OBS after pressing 'Goto https'. Login is 'obs' "
  "and the up to 6 digit pin displayed "
  "on the OBS."
  "<input type=button onclick=\"window.location.href='https://{host}'\" class=btn value='Goto https'>"
  "<input type=button onclick=\"window.location.href='/cert'\" class=btn value='Download Cert'>"
  "<hr/>If you are in a local network under your control with no risk of hostile external access, "
  "you can enable unencrypted access."
  "<input type='submit' name='http' id='http' class=btn value='Enable unencrypted access' />";

// #########################################
// Development
// #########################################

static const char* const development =
  "<h3>Development</h3>"
  "<input type=button onclick=\"window.location.href='/settings/development'\" class=btn value='Development'>";

// #########################################
// Reboot
// #########################################

static const char* const rebootIndex =
  "<h3>Device reboots now.</h3>";

static const char* const gpsColdIndex =
  "<h3>GPS cold start</h3>";

// #########################################
// Wifi
// #########################################

static const char* const wifiSettingsIndexPostfix =
  "<input type='submit' class='btn' value='Save'>"
;


static const char* const backupIndex =
  "<p>This backups and restores the device configuration incl. the Basic Config, Privacy Zones and Wifi Settings.</p>"
  "<h3>Backup</h3>"
  "<input type='button' onclick=\"window.location.href='/settings/backup.json'\" class=btn value='Download' />"
  "<h3>Restore</h3>";

static const char* const updateSdIndex =
  "<p>{description}</p>\n"
  "<h3>From Github (preferred)</h3>\n"
  "List also pre-releases<br><input type='checkbox' id='preReleases' onchange='selectFirmware()'>\n"
  "Ignore TLS Errors (see documentation)<br><input type='checkbox' id='ignoreSSL' onchange='selectFirmware()'>\n"
  "<script>\n"
  "let availableReleases;\n"
  "async function updateFirmwareList() {\n"
  "    (await fetch('{releaseApiUrl}')).json().then(res => {\n"
  "        availableReleases = res;\n"
  "        selectFirmware();\n"
  "    })\n"
  "}\n"
  "function selectFirmware() {\n"
  "   const displayPreReleases = (document.getElementById('preReleases').checked == true);\n"
  "   const ignoreSSL = (document.getElementById('ignoreSSL').checked == true);\n"
  "   url = \"\";\n"
  "   version = \"\";\n"
  "   availableReleases.filter(r => displayPreReleases || !r.prerelease).forEach(release => {\n"
  "           release.assets.filter(asset => asset.name.endsWith(\"flash.bin\") "
#ifdef OBSCLASSIC
  "              || asset.name.endsWith(\"firmware.bin\") "
#else
  "              || asset.name.endsWith(\"firmware-obspro.bin\") "
#endif
  "           ).forEach(\n"
  "                asset => {\n"
  "                  if (!url) {\n"
  "                   version = release.name;\n"
  "                   url = asset.browser_download_url;\n"
  "                  }\n"
  "               }\n"
  "           )\n"
  "        }\n"
  "   )\n"
  "   if (url) {\n"
  "     document.getElementById('version').value = \"Update to \" + version;\n"
  "     document.getElementById('version').disabled = false;\n"
  "     document.getElementById('downloadUrl').value = url;\n"
  "     document.getElementById('directlink').href = url;\n"
  "   } else {\n"
  "     document.getElementById('version').value = \"No version found\";\n"
  "     document.getElementById('version').disabled = true;\n"
  "     document.getElementById('downloadUrl').value = \"\";\n"
  "     document.getElementById('directlink').href = \"\";\n"
  "   }\n"
  "   if (ignoreSSL) {\n"
  "    document.getElementById('unsafe').value = \"1\";\n"
  "   } else {\n"
  "    document.getElementById('unsafe').value = \"0\";\n"
  "   }\n"
  "}\n"
  "updateFirmwareList();\n"
  "</script>\n"
  "<input type='hidden' name='downloadUrl' id='downloadUrl' value=''/>\n"
  "<input type='hidden' name='unsafe' id='unsafe' value='0'/>\n"
  "<input type='submit' name='version' id='version' class=btn value='Update' />\n"
  "If the upgrade via the button above does not work<br/><a id=\"directlink\" href=\"\">download firmware.bin</a><br/> and upload manually below.\n"
  "<h3>File Upload</h3>";

// #########################################
// Config
// #########################################

static const char* const configIndex =
  "<h3>Sensor</h3>"
  "Offset Sensor Left<input name='offsetS1' placeholder='Offset Sensor Left' value='{offset1}'>"
  "<hr>"
  "Offset Sensor Right<input name='offsetS2' placeholder='Offset Sensor Right' value='{offset2}'>"
  "<hr>"
  "Swap Sensors (Left &#8660; Right)<input type='checkbox' name='displaySwapSensors' {displaySwapSensors}>"
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
  "<input name='hostname' placeholder='API URL' value='{hostname}'>"
  "<hr>"
  "<input name='obsUserID' placeholder='API Key' value='{userId}' >"
  "<h3>Operation</h3>"
  "Enable Bluetooth <input type='checkbox' name='bluetooth' {bluetooth}>"
  "<input type=submit class=btn value=Save>";

static const char* const privacyIndexPostfix =
  "<input type='submit' class='btn' value='Save'>"
  "<hr>"
  "Location: <div id='gps'>{gps}</div> <a href='javascript:window.location.reload()'>&#8635;</a>"
  "<input type='submit' name='addCurrent' id='addCurrent' class=btn value='Add current location' />"
  "<script>"
  "async function updateLocation() {"
  "  if (document.readyState == 'complete') {"
  "    const gps = await fetch('/gps').then(res => res.text());"
  "    document.getElementById('gps').innerHTML = gps;"
  "  }"
  "  setTimeout(updateLocation, 1000);"
  "}"
  "setTimeout(updateLocation, 1000);"
  "</script>"
  ;

static const char* const deleteIndex =
  "<h3>Flash</h3>"
  "<p>Flash stores ssl certificate and configuration.</p>"
  "<label for='flash'>Format flash</label>"
  "<input type='checkbox' id='flash' name='flash' "
  "onchange=\"document.getElementById('flashCert').checked = document.getElementById('flashConfig').checked = document.getElementById('flash').checked;\">"
  "<label for='flashCert'>Delete ssl certificate, a new one will be created at the next start.</label>"
  // Link https://support.mozilla.org/en-US/kb/Certificate-contains-the-same-serial-number-as-another-certificate ?
  "<input type='checkbox' id='flashCert' name='flashCert'>"
  "<label for='flashConfig'>Delete configuration, default settings will be used at the next start,"
  " consider storing a <a href='/settings/backup.json'>Backup</a> 1st.</label>"
  "<input type='checkbox' id='flashConfig' name='flashConfig'>"
  "<h3>Memory</h3>"
  "<label for='config'>Clear current configuration, wifi connection will stay.</label>"
  "<input type='checkbox' id='config' name='config'>"
  "<h3>SD Card</h3>"
  "<label for='sdcard'>Delete OBS related content (aid_ini.ubx, tracknumber.txt, current_14d.*, "
  "*.obsdata.csv, sdflash/*, trash/*, uploaded/*). The files are just removed from to filesystem "
  "part of the data might be still read from the card. Be patient.</label>"
  "<input type='checkbox' id='sdcard' name='sdcard'>"
  "<input type='submit' class='btn' value='Delete' onclick=\"return confirm('Are you sure?')\" />";

static const char* const settingsSecurityIndex =
  "<h3>Http</h3>"
  "<label for='pin'>Wish pin for http access, the pin will still be displayed on"
  " the OBS display. Pin must consist out of 3-8 numeric digits.</label>"
  "<input name='pin' type='number' value='{pin}' maxlength='8'>"
//  "<label for='httpAccess'>Allow full access via http. Do this only in networks you"
//  " have control over. All data send or retrieved from the OBS can be intercepted"
//  " from within your network as well as everybody in this network has full access to"
//  " your OBS. The setting wil be reset if you change the WiFi settings.</label>"
//  "<input type='checkbox' id='httpAccess' name='httpAccess' />"
  "<input type=submit class=btn value='Save'>"
//  "<h3>OBS SSL Cert</h3>"
//  "<label for='flashCert'>Delete ssl certificate, a new one will be created at the next start.</label>"
//  "<input type=button onclick=\"window.location.href='/settings/deleteSslCert'\" class=btn value='Renew SSL Cert'>"
//  "<h3>CA Cert Management</h3>"
//  "For outgoing https connections the OBS has to trust different authorities (CA). "
//  "The OBS can not hold all well known authorities like your browser does "
//  "here you can add or remove the CAs your OBS trusts, usually you do not need "
//  "to modify this. You can not remove CAs trusted by the OBS by default, but "
//  "you can add additional CAs to trust here."
//  "{caList}";
;

// #########################################
static String getParameter(const std::vector<std::pair<String,String>> &params, const String& name, const String&  def = "") {
  for (const auto& param : params) {
    if (param.first == name) {
      return param.second;
    }
  }
  return def;
}


static String getParameter(HTTPRequest *req, const String& name, const String&  def = "") {
  std::string value;
  if (req->getParams()->getQueryParameter(name.c_str(), value)) {
    return String(value.c_str());
  }
  return def;
}

static String replacePlain(const String &body, const String &key, const String &value) {
  String str(body);
  str.replace(key, value);
  return str;
}

static String replaceHtml(const String &body, const String &key, const String &value) {
  return replacePlain(body, key, ObsUtils::encodeForXmlAttribute(value));
}

static std::vector<std::pair<String,String>> extractParameters(HTTPRequest *req);

static void handleNotFound(HTTPRequest * req, HTTPResponse * res);
static void handleIndex(HTTPRequest * req, HTTPResponse * res);
static void handleAbout(HTTPRequest * req, HTTPResponse * res);
static void handleReboot(HTTPRequest * req, HTTPResponse * res);
static void handleColdStartGPS(HTTPRequest * req, HTTPResponse * res);
static void handleBackup(HTTPRequest * req, HTTPResponse * res);
static void handleBackupDownload(HTTPRequest * req, HTTPResponse * res);
static void handleBackupRestore(HTTPRequest * req, HTTPResponse * res);
static void handleWifi(HTTPRequest * req, HTTPResponse * res);
static void handleWifiDeleteAction(HTTPRequest * req, HTTPResponse * res);
static void handleWifiSave(HTTPRequest * req, HTTPResponse * res);
static void handleConfig(HTTPRequest * req, HTTPResponse * res);
static void handleConfigSave(HTTPRequest * req, HTTPResponse * res);
static void handleFirmwareUpdateSd(HTTPRequest * req, HTTPResponse * res);
static void handleFirmwareUpdateSdAction(HTTPRequest * req, HTTPResponse * res);
static void handleFirmwareUpdateSdUrlAction(HTTPRequest * req, HTTPResponse * res);
static void handleFlashUpdate(HTTPRequest * req, HTTPResponse * res);
static void handleFlashFileUpdateAction(HTTPRequest * req, HTTPResponse * res);
static void handleFlashUpdateUrlAction(HTTPRequest * req, HTTPResponse * res);
#ifdef DEVELOP
static void handleDev(HTTPRequest * req, HTTPResponse * res);
static void handleDevAction(HTTPRequest * req, HTTPResponse * res);
#endif
static void handlePrivacyAction(HTTPRequest * req, HTTPResponse * res);
static void handleGps(HTTPRequest * req, HTTPResponse * res);
static void handleUpload(HTTPRequest * req, HTTPResponse * res);
static void handlePrivacy(HTTPRequest *req, HTTPResponse *res);
static void handlePrivacyDeleteAction(HTTPRequest *req, HTTPResponse *res);
static void handleSd(HTTPRequest *req, HTTPResponse *res);
static void handleDeleteFiles(HTTPRequest *req, HTTPResponse *res);
static void handleDelete(HTTPRequest *req, HTTPResponse *res);
static void handleDeleteAction(HTTPRequest *req, HTTPResponse *res);
static void handleDownloadCert(HTTPRequest *req, HTTPResponse * res);
static void handleSettingSecurity(HTTPRequest *, HTTPResponse * res);
static void handleSettingSecurityAction(HTTPRequest * req, HTTPResponse * res);

static void handleHttpsRedirect(HTTPRequest *req, HTTPResponse *res);
static void handleHttpAction(HTTPRequest *req, HTTPResponse *res);

static void accessFilter(HTTPRequest * req, HTTPResponse * res, std::function<void()> next);

bool configServerWasConnectedViaHttpFlag = false;
bool wifiNetworkIsTrusted = false;

static void tryWiFiConnect();
static uint16_t countFilesInRoot();
static String ensureSdIsAvailable();
static void moveToUploaded(const String &fileName);

String getIp() {
  if (WiFiClass::status() != WL_CONNECTED) {
    return WiFi.softAPIP().toString();
  } else {
    return WiFi.localIP().toString();
  }
}

void updateDisplay(DisplayDevice * const display, String action = "") {
  if (action.isEmpty()) {
    display->showTextOnGrid(0, 0, "Ver.:");
    display->showTextOnGrid(1, 0, OBSVersion);

    if (WiFiClass::status() == WL_CONNECTED) {
      display->showTextOnGrid(0, 1, "SSID:");
      display->showTextOnGrid(1, 1, WiFi.SSID());
      display->showTextOnGrid(0, 2, "IP:");
      display->showTextOnGrid(1, 2, WiFi.localIP().toString());
    } else if (WiFiGenericClass::getMode() == WIFI_MODE_AP || WiFiGenericClass::getMode() == WIFI_MODE_APSTA) {
      // OK??
      display->showTextOnGrid(0, 1, "AP: " + WiFi.softAPSSID());
      display->showTextOnGrid(0, 2, "IP:");
      display->showTextOnGrid(1, 2, WiFi.softAPIP().toString());
      display->showTextOnGrid(0, 3, "Pass:");
      display->showTextOnGrid(1, 3, "12345678");
    } else {
      display->showTextOnGrid(0, 1, "wifi issue");
      display->showTextOnGrid(0, 2, WiFi.softAPIP().toString());
      display->showTextOnGrid(0, 3, "AP: " + WiFi.softAPSSID());
      display->showTextOnGrid(0, 4, "PW: 12345678");
      log_w("Unexpected wifi mode %d ", WiFiGenericClass::getMode());
    }
  } else {
    display->showTextOnGrid(0, 0,
                               theObsConfig->getProperty<String>(ObsConfig::PROPERTY_OBS_NAME));
    display->showTextOnGrid(0, 1, "IP:");
    display->showTextOnGrid(1, 1, getIp());
    display->showTextOnGrid(1, 2, "");
    display->showTextOnGrid(0, 2, action);
    display->showTextOnGrid(0, 3, "");
    display->showTextOnGrid(1, 3, "");
  }
}


void registerPages(HTTPServer * httpServer) {
  httpServer->setDefaultNode(new ResourceNode("", HTTP_GET, handleNotFound));
  httpServer->registerNode(new ResourceNode("/", HTTP_GET, handleIndex));
  httpServer->registerNode(new ResourceNode("/about", HTTP_GET, handleAbout));
  httpServer->registerNode(new ResourceNode("/reboot", HTTP_GET, handleReboot));
  httpServer->registerNode(new ResourceNode("/cold", HTTP_GET, handleColdStartGPS));
  httpServer->registerNode(new ResourceNode("/settings/backup", HTTP_GET, handleBackup));
  httpServer->registerNode(new ResourceNode("/settings/backup.json", HTTP_GET, handleBackupDownload));
  httpServer->registerNode(new ResourceNode("/settings/restore", HTTP_POST, handleBackupRestore));
  httpServer->registerNode(new ResourceNode("/settings/wifi", HTTP_GET, handleWifi));
  httpServer->registerNode(new ResourceNode("/settings/wifi/action", HTTP_POST, handleWifiSave));
  httpServer->registerNode(new ResourceNode("/settings/wifi/delete", HTTP_GET, handleWifiDeleteAction));
  httpServer->registerNode(new ResourceNode("/settings/general", HTTP_GET, handleConfig));
  httpServer->registerNode(new ResourceNode("/settings/general/action", HTTP_POST, handleConfigSave));
  httpServer->registerNode(new ResourceNode("/updateFlash", HTTP_GET, handleFlashUpdate));
  httpServer->registerNode(new ResourceNode("/updateFlash", HTTP_POST, handleFlashFileUpdateAction));
  httpServer->registerNode(new ResourceNode("/updateFlashUrl", HTTP_POST, handleFlashUpdateUrlAction));
  httpServer->registerNode(new ResourceNode("/updatesd", HTTP_GET, handleFirmwareUpdateSd));
  httpServer->registerNode(new ResourceNode("/updatesd", HTTP_POST, handleFirmwareUpdateSdAction));
  httpServer->registerNode(new ResourceNode("/updateSdUrl", HTTP_POST, handleFirmwareUpdateSdUrlAction));
  httpServer->registerNode(new ResourceNode("/delete", HTTP_GET, handleDelete));
  httpServer->registerNode(new ResourceNode("/delete", HTTP_POST, handleDeleteAction));
  httpServer->registerNode(new ResourceNode("/privacy_action", HTTP_POST, handlePrivacyAction));
  httpServer->registerNode(new ResourceNode("/gps", HTTP_GET, handleGps));
  httpServer->registerNode(new ResourceNode("/upload", HTTP_GET, handleUpload));
  httpServer->registerNode(new ResourceNode("/settings/privacy", HTTP_GET, handlePrivacy));
  httpServer->registerNode(new ResourceNode("/privacy_delete", HTTP_GET, handlePrivacyDeleteAction));
  httpServer->registerNode(new ResourceNode("/sd", HTTP_GET, handleSd));
  httpServer->registerNode(new ResourceNode("/deleteFiles", HTTP_POST, handleDeleteFiles));
  httpServer->registerNode(new ResourceNode("/cert", HTTP_GET, handleDownloadCert));
  httpServer->registerNode(new ResourceNode("/settings/security", HTTP_GET, handleSettingSecurity));
  httpServer->registerNode(new ResourceNode("/settings/security", HTTP_POST, handleSettingSecurityAction));

  httpServer->addMiddleware(&accessFilter);
  httpServer->setDefaultHeader("Server", std::string("OBS/") + OBSVersion);
}

void beginPages() {
  registerPages(server);
  if (wifiNetworkIsTrusted) {
    registerPages(insecureServer);
  } else {
    insecureServer->registerNode(new ResourceNode("/cert", HTTP_GET, handleDownloadCert));
    insecureServer->registerNode(new ResourceNode("/http", HTTP_POST, handleHttpAction));
    insecureServer->setDefaultNode(new ResourceNode("", HTTP_GET, handleHttpsRedirect));
  }
  insecureServer->setDefaultHeader("Server", std::string("OBS/") + OBSVersion);
}

static int ticks;
static void progressTick() {
  obsDisplay->drawWaitBar(5, ticks++);
}

static void createHttpServer() {
  log_i("About to create http server.");
  if (!Https::existsCertificate()) {
    obsDisplay->clear();
    obsDisplay->showTextOnGrid(0, 2, "Creating ssl cert,");
    obsDisplay->showTextOnGrid(0, 3, "be patient.");
  }
  serverSslCert = Https::getCertificate(progressTick);
  server = new HTTPSServer(serverSslCert, 443, 2);
  insecureServer = new HTTPServer(80, 2);

  beginPages();

  log_i("Starting http(s) servers.");
  server->start();
  insecureServer->start();
}


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
  html = replaceHtml(html, "{title}",OBS_ID_SHORT + " - " + subTitle);
  html = replaceHtml(html, "{version}", OBSVersion);
  html = replaceHtml(html, "{subtitle}", subTitle);
  html = replaceHtml(html, "{action}", action);
  obsDisplay->clear();
  updateDisplay(obsDisplay, "Menu: " + subTitle);
  return html;
}

static void sendPlainText(HTTPResponse * res, const String& data) {
  res->setHeader("Content-Type", "text/plain");
  res->setHeader("Connection", "keep-alive");
  res->print(data);
}

static void sendHtml(HTTPResponse * res, const String& data) {
  res->setHeader("Content-Type", "text/html");
  res->print(data);
}

static void sendHtml(HTTPResponse * res, const char * data) {
  res->setHeader("Content-Type", "text/html");
  res->print(data);
}

static void sendRedirect(HTTPResponse * res, const String& location) {
  res->setHeader("Location", location.c_str());
  res->setStatusCode(302);
  res->finalize();
}

static void handleNotFound(HTTPRequest * req, HTTPResponse * res) {
  // Discard request body, if we received any
  // We do this, as this is the default node and may also server POST/PUT requests
  req->discardRequestBody();

  // Set the response status
  res->setStatusCode(404);
  res->setStatusText("Not Found");

  sendHtml(res, replaceDefault(header, "Not Found"));
  res->println("<h3>404 Not Found</h3><p>The requested resource was not found on this server.</p>");
  res->println("<input type=button onclick=\"window.location.href='/'\" class='btn' value='Home' />");
  res->print(footer);
}

bool CreateWifiSoftAP() {
  bool softAccOK;
  // disconnect(bool wifioff = true, bool eraseap = true) 
  // in the hopes of fixing an occasional issue when ap is not connectable any more after fw upgrade
  // https://forum.openbikesensor.org/t/verbindung-zum-obs-wlan-schlaegt-fehl-falsches-passwort/2353/9
  WiFi.disconnect(true, true); 
  log_i("Initialize SoftAP");
  String apName = OBS_ID;
  String APPassword = "12345678";

  // without this sometimes during firmware upgrades when upgrading esp-idf
  // the wifi SoftAP will not accept clients
  esp_wifi_restore();

  softAccOK  =  WiFi.softAP(apName.c_str(), APPassword.c_str(), 1, 0, 2);

  delay(2000); // Without delay I've seen the IP address blank
  /* Soft AP network parameters */
  IPAddress apIP(172, 20, 0, 1);
  IPAddress netMsk(255, 255, 255, 0);

  WiFi.softAPsetHostname("obs");
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAPsetHostname("obs");
  if (softAccOK) {
    dnsServer = new DNSServer();
    // with "*" we get a lot of requests from all sort of apps,
    // use obs.local here
    dnsServer->start(53, "obs.local", apIP);
    log_i("AP successful IP: %s", apIP.toString().c_str());
  } else {
    log_e("Soft AP Error. Name: %s Pass: %s", apName.c_str(), APPassword.c_str());
  }
  updateDisplay(obsDisplay);
  return softAccOK;
}

/* Actions to be taken when we get internet. */
static void wifiConnectedActions() {
  log_i("Connected to %s, IP: %s",
        WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  if (dnsServer) { // was used to announce AP ip
    dnsServer->start(53, "obs.local", WiFi.localIP());
  }
  updateDisplay(obsDisplay);
  if (MDNS.begin("obs")) {
    log_i("MDNS responder started");
  } else {
    log_e("Error setting up MDNS responder!");
  }
  if (WiFiClass::status() == WL_CONNECTED) {
    TimeUtils::setClockByNtpAndWait(WiFi.gatewayIP().toString().c_str());
  }
  if (SD.begin() && WiFiClass::status() == WL_CONNECTED 
       && (!gps.moduleIsAlive() || gps.is_neo6())) {
    AlpData::update(obsDisplay);
  }

  String ssid = WiFi.SSID();
  int configs = theObsConfig->getNumberOfWifiConfigs();
  for (int i = 0; i < configs; i++) {
    auto wifiConfig = theObsConfig->getWifiConfig(i);
    if (ssid == wifiConfig.ssid) {
      wifiNetworkIsTrusted = wifiConfig.trusted;
      break;
    }
  }
}

/* callback function called if wifi data is received via improv */
bool initWifi(const std::string & ssid, const std::string & password) {
  log_i("Received WiFi credentials for SSID '%s'", ssid.c_str());
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_WIFI_SSID, ssid);
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_WIFI_PASSWORD, password);
  obsDisplay->clear();
  obsDisplay->showTextOnGrid(0, 1, "SSID:");
  obsDisplay->showTextOnGrid(1, 1, ssid.c_str());
  obsDisplay->showTextOnGrid(0, 2, "Connecting (IMPROV)...");

  WiFi.disconnect();
  tryWiFiConnect();
  bool connected = WiFiClass::status() == WL_CONNECTED;
  if (connected) {
    theObsConfig->saveConfig();
    wifiConnectedActions();
  } else {
    CreateWifiSoftAP();
    obsDisplay->showTextOnGrid(0, 4, "Connect failed.");
  }
  return connected;
}

/* Callback for improv - status of device */
static ObsImprov::State improvCallbackGetWifiStatus() {
  ObsImprov::State result;
  if (WiFiClass::status() == WL_CONNECTED) {
    result = ObsImprov::State::PROVISIONED;
  } else  { // not sure for STATE_PROVISIONING
    result = ObsImprov::State::READY;
  }
  return result;
}

static std::string improvCallbackGetDeviceUrl() {
  std::string url = "";
  if (WiFiClass::status() == WL_CONNECTED) {
    theObsConfig->saveConfig();
    url += "http://";
    url += WiFi.localIP().toString().c_str();
    url += + "/";
  }
  log_d("Device URL: '%s'", url.c_str());
  return url;
}

static void createImprovServer() {
  obsImprov = new ObsImprov(initWifi,
                            improvCallbackGetWifiStatus,
                            improvCallbackGetDeviceUrl,
                            &Serial);
  obsImprov->setDeviceInfo("OpenBikeSensor", OBSVersion,
                           ESP.getChipModel(),
                           OBS_ID.c_str());
}

void startServer(ObsConfig *obsConfig) {
  theObsConfig = obsConfig;

  const uint64_t chipid_num = ESP.getEfuseMac();
  String esp_chipid = String((uint16_t)(chipid_num >> 32), HEX);
  esp_chipid += String((uint32_t)chipid_num, HEX);
  esp_chipid.toUpperCase();
  OBS_ID = "OpenBikeSensor-" + esp_chipid;
  OBS_ID_SHORT = "OBS-" + String((uint16_t)(ESP.getEfuseMac() >> 32), HEX);
  OBS_ID_SHORT.toUpperCase();

  obsDisplay->clear();
  obsDisplay->showTextOnGrid(0, 0, "Ver.:");
  obsDisplay->showTextOnGrid(1, 0, OBSVersion);

  obsDisplay->showTextOnGrid(0, 1, "SSID:");
  obsDisplay->showTextOnGrid(1, 1,
                             theObsConfig->getProperty<String>(ObsConfig::PROPERTY_WIFI_SSID));

  // We don't really need persistent wifi config from the ESP
  // - it will even potentially leave wifi credentials on ESPs.
  // additionally it can break softAP during ESP-Framework upgrades.
  // The main purpose seems to be to slightly speed up wifi 
  // config which is not so relevant for us.
  WiFi.persistent(false);

  tryWiFiConnect();

  if (WiFiClass::status() != WL_CONNECTED) {
    WiFi.mode(WIFI_MODE_NULL);
    WiFi.setHostname("obs");
    CreateWifiSoftAP();
    touchConfigServerHttp(); // side effect do not allow track upload via button
    MDNS.begin("obs");
  } else {
    wifiConnectedActions();
  }

  createHttpServer();
  createImprovServer();
}

static void setWifiMultiAps() {
  if (theObsConfig->getNumberOfWifiConfigs() == 0) {
    log_w("No wifi SID set - will not try to connect.");
    return;
  }
  for (int i = 0; i < theObsConfig->getNumberOfWifiConfigs(); i++) {
    WifiConfig wifiConfig = theObsConfig->getWifiConfig(i);
    log_i("Adding wifi SID %s", wifiConfig.ssid.c_str());
    wifiMulti.addAP(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());
  }
}

static void tryWiFiConnect() {
  if (WiFiClass::setHostname("obs")) {
    log_e("Failed to set hostname to 'obs'.");
  }
  if (theObsConfig->getNumberOfWifiConfigs() == 0) {
    log_w("No wifi SID set - will not try to connect.");
    return;
  }
  setWifiMultiAps();

  log_w("Connection to wifi.");
  if (wifiMulti.run(10000) == WL_CONNECTED) {
    log_i("Connected to wifi SID: %s strength %ddBm ip: %s.",
          WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str());
  } else {
    log_w("Failed to connect to wifi.");
  }
}

static void handleIndex(HTTPRequest *, HTTPResponse * res) {
// ###############################################################
// ### Index ###
// ###############################################################
  String html = createPage(navigationIndex);
  html = replaceDefault(html, "Navigation");
  sendHtml(res, html);
}

static String keyValue(const String& key, const String& value, const String& suffix = "") {
  return "<b>" + ObsUtils::encodeForXmlText(key) + ":</b> " + value + suffix + "<br />";
}

static String keyValue(const String& key, const uint32_t value, const String& suffix = "") {
  return keyValue(key, String(value), suffix);
}

static String keyValue(const String& key, const int32_t value, const String& suffix = "") {
  return keyValue(key, String(value), suffix);
}

static String keyValue(const String& key, const uint64_t value, const String& suffix = "") {
  // is long this sufficient?
  return keyValue(key, String((unsigned long) value), suffix);
}

static String appVersion(const esp_partition_t *partition) {
  esp_app_desc_t app_desc;
  esp_err_t ret = ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ota_get_partition_description(partition, &app_desc));
  if (ret == ESP_OK) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
             "App '%s', Version: '%s', IDF-Version: '%s', sha-256: %s, date: '%s', time: '%s'",
             app_desc.project_name, app_desc.version, app_desc.idf_ver,
             ObsUtils::sha256ToString(app_desc.app_elf_sha256).substring(0, 24).c_str(),
             app_desc.date, app_desc.time);
    return String(buffer);
  } else {
    return String("No app (") + String(esp_err_to_name(ret)) + String(")");
  }
}

static String wifiSatusAsString() {
  switch (WiFiClass::status()) {
    case WL_NO_SHIELD:
      return "No WiFi shield";
    case WL_IDLE_STATUS:
      return "Idle";
    case WL_NO_SSID_AVAIL:
      return "No SSID available";
    case WL_SCAN_COMPLETED:
      return "Scan completed";
    case WL_CONNECTED:
      return "Connected";
    case WL_CONNECT_FAILED:
      return "Connection failed";
    case WL_CONNECTION_LOST:
      return "Connection lost";
    case WL_DISCONNECTED:
      return "Disconnected";
    default:
      return "Unknown";
  }
}

static String wifiModeAsString() {
switch (WiFiClass::getMode()) {
    case WIFI_OFF:
      return "Off";
    case WIFI_STA:
      return "Station";
    case WIFI_AP:
      return "Access Point";
    case WIFI_AP_STA:
      return "Access Point and Station";
    default:
      return "Unknown";
  }
}

static void handleAbout(HTTPRequest *req, HTTPResponse * res) {
  res->setHeader("Content-Type", "text/html");
  res->print(replaceDefault(header, "About"));
  String page;
  gps.pollStatistics(); // takes ~100ms!

  res->print("<h3>ESP32</h3>");
  res->print(keyValue("Chip Model", ESP.getChipModel()));
  res->print(keyValue("Chip Revision", ESP.getChipRevision()));
  res->print(keyValue("Heap size", ObsUtils::toScaledByteString(ESP.getHeapSize())));
  res->print(keyValue("Free heap", ObsUtils::toScaledByteString(ESP.getFreeHeap())));
  res->print(keyValue("Min. free heap", ObsUtils::toScaledByteString(ESP.getMinFreeHeap())));
  String chipId = String((uint32_t) ESP.getEfuseMac(), HEX) + String((uint32_t) (ESP.getEfuseMac() >> 32), HEX);
  chipId.toUpperCase();
  res->print(keyValue("Chip id", chipId));
  res->print(keyValue("FlashApp Version", Firmware::getFlashAppVersion()));
  res->print(keyValue("IDF Version", esp_get_idf_version()));
  res->print(keyValue("Arduino Version", ESP_ARDUINO_VERSION_MAJOR + String(".") +
                                          ESP_ARDUINO_VERSION_MINOR + String(".") +
                                          ESP_ARDUINO_VERSION_PATCH));

  res->print(keyValue("App size", ObsUtils::toScaledByteString(ESP.getSketchSize())));
  res->print(keyValue("App space", ObsUtils::toScaledByteString(ESP.getFreeSketchSpace())));
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
  res->print(page);
  page.clear();

  page += keyValue("Cores", ESP.getChipCores());
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
    files += TimeUtils::dateTimeToString(file.getLastWrite());
    file.close();
    file = dir.openNextFile();
  }
  dir.close();
  page += keyValue("SPIFFS files", files);
  page += keyValue("System date time", TimeUtils::dateTimeToString(file.getLastWrite()));
  page += keyValue("System millis", String(millis()));

  if (voltageMeter) {
    page += keyValue("Battery voltage", String(voltageMeter->read(), 2), "V");
  }
  res->print(page);
  page.clear();

  page += "<h3>App Partitions</h3>";
  const esp_partition_t *running = esp_ota_get_running_partition();
  page += keyValue("Current Partition", running->label);

  const esp_partition_t *ota0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0,
                                                         nullptr);
  page += keyValue("OTA-0 Partition", ota0->label);
  page += keyValue("OTA-0 Partition Size", ObsUtils::toScaledByteString(ota0->size));
  page += keyValue("OTA-0 App", appVersion(ota0));
  const esp_partition_t *ota1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1,
                                                         nullptr);
  page += keyValue("OTA-1 Partition", ota1->label);
  page += keyValue("OTA-1 Partition Size", ObsUtils::toScaledByteString(ota1->size));
  page += keyValue("OTA-1 App", appVersion(ota1));
  res->print(page);
  page.clear();

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
  page += keyValue("Left Sensor signal errors", sensorManager->getNoSignalReadings(LEFT_SENSOR_ID));
  page += keyValue("Right Sensor raw", sensorManager->getRawMedianDistance(RIGHT_SENSOR_ID), "cm");
  page += keyValue("Right Sensor max duration", sensorManager->getMaxDurationUs(RIGHT_SENSOR_ID), "&#xB5;s");
  page += keyValue("Right Sensor min duration", sensorManager->getMinDurationUs(RIGHT_SENSOR_ID), "&#xB5;s");
  page += keyValue("Right Sensor last start delay", sensorManager->getLastDelayTillStartUs(RIGHT_SENSOR_ID), "&#xB5;s");
  page += keyValue("Right Sensor signal errors", sensorManager->getNoSignalReadings(RIGHT_SENSOR_ID));

  res->print(page);
  page.clear();
  page += "<h3>GPS</h3>";
  page += keyValue("GPS valid checksum", gps.getValidMessageCount());
  page += keyValue("GPS failed checksum", gps.getMessagesWithFailedCrcCount());
  page += keyValue("GPS unexpected chars", gps.getUnexpectedCharReceivedCount());
  page += keyValue("GPS hdop", gps.getCurrentGpsRecord().getHdopString());
  page += keyValue("GPS fix", String(gps.getCurrentGpsRecord().getFixStatus(), 16));
  page += keyValue("GPS fix flags", String(gps.getCurrentGpsRecord().getFixStatusFlags(), 16));
  page += keyValue("GPS satellites", gps.getValidSatellites());
  page += keyValue("GPS uptime", gps.getUptime(), "ms");
  page += keyValue("GPS noise level", gps.getLastNoiseLevel());
  page += keyValue("GPS Antenna Gain", gps.getLastAntennaGain());
  page += keyValue("GPS Jamming Level", gps.getLastJamInd());
  page += keyValue("GPS baud rate", gps.getBaudRate());
  page += keyValue("GPS ALP bytes", gps.getNumberOfAlpBytesSent());
  page += keyValue("GPS messages", gps.getMessagesHtml());

  page += "<h3>Display / Button</h3>";

  page += keyValue("Button State", button.read());
  page += keyValue("Display i2c last error", Wire.getWriteError());
  page += keyValue("Display i2c speed", Wire.getClock() / 1000, "KHz");
  page += keyValue("Display i2c timeout", Wire.getTimeOut(), "ms");

  page += "<h3>WiFi</h3>";
  page += keyValue("Mode", wifiModeAsString());
  if (WiFiGenericClass::getMode() == WIFI_MODE_AP || WiFiGenericClass::getMode() == WIFI_MODE_APSTA) {
    page += keyValue("AP SSID", WiFi.softAPSSID());
    page += keyValue("AP Hostname", WiFi.softAPgetHostname());
    page += keyValue("AP IP", WiFi.softAPIP().toString());
    page += keyValue("AP MAC", WiFi.softAPmacAddress());
    page += keyValue("AP BSSID", WiFi.softAPmacAddress());
    page += keyValue("AP Clients", WiFi.softAPgetStationNum());
  } else {
    page += keyValue("SSID", WiFi.SSID());
    page += keyValue("Hostname", WiFiClass::getHostname());
    page += keyValue("Local IP", WiFi.localIP().toString());
    page += keyValue("Gateway IP", WiFi.gatewayIP().toString());
    page += keyValue("Subnet Mask", WiFi.subnetMask().toString());
    page += keyValue("MAC", WiFi.macAddress());
    page += keyValue("RSSI", WiFi.RSSI(), "dBm");
    page += keyValue("DNS", WiFi.dnsIP().toString());
    page += keyValue("Status", wifiSatusAsString());
    page += keyValue("AutoConnect", WiFi.getAutoConnect() == 0 ? "Disabled" : "Enabled");
    page += keyValue("AutoReconnect", WiFi.getAutoReconnect() == 0 ? "Disabled" : "Enabled");
    page += keyValue("BSSID", WiFi.BSSIDstr());
  }
  page += keyValue("Channel", WiFi.channel());

  page += "<h3>HTTP</h3>";
  page += keyValue("User Agent", req->getHeader("user-agent").c_str());
  page += keyValue("Request Host", req->getHeader("host").c_str());
  page += keyValue("Client IP", req->getClientIP().toString());
  page += keyValue("Language", req->getHeader("Accept-Language").c_str());
#ifdef HTTPS_LOGLEVEL
  page += keyValue("Https log level", String(HTTPS_LOGLEVEL));
#endif

  res->print(page);
  res->print(footer);
}

static void handleReboot(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(rebootIndex);
  html = replaceDefault(html, "Reboot");
  sendHtml(res, html);
  res->finalize();
  delay(1000);
  ESP.restart();
}

static void handleColdStartGPS(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(gpsColdIndex);
  html = replaceDefault(html, "Navigation");
  sendHtml(res, html);
  gps.coldStartGps();
  res->finalize();
}


static void handleBackup(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(backupIndex, xhrUpload);
  html = replaceDefault(html, "Backup & Restore");
  html = replaceHtml(html, "{method}", "/settings/restore");
  html = replaceHtml(html, "{accept}", ".json");
  sendHtml(res, html);
}

static void handleBackupDownload(HTTPRequest *, HTTPResponse * res) {
  const String fileName
    = String(theObsConfig->getProperty<String>(ObsConfig::PROPERTY_OBS_NAME)) + "-" + OBSVersion;
  res->setHeader("Content-Disposition",
                 (String("attachment; filename=\"") + fileName + ".json\"").c_str());
  res->setHeader("Content-Type", "application/json");
  log_d("Sending config for backup %s:", fileName.c_str());
  theObsConfig->printConfig();
  String data = theObsConfig->asJsonString();
  res->print(data);
}

static void handleBackupRestore(HTTPRequest * req, HTTPResponse * res) {
  HTTPMultipartBodyParser parser(req);
  sensorManager->detachInterrupts();

  while(parser.nextField()) {

    if (parser.getFieldName() != "upload") {
      log_i("Skipping form data %s type %s filename %s", parser.getFieldName().c_str(),
            parser.getFieldMimeType().c_str(), parser.getFieldFilename().c_str());
      continue;
    }
    log_i("Got form data %s type %s filename %s", parser.getFieldName().c_str(),
          parser.getFieldMimeType().c_str(), parser.getFieldFilename().c_str());

    String json_buffer = "";
    while (!parser.endOfField()) {
      byte buffer[256];
      size_t len = parser.read(buffer, 256);
      for (int i = 0; i < len; i++) {
        json_buffer.concat((char) buffer[i]);
      }
    }
    if (theObsConfig->parseJson(json_buffer)) {
      theObsConfig->fill(config); // OK here??
      theObsConfig->printConfig();
      theObsConfig->saveConfig();
      res->setHeader("Content-Type", "text/plain");
      res->setStatusCode(200);
      res->setStatusText("Restore successful!");
      res->print("OK");
    } else {
      res->setHeader("Content-Type", "text/plain");
      res->setStatusCode(500);
      res->setStatusText("Invalid data!");
      res->print("ERROR");
    }
  }
  sensorManager->attachInterrupts();
}


static void handleWifi(HTTPRequest *, HTTPResponse * res) {
  String page;
  page = "<br />Please note that the WiFi password is stored as plain Text on the OBS"
      " and can be read by anyone with access to the device. ";
  for (int idx = 0; idx < theObsConfig->getNumberOfWifiConfigs(); ++idx) {
    auto wifi = theObsConfig->getWifiConfig(idx);
    page += "<h3>WiFi #" + String(idx + 1) + "</h3>";
    const String &index = String(idx);
    page += "SSID<input name='ssid" + index + "' value='"
            + ObsUtils::encodeForXmlAttribute(wifi.ssid) + "' />";
    page += "Password<input name='pass" + index + "' type='password' value='******'"
            " onclick='value=\"\"' />";
    page += "Private<input name='private" + index + "' type='checkbox' "
            + String(wifi.trusted ? "checked" : "") + " />";
    page += "<a class='deleteWifi' href='/settings/wifi/delete?erase=" + index + "'>&#x2716;</a>";
  }

  page += "<h3>New WiFi</h3>";
  page += "SSID<input name='newSSID' placeholder='ssid' />";
  page += "Password<input type='password' name='newPassword' placeholder='secret' />";
  page += "Private<input type='checkbox' name='newPrivate' />";
  page += "<small>Select private only for trusted, none public networks, others on the same net will be able to "
          "control your device. Allows direct, unencrypted access to the device.</small>";

  String html = createPage(page, wifiSettingsIndexPostfix);
  html = replaceDefault(html, "WiFi Settings", "/settings/wifi/action");

  log_i("Page %s", page.c_str());

  sendHtml(res, html);
}

static void handleWifiDeleteAction(HTTPRequest *req, HTTPResponse *res) {
  String erase = getParameter(req, "erase");
  if (erase != "") {
    theObsConfig->removeWifiConfig(atoi(erase.c_str()));
    theObsConfig->saveConfig();
  }
  sendRedirect(res, "/settings/wifi");
}

static bool updateWifi(const std::vector<std::pair<String, String>> &params) {
  bool modified = false;
  for (int pos = 0; pos < theObsConfig->getNumberOfWifiConfigs(); ++pos) {
    String idx = String(pos);
    String ssid = getParameter(params, "ssid" + idx);
    String password = getParameter(params, "pass" + idx);
    bool trusted = getParameter(params, "private" + idx) == "on";

    auto oldWifi = theObsConfig->getWifiConfig(pos);
    if (password == "******") {
      password = oldWifi.password;
    }
    if ((ssid != oldWifi.ssid) || (password != oldWifi.password) || (trusted != oldWifi.trusted)) {
      log_i("Update wifi %d!", pos);
      WifiConfig newWifiConfig;
      newWifiConfig.password = password;
      newWifiConfig.ssid = ssid;
      newWifiConfig.trusted = trusted;
      theObsConfig->setWifiConfig(pos, newWifiConfig);
      modified = true;
    }
  }
  return modified;
}


static bool addWifi(const std::vector<std::pair<String, String>> &params) {
  bool modified = false;

  String ssid = getParameter(params, "newSSID");
  String password = getParameter(params, "newPassword");
  bool trusted = getParameter(params, "newPrivate") == "on";

  if ((ssid != "") && (password != "")) {
    log_i("New wifi!");
    WifiConfig newWifiConfig;
    newWifiConfig.password = password;
    newWifiConfig.ssid = ssid;
    newWifiConfig.trusted = trusted;
    theObsConfig->addWifiConfig(newWifiConfig);
    modified = true;
  }
  return modified;
}

static void handleWifiSave(HTTPRequest *req, HTTPResponse *res) {
  const auto params = extractParameters(req);
  bool modified = false;

  modified = updateWifi(params);
  if (addWifi(params)) {
    modified = true;
  }
  if (modified) {
    theObsConfig->saveConfig();
    sendRedirect(res, "/settings/wifi");
  } else {
    sendRedirect(res, "/");
  }
}

static void handleConfigSave(HTTPRequest * req, HTTPResponse * res) {
  const auto params = extractParameters(req);

  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplaySatellites,
                                   getParameter(params, "displayGPS") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayVelocity,
                                   getParameter(params, "displayVELO") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplaySimple,
                                   getParameter(params, "displaySimple") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayLeft,
                                   getParameter(params, "displayLeft") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayRight,
                                   getParameter(params, "displayRight") == "on");
  // NOT a display setting!?
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplaySwapSensors,
                                   getParameter(params, "displaySwapSensors") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayInvert,
                                   getParameter(params, "displayInvert") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayFlip,
                                   getParameter(params, "displayFlip") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayNumConfirmed,
                                   getParameter(params, "displayNumConfirmed") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DISPLAY_CONFIG, DisplayDistanceDetail,
                                   getParameter(params, "displayDistanceDetail") == "on");
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_CONFIRMATION_TIME_SECONDS,
                            atoi(getParameter(params, "confirmationTimeWindow").c_str()));
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_BLUETOOTH,
                            (bool) (getParameter(params, "bluetooth") == "on"));
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_PORTAL_TOKEN,
                            getParameter(params, "obsUserID"));
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_PORTAL_URL,
                            getParameter(params, "hostname"));

  std::vector<int> offsets;
  offsets.push_back(atoi(getParameter(params, "offsetS2").c_str()));
  offsets.push_back(atoi(getParameter(params, "offsetS1").c_str()));
  theObsConfig->setOffsets(0, offsets);

  // TODO: cleanup
  const String privacyOptions = getParameter(params, "privacyOptions");
  const String overridePrivacy = getParameter(params, "overridePrivacy");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_PRIVACY_CONFIG, AbsolutePrivacy,
                                   privacyOptions == "absolutePrivacy");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_PRIVACY_CONFIG, NoPosition,
                                   privacyOptions == "noPosition");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_PRIVACY_CONFIG, NoPrivacy,
                                   privacyOptions == "noPrivacy");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_PRIVACY_CONFIG, OverridePrivacy,
                                   overridePrivacy == "on");

  theObsConfig->saveConfig();

  sendRedirect(res, "/settings/general");
}


static void handleConfig(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(configIndex);
  html = replaceDefault(html, "General", "/settings/general/action");

// Form data
  const std::vector<int> offsets
    = theObsConfig->getIntegersProperty(ObsConfig::PROPERTY_OFFSET);
  html = replaceHtml(html, "{offset1}", String(offsets[LEFT_SENSOR_ID]));
  html = replaceHtml(html, "{offset2}", String(offsets[RIGHT_SENSOR_ID]));
  html = replaceHtml(html, "{hostname}",
               theObsConfig->getProperty<String>(ObsConfig::PROPERTY_PORTAL_URL));
  html = replaceHtml(html, "{userId}",
               theObsConfig->getProperty<String>(ObsConfig::PROPERTY_PORTAL_TOKEN));
  html = replaceHtml(html, "{confirmationTimeWindow}",
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

  html = replaceHtml(html, "{displaySimple}", displaySimple ? "checked" : "");
  html = replaceHtml(html, "{displayGPS}", displayGPS ? "checked" : "");
  html = replaceHtml(html, "{displayVELO}", displayVelo ? "checked" : "");
  html = replaceHtml(html, "{displayLeft}", displayLeft ? "checked" : "");
  html = replaceHtml(html, "{displayRight}", displayRight ? "checked" : "");
  html = replaceHtml(html, "{displaySwapSensors}", displaySwapSensors ? "checked" : "");
  html = replaceHtml(html, "{displayInvert}", displayInvert ? "checked" : "");
  html = replaceHtml(html, "{displayFlip}", displayFlip ? "checked" : "");
  html = replaceHtml(html, "{displayNumConfirmed}", displayNumConfirmed ? "checked" : "");
  html = replaceHtml(html, "{displayDistanceDetail}", displayDistanceDetail ? "checked" : "");

  html = replaceHtml(html, "{bluetooth}",
               theObsConfig->getProperty<bool>(ObsConfig::PROPERTY_BLUETOOTH) ? "checked" : "");

  const uint privacyConfig = (uint) theObsConfig->getProperty<int>(
    ObsConfig::PROPERTY_PRIVACY_CONFIG);
  const bool absolutePrivacy = privacyConfig & AbsolutePrivacy;
  const bool noPosition = privacyConfig & NoPosition;
  const bool noPrivacy = privacyConfig & NoPrivacy;
  const bool overridePrivacy = privacyConfig & OverridePrivacy;

  html = replaceHtml(html, "{absolutePrivacy}", absolutePrivacy ? "checked" : "");
  html = replaceHtml(html, "{noPosition}", noPosition ? "checked" : "");
  html = replaceHtml(html, "{noPrivacy}", noPrivacy ? "checked" : "");
  html = replaceHtml(html, "{overridePrivacy}", overridePrivacy ? "checked" : "");

  sendHtml(res, html);
}

/* Upload tracks found on SD card to the portal server->
 * This method also takes care to give appropriate feedback
 * to the user about the progress. If httpRequest is true
 * a html response page is created. Also the progress can be
 * seen on the display if connected.
 */
void uploadTracks(HTTPResponse *res) {
  const String &portalToken
    = theObsConfig->getProperty<String>(ObsConfig::PROPERTY_PORTAL_TOKEN);
  Uploader uploader(
    theObsConfig->getProperty<String>(ObsConfig::PROPERTY_PORTAL_URL), portalToken);

  configServerWasConnectedViaHttpFlag = true;
  SDFileSystem.mkdir("/uploaded");

  String sdErrorMessage = ensureSdIsAvailable();
  if (sdErrorMessage != "") {
    if (res) {
      res->setStatusCode(500);
      res->print(sdErrorMessage);
    }
    obsDisplay->showTextOnGrid(0, 3, "Error:");
    obsDisplay->showTextOnGrid(0, 4, sdErrorMessage);
    return;
  }

  String html = replaceDefault(header, "Upload Tracks");
  html += "<h3>Uploading tracks...</h3>";
  html += "<div>";
  if (res) {
    sendHtml(res, html);
  }
  html.clear();

  if (portalToken.isEmpty()) {
    if (res) {
      html += "</div><h3>API Key not set</h3/>";
      html += "See <a href='https://www.openbikesensor.org/benutzer-anleitung/konfiguration.html'>";
      html += "configuration</a>.</div>";
      html += "<input type=button onclick=\"window.location.href='/'\" class='btn' value='OK' />";
      html += footer;
      res->print(html);
    }
    obsDisplay->showTextOnGrid(0, 3, "Error:");
    obsDisplay->showTextOnGrid(0, 4, "API Key not set");
    return;
  }

  const uint16_t numberOfFiles = countFilesInRoot();

  uint16_t currentFileIndex = 0;
  uint16_t okCount = 0;
  uint16_t failedCount = 0;
  File root = SDFileSystem.open("/");
  File file = root.openNextFile();
  while (file) {
    const String fileName(String("/") + file.name());
    log_d("Upload file: %s", fileName.c_str());
    if (!file.isDirectory()
        && fileName.endsWith(CSVFileWriter::EXTENSION)) {
      const String friendlyFileName = ObsUtils::stripCsvFileName(fileName);
      currentFileIndex++;

      obsDisplay->showTextOnGrid(0, 4, friendlyFileName);
      obsDisplay->drawProgressBar(3, currentFileIndex, numberOfFiles);
      if (res) {
        res->print(friendlyFileName);
      }
      const boolean uploaded = uploader.upload(fileName);
      file.close();
      if (uploaded) {
        moveToUploaded(fileName);
        html += "<a href='" + ObsUtils::encodeForXmlAttribute(uploader.getLastLocation())
                + "' title='" + ObsUtils::encodeForXmlAttribute(uploader.getLastStatusMessage())
                + "' target='_blank'>" + HTML_ENTITY_OK_MARK  + "</a>";
        okCount++;
      } else {
        html += "<a href='#' title='" + ObsUtils::encodeForXmlAttribute(uploader.getLastStatusMessage())
                + "'>" + HTML_ENTITY_FAILED_CROSS + "</a><p><tt>" + ObsUtils::encodeForXmlAttribute(uploader.getLastStatusMessage()) + "</tt></p>";
        failedCount++;
      }
      if (res) {
        html += "<br />\n";
        res->print(html);
      }
      html.clear();
      obsDisplay->clearProgressBar(5);
    } else {
      file.close();
    }
    file = root.openNextFile();
  }
  root.close();

  obsDisplay->clearProgressBar(3);
  obsDisplay->showTextOnGrid(0, 4, "");
  obsDisplay->showTextOnGrid(1, 3, "Upload done.");
  obsDisplay->showTextOnGrid(0, 5, "OK:");
  obsDisplay->showTextOnGrid(1, 5, String(okCount));
  obsDisplay->showTextOnGrid(2, 5, "Failed:");
  obsDisplay->showTextOnGrid(3, 5, String(failedCount));
  if (res) {
    html += "</div><h3>...all files done</h3/>";
    html += keyValue("OK", okCount);
    html += keyValue("Failed", failedCount);
    html += "<input type=button onclick=\"window.location.href='/'\" class='btn' value='OK' />";
    html += footer;
    res->print(html);
  }
}

static void moveToUploaded(const String &fileName) {
  int i = 0;
  while (!SDFileSystem.rename(
    fileName, String("/uploaded") + fileName + (i == 0 ? "" : String(i)))) {
    i++;
    if (i > 100) {
      break;
    }
  }
}

static void handleUpload(HTTPRequest *, HTTPResponse * res) {
  uploadTracks(res);
}

static String getGpsStatusString() {
  String gpsString;
  auto gpsData = gps.getCurrentGpsRecord();
  if (!gps.moduleIsAlive()) {
    gpsString = "OFF?";
  } else if (gpsData.hasValidFix()) {
    gpsString = "Latitude:&nbsp;" + gpsData.getLatString() +
                +" Longitude:&nbsp;" + gpsData.getLongString() +
                +" Altitude:&nbsp;" + gpsData.getAltitudeMetersString()
                + "m HDOP:&nbsp;"
                + gpsData.getHdopString();
  } else {
    gpsString = "no&nbsp;fix " + String(gps.getValidSatellites()) + "&nbsp;satellites "
       " SN:&nbsp;" + gps.getLastNoiseLevel();
  }
  return gpsString;
}

static void handlePrivacy(HTTPRequest *, HTTPResponse *res) {
  String privacyPage;
  for (int idx = 0; idx < theObsConfig->getNumberOfPrivacyAreas(0); ++idx) {
    auto pa = theObsConfig->getPrivacyArea(0, idx);
    privacyPage += "<h3>Privacy Area #" + String(idx + 1) + "</h3>";
    const String &index = String(idx);
    privacyPage += "Latitude <input name=latitude" + index
                   + " placeholder='latitude' value='" + String(pa.latitude, 6) + "' />";
    privacyPage += "<input type='hidden' name='oldLatitude" + index + "' value='" + String(pa.latitude, 6) + "' />";

    privacyPage += "Longitude <input name='longitude" + index
                   + "' placeholder='longitude' value='" + String(pa.longitude, 6) + "' />";
    privacyPage += "<input type='hidden' name='oldLongitude" + index + "' value='" + String(pa.longitude, 6) + "' />";

    privacyPage += "Radius (m) <input name='radius" + index
                   + "' placeholder='radius' value='" + String(pa.radius, 0) + "' />";
    privacyPage += "<input type='hidden' name='oldRadius" + index + "' value='" + String(pa.radius, 0) + "' />";
    privacyPage += "<a class='deletePrivacyArea' href='/privacy_delete?erase=" + index + "'>&#x2716;</a>";
  }

  privacyPage += "<h3>New Privacy Area</h3>";
  privacyPage += "Latitude<input name='newlatitude' placeholder='latitude' />";
  privacyPage += "Longitude<input name='newlongitude' placeholder='longitude' />";
  privacyPage += "Radius (m)<input name='newradius' placeholder='radius' value='500' />";

  String html = createPage(privacyPage, privacyIndexPostfix);
  html = replaceDefault(html, "Privacy Zones", "/privacy_action");
  html.replace("{gps}", getGpsStatusString());
  sendHtml(res, html);
}

static void handlePrivacyDeleteAction(HTTPRequest *req, HTTPResponse *res) {
  String erase = getParameter(req, "erase");
  if (erase != "") {
    theObsConfig->removePrivacyArea(0, atoi(erase.c_str()));
    theObsConfig->saveConfig();
  }
  sendRedirect(res, "/settings/privacy");
}

static bool makeCurrentLocationPrivate() {
  bool modified = false;
  auto gpsRecord = gps.getCurrentGpsRecord();
  if (gpsRecord.hasValidFix()) {
    theObsConfig->addPrivacyArea(
      0,
      Gps::newPrivacyArea(
          gpsRecord.getLatitude(), gpsRecord.getLongitude(), 500));
    modified = true;
  }
  return modified;
}

static bool updatePrivacyAreas(const std::vector<std::pair<String, String>> &params) {
  bool modified = false;
  for (int pos = 0; pos < theObsConfig->getNumberOfPrivacyAreas(0); ++pos) {
    String idx = String(pos);
    String latitude = getParameter(params, "latitude" + idx);
    String oldLatitude = getParameter(params, "oldLatitude" + idx);
    String longitude = getParameter(params, "longitude" + idx);
    String oldLongitude = getParameter(params, "oldLongitude" + idx);
    String radius = getParameter(params, "radius" + idx);
    String oldRadius = getParameter(params, "oldRadius" + idx);

    if ((latitude != "") && (longitude != "") && (radius != "")
      && ((latitude != oldLatitude) || (longitude != oldLongitude) || (radius != oldRadius))) {
      latitude.replace(",", ".");
      longitude.replace(",", ".");
      log_i("Update privacyArea %d!", pos);
      theObsConfig->setPrivacyArea(
        0, pos,
        Gps::newPrivacyArea(atof(latitude.c_str()), atof(longitude.c_str()),
                            atoi(radius.c_str())));
      modified = true;
    }
  }
  return modified;
}

static bool addPrivacyArea(const std::vector<std::pair<String, String>> &params) {
  bool modified = false;
  String latitude = getParameter(params, "newlatitude");
  latitude.replace(",", ".");
  String longitude = getParameter(params, "newlongitude");
  longitude.replace(",", ".");
  String radius = getParameter(params, "newradius");

  if ((latitude != "") && (longitude != "") && (radius != "")) {
    log_i("New valid privacyArea!");
    theObsConfig->addPrivacyArea(
      0,
      Gps::newPrivacyArea(atof(latitude.c_str()), atof(longitude.c_str()),
                         atoi(radius.c_str())));
    modified = true;
  }
  return modified;
}

static void handlePrivacyAction(HTTPRequest *req, HTTPResponse *res) {
  const auto params = extractParameters(req);
  bool modified = false;

  if (!getParameter(params, "addCurrent").isEmpty()) {
    modified = makeCurrentLocationPrivate();
  } else {
    modified = updatePrivacyAreas(params);
    if (addPrivacyArea(params)) {
      modified = true;
    }
  }
  if (modified) {
    theObsConfig->saveConfig();
    sendRedirect(res, "/settings/privacy");
  } else {
    sendRedirect(res, "/");
  }
}

static void handleGps(HTTPRequest * req, HTTPResponse * res) {
  sendPlainText(res, getGpsStatusString());
}

static void handleFlashUpdate(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(updateSdIndex, xhrUpload);
  html = replaceDefault(html, "Update Flash App", "/updateFlashUrl");
  String flashAppVersion = Firmware::getFlashAppVersion();
  if (!flashAppVersion.isEmpty()) {
    html = replaceHtml(html, "{description}",
                       "Installed Flash App version is " + flashAppVersion + ".");
  } else {
    html = replacePlain(html, "{description}",
                        "Flash App not installed.");
  }
  html = replaceHtml(html, "{method}", "/updateFlash");
  html = replaceHtml(html, "{accept}", ".bin");
  html = replaceHtml(html, "{releaseApiUrl}",
                     "https://api.github.com/repos/openbikesensor/OpenBikeSensorFlash/releases");
  sendHtml(res, html);
}

void updateProgress(size_t pos, size_t all) {
  obsDisplay->drawProgressBar(4, pos, all);
}

static void handleFlashUpdateUrlAction(HTTPRequest * req, HTTPResponse * res) {
  const auto params = extractParameters(req);
  const auto url = getParameter(params, "downloadUrl");
  const auto unsafe = getParameter(params,"unsafe");

  log_i("Flash App Url is '%s'", url.c_str());

  Firmware f(String("OBS/") + String(OBSVersion));
  sensorManager->detachInterrupts();
  if (f.downloadToFlash(url, updateProgress, unsafe[0] == '1')) {
    obsDisplay->showTextOnGrid(0, 3, "Success!");
    sendRedirect(res, "/updatesd");
  } else {
    obsDisplay->showTextOnGrid(0, 3, "Error");
    obsDisplay->showTextOnGrid(0, 4, f.getLastMessage());
    sendRedirect(res, "/about");
  }
  sensorManager->attachInterrupts();
}

static void handleFlashFileUpdateAction(HTTPRequest *req, HTTPResponse *res) {
  // TODO: Add some assertions, cleanup with handleFlashUpdateUrlAction
  HTTPMultipartBodyParser parser(req);
  sensorManager->detachInterrupts();
  Update.begin();
  Update.onProgress([](size_t pos, size_t all) {
    obsDisplay->drawProgressBar(4, pos, all);
  });
  while (parser.nextField()) {
    if (parser.getFieldName() != "upload") {
      log_i("Skipping form data %s type %s filename %s", parser.getFieldName().c_str(),
            parser.getFieldMimeType().c_str(), parser.getFieldFilename().c_str());
      continue;
    }
    log_i("Got form data %s type %s filename %s", parser.getFieldName().c_str(),
          parser.getFieldMimeType().c_str(), parser.getFieldFilename().c_str());

    while (!parser.endOfField()) {
      byte buffer[256];
      size_t len = parser.read(buffer, 256);
      if (Update.write(buffer, len) != len) {
        Update.printError(Serial);
      }
    }
    log_i("Done reading");
    if (Update.end(true)) {
      sendHtml(res, "Flash App update successful!");
      obsDisplay->showTextOnGrid(0, 3, "Success...");
      const esp_partition_t *running = esp_ota_get_running_partition();
      esp_ota_set_boot_partition(running);
    } else {
      String errorMsg = Update.errorString();
      log_e("Update: %s", errorMsg.c_str());
      obsDisplay->showTextOnGrid(0, 3, "Error");
      obsDisplay->showTextOnGrid(0, 4, errorMsg);
      res->setStatusCode(500);
      res->setStatusText("Invalid data!");
      res->print(errorMsg);
    }
  }
  sensorManager->attachInterrupts();
}

static void handleDeleteFiles(HTTPRequest *req, HTTPResponse * res) {
  const auto params = extractParameters(req);
  String path = getParameter(params, "path");
  if (path != "/trash") {
    SD.mkdir("/trash");
  }
  bool moveToRoot = !getParameter(params, "move").isEmpty();

  String html;
  if (moveToRoot) {
    html = replaceDefault(header, "Move to /");
    html += "<h3>Moving files</h3>";
    html += "<div>In: " + ObsUtils::encodeForXmlText(path);
    html += "</div><br /><div>";
  } else {
    html = replaceDefault(header, "Delete Files");
    html += "<h3>Deleting files</h3>";
    html += "<div>In: " + ObsUtils::encodeForXmlText(path);
    html += "</div><br /><div>";
  }
  sendHtml(res, html);
  html.clear();

  for (const auto& param : params) {
    if (param.first == "delete") {
      String file = param.second;

      String fullName = path + (path.endsWith("/") ? "" : "/") + file;

      html += ObsUtils::encodeForXmlText(file) + " &#10140; ";
      if (moveToRoot) {
        if (SD.rename(fullName, "/" + file)) {
          log_i("Moved '%s' to /", fullName.c_str());
          html += HTML_ENTITY_OK_MARK;
        } else {
          log_w("Failed to moved '%s' to /", fullName.c_str());
          html += HTML_ENTITY_FAILED_CROSS;
        }
      } else if (path != "/trash") {
        if (SD.rename(fullName, "/trash/" + file)) {
          log_i("Moved '%s'.", fullName.c_str());
          html += HTML_ENTITY_WASTEBASKET;
        } else {
          log_w("Failed to move '%s'.", fullName.c_str());
          html += HTML_ENTITY_FAILED_CROSS;
        }
      } else {
        if (SD.remove(fullName)) {
          log_i("Deleted '%s'.", fullName.c_str());
          html += HTML_ENTITY_WASTEBASKET;
        } else {
          log_w("Failed to delete '%s'.", fullName.c_str());
          html += HTML_ENTITY_FAILED_CROSS;
        }
      }
      html += "<br />\n";
      res->print(html);
      html.clear();
    }
  }
  html += "</div>";
  html += "<input type=button onclick=\"window.location.href='/sd?path="
    + ObsUtils::encodeForUrl(path) + "'\" class='btn' value='Back' />";
  html += footer;
  res->print(html);
}


static void handleSd(HTTPRequest *req, HTTPResponse *res) {
  String path = getParameter(req, "path", "/");

  File file = SDFileSystem.open(path);

  if (!file) {
    handleNotFound(req, res);
    return;
  }

  if (file.isDirectory()) {
    String html = header;
    html = replaceDefault(html, "SD Card Contents " + String(file.name()), "/deleteFiles");

    html += "<input type='hidden' name='path' value='" + ObsUtils::encodeForXmlAttribute(path) + "'/>";
    html += "<ul class=\"directory-listing\">";
    sendHtml(res, html);
    html.clear();
    obsDisplay->showTextOnGrid(0, 3, "Path:");
    obsDisplay->showTextOnGrid(1, 3, path);

    if (!path.endsWith("/")) {
      path += "/";
    }

// Iterate over directories
    File child = file.openNextFile();
    uint16_t counter = 0;
    while (child) {
      obsDisplay->drawWaitBar(5, counter++);

      auto fileName = String(child.name());
      auto fileTip = ObsUtils::encodeForXmlAttribute(
        TimeUtils::dateTimeToString(child.getLastWrite())
        + " - " + ObsUtils::toScaledByteString(child.size()));

      fileName = ObsUtils::encodeForXmlAttribute(fileName);
      bool isDirectory = child.isDirectory();
      html +=
        ("<li class=\""
         + String(isDirectory ? "directory" : "file")
         + "\" title='" + fileTip + "'>"
         + "<input class='small' type='checkbox' value='" + fileName + "' name='delete'"
         + String(isDirectory ? "disabled" : "")
         + "><a href=\"/sd?path="
         + path + String(child.name())
         + "\">"
         + String(isDirectory ? "&#x1F4C1;" : "&#x1F4C4;")
         + fileName
         + String(isDirectory ? "/" : "")
         + "</a></li>");

      child.close();
      child = file.openNextFile();

      if (html.length() >= (HTTP_UPLOAD_BUFLEN - 80)) {
        res->print(html);
        html.clear();
      }
    }
    file.close();
    if (counter > 0) {
      html += "<hr/>";
      html += "<li class=\"file\"><input class='small' type='checkbox' id='select-all' "
              "onclick='Array.prototype.slice.call(document.getElementsByClassName(\"small\")).filter("
                             "e=>!e.disabled).forEach(e=>e.checked=checked)"
              "'> select/deselect all</li>";
    }
    html += "</ul>";

    if (path != "/") {
      String back = path.substring(
        0, path.lastIndexOf('/', path.length() - 2));
      if (back.isEmpty()) {
        back = "/";
      }
      html += "<input type=button onclick=\"window.location.href='/sd?path="
              + ObsUtils::encodeForUrl(back) + "'\" class='btn' value='Up' />";
    } else {
      html += "<input type=button onclick=\"window.location.href='/'\" "
              "class='btn' value='Home' />";
    }

    if (counter > 0) {
      if (path == "/uploaded/") {
        html += "<hr /><p>Move for new upload will move selected files to the root directory, where "
                "they will be considered as new tracks and uploaded to the portal with the next "
                "Track upload.</p><input type='submit' class='btn' name='move' value='Move for new upload' />";
      }
      if (path == "/trash/") {
        html += "<hr /><input type='submit' class='btn' value='Delete Selected' />";
      } else {
        html += "<hr /><input type='submit' class='btn' value='Move to Trash' />";
      }
    }
    html += footer;
    res->print(html);
    obsDisplay->clearProgressBar(5);
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
  } else if (path.endsWith(".txt")) {
    dataType = "text/plain";
  }

  auto fileName = String(file.name());
  fileName = fileName.substring((int) (fileName.lastIndexOf("/") + 1));
  res->setHeader("Content-Disposition", (String("attachment; filename=\"") + fileName + String("\"")).c_str());
  if (dataType) {
    res->setHeader("Content-Type", dataType.c_str());
  }
  res->setHeader("Content-Length", String((uint32_t) file.size()).c_str());

  uint8_t buffer[HTTP_UPLOAD_BUFLEN];
  size_t read;
  while ((read = file.read(buffer, HTTP_UPLOAD_BUFLEN)) > 0) {
    size_t written = res->write(buffer, read);
    if (written != read) {
      log_d("Broken http connection wanted to write %d, but only %d written, abort.", read, written);
      break;
    }
  }
  file.close();
}

static String ensureSdIsAvailable() {
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

static uint16_t countFilesInRoot() {
  uint16_t numberOfFiles = 0;
  File root = SDFileSystem.open("/");
  File file = root.openNextFile("r");
  while (file) {
    if (!file.isDirectory()
        && String(file.name()).endsWith(CSVFileWriter::EXTENSION)) {
      numberOfFiles++;
      obsDisplay->drawWaitBar(4, numberOfFiles);
    }
    file = root.openNextFile();
  }
  root.close();
  obsDisplay->clearProgressBar(4);
  return numberOfFiles;
}

static void accessFilter(HTTPRequest * req, HTTPResponse * res, std::function<void()> next) {
  configServerWasConnectedViaHttpFlag = true;

  const String incomingPassword(req->getBasicAuthPassword().c_str());

  String httpPin = theObsConfig->getProperty<String>(ObsConfig::PROPERTY_HTTP_PIN);

  if (httpPin.length() < 1) {
    // Generate a new random PIN
    char defaultHttpPin[7];
    snprintf(defaultHttpPin, 7, "%06u", esp_random() % 1000000);
    httpPin = String(defaultHttpPin);

    // Store the PIN so it does not change
    theObsConfig->setProperty(0, ObsConfig::PROPERTY_HTTP_PIN, httpPin);
    theObsConfig->saveConfig();
  }

  if (incomingPassword != httpPin) {
    log_i("HTTP password: %s", httpPin.c_str());
    res->setStatusCode(401);
    res->setStatusText("Unauthorized");
    res->setHeader("Content-Type", "text/plain");
    res->setHeader("WWW-Authenticate", std::string("Basic realm=\"") + OBS_ID_SHORT.c_str() + "\"");
    res->println("401: See OBS display");

    obsDisplay->clearProgressBar(5);
    obsDisplay->cleanGridCell(0, 3);
    obsDisplay->cleanGridCell(0, 4);
    obsDisplay->cleanGridCell(1, 3);
    obsDisplay->cleanGridCell(1, 4);
    obsDisplay->showTextOnGrid(1, 3, httpPin, MEDIUM_FONT);
    // No call denies access to protected handler function.
  } else {
    obsDisplay->cleanGridCell(1, 3);
    // Everything else will be allowed, so we call next()
    next();
  }
}

static void handleHttpsRedirect(HTTPRequest *req, HTTPResponse *res) {
  String html = createPage(httpsRedirect);
  html = replaceDefault(html, "Https Redirect", "/http");
  String host(req->getHeader("host").c_str());
  if (!host || host == "") {
    host = getIp();
  }
  html = replaceHtml(html, "{host}", host);
  sendHtml(res, html);
}

static void handleHttpAction(HTTPRequest *req, HTTPResponse *res) {
  const String referer = req->getHTTPHeaders()->getValue("referer").c_str();
  req->discardRequestBody();
  if (referer.endsWith("/") && referer.startsWith("http://")) {
    registerPages(insecureServer);
  } else {
    log_w("Refused to enable http access, referer: '%s' suspect. ", referer.c_str());
  }
  sendRedirect(res, "/");
}

void configServerHandle() {
  server->loop();
  insecureServer->loop();
  if (dnsServer) {
    dnsServer->processNextRequest();
  }
  if (obsImprov) {
    obsImprov->handle();
  }
}

std::vector<std::pair<String,String>> extractParameters(HTTPRequest *req) {
  log_e("Extracting parameters");
  std::vector<std::pair<String,String>> parameters;
//  if (String(req->getHeader("Content-Type").c_str()).startsWith("application/x-www-form-urlencoded")) {
    HTTPURLEncodedBodyParser parser(req);
    while(parser.nextField()) {
      std::pair<String,String> data;
      data.first = String(parser.getFieldName().c_str());
      data.second = String();
      while (!parser.endOfField()) {
        char buf[513];
        size_t readLength = parser.read((uint8_t *)buf, 512);
        buf[readLength] = 0;
        data.second += String(buf);
      }
      log_e("Http Parameter %s = %s", data.first.c_str(), data.second.c_str());
      parameters.push_back(data);
    }
//  } else {
//    log_e("Unexpected content type: %s", req->getHeader("Content-Type").c_str());
//  }
  return parameters;
}

static void handleFirmwareUpdateSd(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(updateSdIndex, xhrUpload);
  html = replaceDefault(html, "Update Firmware", "/updateSdUrl");
  html = replaceHtml(html, "{releaseApiUrl}",
                     "https://api.github.com/repos/openbikesensor/OpenBikeSensorFirmware/releases?per_page=5");
  String flashAppVersion = Firmware::getFlashAppVersion();
  if (!flashAppVersion.isEmpty()) {
    html = replaceHtml(html, "{description}",
                       "Update Firmware, device reboots after download. "
                       "Current Flash App version is " + flashAppVersion + ".");
  } else {
    html = replacePlain(html, "{description}",
                       "<a href='/updateFlash'>Install Flash App 1st!</a>");
  }
  html = replaceHtml(html, "{method}", "/updatesd");
  html = replaceHtml(html, "{accept}", ".bin");

  sendHtml(res, html);
}

static bool mkSdFlashDir() {
  bool success = true;
  if (!SD.exists("/sdflash")) {
    if (SD.mkdir("/sdflash")) {
      log_i("Created sdflash directory.");
    } else {
      success = false;
      log_e("Error creating sdflash directory.");
    }
  }
  return success;
}

static void handleFirmwareUpdateSdUrlAction(HTTPRequest * req, HTTPResponse * res) {
  const auto params = extractParameters(req);
  const auto url = getParameter(params, "downloadUrl");
  const auto unsafe = getParameter(params, "unsafe");

  log_i("OBS Firmware URL is '%s'", url.c_str());

  if (!mkSdFlashDir()) {
    obsDisplay->showTextOnGrid(0, 3, "Error");
    obsDisplay->showTextOnGrid(0, 4, "mkdir 'sdflash' failed");
    sendRedirect(res, "/");
    return;
  }
  // TODO: Progress bar display && http!
  Firmware f(String("OBS/") + String(OBSVersion));
  f.downloadToSd(url, "/sdflash/app.bin", unsafe[0] == '1');
  obsDisplay->showTextOnGrid(0, 3, unsafe);


  String firmwareError = Firmware::checkSdFirmware();
  if (Firmware::getFlashAppVersion().isEmpty()) {
    firmwareError += "Flash App not installed!";
  }
  if (firmwareError.isEmpty()) {
    Firmware::switchToFlashApp();
    obsDisplay->showTextOnGrid(0, 3, "Success...");
    sendRedirect(res, "/reboot");
  } else {
    log_e("Abort update action %s.", firmwareError.c_str());
    obsDisplay->showTextOnGrid(0, 3, "Error");
    obsDisplay->showTextOnGrid(0, 4, firmwareError);
    sendRedirect(res, "/");
  }
}

static void handleFirmwareUpdateSdAction(HTTPRequest * req, HTTPResponse * res) {
  if (!mkSdFlashDir()) {
    obsDisplay->showTextOnGrid(0, 3, "Error");
    obsDisplay->showTextOnGrid(0, 4, "mkdir 'sdflash' failed!");
    sendHtml(res, "mkdir 'sdflash' failed!");
    return;
  }
  HTTPMultipartBodyParser parser(req);
  File newFile = SD.open("/sdflash/app.bin", FILE_WRITE);

  while(parser.nextField()) {
    if (parser.getFieldName() != "upload") {
      log_i("Skipping form data %s type %s filename %s", parser.getFieldName().c_str(),
            parser.getFieldMimeType().c_str(), parser.getFieldFilename().c_str());
      continue;
    }
    log_i("Got form data %s type %s filename %s", parser.getFieldName().c_str(),
          parser.getFieldMimeType().c_str(), parser.getFieldFilename().c_str());

    int tick = 0;
    size_t pos = 0;
    while (!parser.endOfField()) {
      byte buffer[256];
      size_t len = parser.read(buffer, 256);
      obsDisplay->drawWaitBar(4, tick++);
      log_v("Read data %d", len);
      pos += len;
      if (newFile.write(buffer, len) != len) {
        obsDisplay->showTextOnGrid(0, 3, "Write Error");
        obsDisplay->showTextOnGrid(0, 4, (String("Failed @") + String(pos)).c_str());
        res->setStatusCode(500);
        res->setStatusText((String("Failed to write @") + String(pos)).c_str());
        res->print((String("Failed to write @") + String(pos)).c_str());
        return;
      }
    }
    obsDisplay->clearProgressBar(4);
    log_i("Done reading");
    newFile.close();
    String firmwareError = Firmware::checkSdFirmware();
    if (Firmware::getFlashAppVersion().isEmpty()) {
      firmwareError += "Flash App not installed!";
    }
    if (firmwareError.isEmpty()) { //true to set the size to the current progress
      obsDisplay->showTextOnGrid(0, 3, "Success...");
      obsDisplay->showTextOnGrid(0, 4, "...will reboot");
      if (Firmware::switchToFlashApp()) {
        sendHtml(res, "Upload ok, will reboot!");
        res->finalize();
        log_e("OK!");
        delay(1000);
        ESP.restart();
      } else {
        res->setStatusCode(500);
        res->setStatusText("Failed to switch!");
        res->print("Failed to switch!");
      }
    } else {
      log_e("Update: %s", firmwareError.c_str());
      obsDisplay->showTextOnGrid(0, 3, "Error");
      obsDisplay->showTextOnGrid(0, 4, firmwareError);
      res->setStatusCode(500);
      res->setStatusText(firmwareError.c_str());
      res->print(firmwareError);
    }
  }
}

static void handleDownloadCert(HTTPRequest *req, HTTPResponse * res) {
  if (serverSslCert == nullptr) {
    handleNotFound(req, res);
    return;
  }
  uint8_t out[4096];
  size_t outLen;
  int err = mbedtls_base64_encode(
    out, sizeof(out), &outLen, serverSslCert->getCertData(), serverSslCert->getCertLength());

  if (err != 0) {
    log_e("Base64 encode returned 0x%02X.", err);
    res->setHeader("Content-Type", "text/plain");
    res->setStatusCode(500);
    res->setStatusText("Failed to convert cert.");
    res->print("ERROR");
  }
  res->setHeader("Content-Type", "application/octet-stream");
  res->setHeader("Content-Disposition", "attachment; filename=\"obs.crt\"");
  res->setStatusCode(200);
  res->setStatusText("OK");

  res->print("-----BEGIN CERTIFICATE-----\n");
  for (int i = 0; i < outLen; i += 64) {
    size_t ll = 64;
    if (i + ll > outLen) {
      ll = outLen - i;
    }
    res->write(&out[i], ll);
    res->print("\n");
  }
  res->print("-----END CERTIFICATE-----\n");
  res->finalize();
}

static void handleDelete(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(deleteIndex);
  html = replaceDefault(html, "Danger: Delete", "/delete");
  html = replaceHtml(html, "{description}",
                       "Warning there is not safety question!");
  sendHtml(res, html);
}

static void deleteFilesFromDirectory(File dir) {
  File entry =  dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String fileName = entry.name();
      entry.close();
      log_i("Will delete %s", fileName.c_str());
      SD.remove(fileName);
    }
    entry = dir.openNextFile();
  }
}

static void deleteFilesFromDirectory(String dirName) {
  File dir = SD.open(dirName);
  if (dir.isDirectory()) {
    deleteFilesFromDirectory(dir);
  }
  dir.close();
}

static void deleteObsdataFiles() {
  File dir = SD.open("/");
  if (dir.isDirectory()) {
    File entry =  dir.openNextFile();
    while (entry) {
      if (!entry.isDirectory()) {
        String fileName = entry.name();
        entry.close();
        if (fileName.endsWith("obsdata.csv")) {
          log_d("Will delete %s", fileName.c_str());
          SD.remove(fileName);
        }
      }
      entry = dir.openNextFile();
    }
  }
  dir.close();
}

static void deleteAllFromSd() {
  // ald_ini.ubx, tracknumber.txt, current_14d.*, *.obsdata.csv, sdflash/*, trash/*, uploaded/*
  deleteFilesFromDirectory("/trash");
  SD.rmdir("/trash");
  deleteFilesFromDirectory("/uploaded");
  SD.rmdir("/uploaded");
  deleteFilesFromDirectory("/sdflash");
  SD.rmdir("/sdflash");
  SD.remove("/tracknumber.txt");
  SD.remove("/aid_ini.ubx");
  SD.remove(LAST_MODIFIED_HEADER_FILE_NAME);
  SD.remove(ALP_DATA_FILE_NAME);
  SD.remove(ALP_NEW_DATA_FILE_NAME);
  deleteObsdataFiles();
}

static void handleDeleteAction(HTTPRequest *req, HTTPResponse * res) {
  // TODO: Result page with status!
  const auto params = extractParameters(req);

  if (getParameter(params, "flash") == "on") {
    SPIFFS.format();
  } else {
    if (getParameter(params, "flashConfig") == "on") {
      theObsConfig->removeConfig();
    }
    if (getParameter(params, "flashCert") == "on") {
      Https::removeCertificate();
    }
  }
  if (getParameter(params, "config") == "on") {
#ifdef CUSTOM_OBS_DEFAULT_CONFIG
    theObsConfig->parseJson(CUSTOM_OBS_DEFAULT_CONFIG);
#else
    theObsConfig->parseJson("{}");
#endif
    theObsConfig->fill(config);
  }
  if (getParameter(params, "sdcard") == "on") {
    deleteAllFromSd();
  }
  sendRedirect(res, "/");
}

static void handleSettingSecurity(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(settingsSecurityIndex);
  html = replaceDefault(html, "Settings Security", "/settings/security");
  html = replaceHtml(html, "{pin}",
      theObsConfig->getProperty<String>(ObsConfig::PROPERTY_HTTP_PIN));
  sendHtml(res, html);
}

static void handleSettingSecurityAction(HTTPRequest * req, HTTPResponse * res) {
  const auto params = extractParameters(req);
  const auto pin = getParameter(params, "pin");
  if (pin && pin.length() > 3) {
    theObsConfig->setProperty(0, ObsConfig::PROPERTY_HTTP_PIN, pin);
    theObsConfig->saveConfig();
  }
  sendRedirect(res, "/settings/security");
}

