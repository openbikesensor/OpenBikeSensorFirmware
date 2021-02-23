#ifndef OBS_BLUETOOTH_CLOSEPASSSERVICE_H
#define OBS_BLUETOOTH_CLOSEPASSSERVICE_H

#include "_IBluetoothService.h"
#include "globals.h"

#define SERVICE_CLOSEPASS_UUID "1FE7FAF9-CE63-4236-0003-000000000000"
#define SERVICE_CLOSEPASS_CHAR_EVENT_UUID "1FE7FAF9-CE63-4236-0003-000000000002"

class ClosePassService : public IBluetoothService {
  public:
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService *getService() override;

    void newPassEvent(uint32_t millis, uint16_t leftValue, uint16_t rightValue) override;

  private:
    BLEService *mService;
    BLECharacteristic *mEventCharacteristic;
};

#endif
