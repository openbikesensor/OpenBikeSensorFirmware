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

#ifndef OPENBIKESENSORFIRMWARE_GPSRECORD_H
#define OPENBIKESENSORFIRMWARE_GPSRECORD_H


#include <Arduino.h>
#include <cstdint>

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
    String getAltitudeMetersString() const;
    String getCourseString() const;
    String getSpeedKmHString() const;
    String getHdopString() const;
    String getLatString() const;
    String getLongString() const;
    bool hasValidFix() const;
    double getLatitude() const;
    double getLongitude() const;
    uint8_t getSatellitesUsed() const;
    uint8_t getFixStatusFlags() const;
    GPS_FIX getFixStatus() const;
    uint32_t getTow() const;
    uint32_t getWeek() const;
    uint32_t getCreatedAtMillisTicks() const;

  protected:
    /* Clear all collected data */
    void reset(uint32_t tow, uint32_t gpsWeek, uint32_t createdAtMillisTicks);

    /* Store tow and related date time data. */
    void setWeek(uint32_t gpsWeek);

    void setPosition(int32_t lon, int32_t lat, int32_t height);

    void setVelocity(uint32_t speedOverGround, int32_t heading);

    void setInfo(uint8_t satellitesInUse, GPS_FIX gpsFix, uint8_t flags);

    void setHdop(uint16_t hDop);

    bool isAllSet() const;

  private:
    /* Just the GPS Millisecond time of Week of the record, to be able to
     * merge records together.
     */
    uint32_t mCollectTow = 0;
    uint32_t mCollectWeek = 0;
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
    bool mPositionSet = false;
    bool mVelocitySet = false;
    bool mInfoSet = false;
    bool mHdopSet = false;
    uint32_t mCreatedAtMillisTicks;
    static const int32_t pow10[10];
    static String toScaledString(int32_t value, uint16_t scale);
    String hwVer;
    String swVer;
};


#endif //OPENBIKESENSORFIRMWARE_GPSRECORD_H
