#ifndef ISERVICE_H
#define ISERVICE_H

#include <Arduino.h>

class IService {
public:
    virtual void setup() = 0;
    virtual void newSensorValue(float value) = 0;
    virtual void deviceConnected() = 0;
    virtual void deviceDisconnected() = 0;
};

#endif
