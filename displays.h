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


//#include <TM1637Display.h>
#include "SSD1306.h"
#include "font.h"
#include "logo.h"

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

  protected:
    virtual void init() = 0;
    virtual void clear() = 0;
    virtual void setMaxBrightness() = 0;




};
/*
  class TM1637DisplayDevice : public DisplayDevice
  {
  public:
    TM1637DisplayDevice() : DisplayDevice() {
      m_display = new TM1637Display(CLK, DIO);
      m_display->setBrightness(0x07);
    }
    ~TM1637DisplayDevice() {
      delete m_display;
    }
    void showValue(uint8_t value)
    {
      if (value == 255)
      {
        m_display->setSegments(segments);
      }
      else
      {
        m_display->showNumberDec(value);
      }
    }
  protected:
    void init()
    {
      m_display->setBrightness(0x07);
    }
    void clear() {

    }
    void setMaxBrightness() {

    }

  private:
    TM1637Display* m_display;

  };
*/
class SSD1306DisplayDevice : public DisplayDevice
{
  public:
    //SSD1306  displayOLED(0x3c, 21, 22);
    SSD1306DisplayDevice() : DisplayDevice() {
      m_display = new SSD1306(0x3c, 21, 22);
      m_display->init();
      m_display->setBrightness(255);
      m_display->setFont(Dialog_plain_50);
      m_display->drawXbm(0, 0, OBSLogo_width, OBSLogo_height, OBSLogo);
      m_display->display();
    }
    ~SSD1306DisplayDevice() {
      delete m_display;
    }
    void showValue(uint8_t value)
    {
      m_display->clear();
      if (value == 255)
      {
        m_display->drawString(0, 0, "---");
      }
      else
      {
        m_display->drawString(0, 0, String(value));
      }
      m_display->display();
    }
    void invert() {
      m_display->invertDisplay();
      m_display->display();
    }
    void normalDisplay() {
      m_display->normalDisplay();
      m_display->display();
    }
    void drawString(int16_t x, int16_t y, String text) {
      
    }
  protected:
    void init()
    {
      m_display->init();
      m_display->setBrightness(255);
      m_display->setFont(Dialog_plain_50);
    }
    void clear() {

    }
    void setMaxBrightness() {

    }
  private:
    SSD1306* m_display;
    uint8_t m_value;
};
