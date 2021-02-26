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

void SSD1306DisplayDevice::showValues(
  HCSR04SensorInfo sensor1, HCSR04SensorInfo sensor2, uint16_t minDistanceToConfirm,  int16_t BatterieVolt,
  int16_t TemperaturValue, int lastMeasurements, boolean insidePrivacyArea,
  double speed, uint8_t satellites) {
  // Show sensor1, when DisplaySimple or DisplayLeft is configured
  if (config.displayConfig & DisplaySimple || config.displayConfig & DisplayLeft) {
    uint16_t value1 = sensor1.minDistance;
    if (minDistanceToConfirm != MAX_SENSOR_VALUE) {
      value1 = minDistanceToConfirm;
    }

    String loc1 = sensor1.sensorLocation;
    if (insidePrivacyArea) {
      loc1 = "(" + loc1 + ")";
    }

    // Do not show location, when DisplaySimple is configured
    if (!(config.displayConfig & DisplaySimple)) {
      this->prepareTextOnGrid(0, 0, loc1);
    }

    if (value1 == MAX_SENSOR_VALUE) {
      this->prepareTextOnGrid(0, 1, "---", LARGE_FONT);
    } else {
      String val = String(value1);
      if (value1 <= 9) {
        val = "00" + val;
      } else if (value1 >= 10 && value1 <= 99) {
        val = "0" + val;
      }

      this->prepareTextOnGrid(0, 1, val, LARGE_FONT);
    }

    if (config.displayConfig & DisplaySimple) {
      this->prepareTextOnGrid(2, 2, "cm", MEDIUM_FONT,5,0);
    }
  }

  if (!(config.displayConfig & DisplaySimple)) {

    // Show sensor2, when DisplayRight is configured
    if (config.displayConfig & DisplayRight) {
      uint16_t value2 = sensor2.distance;
      String loc2 = sensor2.sensorLocation;

      this->prepareTextOnGrid(3, 0, loc2);
      if (value2 == MAX_SENSOR_VALUE || value2 == 0) {
        this->prepareTextOnGrid(2, 1, "---", LARGE_FONT,5,0);
      } else {
        String val = String(value2);
        if (value2 <= 9) {
          val = "00" + val;
        } else if (value2 <= 99) {
          val = "0" + val;
        }
        this->prepareTextOnGrid(2, 1, val, LARGE_FONT,5,0);
      }
    }
    if (config.displayConfig & DisplayDistanceDetail) {
      const int bufSize = 64;
      char buffer[bufSize];
// #ifdef NERD_SENSOR_DISTANCE
      snprintf(buffer, bufSize - 1, "%03d|%02d|%03d", sensor1.rawDistance,
        lastMeasurements, sensor2.rawDistance);
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
  }
  // Show Batterie voltage
  #warning not checked if colliding with other stuff

    if(BatterieVolt >=-1)
        showBatterieValue((BatterieVolt));

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
      cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
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
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo1);
       }else if (input_val > 70)
       {
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo2);
       }else if (input_val> 50)
       {
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo3);
       }else if (input_val > 30)
       {
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo4);
       }else if (input_val >10)
       {
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo5);
       }else
       {
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
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
    prepareTextOnGrid(2, i, displayTest->get_gridTextofCell(2, i + 1));
  }
  m_display->display();
  return mCurrentLine--;
}

uint8_t SSD1306DisplayDevice::startLine() {
  return mCurrentLine = 0;
}
