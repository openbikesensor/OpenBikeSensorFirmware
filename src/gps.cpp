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

/* Most input from u-blox6_ReceiverDescrProtSpec_(GPS.G6-SW-10018)_Public.pdf */


/* Value is in the past (just went by at the time of writing). */
static const time_t PAST_TIME = 1606672131;

void Gps::begin() {
  mSerial.begin(9600, SERIAL_8N1); // , 16, 17);
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

void Gps::sendUbx(UBX_MSG ubxMsgId, const uint8_t payload[], uint16_t length) {
  sendUbx(static_cast<uint16_t>(ubxMsgId), payload, length);
}

void Gps::sendUbx(uint16_t ubxMsgId, const uint8_t payload[], uint16_t length) {

  // We copy over all in one go, assume to be more efficient than byte by byte to serial
  uint8_t buffer[length + 8];
  uint8_t chkA = 0;
  uint8_t chkB = 0;

  buffer[0] = 0xb5;
  buffer[1] = 0x62;
  buffer[2] = ubxMsgId & 0xFFU;
  chkA += buffer[2]; chkB += chkA;
  buffer[3] = ubxMsgId >> 8;
  chkA += buffer[3]; chkB += chkA;
  buffer[4] = length & 0xFFU;
  chkA += buffer[4]; chkB += chkA;
  buffer[5] = length >> 8;
  chkA += buffer[5]; chkB += chkA;
  for (int i = 0; i < length; i++) {
    const uint8_t data = payload[i];
    buffer[i + 6] = data;
    chkA += data; chkB += chkA;
  }
  buffer[6 + length] = chkA;
  buffer[7 + length] = chkB;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  String out = String(length) + ": ";
  for (int i = 0; i < sizeof(buffer); i++) {
    out += String(buffer[i], 16) + " ";
  }
  log_d("%s", out.c_str());
#endif
  mSerial.write(buffer, sizeof(buffer));
}

void Gps::configureGpsModule() {
  log_d("Sending config to GPS module.");
  setBaud();

  // TODO: Only if our setting was not found already (at 115200!?)
  // RESET TO DEFAULT OK?
  const uint8_t UBX_CFG_CFG[] = {
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFE, 0xFF, 0x00, 0x00, 0x03
  };
  sendAndWaitForAck(UBX_MSG::CFG_CFG, UBX_CFG_CFG, sizeof(UBX_CFG_CFG));
  setBaud();


  // switch of periodic gps messages that we do not use, leaves GGA and RMC on
  mSerial.print(F("$PUBX,40,GSV,0,0,0,0*59\r\n"));
  mSerial.print(F("$PUBX,40,GSA,0,0,0,0*4E\r\n"));
  mSerial.print(F("$PUBX,40,GLL,0,0,0,0*5C\r\n"));
  mSerial.print(F("$PUBX,40,VTG,0,0,0,0*5E\r\n"));
  mSerial.print(F("$PUBX,40,GGA,0,1,0,0*5B\r\n"));
  mSerial.print(F("$PUBX,40,RMC,0,1,0,0*46\r\n"));

/*  // Messages we want
  // Requesting $PUBX,00
  const uint8_t UBX_CFG_MSG[] = {
    0xf1, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  sendAndWaitForAck(UBX_MSG::CFG_MSG, UBX_CFG_MSG, sizeof(UBX_CFG_MSG));
*/

  // setting also the default values here - in the net you see reports of modules with wired defaults
  // "dynModel" - "3: pedestrian"
  // "static hold" - 80cm/s == 2.88 km/h
  // "staticHoldMaxDist" - 20m (not supported by our GPS receivers)
  const uint8_t UBX_CFG_NAV5[] = {
    0xff, 0xff, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x27, 0x00, 0x00, 0x05, 0x00, 0xfa, 0x00,
    0xfa, 0x00, 0x64, 0x00, 0x2c, 0x01, 0x50, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  sendAndWaitForAck(UBX_MSG::CFG_NAV5, UBX_CFG_NAV5, sizeof(UBX_CFG_NAV5));

  // INF messages via UBX only
  const uint8_t UBX_CFG_INF[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
    0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
  };
  sendAndWaitForAck(UBX_MSG::CFG_INF, UBX_CFG_INF, sizeof(UBX_CFG_INF));


  // "timepulse" - affecting the led, switching to 10ms pulse every 10sec, should be clearly differentiable
  // from the default (100ms each second)
  const uint8_t UBX_CFG_TP[] = {
    0x80, 0x96, 0x98, 0x00, 0x10, 0x27, 0x00, 0x00,
    0x01, 0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  sendAndWaitForAck(UBX_MSG::CFG_TP, UBX_CFG_TP, sizeof(UBX_CFG_TP));

  updateStatistics();
}


bool Gps::updateStatistics() {
  sendUbx(UBX_MSG::MON_HW);
  sendUbx(UBX_MSG::MON_VER);
  sendUbx(UBX_MSG::NAV_STATUS);
  return true;
}

bool Gps::setBaud() {
  const uint8_t UBX_CFG_PRT[] = {
    0x01, 0x00, 0x00, 0x00, 0xd0, 0x08, 0x00, 0x00,
    0x00, 0xc2, 0x01, 0x00, 0x03, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00
  };

  // switch to 115200 "blind"
  sendUbx(UBX_MSG::CFG_PRT, UBX_CFG_PRT, sizeof(UBX_CFG_PRT));
  mSerial.flush();
  mSerial.updateBaudRate(115200);

  // check if connected:
  bool connected = checkCommunication();
  if (!connected) {
    mSerial.updateBaudRate(9600);
    connected = checkCommunication();
  }
  if (!connected) {
    log_e("NO GPS????");
    delay(5000);
  }
  if (connected && mSerial.baudRate() / 10 != 115200 / 10) {
    log_e("Reported rate: %d", mSerial.baudRate());
    if (sendAndWaitForAck(UBX_MSG::CFG_PRT, UBX_CFG_PRT, sizeof(UBX_CFG_PRT))) {
      mSerial.updateBaudRate(115200);
    }
  }
  return checkCommunication();
}

bool Gps::checkCommunication() {
  const uint8_t UBX_CFG_PRT_POLL[] = {
    0x01
  };
  return sendAndWaitForAck(UBX_MSG::CFG_PRT, UBX_CFG_PRT_POLL, sizeof(UBX_CFG_PRT_POLL));
}

bool Gps::sendAndWaitForAck(UBX_MSG ubxMsgId, const uint8_t *buffer, size_t size) {
  const int tries = 10;
  const int timeoutMs = 500;

  bool result = false;
  for (int i = 0; i < tries; i++) {
    handle();

    const int start = millis();
    mNakReceived = false;
    mAckReceived = false;

    sendUbx(ubxMsgId, buffer, size);
    mSerial.flush();
    do {
      delay(20);
      handle();
    } while (!mAckReceived && !mNakReceived && millis() - start < timeoutMs);
    if (mAckReceived) {
      result = true;
      break;
    }
  }
  if (result) {
    log_e("Success in sending cfg. %d", size);
  } else {
    log_e("Failed to send cfg. %d NAK: %d ", size, mNakReceived);
  }
  return result;
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
    int data = mSerial.read();
    encodeUbx(data);
    if (encode(data)) {
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
    }
  }
  // send configuration multiple times
  if (gotGpsData && passedChecksum() > 1 && passedChecksum() < 110 && 0 == passedChecksum() % 11) {
 //   configureGpsModule();
  }
}

void Gps::addStatisticsMessage(String newMessage) {
  for (String& message : mMessages) {
    if (message == newMessage) {
      newMessage.clear();
      break;
    }
  }
  if (!newMessage.isEmpty()) {
    mMessages.push_back(newMessage);
    log_e("New GPS statistic message (%d): %s", mMessages.size(), newMessage.c_str());
  }
  newMessage.clear();
  if (mMessages.size() > 10) {
    mMessages.erase(mMessages.cbegin());
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

bool Gps::encodeUbx(uint8_t data) {
  // TODO: Re-parse buffer on failure
  // TODO: Detect delay while inside a message
  // TODO: Write wait for Ack(UBX_MES_ID))
  if (mReceiverState >= NMEA_START && mReceiverState < NMEA_CR
    && !validNmeaMessageChar(data)) {
      log_e("Invalid char in NMEA message, reset: %02x '%c' \nMSG: '%s'",
            data, data,
            String(mGpsBuffer.charData).substring(0, mGpsBufferBytePos).c_str());
      mReceiverState = GPS_NULL;
  }
  if (mReceiverState == GPS_NULL) {
    mGpsBufferBytePos = 0;
  }
  mGpsBuffer.u1Data[mGpsBufferBytePos++] = data;
  switch (mReceiverState) {
    case GPS_NULL:
      if (data == 0xB5) {
        mReceiverState = UBX_SYNC;
        mMessageStarted = millis(); // only of buffer was clear?
        mUbxChA = 0;
        mUbxChB = 0;
      } else if (data == '$') {
        mReceiverState = NMEA_START;
        mMessageStarted = millis(); // only of buffer was clear?
        mNmeaChk = 0;
      } else {
//        unexpected(data);
if (data != 0) {
  log_e("Unexpected GPS char in state null: %02x %c", data, data);
}
      }
      break;
    case UBX_SYNC:
      if (data == 0x62) {
        mReceiverState = UBX_SYNC1;
      } else {
//        unexpected(data);
        log_e("Unexpected GPS char in state ubx sync: %02x", data);
        mReceiverState = GPS_NULL;
      }
      mGpsBufferBytePos = 0;
      break;
    case UBX_SYNC1:
      mUbxChA += data;
      mUbxChB += mUbxChA;
      if (mGpsBufferBytePos == 4) {
        mReceiverState = UBX_PAYLOAD;
        mGpsPayloadLength = mGpsBuffer.u2Data[1];
        // TODO: Error handling!
        log_v("Expecting UBX Payload: %d bytes", mGpsPayloadLength);
      }
      break;
    case UBX_PAYLOAD:
      mUbxChA += data;
      mUbxChB += mUbxChA;
      if (mGpsBufferBytePos == 4 + mGpsPayloadLength) {
        mReceiverState = UBX_CHECKSUM;
      }
      break;
    case UBX_CHECKSUM:
      if (mUbxChA != data) {
        // ERROR!
        log_e("UBX CK_A error: %02x != %02x", mUbxChA, data);
        mReceiverState = GPS_NULL;
      } else {
        mReceiverState = UBX_CHECKSUM1;
      }
      break;
    case UBX_CHECKSUM1:
      if (mUbxChB != data) {
        // ERROR!
        log_e("UBX CK_B error: %02x != %02x", mUbxChB, data);
      } else {
        parseUbxMessage();
        mValidMessagesReceived++;
      }
      mReceiverState = GPS_NULL;
      break;
    case NMEA_START:
      if (mGpsBufferBytePos == 6) {
        log_v("Start of %c%c%c%c%c", mGpsBuffer.u1Data[1], mGpsBuffer.u1Data[2],
              mGpsBuffer.u1Data[3], mGpsBuffer.u1Data[4], mGpsBuffer.u1Data[5]);
        mReceiverState = NMEA_DATA;
      }
      mNmeaChk ^= data;
      break;
    case NMEA_DATA:
      if (data == '*') {
        mReceiverState = NMEA_CHECKSUM1;
        mGpsPayloadLength = mGpsBufferBytePos;
      } else {
        mNmeaChk ^= data;
      }
      break;
    case NMEA_CHECKSUM1:
      if (mNmeaChk >> 4U  == hexValue(data)) {
        mReceiverState = NMEA_CHECKSUM2;
      } else {
        // ERROR!
        log_e("NMEA chk 1st char error: %cX != %02x msg: %s",
              data, mNmeaChk,
              String(mGpsBuffer.charData).substring(0, mGpsBufferBytePos).c_str());
        mReceiverState = GPS_NULL;
      }
      break;
    case NMEA_CHECKSUM2:
      if ((mNmeaChk & 0x0F) == hexValue(data)) {
        parseNmeaMessage();
        mValidMessagesReceived++;
        mReceiverState = NMEA_CR;
      } else {
        // ERROR!
        log_e("NMEA chk 1st char error: %cX != %02x msg: %s",
              data, mNmeaChk,
              String(mGpsBuffer.charData).substring(0, mGpsBufferBytePos).c_str());
        mReceiverState = GPS_NULL;
      }
      break;
    case NMEA_CR:
      if (data == '\r') {
        mReceiverState = NMEA_LF;
      } else {
        log_e("Expected NMEA CR END but got %0x '%c'", data, data);
        mReceiverState = GPS_NULL;
      }
      break;
    case NMEA_LF:
      if (data != '\n') {
        log_e("Expected NMEA LF END but got %0x '%c'", data, data);
      }
      mReceiverState = GPS_NULL;
      break;
  }
  return true;
}

void Gps::parseUbxMessage() {
  switch (mGpsBuffer.u2Data[0]) {
    case (uint16_t) UBX_MSG::ACK_ACK:
      log_e("ACK-ACK %02x%02x", mGpsBuffer.u1Data[4], mGpsBuffer.u1Data[5]);
      mAckReceived = true;
      mNakReceived = false;
      break;
    case (uint16_t) UBX_MSG::ACK_NAK:
      log_e("ACK-NAK %02x%02x", mGpsBuffer.u1Data[4], mGpsBuffer.u1Data[5]);
      mAckReceived = false;
      mNakReceived = true;
      break;
    case (uint16_t) UBX_MSG::CFG_PRT:
      log_e("CFG-PRT Port: %d, Baud: %d %02x%02x",
            mGpsBuffer.u1Data[4], mGpsBuffer.u4Data[3],
            mGpsBuffer.u2Data[8], mGpsBuffer.u2Data[9]);
      break;
    case (uint16_t) UBX_MSG::MON_VER:
      addStatisticsMessage("swVersion: " + String(&mGpsBuffer.charData[4]));
      addStatisticsMessage("hwVersion: " + String(&mGpsBuffer.charData[34]));
      log_e("MON-VER SW Version: %s, HW Version %s, len %d",
            String(&mGpsBuffer.charData[4]).c_str(),
            String(&mGpsBuffer.charData[34]).c_str(),
            mGpsBuffer.u2Data[1]);
      break;
    case (uint16_t) UBX_MSG::MON_HW: {
        const uint8_t aStatus = mGpsBuffer.u1Data[24];
        String antennaStatus;
        switch (aStatus) {
          case 0:
            antennaStatus = String("INIT");
            break;
          case 1:
            antennaStatus = String("DONTKNOW");
            break;
          case 2:
            antennaStatus = String("OK");
            break;
          case 3:
            antennaStatus = String("SHORT");
            break;
          case 4:
            antennaStatus = String("OPEN");
            break;
          default:
            antennaStatus = String(aStatus);
        }
        log_e("MON-HW Antenna Status %s, noise level %d", antennaStatus.c_str(),
              mGpsBuffer.u2Data[10]);
        addStatisticsMessage("aStatus: " + String(antennaStatus));
      }
      break;
    case (uint16_t) UBX_MSG::NAV_STATUS:
      log_e("NAV-STATUS uptime: %d, timeToFix: %d, gpsFix: %02x",
            mGpsBuffer.u4Data[4], mGpsBuffer.u4Data[3], mGpsBuffer.u1Data[8]);
      mGpsUptime = mGpsBuffer.u4Data[4];
      if (mGpsBuffer.u4Data[3] != 0) {
        addStatisticsMessage("TimeToFix " + String(mGpsBuffer.u4Data[3]) + "ms");
      }
      break;
    case (uint16_t) UBX_MSG::INF_ERROR:
    case (uint16_t) UBX_MSG::INF_WARNING:
    case (uint16_t) UBX_MSG::INF_NOTICE:
    case (uint16_t) UBX_MSG::INF_TEST:
    case (uint16_t) UBX_MSG::INF_DEBUG:
      mGpsBuffer.u1Data[mGpsBufferBytePos - 2] = 0;
      log_e("INF %d message: %s",
            mGpsBuffer.u1Data[1], String(&mGpsBuffer.charData[4]).c_str());
      addStatisticsMessage(String(&mGpsBuffer.charData[4]).c_str());
      break;
    default:
      log_e("Got UBX_MESSAGE! Class: %0x, Id: %0x Len %04x", mGpsBuffer.u1Data[0],
            mGpsBuffer.u1Data[1], mGpsBuffer.u2Data[1]);
  }
}


bool Gps::validNmeaMessageChar(uint8_t chr) {
  return (isPrintable(chr) && chr != '$');
}

uint8_t Gps::hexValue(uint8_t data) {
  if (data >= '0' && data <='9') {
    return data - '0';
  }
  if (data >= '0' && data <='9') {
    return data - '0';
  }
  if (data >= 'A' && data <='F') {
    return data - 'A' + 10;
  }
  if (data >= 'a' && data <='f') {
    return data - 'a' + 10;
  }
  return 99; // ERROR
}

void Gps::parseNmeaMessage() {
  // TODO: Parse all in one message from UBX here:


  if (mGpsBuffer.charData[3] == 'T' && mGpsBuffer.charData[4] == 'X' &&
      mGpsBuffer.charData[5] == 'T') { // TXT
    mGpsBuffer.charData[mGpsBufferBytePos - 3] = 0;
    String msg = String(&mGpsBuffer.charData[16]);
    addStatisticsMessage(msg);
  } else if (mGpsBuffer.charData[3] == 'R' && mGpsBuffer.charData[4] == 'M' &&
             mGpsBuffer.charData[5] == 'C') { // RMC
    mGpsBuffer.charData[mGpsBufferBytePos - 3] = 0;
    log_e("??RMC Message '%s'", &mGpsBuffer.charData[0]);
  } else if (mGpsBuffer.charData[3] == 'G' && mGpsBuffer.charData[4] == 'G' &&
             mGpsBuffer.charData[5] == 'A') { // GGA
    mGpsBuffer.charData[mGpsBufferBytePos - 3] = 0;
    log_e("??GGA Message '%s'", &mGpsBuffer.charData[0]);
  } else if (mGpsBuffer.charData[1] == 'P' && mGpsBuffer.charData[2] == 'U' &&
    mGpsBuffer.charData[3] == 'B' && mGpsBuffer.charData[4] == 'X' &&
    mGpsBuffer.charData[5] == ',' && mGpsBuffer.charData[6] == '0' &&
    mGpsBuffer.charData[7] == '0' && mGpsBuffer.charData[8] == ',') { // 21.2 UBX,00 // 0xF1 0x00
    mGpsBuffer.charData[mGpsBufferBytePos - 3] = 0;
    log_e("PUBX00 Message '%s'", &mGpsBuffer.charData[9]);
  } else {
    log_e("Unparsed NMEA %c%c%c%c%c", mGpsBuffer.u1Data[1], mGpsBuffer.u1Data[2],
          mGpsBuffer.u1Data[3], mGpsBuffer.u1Data[4], mGpsBuffer.u1Data[5]);
  }
}
