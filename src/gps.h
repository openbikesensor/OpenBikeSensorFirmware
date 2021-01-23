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

#ifndef OBS_GPS_H
#define OBS_GPS_H

#include <Arduino.h>
#include <TinyGPS++.h> // http://arduiniana.org/libraries/tinygpsplus/
#include <HardwareSerial.h>
#include "utils/alpdata.h"
#include "config.h" // PrivacyArea
#include "displays.h"

class SSD1306DisplayDevice;

class GpsRecord {
    uint32_t mRetrievedAt; // millis counter!
    time_t mDateTime;
    long mLongitude;
    long mLatitude;
    int mSpeedKmH; // * 100?
    int mCourseOverGround; // * 100
    int mHdop; // * 100
    int mAltitudeCentiMeters; // * 10?
    uint8_t mSatellitesUsed;
    uint8_t mFixStatus; //? 0 = NoFix // 1 = Standard // 2 = diff GPS // 6 Estimated (DR) fix
};

class Gps : public TinyGPSPlus {
  public:
    enum class WaitFor {
      FIX_NO_WAIT = 0,
      FIX_TIME = -1,
      FIX_POS = -2,
    };
    void begin();
    /* read and process data from serial. */
    void handle();
    /* Returns the current time - GPS time if available, system time otherwise. */
    time_t currentTime();
    bool hasState(int state, SSD1306DisplayDevice *display);
    /* Returns true if valid communication with the gps module was possible. */
    bool moduleIsAlive() const;
    bool isInsidePrivacyArea();
    uint8_t getValidSatellites();
    void showWaitStatus(SSD1306DisplayDevice *display);
    /* Returns current speed, negative value means unknown speed. */
    double getSpeed();
    String getHdopAsString();
    String getMessages() const;
    static PrivacyArea newPrivacyArea(double latitude, double longitude, int radius);
    bool updateStatistics();
    uint32_t getUptime();


  private:
    static const int MAX_MESSAGE_LENGTH = 128 * 3; // ALP msgs up to 0x16A, might be more
    HardwareSerial mSerial = HardwareSerial(2);
    int16_t mGpsBufferBytePos = 0;
    enum GpsReceiverState {
      GPS_NULL,
      NMEA_START,
      NMEA_ADDRESS,
      NMEA_DATA,
      NMEA_CHECKSUM1,
      NMEA_CHECKSUM2,
      NMEA_CR,
      NMEA_LF,

      UBX_SYNC,  // 0xB5
      UBX_SYNC1, // 0x62
      UBX_CLASS,
      UBX_ID,
      UBX_LENGTH,
      UBX_PAYLOAD,
      UBX_CHECKSUM,
      UBX_CHECKSUM1

    };
    enum class UBX_MSG {
        // NAV 0x01
        NAV_STATUS = 0x0301,
        NAV_AOPSTATUS = 0x6001,

        // RXM 0x02

        // INF 0x04
        INF_ERROR = 0x0004,
        INF_WARNING = 0x0104,
        INF_NOTICE = 0x0204,
        INF_TEST = 0x0304,
        INF_DEBUG = 0x0404,

        // ACK 0x05
        ACK_ACK = 0x0105,
        ACK_NAK = 0x0005,

        // ACK 0x06
        CFG_PRT = 0x0006,
        CFG_MSG = 0x0106,
        CFG_INF = 0x0206,
        CFG_RST = 0x0406,
        CFG_TP = 0x0706,
        CFG_CFG = 0x0906,
        CFG_NAVX5 = 0x2306,
        CFG_NAV5 = 0x2406,

        // MON 0x0A
        MON_VER = 0x040a,
        MON_HW = 0x090a,

        // AID 0x0B
        AID_ALPSRV = 0x320B,
        AID_ALP = 0x500B,

        // TIM 0x0D
        // ESF 0x10

        // NMEA, special 0xF0
        NMEA_TXT = 0x41F0,
    };
    union GpsBuffer {
      UBX_MSG ubxMsgId;
      uint8_t u1Data[MAX_MESSAGE_LENGTH];
      uint16_t u2Data[MAX_MESSAGE_LENGTH / 2];
      uint32_t u4Data[MAX_MESSAGE_LENGTH / 4];
      int8_t i1Data[MAX_MESSAGE_LENGTH];
      int16_t i2Data[MAX_MESSAGE_LENGTH / 2];
      int32_t i4Data[MAX_MESSAGE_LENGTH / 4];
      float r4Data[MAX_MESSAGE_LENGTH / 4];
      double r8Data[MAX_MESSAGE_LENGTH / 8];
      char charData[MAX_MESSAGE_LENGTH];
      struct __attribute__((__packed__)) UBX_HEADER {
        uint8_t syncChar1;
        uint8_t syncChar2;
        uint16_t ubxMsgId;
        uint16_t length;
      } ubxHeader;
      struct __attribute__((__packed__)) {
        UBX_HEADER ubxHeader;
        uint16_t ubxMsgId;
      } ack;
      struct __attribute__((__packed__)) {
        UBX_HEADER ubxHeader;
        uint8_t portId;
        uint8_t reserved0;
        uint16_t txReady;
        uint32_t mode;
        uint32_t baudRate;
        uint16_t inProtoMask;
        uint16_t outProtoMask;
        uint16_t reserved4;
        uint16_t reserved5;
      } cfgPrt;
      struct __attribute__((__packed__)) {
        UBX_HEADER ubxHeader;
        uint16_t version;
        uint16_t mask1;
        uint32_t reserved0;
        uint8_t reserved1;
        uint8_t reserved2;
        uint8_t minSvs;
        uint8_t maxSvs;
        uint8_t minCno;
        uint8_t reserved5;
        uint8_t iniFix3d;
        uint8_t reserved6;
        uint8_t reserved7;
        uint8_t reserved8;
        uint16_t wknRollover;
        uint32_t reserved9;
        uint8_t reserved10;
        uint8_t reserved11;
        uint8_t usePpp;
        uint8_t useAop;
        uint8_t reserved12;
        uint8_t reserved13;
        uint16_t aopOrbMaxErr;
        uint32_t reserved3;
        uint32_t reserved4;
      } cfgNavx5;
      struct __attribute__((__packed__)) {
        UBX_HEADER ubxHeader;
        char swVersion[30];
        char hwVersion[10];
        char romVersion[30];
        char extension0[30]; // could be multiple...
        char extension1[30]; // could be multiple...
      } monVer;
      struct __attribute__((__packed__)) {
        UBX_HEADER ubxHeader;
        uint32_t pinSel;
        uint32_t pinBank;
        uint32_t pinDir;
        uint32_t pinVal;
        uint16_t noisePerMs;
        enum ANT_STATUS : uint8_t {
          INIT = 0,
          DONTKNOW = 1,
          OK = 2,
          SHORT = 3,
          OPEN = 4,
        } aStatus;
        enum ANT_POWER : uint8_t {
          OFF = 0,
          ON = 1,
          POWER_DONTKNOW = 2,
        } aPower;
        uint8_t flags;
        uint8_t reserved1;
        uint32_t usedMask;
        uint8_t vp[25];
        uint8_t jamInd;
        uint16_t reserved3;
        uint32_t pinIrq;
        uint32_t pullH;
        uint32_t pullL;
      } monHw;
      struct __attribute__((__packed__)) {
        UBX_HEADER ubxHeader;
        uint32_t iTow;
        enum GPS_FIX : uint8_t {
          NO_FIX = 0,
          DEAD_RECKONING_ONLY = 1,
          FIX_2D = 2,
          FIX_3D = 3,
          GPS_AND_DEAD_RECKONING = 4,
          TIME_ONLY = 5,
        } gpsFix;
        uint8_t flags;
        uint8_t fixStat;
        uint8_t flags2;
        uint32_t ttff; // Time to first fix (millisecond time tag)
        uint32_t msss; // Milliseconds since Startup / Reset
      } navStatus;
      struct __attribute__((__packed__)) {
        UBX_HEADER ubxHeader;
        uint32_t iTow;
        uint8_t config;
        uint8_t status;
        uint8_t reserved0;
        uint8_t reserved1;
        uint32_t avail;
        uint32_t reserved2;
        uint32_t reserved3;
      } navAopStatus;
      struct __attribute__((__packed__)) {
        UBX_HEADER ubxHeader;
        uint8_t idSize;
        uint8_t type;
        uint16_t ofs;
        uint16_t size;
        uint16_t fileId;
        uint16_t dataSize;
        uint8_t id1;
        uint8_t id2;
        uint32_t id3;
      } aidAlpsrvClientReq;
      struct __attribute__((__packed__)) {
        UBX_HEADER ubxHeader;
        uint32_t predTow;
        uint32_t predDur;
        int32_t age;
        uint16_t predWno;
        uint16_t almWno;
        uint32_t reserved1;
        uint8_t svs;
        uint8_t reserved2;
        uint16_t reserved3;
      } aidAlpStatus;
      struct __attribute__((__packed__)) {
        UBX_HEADER ubxHeader;
        char message[MAX_MESSAGE_LENGTH - 6];
      } inf;
    };
    GpsBuffer mGpsBuffer;
    GpsReceiverState mReceiverState = GPS_NULL;
    uint32_t mMessageStarted = 0;
    uint32_t mGpsUptime = 0;
    uint8_t mUbxChA = 0;
    uint8_t mUbxChB = 0;
    std::vector<String> mMessages;
    bool mAckReceived = false;
    bool mNakReceived = false;
    uint32_t mGpsPayloadLength;
    uint16_t mValidMessagesReceived = 0;
    uint8_t mNmeaChk;
    uint32_t mLastStatisticsRequest = 0;
    uint8_t hexValue(uint8_t data);
    uint16_t mLastNoiseLevel;
    AlpData mAlpData;

    time_t getGpsTime();
    void configureGpsModule();
    bool encodeUbx(uint8_t data);
    bool setBaud();
    bool checkCommunication();
    void addStatisticsMessage(String message);
    void sendUbx(uint16_t ubxMsgId, const uint8_t *payload = {}, uint16_t length = 0);
    void sendUbx(UBX_MSG ubxMsgId, const uint8_t *payload = {}, uint16_t length = 0);
    bool sendAndWaitForAck(UBX_MSG ubxMsgId, const uint8_t *buffer, size_t size);
    void parseUbxMessage();
    bool validNmeaMessageChar(uint8_t chr);
    void parseNmeaMessage();
    void sendUbxDirect();
    static double haversine(double lat1, double lon1, double lat2, double lon2);
    static void randomOffset(PrivacyArea &p);
    static time_t toTime(uint16_t week, uint32_t weekTime);
    static void logHexDump(const uint8_t *buffer, uint16_t length);
};



#endif
