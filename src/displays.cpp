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

#include "displays.h"

#include "fonts/fonts.h"

void SSD1306DisplayDevice::showNumConfirmed() {
  String val = String(confirmedMeasurements);
  if (confirmedMeasurements <= 9) {
    val = "0" + val;
  }
  this->prepareTextOnGrid(2, 4, val, MEDIUM_FONT);
  this->prepareTextOnGrid(3, 5, "conf");
}

void SSD1306DisplayDevice::showNumButtonPressed() {
  String val = String(numButtonReleased);
  if (numButtonReleased <= 9) {
    val = "0" + val;
  }
  this->prepareTextOnGrid(0, 4, val, MEDIUM_FONT);
  this->prepareTextOnGrid(1, 5, "press");
}

void SSD1306DisplayDevice::displaySimple(uint16_t value) {
  if (value == MAX_SENSOR_VALUE) {
    this->prepareTextOnGrid(0, 0,
                            "", HUGE_FONT, -7, -7);
  } else {
    this->prepareTextOnGrid(0, 0,
                            ObsUtils::to3DigitString(value), HUGE_FONT, -7, -7);
  }
  this->prepareTextOnGrid(3, 2, "cm", MEDIUM_FONT, -7, -5);
}

void SSD1306DisplayDevice::showValues(
  DistanceSensor* sensor1, DistanceSensor* sensor2, uint16_t minDistanceToConfirm,  int16_t batteryPercentage,
  int16_t TemperaturValue, int lastMeasurements, boolean insidePrivacyArea,
  double speed, uint8_t satellites) {

  handleHighlight();

  uint16_t value1 = sensor1->minDistance;
  if (minDistanceToConfirm != MAX_SENSOR_VALUE) {
    value1 = minDistanceToConfirm;
  }
  if (config.displayConfig & DisplaySimple) {
    displaySimple(value1);
  } else {
    if (config.displayConfig & DisplayLeft) {
      String loc1 = sensor1->sensorLocation;
      if (insidePrivacyArea) {
        loc1 = "(" + loc1 + ")";
      }
      this->prepareTextOnGrid(0, 0, loc1);
      if (value1 == MAX_SENSOR_VALUE) {
        this->prepareTextOnGrid(0, 1, "---", LARGE_FONT);
      } else {
        this->prepareTextOnGrid(0, 1,
                                ObsUtils::to3DigitString(value1), LARGE_FONT);
      }
    }
    // Show sensor2, when DisplayRight is configured
    if (config.displayConfig & DisplayRight) {
      uint16_t value2 = sensor2->distance;
      String loc2 = sensor2->sensorLocation;
      this->prepareTextOnGrid(3, 0, loc2);
      if (value2 == MAX_SENSOR_VALUE || value2 == 0) {
        this->prepareTextOnGrid(2, 1, "---", LARGE_FONT, 5, 0);
      } else {
        this->prepareTextOnGrid(2, 1,
                                ObsUtils::to3DigitString(value2), LARGE_FONT, 5, 0);
      }
    }
  } // NOT SIMPLE
  if (config.displayConfig & DisplayDistanceDetail) {
    const int bufSize = 64;
    char buffer[bufSize];
// #ifdef NERD_SENSOR_DISTANCE
    snprintf(buffer, bufSize - 1, "%03d|%02d|%03d", sensor1->rawDistance,
             lastMeasurements, sensor2->rawDistance);
// #endif
#ifdef NERD_HEAP
    snprintf(buffer, bufSize - 1, "%03d|%02d|%uk", sensor1.rawDistance,
             lastMeasurements, ESP.getFreeHeap() / 1024);
#endif
#ifdef NERD_VOLT
    snprintf(buffer, bufSize - 1, "%03d|%02d|%3.2fV", sensor1.rawDistance,
             lastMeasurements, voltageMeter->read());
#endif
#ifdef NERD_GPS
    snprintf(buffer, bufSize - 1, "%02ds|%s|%03u",
             satellites,
             gps.getHdopAsString().c_str(), gps.getLastNoiseLevel() );
#endif
    this->prepareTextOnGrid(0, 4, buffer, MEDIUM_FONT);
  } else if (config.displayConfig & DisplayNumConfirmed) {
    showNumButtonPressed();
    showNumConfirmed();
  } else {
    // Show GPS info, when DisplaySatellites is configured
    if (config.displayConfig & DisplaySatellites) {
      showGPS(satellites);
    }

    // Show velocity, when DisplayVelocity is configured
    if (config.displayConfig & DisplayVelocity) {
      showSpeed(speed);
    }
  }
  if (batteryPercentage >= -1) {
    showBatterieValue(batteryPercentage);
  }
  if (!(config.displayConfig & DisplaySimple)){
    if(BMP280_active == true)
      showTemperatureValue(TemperaturValue);
  }

  m_display->display();

}

void SSD1306DisplayDevice::showGPS(uint8_t sats) {
  String val = String(sats);
  if (sats <= 9) {
    val = "0" + val;
  }
  this->prepareTextOnGrid(2, 4, val, MEDIUM_FONT);
  this->prepareTextOnGrid(3, 5, "sats");
}

void SSD1306DisplayDevice::showBatterieValue(int16_t input_val){

    uint8_t x_offset_batterie_logo = 65;
    uint8_t y_offset_batterie_logo = 2;
    int8_t xlocation = 2;

     if ((config.displayConfig & DisplaySimple)){
       x_offset_batterie_logo += 32;
       xlocation += 1;
     }

    //cleanGridCellcomplete(3,0);

/*     if(input_val == -1){
      cleanBattery(x_offset_batterie_logo, y_offset_batterie_logo);
      m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo6);
      m_display->setColor(BLACK);
      this->showTextOnGrid(3, 0, " " + String(0) + "%", Dialog_plain_8,3,0);
      m_display->setColor(WHITE);
      m_display->display();
      this->showTextOnGrid(3, 0, "calc", Dialog_plain_8,6,0);
    }else{
      m_display->setColor(BLACK);
      this->showTextOnGrid(3, 0, "calc", Dialog_plain_8,6,0);
      m_display->setColor(WHITE);
      m_display->display();
    } */
		if(input_val >= 0){
			String val = String(input_val);
      //showLogo(true);
      this->showTextOnGrid(xlocation, 0, val + "%", TINY_FONT, 6, 0);
       //m_display[0]->drawXbm(192, 0, 8, 9, BatterieLogo1);

       if(input_val > 90){
         cleanBattery(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo1);
       }else if (input_val > 70)
       {
         cleanBattery(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo2);
       }else if (input_val> 50)
       {
         cleanBattery(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo3);
       }else if (input_val > 30)
       {
         cleanBattery(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo4);
       }else if (input_val >10)
       {
         cleanBattery(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo5);
       }else
       {
         cleanBattery(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo6);
       }

		}
    //m_display->display();
	}

void SSD1306DisplayDevice::showTemperatureValue(int16_t input_val){
    uint8_t x_offset_temp_logo = 30;
    uint8_t y_offset_temp_logo = 2;
    cleanTemperatur(x_offset_temp_logo,y_offset_temp_logo);
    m_display->drawXbm(x_offset_temp_logo, y_offset_temp_logo, 8, 9, TempLogo);
    String val = String(input_val);
    this->showTextOnGrid(1, 0, val + "°C", TINY_FONT);
}

void SSD1306DisplayDevice::showSpeed(double velocity) {
  const int bufSize = 4;
  char buffer[bufSize];
  if (velocity >= 0) {
    snprintf(buffer, bufSize - 1, "%02d", (int) velocity);
  } else {
    snprintf(buffer, bufSize - 1, "--");
  }
  this->prepareTextOnGrid(0, 4, buffer, MEDIUM_FONT);
  this->prepareTextOnGrid(1, 5, "km/h");
}

uint8_t SSD1306DisplayDevice::currentLine() const {
  return mCurrentLine;
}

uint8_t SSD1306DisplayDevice::newLine() {
  if (mCurrentLine >= 5) {
    scrollUp();
  }
  return ++mCurrentLine;
}

uint8_t SSD1306DisplayDevice::scrollUp() {
  for (uint8_t i = 0; i < 5; i++) {
    prepareTextOnGrid(2, i, obsDisplay->get_gridTextofCell(2, i + 1));
  }
  m_display->display();
  return mCurrentLine--;
}

uint8_t SSD1306DisplayDevice::startLine() {
  return mCurrentLine = 0;
}

void SSD1306DisplayDevice::highlight(uint32_t highlightTimeMillis) {
  mHighlightTill = millis() + highlightTimeMillis;
  if (!mHighlighted) {
    if (mInverted) {
      m_display->normalDisplay();
    } else {
      m_display->invertDisplay();
    }
    mHighlighted = true;
  }
}

void SSD1306DisplayDevice::handleHighlight() {
  if (mHighlighted && mHighlightTill < millis()) {
    if (mInverted) {
      m_display->invertDisplay();
    } else {
      m_display->normalDisplay();
    }
    mHighlighted = false;
  }
}
