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

void SSD1306DisplayDevice::showValues(HCSR04SensorInfo sensor1, HCSR04SensorInfo sensor2, int minDistanceToConfirm, int lastMeasurements) {
  // Show sensor1, when DisplaySimple or DisplayLeft is configured
  if (config.displayConfig & DisplaySimple || config.displayConfig & DisplayLeft) {
    uint16_t value1 = sensor1.minDistance;
    if (minDistanceToConfirm != MAX_SENSOR_VALUE) {
      value1 = minDistanceToConfirm;
    }

    String loc1 = sensor1.sensorLocation;

    // Do not show location, when DisplaySimple is configured
    if (!(config.displayConfig & DisplaySimple)) {
      this->showTextOnGrid(0, 0, loc1);
    }

    if (value1 == MAX_SENSOR_VALUE) {
      this->showTextOnGrid(0, 1, "---", Dialog_plain_30);
    } else {
      String val = String(value1);
      if (value1 <= 9) {
        val = "00" + val;
      } else if (value1 >= 10 && value1 <= 99) {
        val = "0" + val;
      }

      this->showTextOnGrid(0, 1, val, Dialog_plain_30);
    }

    if (config.displayConfig & DisplaySimple) {
      this->showTextOnGrid(2, 2, "cm", Dialog_plain_20);
    }
  }

  if (!(config.displayConfig & DisplaySimple)) {

    // Show sensor2, when DisplayRight is configured
    if (config.displayConfig & DisplayRight) {
      uint16_t value2 = sensor2.minDistance;
      String loc2 = sensor2.sensorLocation;

      this->showTextOnGrid(2, 0, loc2);
      if (value2 == MAX_SENSOR_VALUE) {
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
    if (config.displayConfig & DisplayDistanceDetail) {
      const int bufSize = 64;
      char buffer[bufSize];
//      snprintf(buffer, bufSize - 1, "%03d|%02d|%03d", sensor1.rawDistance,
//               lastMeasurements, sensor2.rawDistance);
      snprintf(buffer, bufSize - 1, "%03d|%02d|%uk", sensor1.rawDistance,
               lastMeasurements, ESP.getFreeHeap() / 1024);
      this->showTextOnGrid(0, 4, buffer, Dialog_plain_20);
    } else if (config.displayConfig & DisplayNumConfirmed) {
      showNumButtonPressed();
      showNumConfirmed();
    } else {
      // Show GPS info, when DisplaySatellites is configured
      if (config.displayConfig & DisplaySatelites) showGPS();

      // Show velocity, when DisplayVelocity is configured
      if (config.displayConfig & DisplayVelocity) {
        if (gps.speed.age() < 2000) {
          showVelocity(gps.speed.kmph());
        } else {
          showVelocity(-1);
        }
      }
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

void SSD1306DisplayDevice::showVelocity(double velocity)
{
  const int bufSize = 4;
  char buffer[bufSize];
  if (velocity >= 0) {
    snprintf(buffer, bufSize - 1, "%02d", (int) velocity);
  } else {
    snprintf(buffer, bufSize - 1, "--");
  }
  this->showTextOnGrid(0, 4, buffer, Dialog_plain_20);
  this->showTextOnGrid(1, 5, "km/h");
}
