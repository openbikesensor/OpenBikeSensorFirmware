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

#ifndef OBS_DISPLAYS_H
#define OBS_DISPLAYS_H

#include <Arduino.h>
#include <SSD1306.h>

#include "config.h"
#include "globals.h"
#include "gps.h"
#include "logo.h"
#include "sensor.h"


extern const uint8_t Ubuntu_Regular_Plain_8[];
extern const uint8_t ArialMT_Plain_10[]; // :(
extern const uint8_t Ubuntu_Regular_Plain_10[];
extern const uint8_t Ubuntu_Regular_Plain_22[];
extern const uint8_t Ubuntu_Regular_Plain_34[];
extern const uint8_t Ubuntu_Regular_Plain_54[];
extern const uint8_t BatterieLogo1[];
extern const uint8_t TempLogo[];

#define TINY_FONT Ubuntu_Regular_Plain_8
// this font is part of OLEDDisplay::OLEDDisplay :/
// #define SMALL_FONT ArialMT_Plain_10
#define SMALL_FONT Ubuntu_Regular_Plain_10
#define MEDIUM_FONT Ubuntu_Regular_Plain_22
#define LARGE_FONT Ubuntu_Regular_Plain_34
#define HUGE_FONT Ubuntu_Regular_Plain_54

// Forward declare classes to build (because there is a cyclic dependency between sensor.h and displays.h)
class HCSR04SensorInfo;

const int CLK = 33; //Set the CLK pin connection to the display
const int DIO = 25; //Set the DIO pin connection to the display
//Segments for line of dashes on display
//uint8_t segments[] = {64, 64, 64, 64};

extern bool BMP280_active;

class DisplayDevice {
  public:
    DisplayDevice() {}
    virtual ~DisplayDevice() {}
    virtual void invert() = 0;
    virtual void normalDisplay() = 0;
    //virtual void drawString(int16_t, int16_t, String) = 0;
    virtual void clear() = 0;
};


class SSD1306DisplayDevice : public DisplayDevice {
  private:
    SSD1306* m_display;
    String gridText[ 4 ][ 6 ];
    uint8_t mLastProgress = 255;
    uint8_t mCurrentLine = 0;

  public:
    SSD1306DisplayDevice() : DisplayDevice() {
      m_display = new SSD1306(0x3c, 21, 22); // ADDRESS, SDA, SCL
      m_display->init();
      m_display->setBrightness(255);
      m_display->setTextAlignment(TEXT_ALIGN_LEFT);
      m_display->display();
    }

    ~SSD1306DisplayDevice() {
      delete m_display;
    }

    uint8_t currentLine() const;
    uint8_t newLine();
    uint8_t scrollUp();
    uint8_t startLine();

    //##############################################################
    // Basic display configuration
    //##############################################################

    void invert() {
      m_display->invertDisplay();
      m_display->display();
    }

    void normalDisplay() {
      m_display->normalDisplay();
      m_display->display();
    }

    void flipScreen() {
      m_display->flipScreenVertically();
      m_display->display();
    }

    void clear() {
      m_display->clear();
      this->cleanGrid();
    }

    //##############################################################
    // Handle Logo
    //##############################################################

    void showLogo(bool val) {
      m_display->drawXbm(0, 0, OBSLogo_width, OBSLogo_height, OBSLogo);
      m_display->display();
    }

    //##############################################################
    // Draw the Grid
    //##############################################################

    void showGrid(bool val) {
      // Horizontal lines
      m_display->drawHorizontalLine(0, 2, 128);
      m_display->drawHorizontalLine(0, 12, 128);
      m_display->drawHorizontalLine(0, 22, 128);
      m_display->drawHorizontalLine(0, 32, 128);
      m_display->drawHorizontalLine(0, 42, 128);
      m_display->drawHorizontalLine(0, 52, 128);
      m_display->drawHorizontalLine(0, 62, 128);

      // Vertical lines
      m_display->drawVerticalLine(32, 0, 64);
      m_display->drawVerticalLine(64, 0, 64);
      m_display->drawVerticalLine(96, 0, 64);
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
        m_display->display();
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
        m_display->drawString(x * 32 + x_offset + offset_x_, y * 10 + 1 + offset_y_, gridText[x][y]);
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
      m_display->display();
    }

    // Override the existing WHITE text with BLACK
    void cleanGridCell(int16_t x, int16_t y) {
        this->cleanGridCell(x,y,0,0);
    }

    void cleanGridCell(int16_t x, int16_t y,int8_t offset_x_, int8_t offset_y_) {
      m_display->setColor(BLACK);
      int x_offset = 8 - (x * 2);
      m_display->drawString(x * 32 + x_offset + offset_x_, y * 10 + 1 + offset_y_, gridText[x][y]);
      m_display->setColor(WHITE);
    }

    void cleanBatterie(int16_t x, int16_t y){
      m_display->setColor(BLACK);
      m_display->drawXbm(x, y, 8, 9, BatterieLogo1);
      m_display->setColor(WHITE);
    }

    void cleanTemperatur(int16_t x, int16_t y){
      m_display->setColor(BLACK);
      m_display->drawXbm(x, y, 8, 9, TempLogo);
      m_display->setColor(WHITE);
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
        m_display->drawRect(12, rowOffset, 104, 8);
        if (progress != 0) {
          m_display->fillRect(14, rowOffset + 2, progress > 100 ? 100 : progress, 4);
        }
        m_display->display();
        mLastProgress = progress;
      }
    }

    void drawWaitBar(uint8_t y, uint16_t tick) {
      uint16_t rowOffset = y * 10 + 3;
      m_display->drawRect(12, rowOffset, 104, 8);
      m_display->setColor(BLACK);
      m_display->fillRect(14, rowOffset + 2, 100, 4);
      m_display->setColor(WHITE);

      int16_t pos = 45 + (45.0 * sin(tick / 10.0));
      m_display->fillRect(14 + pos, rowOffset + 2, 10, 4);
      m_display->display();
    }

    void clearProgressBar(uint8_t y) {
      clearTextLine(y);
      uint16_t rowOffset = y * 10 + 3;
      m_display->setColor(BLACK);
      m_display->fillRect(12, rowOffset, 104, 8);
      m_display->setColor(WHITE);
      m_display->display();
      mLastProgress = UINT8_MAX;
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
      HCSR04SensorInfo sensor1, HCSR04SensorInfo sensor2,
      uint16_t minDistanceToConfirm,int16_t BatterieVolt, int16_t TemperaturValue,
      int lastMeasurements, boolean insidePrivacyArea, double speed, uint8_t satellites);

};

#endif
