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
#include "config.h" // PrivacyArea
#include "displays.h"

class SSD1306DisplayDevice;

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


  private:
    HardwareSerial mSerial = HardwareSerial(2);
    TinyGPSCustom mTxtCount = TinyGPSCustom(*this, "GPTXT", 1);
    TinyGPSCustom mTxtSeq = TinyGPSCustom(*this, "GPTXT", 2);
    TinyGPSCustom mTxtSeverity = TinyGPSCustom(*this, "GPTXT", 3);
    TinyGPSCustom mTxtMessage = TinyGPSCustom(*this, "GPTXT", 4);
    String mMessage;
    std::vector<String> mMessages;

    time_t getGpsTime();
    void configureGpsModule();
    void handleNewTxtData();
    static double haversine(double lat1, double lon1, double lat2, double lon2);
    static void randomOffset(PrivacyArea &p);

};



#endif
