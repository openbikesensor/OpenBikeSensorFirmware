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
#ifndef OPENBIKESENSORFIRMWARE_GPSRECORD_H
#define OPENBIKESENSORFIRMWARE_GPSRECORD_H


#include <Arduino.h>
#include <cstdint>
#include <ctime>

class Gps;

class GpsRecord {
    enum GPS_FIX : uint8_t {
      NO_FIX = 0,
      DEAD_RECKONING_ONLY = 1,
      FIX_2D = 2,
      FIX_3D = 3,
      GPS_AND_DEAD_RECKONING = 4,
      TIME_ONLY = 5,
    };



    friend Gps;
  public:
    uint32_t mRetrievedAt; // millis counter!
    time_t mDateTime;
    /* deg, scale 1e-7 */
    int32_t mLongitude;
    /* deg, scale 1e-7 */
    int32_t mLatitude;
    int mSpeed; // * 100?
    int mCourseOverGround; // * 100
    int mHdop; // * 100
    /* millimeter */
    int32_t mHeight; // * 10?
    uint8_t mSatellitesUsed;
    GPS_FIX mFixStatus; //
    uint8_t mFixStatusFlags;
    static String posAsString(uint32_t pos);

    String getAltitudeMetersString() const;

    String getCourseString() const;

    String getSpeedKmHString() const;

    String getHdopString() const;
    String getLatString() const;
    String getLongString() const;

    bool hasValidFix() const;

    double getLatitude() const;

    double getLongitude() const;

  protected:
    /* Just the GPS Millisecond time of Week of the record, to be able to
     * merge records together.
     */
    uint32_t mCollectTow = 0;

    /* Clear all collected data */
    void reset();

    /* Store tow and related date time data. */
    void setTow(uint32_t tow);

    void setPosition(int32_t lon, int32_t lat, int32_t height);

    void setVelocity(uint32_t speedOverGround, int32_t heading);

    void setInfo(uint8_t satellitesInUse, GPS_FIX gpsFix, uint8_t flags);

    void setHdop(uint16_t hDop);

    bool isAllSet();

    bool mPositionSet = false;
    bool mVelocitySet = false;
    bool mInfoSet = false;
    bool mHdopSet = false;

    static uint32_t pow10[10];

    static String toScaledString(uint32_t value, uint16_t scale);
};


#endif //OPENBIKESENSORFIRMWARE_GPSRECORD_H
