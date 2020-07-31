#include "displays.h"

void SSD1306DisplayDevice::showNumConfirmed() {
  String val = String(confirmedMeasurements);
  if (confirmedMeasurements <= 9) {
    val = "0" + val;
  }
  this->showTextOnGrid(2, 4, val, Dialog_plain_20);
  this->showTextOnGrid(3, 5, "conf");
}

void SSD1306DisplayDevice::showNumButtonPressed() {
  String val = String(numButtonReleased);
  if (numButtonReleased <= 9) {
    val = "0" + val;
  }
  this->showTextOnGrid(0, 4, val, Dialog_plain_20);
  this->showTextOnGrid(1, 5, "press");
}

void SSD1306DisplayDevice::showValues(HCSR04SensorInfo sensor1, HCSR04SensorInfo sensor2, int16_t BatterieVolt) {
  // Show sensor1, when DisplaySimple or DisplayLeft is configured
  if (config.displayConfig & DisplaySimple || config.displayConfig & DisplayLeft) {
    uint8_t value1 = sensor1.minDistance;
    String loc1 = sensor1.sensorLocation;

    // Do not show location, when DisplaySimple is configured
    if (!(config.displayConfig & DisplaySimple)) {
      this->showTextOnGrid(0, 0, loc1);
    }

    if (value1 == 255) {
      this->showTextOnGrid(0, 1, "---", Dialog_plain_30);
    } else {
      String val = String(value1);
      if (value1 <= 9) {
        val = "00" + val;
      } else if (value1 >= 10 && value1 <= 99) {
        val = "0" + val;
      }

      this->showTextOnGrid(0, 1, val, Dialog_plain_30);

      // For DisplaySimple, show "cm"
      if (config.displayConfig & DisplaySimple) {
        this->showTextOnGrid(2, 2, "cm", Dialog_plain_20);
      }
    }
  }

  // Show "cm" when DisplaySimple is configured
  if (!(config.displayConfig & DisplaySimple)) {

    // Show sensor2, when DisplayRight is configured
    if (config.displayConfig & DisplayRight) {
      uint8_t value2 = sensor2.minDistance;
      String loc2 = sensor2.sensorLocation;

      this->showTextOnGrid(2, 0, loc2);
      if (value2 == 255) {
        this->showTextOnGrid(2, 1, "---", Dialog_plain_30);
      } else {
        String val = String(value2);
        if (value2 <= 9) {
          val = "00" + val;
        } else if (value2 >= 10 && value2 <= 99) {
          val = "0" + val;
        }
        this->showTextOnGrid(2, 1, val, Dialog_plain_30);
      }
    }
    if (config.displayConfig & DisplayNumConfirmed) {
      showNumButtonPressed();
      showNumConfirmed();
    } else {
      // Show GPS info, when DisplaySatelites is configured
      if (config.displayConfig & DisplaySatelites) showGPS();

      // Show velocity, when DisplayVelocity is configured
      if (config.displayConfig & DisplayVelocity) showVelocity(gps.speed.kmph());
	  // Show Batterie voltage
	  if(BatterieVolt >=0) showBatterieValue(BatterieVolt);
    }
  }

  m_display->display();
}

void SSD1306DisplayDevice::showGPS() {
  int sats = gps.satellites.value();
  String val = String(sats);
  if (sats <= 9) {
    val = "0" + val;
  }
  this->showTextOnGrid(2, 4, val, Dialog_plain_20);
  this->showTextOnGrid(3, 5, "sats");
}
