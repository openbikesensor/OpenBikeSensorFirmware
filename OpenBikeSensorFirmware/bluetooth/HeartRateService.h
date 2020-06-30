#ifndef HEARTRATESERVICE_H
#define HEARTRATESERVICE_H

#include <CircularBuffer.h>
#include "_IService.h"
#include "ble.h"
#include "sensor.h"

class HeartRateService : public IService {
public:
    void setup() override;
    void newSensorValue(float value) override;
    void deviceConnected() override {};
    void deviceDisconnected() override {};

private:
    unsigned long mCollectionStartTime;
    CircularBuffer<float, 20> mDistances;
};

#endif
