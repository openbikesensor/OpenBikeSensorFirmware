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


void readGPSData() {
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
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
  //Offset in m
  int dLatM = random(-p.radius / 2.0, p.radius / 2.0);
  int dLongM = random(-p.radius / 2.0, p.radius / 2.0);
#ifdef dev
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
#ifdef dev
  Serial.print(F("dLat = "));
  Serial.println(String(dLat, 5));

  Serial.print(F("dLong = "));
  Serial.println(String(dLon, 5));
#endif
  //OffsetPosition, decimal degrees
  p.transformedLatitude = p.latitude + dLat * 180.0 / M_PI;
  p.transformedLongitude = p.longitude + dLon * 180.0 / M_PI ;
#ifdef dev
  Serial.print(F("p.transformedLatitude = "));
  Serial.println(String(p.transformedLatitude, 5));

  Serial.print(F("p.transformedLongitude = "));
  Serial.println(String(p.transformedLongitude, 5));
#endif
}
