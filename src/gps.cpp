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

#include "gps.h"
#include <sys/time.h>

/* Value is in the past (just went by at the time of writing). */
static const time_t PAST_TIME = 1606672131;

void Gps::begin() {
  mSerial.begin(9600, SERIAL_8N1); // , 16, 17);
  // TODO: Increase speed etc.
  configureGpsModule();
}

time_t Gps::getGpsTime() {
  struct tm t;
  t.tm_year = date.year() - 1900;
  t.tm_mon = date.month() - 1; // Month, 0 - jan
  t.tm_mday = date.day();
  t.tm_hour = time.hour();
  t.tm_min = time.minute();
  t.tm_sec = time.second();
  return mktime(&t);
}

time_t Gps::currentTime() {
  time_t result = 0;
  if (date.isValid() && date.age() < 2000) {
    result = getGpsTime();
  }
  if (result < PAST_TIME) {
    result = ::time(nullptr);
  }
  return result;
}

void Gps::configureGpsModule() {
  log_d("Sending config to GPS module.");
  // switch of periodic gps messages that we do not use, leaves GGA and RMC on
  mSerial.print(F("$PUBX,40,GSV,0,0,0,0*59\r\n"));
  mSerial.print(F("$PUBX,40,GSA,0,0,0,0*4E\r\n"));
  mSerial.print(F("$PUBX,40,GLL,0,0,0,0*5C\r\n"));
  mSerial.print(F("$PUBX,40,VTG,0,0,0,0*5E\r\n"));
  mSerial.print(F("$PUBX,40,GGA,0,1,0,0*5B\r\n"));
  mSerial.print(F("$PUBX,40,RMC,0,1,0,0*46\r\n"));

  // setting also the default values here - in the net you see reports of modules with wired defaults
  // "dynModel" - "3: pedestrian"
  // "static hold" - 80cm/s == 2.88 km/h
  // "staticHoldMaxDist" - 20m (not supported by our GPS receivers)
  const uint8_t UBX_CFG_NAV5[] = {
    0xb5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xff, 0xff, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27,
    0x00, 0x00, 0x05, 0x00, 0xfa, 0x00, 0xfa, 0x00, 0x64, 0x00, 0x2c, 0x01, 0x50, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0x76
  };
  mSerial.write(UBX_CFG_NAV5, sizeof(UBX_CFG_NAV5));

  // "timepulse" - affecting the led, switching to 10ms pulse every 10sec, should be clearly differentiable
  // from the default (100ms each second)
  const uint8_t UBX_CFG_TP[] = {
    0xb5, 0x62, 0x06, 0x07, 0x14, 0x00, 0x80, 0x96, 0x98, 0x00, 0x10, 0x27, 0x00, 0x00, 0x01, 0x01,
    0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3a, 0xab
  };
  mSerial.write(UBX_CFG_TP, sizeof(UBX_CFG_TP));
}

void Gps::handle() {
#ifdef DEVELOP
  if (mSerial.available() > 0) {
    time_t now;
    char buffer[64];
    struct tm timeInfo;

    ::time(&now);
    localtime_r(&now, &timeInfo);
    strftime(buffer, sizeof(buffer), "%c", &timeInfo);
    Serial.printf("readGPSData(av: %d bytes, %s)\n",
                  mSerial.available(), buffer);
  }
#endif

  boolean gotGpsData = false;
  while (mSerial.available() > 0) {
    if (encode(mSerial.read())) {
      gotGpsData = true;
      // set system time once every minute
      if (time.isValid() && date.isValid() && time.isUpdated()
          && date.month() != 0 && date.day() != 0 && date.year() > 2019
          && (time.second() == 0 || ::time(nullptr) < PAST_TIME)
          && time.age() < 100) {
        time.value(); // reset "updated" flag
        // We are in the precision of +/- 1 sec, ok for our purpose
        const time_t t = getGpsTime();
        const struct timeval now = {.tv_sec = t};
        settimeofday(&now, nullptr);
        log_d("Time set %ld.\n", t);
      }

      handleNewTxtData();
    }
  }
  // send configuration multiple times
  if (gotGpsData && passedChecksum() > 1 && passedChecksum() < 110 && 0 == passedChecksum() % 11) {
    configureGpsModule();
  }
}

void Gps::handleNewTxtData() {
  if (mTxtMessage.isUpdated()) {
    log_v("GPX Text SEQ %s, CNT %s, SEVERITY: %s, Message %s",
          gpsTxtSeq.value(), gpsTxtCount.value(), gpsTxtSeverity.value(), gpsTxtMessage.value());
    // We only get start of the message, cause tiny gps limits it.
    if (String("01") == mTxtSeq.value()) {
      mMessage = mTxtMessage.value();
    } else {
      mMessage += mTxtMessage.value();
    }
    if (String(mTxtCount.value()) == String(mTxtSeq.value())) {
      for (String& message : mMessages) {
        if (message == mMessage) {
          mMessage.clear();
          break;
        }
      }
      if (!mMessage.isEmpty()) {
        mMessages.push_back(mMessage);
      }
      mMessage.clear();
      if (mMessages.size() > 10) {
        mMessages.erase(mMessages.cbegin());
      }
    }
  }
}

bool Gps::isInsidePrivacyArea() {
  // quite accurate haversine formula
  // consider using simplified flat earth calculation to save time

  // TODO: Config must not be read from the globals here!
  for (auto pa : config.privacyAreas) {
    double distance = haversine(
      location.lat(), location.lng(), pa.transformedLatitude, pa.transformedLongitude);
    if (distance < pa.radius) {
      return true;
    }
  }
  return false;
}

double Gps::haversine(double lat1, double lon1, double lat2, double lon2) {
  // TODO: There is also TinyGPSPlus::distanceBetween()
  // https://www.geeksforgeeks.org/haversine-formula-to-find-distance-between-two-points-on-a-sphere/
  // distance between latitudes and longitudes
  double dLat = (lat2 - lat1) * M_PI / 180.0;
  double dLon = (lon2 - lon1) * M_PI / 180.0;

  // convert to radians
  lat1 = (lat1) * M_PI / 180.0;
  lat2 = (lat2) * M_PI / 180.0;

  // apply formulae
  double a = pow(sin(dLat / 2), 2) + pow(sin(dLon / 2), 2) * cos(lat1) * cos(lat2);
  double rad = 6371000;
  double c = 2 * asin(sqrt(a));
  return rad * c;
}

void Gps::randomOffset(PrivacyArea &p) {
  randomSeed(analogRead(0));
  // Offset in degree and distance
  int offsetAngle = random(0, 360);
  int offsetDistance = random(p.radius / 10.0, p.radius / 10.0 * 9.0);
  //Offset in m
  int dLatM = sin(offsetAngle / 180.0 * M_PI ) * offsetDistance;
  int dLongM = cos(offsetAngle / 180.0 * M_PI ) * offsetDistance;
#ifdef DEVELOP
  Serial.print(F("offsetAngle = "));
  Serial.println(String(offsetAngle));

  Serial.print(F("offsetDistance = "));
  Serial.println(String(offsetDistance));

  Serial.print(F("dLatM = "));
  Serial.println(String(dLatM));

  Serial.print(F("dLongM = "));
  Serial.println(String(dLongM));
#endif
  //Earth’s radius, sphere
  double R = 6378137.0;

  //Coordinate offsets in radians
  double dLat = dLatM / R;
  double dLon = dLongM / (R * cos(M_PI * p.latitude / 180.0));
#ifdef DEVELOP
  Serial.print(F("dLat = "));
  Serial.println(String(dLat, 5));

  Serial.print(F("dLong = "));
  Serial.println(String(dLon, 5));
#endif
  //OffsetPosition, decimal degrees
  p.transformedLatitude = p.latitude + dLat * 180.0 / M_PI;
  p.transformedLongitude = p.longitude + dLon * 180.0 / M_PI ;
#ifdef DEVELOP
  Serial.print(F("p.transformedLatitude = "));
  Serial.println(String(p.transformedLatitude, 5));

  Serial.print(F("p.transformedLongitude = "));
  Serial.println(String(p.transformedLongitude, 5));
#endif
}

PrivacyArea Gps::newPrivacyArea(double latitude, double longitude, int radius) {
  PrivacyArea newPrivacyArea;
  newPrivacyArea.latitude = latitude;
  newPrivacyArea.longitude = longitude;
  newPrivacyArea.radius = radius;
  randomOffset(newPrivacyArea);
  return newPrivacyArea;
}

bool Gps::hasState(int state, SSD1306DisplayDevice *display) {
  bool result = false;
  switch (state) {
    case (int) WaitFor::FIX_POS:
      if (sentencesWithFix() > 0) {
        log_d("Got location...");
        display->showTextOnGrid(2, 4, "Got location");
        result = true;
      }
      break;
    case (int) WaitFor::FIX_TIME:
      if (time.isValid()
                     && !(time.second() == 00 && time.minute() == 00 && time.hour() == 00)) {
        log_d("Got time...");
        display->showTextOnGrid(2, 4, "Got time");
        result = true;
      }
      break;
    case (int) WaitFor::FIX_NO_WAIT:
      log_d("GPS, no wait");
      display->showTextOnGrid(2, 4, "GPS, no wait");
      result = true;
      break;
    default:
      if (satellites.value() >= state) {
        log_d("Got required number of satellites...");
        display->showTextOnGrid(2, 4, "Got satellites");
        result = true;
      }
      break;
  }
  return result;
}

void Gps::showWaitStatus(SSD1306DisplayDevice *display) {
  String satellitesString[2];
  if (gps.passedChecksum() == 0) { // could not get any valid char from GPS module
    satellitesString[0] = "OFF?";
  } else if (!gps.time.isValid()
             || (gps.time.second() == 00 && gps.time.minute() == 00 && gps.time.hour() == 00)) {
    satellitesString[0] = "no time";
  } else {
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
             gps.time.hour(), gps.time.minute(), gps.time.second());
    satellitesString[0] = String(timeStr);
    satellitesString[1] = String(gps.satellites.value()) + " satellites";
  }

  if (gps.passedChecksum() != 0    //only do this if a communication is there and a valid time is there
      && gps.time.isValid()
      && !(gps.time.second() == 00 && gps.time.minute() == 00 && gps.time.hour() == 00)) {
    // This is a hack :) if still the "Wait for GPS" version is displayed original line
    if (displayTest->get_gridTextofCell(2, 4).startsWith("Wait")) {
      display->newLine();
    }
    displayTest->showTextOnGrid(2, display->currentLine() - 1, satellitesString[0]);
    displayTest->showTextOnGrid(2, display->currentLine(), satellitesString[1]);
  } else { //if no gps comm or no time is there, just write in the last row
    displayTest->showTextOnGrid(2, display->currentLine(), satellitesString[0]);
  }
}

bool Gps::moduleIsAlive() const {
  return passedChecksum() > 0;
}

uint8_t Gps::getValidSatellites() {
  return satellites.isValid() ? (uint8_t) satellites.value() : 0;
}

double Gps::getSpeed() {
  double theSpeed;
  if (speed.age() < 2000) {
    theSpeed = speed.kmph();
  } else {
    theSpeed = -1;
  }
  return theSpeed;
}

String Gps::getHdopAsString() {
  return String(hdop.hdop(), 2);
}

String Gps::getMessages() const {
  String theGpsMessage = "";
  for (const String& msg : mMessages) {
    theGpsMessage += "<br/>";
    theGpsMessage += msg;
  }
  return theGpsMessage;
}
