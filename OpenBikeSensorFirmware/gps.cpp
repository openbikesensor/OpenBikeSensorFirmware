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

HardwareSerial SerialGPS(1);
TinyGPSPlus gps;

time_t gpsTime() {
  struct tm t;
  t.tm_year = gps.date.year() - 1900;
  t.tm_mon = gps.date.month() - 1; // Month, 0 - jan
  t.tm_mday = gps.date.day();
  t.tm_hour = gps.time.hour();
  t.tm_min = gps.time.minute();
  t.tm_sec = gps.time.second();
  return mktime(&t);
}

time_t currentTime() {
  time_t result;
  if (gps.date.isValid() && gps.date.age() < 2000 && gps.date.year() > 2019) {
    result = gpsTime();
  } else {
    result = time(NULL);
  }
  return result;
}

void configureGpsModule() {
  // switch of periodic gps messages that we do not use, leaves GGA and RMC on
  SerialGPS.print(F("$PUBX,40,GSV,0,0,0,0*59\r\n"));
  SerialGPS.print(F("$PUBX,40,GSA,0,0,0,0*4E\r\n"));
  SerialGPS.print(F("$PUBX,40,GLL,0,0,0,0*5C\r\n"));
  SerialGPS.print(F("$PUBX,40,VTG,0,0,0,0*5E\r\n"));
  SerialGPS.print(F("$PUBX,40,GGA,0,1,0,0*5B\r\n"));
  SerialGPS.print(F("$PUBX,40,RMC,0,1,0,0*46\r\n"));

  // setting also the default values here - in the net you see reports of modules with wired defaults
  // "dynModel" - "3: pedestrian"
  // "static hold" - 80cm/s == 2.88 km/h
  // "staticHoldMaxDist" - 20m (not supported by our GPS receivers)
  const uint8_t UBX_CFG_NAV5[] =
    { 0xb5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xff, 0xff, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27,
      0x00, 0x00, 0x05, 0x00, 0xfa, 0x00, 0xfa, 0x00, 0x64, 0x00, 0x2c, 0x01, 0x50, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0x76 };
  SerialGPS.write(UBX_CFG_NAV5, sizeof(UBX_CFG_NAV5));

  // "timepulse" - affecting the led, switching to 10ms pulse every 10sec, should be clearly differentiable
  // from the default (100ms each second)
  const uint8_t UBX_CFG_TP[] =
    { 0xb5, 0x62, 0x06, 0x07, 0x14, 0x00, 0x80, 0x96, 0x98, 0x00, 0x10, 0x27, 0x00, 0x00, 0x01, 0x01,
      0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3a, 0xab };
  SerialGPS.write(UBX_CFG_TP, sizeof(UBX_CFG_TP));
}

void readGPSData() {
  while (SerialGPS.available() > 0) {
    if (gps.encode(SerialGPS.read())) {
      // set system time once every minute
      if (gps.time.isValid() && gps.time.second() == 0 && gps.time.isUpdated() && gps.time.age() < 100) {
        // We are in the precision of +/- 1 sec, ok for our purpose
        const time_t t = gpsTime();
        const struct timeval now = {.tv_sec = t};
        settimeofday(&now, nullptr);
      }
    }
  }

  // send configuration multiple times after switch on
  if (gps.passedChecksum() > 1 && gps.passedChecksum() < 100 && 0 == gps.passedChecksum() % 11) {
    configureGpsModule();
  }
}

bool isInsidePrivacyArea(TinyGPSLocation location) {
  // quite accurate haversine formula
  // consider using simplified flat earth calculation to save time
  for (size_t idx = 0; idx < config.numPrivacyAreas; ++idx)
  {
    double distance = haversine(location.lat(), location.lng(), config.privacyAreas[idx].transformedLatitude, config.privacyAreas[idx].transformedLongitude);
    if (distance < config.privacyAreas[idx].radius)
    {
      return true;
    }
  }
  return false;
}

double haversine(double lat1, double lon1, double lat2, double lon2)
{
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

void randomOffset(PrivacyArea &p) {
  randomSeed(analogRead(0));
  // Offset in degree and distance
  int offsetAngle = random(0, 360);
  int offsetDistance = random(p.radius / 10.0 , p.radius / 10.0 * 9.0);
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

void addNewPrivacyArea(double latitude, double longitude, int radius) {
  PrivacyArea newPrivacyArea;
  newPrivacyArea.latitude = latitude;
  newPrivacyArea.longitude = longitude;
  newPrivacyArea.radius = radius;
  randomOffset(newPrivacyArea);

  config.privacyAreas.push_back(newPrivacyArea);
  config.numPrivacyAreas = config.privacyAreas.size();
  Serial.println(F("Print config file..."));
  printConfig(config);
  Serial.println(F("Saving configuration..."));
  saveConfiguration(configFilename, config);
}
