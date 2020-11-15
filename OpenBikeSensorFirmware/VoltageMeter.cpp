#include "OpenBikeSensorFirmware.h"


VoltageMeter::VoltageMeter() {
  pinMode(BATTERY_PIN, INPUT);
  // Suggested range is 150 - 2450 mV, we are a bit above (4.22V * 2/3 == 2.81V)
  analogSetWidth(12 /*ADC_WIDTH_12Bit*/); // more than needed but default
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db); //0db attenuation on pin A18
}

double VoltageMeter::read() {
  const double readValue = readPlain();
  const double scale1v = 1240; // for default ADC_11db / 4095 == 3.3V
  return (readValue / scale1v) * 3 / 2;
}

int VoltageMeter::readPlain() {
  lastReading =
    (lastReading * 31 + analogRead(BATTERY_PIN)) / 32;
  return lastReading;
}