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
  "#file-input,input {width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
  "input {background:#f1f1f1;border:0;padding:0 15px}"
  "body {background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
  "#file-input {padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
  "#bar,#prgbar {background-color:#f1f1f1;border-radius:10px}"
  "#bar {background-color:#3498db;width:0%;height:10px}"
  "form {background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
  ".btn {background:#3498db;color:#fff;cursor:pointer}"
  "h1,h2 {padding:0;margin:0;}"
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
  "Offset S1<input name='offsetS1' placeholder='Offset Sensor 1' value='{offset1}'>"
  "Offset S2<input name='offsetS2' placeholder='Offset Sensor 2' value='{offset2}'>"
  "Upload Host<input name='hostname' placeholder='hostname' value='{hostname}'>"
  "Upload UserID<input name='obsUserID' placeholder='API ID' value='{userId}'>"
  "Display Both<input type='checkbox' name='displayBoth' {displayBoth}>"
  "Display Satellites<input type='checkbox' name='displayGPS' {displayGPS}>"
  "Display Velocity<input type='checkbox' name='displayVELO' {displayVELO}>"
  "<input type=submit class=btn value=Save>"
  + footer;

// #########################################
// Upload
// #########################################

/* Server Index Page */
String uploadIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  + header +
  "<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
  "<label id='file-input' for='file'>   Choose file...</label>"
  "<input id='btn' type='submit' class=btn value='Update'>"
  "<br><br>"
  "<div id='prg'></div>"
  "<br><div id='prgbar'><div id='bar'></div></div><br></form>"
  "<script>"
  ""
  "$(document).ready(function() {"
  "console.log('Document Ready');"
  "$('#prgbar').hide();"
  "$('#prg').hide();"
  "});"
  ""
  "var fileName = '';"
  "function sub(obj){"
  "fileName = obj.value.split('\\\\');"
  "$('#file-input').html(fileName[fileName.length-1]);"
  "};"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "console.log('Start upload...');"
  "if (fileName == '') { alert('No file choosen'); return; }"
  "var form = $('form')[0];"
  "var data = new FormData(form);"
  "$('#file-input').hide();"
  "$('#btn').hide();"
  "$('#prgbar').show();"
  "$('#prg').show();"
  "$.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr_inner = new window.XMLHttpRequest();"
  "xhr_inner.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('Progress: ' + Math.round(per*100) + '%');"
  "$('#bar').css('width',Math.round(per*100) + '%');"
  "}" // xhr
  "}, false);" // ajax
  "return xhr_inner;"
  "},"
  "success:function(d, s) {"
  "console.log('success!');"
  "$('#prg').html('Device reboots now!');"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>" + footer;
