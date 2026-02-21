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

#ifndef OBS_DISPLAYS_H
#define OBS_DISPLAYS_H

#include <Arduino.h>
#include <U8g2lib.h>

#include "config.h"
#include "globals.h"
#include "gps.h"
#include "logo.h"
#include "fonts/fonts.h"
#include "sensor.h"
#include "variant.h"

#define BLACK 0
#define WHITE 1

extern const uint8_t BatterieLogo1[];
extern const uint8_t TempLogo[];

#define TINY_FONT OpenSans_Regular_6
#define SMALL_FONT OpenSans_Regular_7
#define MEDIUM_FONT OpenSans_Regular_14
#define LARGE_FONT OpenSans_Regular_24
#define HUGE_FONT OpenSans_Regular_33

// Forward declare classes to build (because there is a cyclic dependency between sensor.h and displays.h)
class HCSR04SensorInfo;

const int CLK = 33; //Set the CLK pin connection to the display
const int DIO = 25; //Set the DIO pin connection to the display
//Segments for line of dashes on display
//uint8_t segments[] = {64, 64, 64, 64};

extern bool BMP280_active;

class DisplayDevice {
  private:
    void handleHighlight();
    void displaySimple(uint16_t value);
    U8G2* m_display = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE); // original OBSClassic display
    String gridText[ 4 ][ 6 ];
    uint8_t mLastProgress = 255;
    uint8_t mCurrentLine = 0;
    bool mInverted = false;
    bool mFlipped = true;
    uint32_t mHighlightTill = 0;
    bool mHighlighted = false;

  public:
    DisplayDevice() {
      m_display->begin();
      m_display->setFlipMode(mFlipped);
      m_display->setContrast(74);
      m_display->setFontPosTop();
      m_display->setFontMode(1);
      m_display->setDrawColor(WHITE);
      m_display->updateDisplay();
    }

    ~DisplayDevice() {
      delete m_display;
    }

    uint8_t currentLine() const;
    uint8_t newLine();
    uint8_t scrollUp();
    uint8_t startLine();
    void highlight(uint32_t highlightTimeMillis = 500);

    //##############################################################
    // Basic display configuration
    //##############################################################

    void invert() {
      mInverted = true;
      setInversion(mInverted);
    }

    void normalDisplay() {
      mInverted = false;
      setInversion(mInverted);
    }

    void flipScreen() {
      mFlipped = !mFlipped;
      m_display->setFlipMode(mFlipped);
      m_display->updateDisplay();
    }

    void clear() {
      m_display->clear();
      this->cleanGrid();
    }

    //##############################################################
    // Handle Logo
    //##############################################################

    void showLogo(bool val) {
      m_display->drawXBM(0, 0, OBSLogo_width, OBSLogo_height, OBSLogo);
      m_display->updateDisplay();
    }

    //##############################################################
    // Draw the Grid
    //##############################################################

    void showGrid(bool val) {
      // Horizontal lines
      m_display->drawHLine(0, 2, 128);
      m_display->drawHLine(0, 12, 128);
      m_display->drawHLine(0, 22, 128);
      m_display->drawHLine(0, 32, 128);
      m_display->drawHLine(0, 42, 128);
      m_display->drawHLine(0, 52, 128);
      m_display->drawHLine(0, 62, 128);

      // Vertical lines
      m_display->drawVLine(32, 0, 64);
      m_display->drawVLine(64, 0, 64);
      m_display->drawVLine(96, 0, 64);
    }

    //##############################################################
    // Draw Text on Grid
    //##############################################################

    // ---------------------------------
    // | (0,0) | (1,0) | (2,0) | (3,0) |
    // ---------------------------------
    // | (0,1) | (1,1) | (2,1) | (3,1) |
    // ---------------------------------
    // | (0,2) | (1,2) | (2,2) | (3,2) |
    // ---------------------------------
    // | (0,3) | (1,3) | (2,3) | (3,3) |
    // ---------------------------------
    // | (0,4) | (1,4) | (2,4) | (3,4) |
    // ---------------------------------
    // | (0,5) | (1,5) | (2,5) | (3,5) |
    // ---------------------------------

    void showTextOnGrid(int16_t x, int16_t y, String text, const uint8_t* font = SMALL_FONT, int8_t offset_x_ = 0, int8_t offset_y_ = 0) {
      if (prepareTextOnGrid(x, y, text, font,offset_x_,offset_y_)) {
        m_display->updateDisplay();
      }
    }

    bool prepareTextOnGrid(
      int16_t x, int16_t y, String text, const uint8_t* font = SMALL_FONT , int8_t offset_x_ = 0, int8_t offset_y_ = 0) {
      bool changed = false;
      if (!text.equals(gridText[x][y])) {
        m_display->setFont(font);

        // Override the existing text with the inverted color
        this->cleanGridCell(x, y,offset_x_,offset_y_);

        // Write the new text
        gridText[x][y] = text;
        // 0 => 8 - (0*2) = 8
        // 1 => 8 - (1*2) = 6
        // 2 => 8 - (2*2) = 4
        // 3 => 8 - (3*2) = 2
        int x_offset = 8 - (x * 2);
        m_display->drawStr(x * 32 + x_offset + offset_x_, y * 10 + 1 + offset_y_, gridText[x][y].c_str());
        changed = true;
      }
      return changed;
    }

    void cleanGrid() {
      for (int x = 0; x <= 3; x++) {
        for (int y = 0; y <= 5; y++) {
          this->cleanGridCell(x, y);
          gridText[x][y] = "";
        }
      }
      m_display->updateDisplay();
    }

    // Override the existing WHITE text with BLACK
    void cleanGridCell(int16_t x, int16_t y) {
        this->cleanGridCell(x,y,0,0);
    }

    void cleanGridCell(int16_t x, int16_t y,int8_t offset_x_, int8_t offset_y_) {
      m_display->setDrawColor(BLACK);
      int x_offset = 8 - (x * 2);
      m_display->drawStr(x * 32 + x_offset + offset_x_, y * 10 + 1 + offset_y_, gridText[x][y].c_str());
      gridText[x][y] = "";
      m_display->setDrawColor(WHITE);
    }

    void cleanBattery(int16_t x, int16_t y){
      m_display->setDrawColor(BLACK);
      m_display->drawXBM(x, y, 8, 9, BatterieLogo1);
      m_display->setDrawColor(WHITE);
    }

    void cleanTemperatur(int16_t x, int16_t y){
      m_display->setDrawColor(BLACK);
      m_display->drawXBM(x, y, 8, 9, TempLogo);
      m_display->setDrawColor(WHITE);
    }

    void drawProgressBar(uint8_t y, uint32_t current, uint32_t target) {
      uint8_t progress;
      if (target > 0) {
        progress = (100 * current) / target;
      } else {
        progress = 100;
      }
      drawProgressBar(y, progress);
    }

    void drawProgressBar(uint8_t y, uint8_t progress) {
      clearTextLine(y);
      uint16_t rowOffset = y * 10 + 3;

      if (mLastProgress != progress) {
        m_display->drawFrame(12, rowOffset, 104, 8);
        if (progress != 0) {
          m_display->drawBox(14, rowOffset + 2, progress > 100 ? 100 : progress, 4);
        }
        m_display->updateDisplay();
        mLastProgress = progress;
      }
    }

    void drawWaitBar(uint8_t y, uint16_t tick) {
      uint16_t rowOffset = y * 10 + 3;
      m_display->drawFrame(12, rowOffset, 104, 8);
      m_display->setDrawColor(BLACK);
      m_display->drawBox(14, rowOffset + 2, 100, 4);
      m_display->setDrawColor(WHITE);

      int16_t pos = 45 + (45.0 * sin(tick / 10.0));
      m_display->drawBox(14 + pos, rowOffset + 2, 10, 4);
      m_display->updateDisplay();
    }

    void clearProgressBar(uint8_t y) {
      if (UINT8_MAX != mLastProgress) {
        clearTextLine(y);
        uint16_t rowOffset = y * 10 + 3;
        m_display->setDrawColor(BLACK);
        m_display->drawBox(12, rowOffset, 104, 8);
        m_display->setDrawColor(WHITE);
        m_display->display();
        mLastProgress = UINT8_MAX;
      }
    }

    void clearTextLine(uint8_t y) {
      for(int i = 0; i < 4; i++) {
        if (!gridText[i][y].isEmpty()) {
          prepareTextOnGrid(i, y, "");
        }
      }
    }

    String get_gridTextofCell(uint8_t x, uint8_t y){
      return gridText[x][y];
    }

    //##############################################################
    // Other
    //##############################################################

    // TODO: Move to the logic, since this is only the basic "display" class

    void showGPS(uint8_t sats);

    void showSpeed(double velocity);

    void showBatterieValue(int16_t input_val);

    void showTemperatureValue(int16_t input_val);

    void showNumConfirmed();

    void showNumButtonPressed();

    void showValues(
      uint16_t sensor1MinDistance, const char* sensor1Location, uint16_t sensor1RawDistance,
      uint16_t sensor2MinDistance, const char* sensor2Location, uint16_t sensor2RawDistance, uint16_t sensor2Distance,
      uint16_t minDistanceToConfirm, int16_t batteryPercentage, int16_t TemperaturValue,
      int lastMeasurements, boolean insidePrivacyArea, double speed, uint8_t satellites);


    //##############################################################
    // Internal methods
    //##############################################################

  private:
    void setInversion(bool enabled) {
      m_display->sendF("c", enabled ? 0xa7 : 0xa6);  // set inversion of the SSD1306 OLED
    }
};

#endif
