#include "OpenBikeSensorFirmware.h"


VoltageMeter::VoltageMeter() {
  pinMode(BATTERY_PIN, INPUT);
  // Suggested range is 150 - 2450 mV, we are a bit above (4.22V * 2/3 == 2.81V)
  analogSetWidth(12 /*ADC_WIDTH_12Bit*/); // more than needed but default
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db); //0db attenuation on pin A18
}

/* on MY OBS: Off at 2.84V stop charging at 4.00V
 * Todo:
 *  - better start value
 *  - add method for trend
 *  - persist max/min value?
 *  - decide reliable "alert" threshold
 */
double VoltageMeter::read() {
  const double readValue = readPlain();
  const double scale1v = 1240; // for default ADC_11db / 4095 == 3.3V
  return (readValue / scale1v) * 3 / 2;
}

int VoltageMeter::readPlain() {
  lastReading =
    (lastReading * 63 + analogRead(BATTERY_PIN)) / 64;
  return lastReading;
}