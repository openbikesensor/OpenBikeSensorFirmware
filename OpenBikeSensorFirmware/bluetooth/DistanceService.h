#ifndef DISTANCESERVICE_H
#define DISTANCESERVICE_H

#include "_IService.h"
#include "ble.h"

class DistanceService : public IService {
public:
    void setup() override;
    void newSensorValue(float value) override;
    void deviceConnected() override {};
    void deviceDisconnected() override {};
};

#endif
