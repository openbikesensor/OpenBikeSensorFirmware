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
static DNSServer *dnsServer;

// TODO
//  - Fix CSS Style for mobile && desktop
//  - a vs. button
//  - back navigation after save
static const char* const header =
  "<!DOCTYPE html>\n"
  "<html lang='en'><head><meta charset='utf-8'/><title>{title}</title>"
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
  "<input type=button onclick=\"window.location.href='/reboot'\" class=btn value='Reboot'>"
  "{dev}";

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

// #########################################
// Wifi
// #########################################

static const char* const wifiSettingsIndex =
  "<script>"
  "function resetPassword() { document.getElementById('pass').value = ''; }"
  "</script>"
  "<h3>Settings</h3>"
  "SSID"
  "<input name=ssid placeholder='ssid' value='{ssid}'>"
  "Password"
  "<input id=pass name=pass placeholder='password' type='Password' value='{password}' onclick='resetPassword()'>"
  "<input type=submit class=btn value=Save>";

static const char* const backupIndex =
  "<p>This backups and restores the device configuration incl. the Basic Config, Privacy Zones and Wifi Settings.</p>"
  "<h3>Backup</h3>"
  "<input type='button' onclick=\"window.location.href='/settings/backup.json'\" class=btn value='Download' />"
  "<h3>Restore</h3>";

static const char* const updateSdIndex = R""""(
<p>{description}</p>
<h3>From Github (preferred)</h3>
List also pre-releases<br><input type='checkbox' id='preReleases' onchange='selectFirmware()'>
<script>
let availableReleases;
async function updateFirmwareList() {
    (await fetch('{releaseApiUrl}')).json().then(res => {
        availableReleases = res;
        selectFirmware();
    })
}
function selectFirmware() {
   const displayPreReleases = (document.getElementById('preReleases').checked == true);
   url = "";
   version = "";
   availableReleases.filter(r => displayPreReleases || !r.prerelease).forEach(release => {
           release.assets.filter(asset => asset.name.endsWith(".bin")).forEach(
                asset => {
                  if (!url) {
                   version = release.name;
                   url = asset.browser_download_url;
                  }
               }
           )
        }
   )
   if (url) {
     document.getElementById('version').value = "Update to " + version;
     document.getElementById('version').disabled = false;
     document.getElementById('downloadUrl').value = url;
   } else {
     document.getElementById('version').value = "No version found";
     document.getElementById('version').disabled = true;
     document.getElementById('downloadUrl').value = "";
   }
}
updateFirmwareList();
</script>
<input type='hidden' name='downloadUrl' id='downloadUrl' value=''/>
<input type='submit' name='version' id='version' class=btn value='Update' />
<h3>File Upload</h3>
)"""";



// #########################################
// Development Index
// #########################################

static const char* const devIndex =
  "<h3>Display</h3>"
  "Show Grid<br><input type='checkbox' name='showGrid' {showGrid}>"
  "Print WLAN password to serial<br><input type='checkbox' name='printWifiPassword' {printWifiPassword}>"
  "<input type=submit class=btn value=Save>"
  "<hr>";

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
  "<input name='hostname' placeholder='hostname' value='{hostname}'>"
  "<hr>"
  "<input name='obsUserID' placeholder='API ID' value='{userId}' >"
  "<h3>Operation</h3>"
  "Enable Bluetooth <input type='checkbox' name='bluetooth' {bluetooth}>"
  "<hr>"
  "SimRa Mode <input type='checkbox' name='simRaMode' {simRaMode}>"
  "<input type=submit class=btn value=Save>";

static const char* const privacyIndexPostfix =
  "<input type=submit onclick=\"window.location.href='/'\" class=btn value=Save>"
  "<input type=button onclick=\"window.location.href='/settings/privacy/makeCurrentLocationPrivate'\" class=btn value='Make current location private'>";

static const char* const makeCurrentLocationPrivateIndex =
  "<div>Making current location private, waiting for fix. Press device button to cancel.</div>";

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
  "<label for='sdcard'>Delete OBS related content (ald_ini.ubx, tracknumber.txt, current_14d.*, *.obsdata.csv, sdflash/*, trash/*, uploaded/*)</label>"
  "<input type='checkbox' id='sdcard' name='sdcard' disabled='true'>"
  "<input type=submit class=btn value='Delete'>";

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
static void handleBackup(HTTPRequest * req, HTTPResponse * res);
static void handleBackupDownload(HTTPRequest * req, HTTPResponse * res);
static void handleBackupRestore(HTTPRequest * req, HTTPResponse * res);
static void handleWifi(HTTPRequest * req, HTTPResponse * res);
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
static void handleUpload(HTTPRequest * req, HTTPResponse * res);
static void handleMakeCurrentLocationPrivate(HTTPRequest * req, HTTPResponse * res);
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

static void tryWiFiConnect(const ObsConfig *obsConfig);
static uint16_t countFilesInRoot();
static String ensureSdIsAvailable();
static void moveToUploaded(const String &fileName);

void registerPages(HTTPServer * httpServer) {
  httpServer->setDefaultNode(new ResourceNode("", HTTP_GET, handleNotFound));
  httpServer->registerNode(new ResourceNode("/", HTTP_GET, handleIndex));
  httpServer->registerNode(new ResourceNode("/about", HTTP_GET, handleAbout));
  httpServer->registerNode(new ResourceNode("/reboot", HTTP_GET, handleReboot));
  httpServer->registerNode(new ResourceNode("/settings/backup", HTTP_GET, handleBackup));
  httpServer->registerNode(new ResourceNode("/settings/backup.json", HTTP_GET, handleBackupDownload));
  httpServer->registerNode(new ResourceNode("/settings/restore", HTTP_POST, handleBackupRestore));
  httpServer->registerNode(new ResourceNode("/settings/wifi", HTTP_GET, handleWifi));
  httpServer->registerNode(new ResourceNode("/settings/wifi/action", HTTP_POST, handleWifiSave));
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
#ifdef DEVELOP
  httpServer->registerNode(new ResourceNode("/settings/development/action", HTTP_GET, handleDevAction));
    httpServer->registerNode(new ResourceNode("/settings/development", HTTP_GET, handleDev));
#endif
  httpServer->registerNode(new ResourceNode("/privacy_action", HTTP_POST, handlePrivacyAction));
  httpServer->registerNode(new ResourceNode("/upload", HTTP_GET, handleUpload));
  httpServer->registerNode(new ResourceNode("/settings/privacy/makeCurrentLocationPrivate", HTTP_GET, handleMakeCurrentLocationPrivate));
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

  insecureServer->registerNode(new ResourceNode("/cert", HTTP_GET, handleDownloadCert));
  insecureServer->registerNode(new ResourceNode("/http", HTTP_POST, handleHttpAction));
  insecureServer->setDefaultNode(new ResourceNode("", HTTP_GET,  handleHttpsRedirect));
  insecureServer->setDefaultHeader("Server", std::string("OBS/") + OBSVersion);
}

static int ticks;
static void progressTick() {
  displayTest->drawWaitBar(5, ticks++);
}

static void createHttpServer() {
  if (!Https::existsCertificate()) {
    displayTest->showTextOnGrid(1, 4, "");
    displayTest->showTextOnGrid(0, 5, "");
    displayTest->showTextOnGrid(0, 4, "Creating ssl cert!");
  }
  serverSslCert = Https::getCertificate(progressTick);
  server = new HTTPSServer(serverSslCert, 443, 1);
  displayTest->clearProgressBar(5);
  displayTest->showTextOnGrid(0, 4, "");
  insecureServer = new HTTPServer(80, 1);

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

String getIp() {
  if (WiFiClass::status() != WL_CONNECTED) {
    return WiFi.softAPIP().toString();
  } else {
    return WiFi.localIP().toString();
  }
}

bool CreateWifiSoftAP() {
  bool softAccOK;
  WiFi.disconnect();
  Serial.print(F("Initalize SoftAP "));
  String apName = OBS_ID;
  String APPassword = "12345678";
  softAccOK  =  WiFi.softAP(apName.c_str(), APPassword.c_str(), 1, 0, 1); // Passwortlänge mindestens 8 Zeichen !
  delay(2000); // Without delay I've seen the IP address blank
  /* Soft AP network parameters */
  IPAddress apIP(172, 20, 0, 1);
  IPAddress netMsk(255, 255, 255, 0);

  displayTest->showTextOnGrid(0, 1, "AP:");
  displayTest->showTextOnGrid(1, 1, "");
  displayTest->showTextOnGrid(0, 2, apName.c_str());


  WiFi.softAPConfig(apIP, apIP, netMsk);
  if (softAccOK) {
    dnsServer = new DNSServer();
    // with "*" we get a lot of requests from all sort of apps,
    // use obs.local here
    dnsServer->start(53, "obs.local", apIP);

    log_i("AP successful IP: %s", apIP.toString().c_str());

    displayTest->showTextOnGrid(0, 3, "Pass:");
    displayTest->showTextOnGrid(1, 3, APPassword);

    displayTest->showTextOnGrid(0, 4, "IP:");
    displayTest->showTextOnGrid(1, 4, WiFi.softAPIP().toString());
  } else {
    log_e("Soft AP Error. Name: %s Pass: %s", apName.c_str(), APPassword.c_str());
  }
  return softAccOK;
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

  displayTest->clear();
  displayTest->showTextOnGrid(0, 0, "Ver.:");
  displayTest->showTextOnGrid(1, 0, OBSVersion);

  displayTest->showTextOnGrid(0, 1, "SSID:");
  displayTest->showTextOnGrid(1, 1,
                              theObsConfig->getProperty<String>(ObsConfig::PROPERTY_WIFI_SSID));

  tryWiFiConnect(obsConfig);

  if (WiFiClass::status() != WL_CONNECTED) {
    CreateWifiSoftAP();
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

  MDNS.begin("obs");

  TimeUtils::setClockByNtp(WiFi.gatewayIP().toString().c_str());
  if (!voltageMeter) {
    voltageMeter = new VoltageMeter();
  }

  if (SD.begin() && WiFiClass::status() == WL_CONNECTED) {
    AlpData::update(displayTest);
  }

  log_i("About to create http server.");
  createHttpServer();
}

static void tryWiFiConnect(const ObsConfig *obsConfig) {
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


static void handleIndex(HTTPRequest *, HTTPResponse * res) {
// ###############################################################
// ### Index ###
// ###############################################################
  String html = createPage(navigationIndex);
  html = replaceDefault(html, "Navigation");
#ifdef DEVELOP
  html.replace("{dev}", development);
#else
  html.replace("{dev}", "");
#endif
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


static void handleAbout(HTTPRequest *req, HTTPResponse * res) {
  res->setHeader("Content-Type", "text/html");
  res->print(replaceDefault(header, "About"));
  String page;
  gps.pollStatistics(); // takes ~100ms!

  res->print("<h3>ESP32</h3>"); // SPDIFF
  res->print(keyValue("Heap size", ObsUtils::toScaledByteString(ESP.getHeapSize())));
  res->print(keyValue("Free heap", ObsUtils::toScaledByteString(ESP.getFreeHeap())));
  res->print(keyValue("Min. free heap", ObsUtils::toScaledByteString(ESP.getMinFreeHeap())));
  String chipId = String((uint32_t) ESP.getEfuseMac(), HEX) + String((uint32_t) (ESP.getEfuseMac() >> 32), HEX);
  chipId.toUpperCase();
  res->print(keyValue("Chip id", chipId));
  res->print(keyValue("FlashApp Version", Firmware::getFlashAppVersion()));
  res->print(keyValue("IDF Version", esp_get_idf_version()));

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
  page += keyValue("Button State", digitalRead(PUSHBUTTON_PIN));
  page += keyValue("Display i2c last error", Wire.lastError());
  page += keyValue("Display i2c speed", Wire.getClock() / 1000, "KHz");
  page += keyValue("Display i2c timeout", Wire.getTimeOut(), "ms");

  page += "<h3>WiFi</h3>";
  page += keyValue("Local IP", WiFi.localIP().toString());
  page += keyValue("AP IP", WiFi.softAPIP().toString());
  page += keyValue("Gateway IP", WiFi.gatewayIP().toString());
  page += keyValue("Hostname", WiFi.getHostname());
  page += keyValue("SSID", WiFi.SSID());

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
  String html = createPage(wifiSettingsIndex);
  html = replaceDefault(html, "WiFi", "/settings/wifi/action");

  // Form data
  html = replaceHtml(html, "{ssid}", theObsConfig->getProperty<String>(ObsConfig::PROPERTY_WIFI_SSID));
  if (theObsConfig->getProperty<String>(ObsConfig::PROPERTY_WIFI_SSID).length() > 0) {
    html = replaceHtml(html, "{password}", "******");
  } else {
    html = replaceHtml(html, "{password}", "");
  }
  sendHtml(res, html);
}

static void handleWifiSave(HTTPRequest * req, HTTPResponse * res) {
  const auto params = extractParameters(req);
  const auto ssid = getParameter(params, "ssid");
  if (ssid) {
    theObsConfig->setProperty(0, ObsConfig::PROPERTY_WIFI_SSID, ssid);
  }
  const auto password = getParameter(params, "pass");
  if (password != "******") {
    theObsConfig->setProperty(0, ObsConfig::PROPERTY_WIFI_PASSWORD, password);
  }
  theObsConfig->saveConfig();
  sendRedirect(res, "/settings/wifi");
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
  theObsConfig->setProperty(0, ObsConfig::PROPERTY_SIM_RA,
                            (bool) (getParameter(params, "simRaMode") == "on"));
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
  html = replaceHtml(html, "{simRaMode}",
               theObsConfig->getProperty<bool>(ObsConfig::PROPERTY_SIM_RA) ? "checked" : "");

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

#ifdef DEVELOP
static void handleDevAction(HTTPRequest *req, HTTPResponse *res) {
  auto params = req->getParams();
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DEVELOPER, ShowGrid,
                                   getParameter(params, "showGrid") == "on");
  theObsConfig->setBitMaskProperty(0, ObsConfig::PROPERTY_DEVELOPER, PrintWifiPassword,
                                   getParameter(params, "printWifiPassword") == "on");
  theObsConfig->saveConfig();
  sendRedirect(res, "/settings/development");
}

static void handleDev(HTTPRequest *, HTTPResponse *res) {
  String html = createPage(devIndex);
  html = replaceDefault(html, "Development Setting", "/settings/development/action");

  // SHOWGRID
  bool showGrid = theObsConfig->getBitMaskProperty(
    0, ObsConfig::PROPERTY_DEVELOPER, ShowGrid);
  html.replace("{showGrid}", showGrid ? "checked" : "");

  bool printWifiPassword = theObsConfig->getBitMaskProperty(
    0, ObsConfig::PROPERTY_DEVELOPER, PrintWifiPassword);
  html.replace("{printWifiPassword}", printWifiPassword ? "checked" : "");

  sendHtml(res, html);
}
#endif

static void handlePrivacyAction(HTTPRequest *req, HTTPResponse *res) {
  const auto params = extractParameters(req);

  String latitude = getParameter(params, "newlatitude");
  latitude.replace(",", ".");
  String longitude = getParameter(params, "newlongitude");
  longitude.replace(",", ".");
  String radius = getParameter(params, "newradius");

  if ( (latitude != "") && (longitude != "") && (radius != "") ) {
    Serial.println(F("Valid privacyArea!"));
    theObsConfig->addPrivacyArea(0,
      Gps::newPrivacyArea(atof(latitude.c_str()), atof(longitude.c_str()), atoi(radius.c_str())));
  }

  String s = "<meta http-equiv='refresh' content='0; url=/settings/privacy'><a href='/settings/privacy'>Go Back</a>";
  sendHtml(res, s);
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
    displayTest->showTextOnGrid(0, 3, "Error:");
    displayTest->showTextOnGrid(0, 4, sdErrorMessage);
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
      if (res) {
        res->print(friendlyFileName);
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
      if (res) {
        html += "<br />\n";
        res->print(html);
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

static void handleMakeCurrentLocationPrivate(HTTPRequest *, HTTPResponse *res) {
// new implementation in other branch
}

static void handlePrivacy(HTTPRequest *, HTTPResponse *res) {
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
  displayTest->drawProgressBar(4, pos, all);
}

static void handleFlashUpdateUrlAction(HTTPRequest * req, HTTPResponse * res) {
  const auto params = extractParameters(req);
  const auto url = getParameter(params, "downloadUrl");
  log_i("Flash App Url is '%s'", url.c_str());

  Firmware f(String("OBS/") + String(OBSVersion));
  sensorManager->detachInterrupts();
  if (f.downloadToFlash(url, updateProgress)) {
    displayTest->showTextOnGrid(0, 3, "Success!");
    sendRedirect(res, "/updatesd");
  } else {
    displayTest->showTextOnGrid(0, 3, "Error");
    displayTest->showTextOnGrid(0, 4, f.getLastMessage());
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
    displayTest->drawProgressBar(4, pos, all);
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
      displayTest->showTextOnGrid(0, 3, "Success...");
      const esp_partition_t *running = esp_ota_get_running_partition();
      esp_ota_set_boot_partition(running);
    } else {
      String errorMsg = Update.errorString();
      log_e("Update: %s", errorMsg.c_str());
      displayTest->showTextOnGrid(0, 3, "Error");
      displayTest->showTextOnGrid(0, 4, errorMsg);
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
  if (path != "trash") {
    SD.mkdir("/trash");
  }
  bool moveToRoot = !getParameter(params, "move").isEmpty();

  String html = replaceDefault(header, "Delete Files");
  html += "<h3>Deleting files</h3>";
  html += "<div>In: " + ObsUtils::encodeForXmlText(path);
  html += "</div><br /><div>";
  sendHtml(res, html);
  html.clear();

  for (const auto& param : params) {
    if (param.first == "delete") {
      String file = param.second;

      String fullName = path + (path.length() > 1 ? "/" : "") + file;

      html += ObsUtils::encodeForXmlText(file) + " &#10140; ";
      if (moveToRoot) {
        if (SD.rename(fullName, "/" + file)) {
          log_i("Moved '%s' to /", fullName.c_str());
          html += HTML_ENTITY_OK_MARK;
        } else {
          log_w("Failed to moved '%s' to /", fullName.c_str());
          html += HTML_ENTITY_FAILED_CROSS;
        }
      } else if (path != "trash") {
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
    displayTest->showTextOnGrid(0, 3, "Path:");
    displayTest->showTextOnGrid(1, 3, path);

// Iterate over directories
    File child = file.openNextFile();
    uint16_t counter = 0;
    while (child) {
      displayTest->drawWaitBar(5, counter++);

      auto fileName = String(child.name());
      auto fileTip = ObsUtils::encodeForXmlAttribute(
        TimeUtils::dateTimeToString(child.getLastWrite())
        + " - " + ObsUtils::toScaledByteString(child.size()));

      fileName = fileName.substring(int(fileName.lastIndexOf("/") + 1));
      fileName = ObsUtils::encodeForXmlAttribute(fileName);
      bool isDirectory = child.isDirectory();
      html +=
        ("<li class=\""
         + String(isDirectory ? "directory" : "file")
         + "\" title='" + fileTip + "'>"
         + "<input class='small' type='checkbox' value='" + fileName + "' name='delete'"
         + String(isDirectory ? "disabled" : "")
         + "><a href=\"/sd?path="
         + String(child.name())
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
    html += "</ul>";

    if (path != "/") {
      String back = path.substring(0, path.lastIndexOf('/'));
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
      if (path == "/uploaded") {
        html += "<hr /><p>Move for new upload will move selected files to the root directory, where "
                "they will be considered as new tracks and uploaded to the portal with the next "
                "Track upload.</p><input type='submit' class='btn' name='move' value='Move for new upload' />";
      }
      if (path == "/trash") {
        html += "<hr /><input type='submit' class='btn' value='Delete Selected' />";
      } else {
        html += "<hr /><input type='submit' class='btn' value='Move to Trash' />";
      }
    }
    html += footer;
    res->print(html);
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
      displayTest->drawWaitBar(4, numberOfFiles);
    }
    file = root.openNextFile();
  }
  root.close();
  displayTest->clearProgressBar(4);
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

    displayTest->clearProgressBar(5);
    displayTest->cleanGridCell(0, 3);
    displayTest->cleanGridCell(0, 4);
    displayTest->cleanGridCell(1, 3);
    displayTest->cleanGridCell(1, 4);
    displayTest->showTextOnGrid(1,3, httpPin, MEDIUM_FONT);
    // No call denies access to protected handler function.
  } else {
    displayTest->cleanGridCell(1, 3);
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
  log_i("OBS Firmware URL is '%s'", url.c_str());

  if (!mkSdFlashDir()) {
    displayTest->showTextOnGrid(0, 3, "Error");
    displayTest->showTextOnGrid(0, 4, "mkdir 'sdflash' failed");
    sendRedirect(res, "/");
    return;
  }
  // TODO: Progress bar display && http!
  Firmware f(String("OBS/") + String(OBSVersion));
  f.downloadToSd(url, "/sdflash/app.bin");

  String firmwareError = Firmware::checkSdFirmware();
  if (Firmware::getFlashAppVersion().isEmpty()) {
    firmwareError += "Flash App not installed!";
  }
  if (firmwareError.isEmpty()) {
    Firmware::switchToFlashApp();
    displayTest->showTextOnGrid(0, 3, "Success...");
    sendRedirect(res, "/reboot");
  } else {
    log_e("Abort update action %s.", firmwareError.c_str());
    displayTest->showTextOnGrid(0, 3, "Error");
    displayTest->showTextOnGrid(0, 4, firmwareError);
    sendRedirect(res, "/");
  }
}

static void handleFirmwareUpdateSdAction(HTTPRequest * req, HTTPResponse * res) {
  if (!mkSdFlashDir()) {
    displayTest->showTextOnGrid(0, 3, "Error");
    displayTest->showTextOnGrid(0, 4, "mkdir 'sdflash' failed!");
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
      displayTest->drawWaitBar(4, tick++);
      log_v("Read data %d", len);
      pos += len;
      if (newFile.write(buffer, len) != len) {
        displayTest->showTextOnGrid(0, 3, "Write Error");
        displayTest->showTextOnGrid(0, 4, (String("Failed @") + String(pos)).c_str());
        res->setStatusCode(500);
        res->setStatusText((String("Failed to write @") + String(pos)).c_str());
        res->print((String("Failed to write @") + String(pos)).c_str());
        return;
      }
    }
    displayTest->clearProgressBar(4);
    log_i("Done reading");
    newFile.close();
    String firmwareError = Firmware::checkSdFirmware();
    if (Firmware::getFlashAppVersion().isEmpty()) {
      firmwareError += "Flash App not installed!";
    }
    if (firmwareError.isEmpty()) { //true to set the size to the current progress
      displayTest->showTextOnGrid(0, 3, "Success...");
      displayTest->showTextOnGrid(0, 4, "...will reboot");
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
      displayTest->showTextOnGrid(0, 3, "Error");
      displayTest->showTextOnGrid(0, 4, firmwareError);
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
};

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
    theObsConfig->parseJson("{}");
    theObsConfig->fill(config);
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
  }
  sendRedirect(res, "/settings/security");
}

