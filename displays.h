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
    virtual void invert() = 0;
    virtual void normalDisplay() = 0;
    //virtual void drawString(int16_t, int16_t, String) = 0;
    virtual void clear() = 0;
};


class SSD1306DisplayDevice : public DisplayDevice
{
  private:
    SSD1306* m_display;  
    String gridText[ 4 ][ 6 ];

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
    void showTextOnGrid(int16_t x, int16_t y, String text) {
      this->showTextOnGrid(x, y, text, ArialMT_Plain_10);
    }

    void showTextOnGrid(int16_t x, int16_t y, String text, const uint8_t* font) {
      if(!text.equals(gridText[x][y])) {
        m_display->setFont(font);

        // Override the existing text with the inverted color
        this->cleanGridCell(x, y);

        // Write the new text
        gridText[x][y] = text;
        m_display->drawString(x * 32 + 0, y*10 + 1, gridText[x][y]);
        m_display->display();
      }
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
      m_display->setColor(BLACK);
      m_display->drawString(x * 32 + 0, y * 10 + 1, gridText[x][y]);
      m_display->setColor(WHITE);
    }

    //##############################################################
    // Other
    //##############################################################

    // TODO: Move to the logic, since this is only the basic "display" class

    void showGPS()
    {
      if(config.displayConfig & DisplaySatelites)
      { 
        String satellitesString = String(gps.satellites.value()) + " / " + String(config.satsForFix) +" sats";
        this->showTextOnGrid(2, 5, satellitesString);
      }
    }
    void showVelocity(double velocity)
    {
      if(config.displayConfig & DisplayVelocity)
      { 
        String velotext = String(int(velocity)) + " km/h";
        this->showTextOnGrid(0, 5, velotext);
      }
    }

    void showValues(HCSR04SensorInfo sensor1, HCSR04SensorInfo sensor2)
    {

      uint8_t value1 = sensor1.minDistance;
      uint8_t value2 = sensor2.minDistance;
      String loc1 = sensor1.sensorLocation;
      String loc2 = sensor2.sensorLocation;
  
      // Show sensor1    
      if (value1 == 255)
      {
        this->showTextOnGrid(0, 0, loc1);
        this->showTextOnGrid(0, 1, "---", Dialog_plain_40);
      }
      else
      {
        this->showTextOnGrid(0, 0, loc1);
        this->showTextOnGrid(0, 1, String(value1), Dialog_plain_40);
      }

      // Show sensor2, when DisplayBoth is configured
      if(config.displayConfig & DisplayBoth)
      {
        if (value2 == 255)
        {
          this->showTextOnGrid(2, 0, loc2);
          this->showTextOnGrid(2, 1, "---", Dialog_plain_40);
        }
        else
        {
          this->showTextOnGrid(2, 0, loc2);
          this->showTextOnGrid(2, 1, String(value2), Dialog_plain_40);
        }
      }
      
      showGPS();
      showVelocity(gps.speed.kmph());
      m_display->display();
    }

};
