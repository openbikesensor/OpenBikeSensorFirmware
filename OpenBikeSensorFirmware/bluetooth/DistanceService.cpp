#include "DistanceService.h"

void DistanceService::setup() {
}

void DistanceService::newSensorValue(float value) {
    String transmitValue = String(uint8_t(value)) + "," + String(millis());

    writeToDistanceService(transmitValue.c_str());
}
