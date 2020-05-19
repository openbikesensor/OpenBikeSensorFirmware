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


#include "SSD1306.h"
#include "font.h"
#include "logo.h"


extern Config config;
const int CLK = 33; //Set the CLK pin connection to the display
const int DIO = 25; //Set the DIO pin connection to the display
//Segments for line of dashes on display
uint8_t segments[] = {64, 64, 64, 64};

class DisplayDevice
{
  public:
    DisplayDevice() {}
    virtual ~DisplayDevice() {}
    virtual void showValue(uint8_t) = 0;
    virtual void invert() = 0;
    virtual void normalDisplay() = 0;
    virtual void drawString(int16_t, int16_t, String) = 0;
    virtual void clear() = 0;

  protected:
    virtual void init() = 0;
};


class SSD1306DisplayDevice : public DisplayDevice
{
  private:
    bool isShowLogo;
    bool isShowGrid;
    String gridText[ 4 ][ 6 ];
  public:
    //SSD1306  displayOLED(0x3c, 21, 22);
    SSD1306DisplayDevice() : DisplayDevice() {
      isShowLogo = false;
      isShowGrid = false;
      
      m_display = new SSD1306(0x3c, 21, 22);
      m_display->init();
      m_display->setBrightness(255);
      m_display->setTextAlignment(TEXT_ALIGN_LEFT);
      m_display->setFont(ArialMT_Plain_10);
      m_display->display();      
    }
    ~SSD1306DisplayDevice() {
      delete m_display;
    }

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
    }    

    //##############################################################
    // rebuild the display, e.g.,
    // - show the logo
    // - show the grid (for debug)
    // - show the text on the grid
    //##############################################################

    void rebuild()
    {
      m_display->clear();
      if(isShowLogo) {
        this->drawLogo();
      }
      if(isShowGrid) {
        this->drawGrid();
      }
      this->drawTextOnGrid();
      m_display->display();
    }
    
    //##############################################################
    // Handle Logo
    //##############################################################
    
    void showLogo(bool val) {
      this->isShowLogo = val;
      this->rebuild();
    }
    void drawLogo() {
      m_display->drawXbm(0, 0, OBSLogo_width, OBSLogo_height, OBSLogo);
    }

    //##############################################################
    // Draw Grid
    //##############################################################

    void showGrid(bool val) {
      isShowGrid = val;
      this->rebuild();
    }
    void drawGrid() {
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
    void showTextOnGrid(int16_t x, int16_t y, String text) {
      gridText[x][y] = text;
      this->rebuild();
    }
    
    void drawTextOnGrid() {
      for (int x = 0; x <= 3; x++) {
        for (int y = 0; y <= 5; y++) {
          String xy = String(x) + "/" + String(y) + " = " + gridText[x][y];
          Serial.println(xy);
          this->drawString(x * 32 + 0, y*10 + 1, gridText[x][y]);
        }
      }
    }

    void drawString(int16_t x, int16_t y, String text) {
      m_display->drawString(x, y, text);
      m_display->display();
    }

    //##############################################################
    // Other
    //##############################################################
    


    void showGPS()
    {
      if(config.displayConfig & DisplaySatelites)
      { 
        m_display->setFont(ArialMT_Plain_10);
        String satellitesString = String(gps.satellites.value()) + " sats";
        m_display->drawString(64, 48, satellitesString);
      }
    }
    void showVelocity(double velocity)
    {
      if(config.displayConfig & DisplayVelocity)
      { 
        m_display->setFont(ArialMT_Plain_10);
        String velotext = String(int(velocity)) + " km/h";
        m_display->drawString(0, 48, velotext);
      }

    }
    
    void showValue(uint8_t minDistanceToConfirm)
    {
      // not used any more
    }
    void showValues(uint8_t minDistanceToConfirm, uint8_t value1,uint8_t value2)
    {
      m_display->clear();
      m_display->setFont(Dialog_plain_40);
      
      if(!(config.displayConfig&DisplayBoth))
      {
        value1 = minDistanceToConfirm;
      }
      if (value1 == 255)
      {
        m_display->drawString(0, 0, "---");
      }
      else
      {
        m_display->drawString(0, 0, String(value1));
      }
      if(config.displayConfig&DisplayBoth)
      {
        if (value2 == 255)
        {
          m_display->drawString(64, 0, "---");
        }
        else
        {
          m_display->drawString(64, 0, String(value2));
        }
      }
      
      showGPS();
      showVelocity(gps.speed.kmph());
      m_display->display();
    }




    


    
  protected:
    void init()
    {
      m_display->init();
      m_display->setBrightness(255);
    }
  private:
    SSD1306* m_display;
    uint8_t m_value;
};
