#ifndef OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H
#define OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H

#include <cstdint>

class VoltageMeter {
public:
  VoltageMeter();
  double read();

private:
  int readPlain();
  const int8_t BATTERY_PIN = 34;
  const int8_t REFERENCE_PIN = 35;
  int16_t lastReading = 4095;
};


#endif //OPENBIKESENSORFIRMWARE_VOLTAGEMETER_H
