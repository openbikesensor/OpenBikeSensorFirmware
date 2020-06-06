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

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

const char* host = "openbikesensor";

// DNS server
const byte DNS_PORT = 53;
//DNSServer dnsServer;

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
  "h1,h2 {padding:0;margin:0;}"
  "h3 {padding:10px 0;margin:0;}"
  "h1 a {color:#777}"
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
  "<input type=button onclick=window.location.href='/privacy' class=btn value='Privacy'>"
  "<input type=button onclick=window.location.href='/wifi' class=btn value='Wifi Settings'>"
  "<input type=button onclick=window.location.href='/update' class=btn value='Update Firmware'>"
  "<input type=button onclick=window.location.href='/reboot' class=btn value='Reboot'>"
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
  "Offset Sensor Right<input name='offsetS2' placeholder='Offset Sensor Right' value='{offset2}'>"
  "Swap Sensors (Left <--> Right)<input type='checkbox' name='displaySwapSensors' {displaySwapSensors}>"
  "<h3>GPS</h3>"
  "<label for='location'>Valid Location</label>"
  "<input type='radio' id='location' name='validGPS' value='validLocation' {validLocation}>"
  "<label for='female'>Valid Time</label>"
  "<input type='radio' id='time' name='validGPS' value='validTime' {validTime}> "
  "<label for='other'>Number of satellites</label> "
  "<input type='radio' id='numsatellites' name='validGPS' value='numberSatellites' {numberSatellites}>"
  "<br>Number of Satellites for fix<input name='satsForFix' placeholder='Number of Satellites for fix' value='{satsForFix}'>"
  "<h3>Generic Display</h3>"
  "Invert<br>(black <--> white)<input type='checkbox' name='displayInvert' {displayInvert}>"
  "Flip<br>(upside down)<input type='checkbox' name='displayFlip' {displayFlip}>"
  "<h3>Measurement Display</h3>"
  "Confirmation Time Window<br>(time in seconds to confirm until new measurement starts)<input name='confirmationTimeWindow' placeholder='Milliseconds' value='{confirmationTimeWindow}'>"
  "Simple Mode<br>(all measurement display options below are ignored)<input type='checkbox' name='displaySimple' {displaySimple}>"
  "Show Left Measurement<input type='checkbox' name='displayLeft' {displayLeft}>"
  "Show Right Measurement<input type='checkbox' name='displayRight' {displayRight}>"
  "Show Satellites<input type='checkbox' name='displayGPS' {displayGPS}>"
  "Show Velocity<input type='checkbox' name='displayVELO' {displayVELO}>"
  "Show Confirmation Stats<input type='checkbox' name='displayNumConfirmed' {displayNumConfirmed}>"
  //"Upload Host"
  "<input name='hostname' placeholder='hostname' value='{hostname}' style='display:none'>"
  //"Upload UserID"
  "<input name='obsUserID' placeholder='API ID' value='{userId}' style='display:none'>"
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
