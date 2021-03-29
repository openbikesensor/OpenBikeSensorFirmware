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

#include "gpsrecord.h"

/* Clear all collected data */
void GpsRecord::reset() {
  mCollectTow = 0;
  mLatitude = 0;
  mLongitude = 0;
  mCourseOverGround = 0;
  mSatellitesUsed = 0;
  mFixStatus = GPS_FIX::NO_FIX;
  mFixStatusFlags = 0;
  mHdop = 0;
  mHeight = 0;
  mSpeed = 0;
  mPositionSet = false;
  mVelocitySet = false;
  mInfoSet = false;
  mHdopSet = false;
}

/* Store tow and related date time data. */
void GpsRecord::setTow(uint32_t tow) {
  mCollectTow = tow;
}

void GpsRecord::setPosition(int32_t lon, int32_t lat, int32_t height) {
  mLongitude = lon;
  mLatitude = lat;
  mHeight = height;
  mPositionSet = true;
}

void GpsRecord::setVelocity(uint32_t speedOverGround, int32_t heading) {
  mSpeed = speedOverGround;
  mCourseOverGround = heading;
  mVelocitySet = true;
}

void GpsRecord::setInfo(uint8_t satellitesInUse, GPS_FIX gpsFix, uint8_t flags) {
  mSatellitesUsed = satellitesInUse;
  mFixStatus = gpsFix;
  mFixStatusFlags = flags;
  mInfoSet = true;
}

void GpsRecord::setHdop(uint16_t hDop) {
  mHdop = hDop;
  mHdopSet = true;
}

bool GpsRecord::isAllSet() const {
  return mPositionSet && mVelocitySet && mInfoSet && mHdopSet;
}

String GpsRecord::getAltitudeMetersString() const {
  // 3 digits is problematic, 2 are enough any way.
  return toScaledString((mHeight + 5) / 10, 2);
}

String GpsRecord::getCourseString() const {
  return toScaledString(mCourseOverGround, 5);
}

String GpsRecord::getSpeedKmHString() const {
  return toScaledString((mSpeed * 60 * 60) / 10000, 1);
}

String GpsRecord::getHdopString() const {
  return toScaledString(mHdop, 2);
}


const int32_t GpsRecord::pow10[] = {
  1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
};

String GpsRecord::toScaledString(const int32_t value, const uint16_t scale) {
  if (value == 0) {
    return "";
  }
  char buffer[32];
  const int32_t scl = pow10[scale];
  snprintf(buffer, sizeof(buffer), "%d.%0*d", value / scl, scale, value % scl );
  return String(buffer);
}

bool GpsRecord::hasValidFix() const {
  return (mFixStatus == FIX_2D || mFixStatus == FIX_3D || mFixStatus == GPS_AND_DEAD_RECKONING)
    && mHdop != 9999 && mSatellitesUsed != 0;
}

String GpsRecord::getLatString() const {
  return toScaledString(mLatitude, 7);
}

String GpsRecord::getLongString() const {
  return toScaledString(mLongitude, 7);
}

double GpsRecord::getLatitude() const {
  return ((double) mLatitude) / 10000000.0;
}

double GpsRecord::getLongitude() const {
  return ((double) mLongitude) / 10000000.0;
}

uint8_t GpsRecord::getSatellitesUsed() const {
  return mSatellitesUsed;
}

GpsRecord::GPS_FIX GpsRecord::getFixStatus() const {
  return mFixStatus;
}

uint8_t GpsRecord::getFixStatusFlags() const {
  return mFixStatusFlags;
}
