#ifndef OBS_BLUETOOTH_CLOSEPASSSERVICE_H
#define OBS_BLUETOOTH_CLOSEPASSSERVICE_H

#include <CircularBuffer.h>
#include "_IBluetoothService.h"
#include "globals.h"

#define SERVICE_CLOSEPASS_UUID "1FE7FAF9-CE63-4236-0003-000000000000"
#define SERVICE_CLOSEPASS_CHAR_DISTANCE_UUID "1FE7FAF9-CE63-4236-0003-000000000001"
#define SERVICE_CLOSEPASS_CHAR_EVENT_UUID "1FE7FAF9-CE63-4236-0003-000000000002"

#define THRESHOLD_CLOSEPASS 200

#define PHASE_PRE          0x00
#define PHASE_TRANSMITTING 0x01

class ClosePassService : public IBluetoothService {
  public:
    void setup(BLEServer *pServer) override;
    bool shouldAdvertise() override;
    BLEService *getService() override;

    void newSensorValues(uint32_t millis, uint16_t leftValue, uint16_t rightValue) override;
    void newPassEvent(uint32_t millis, uint16_t leftValue, uint16_t rightValue) override;

  private:
    void writeToDistanceCharacteristic(uint32_t millis, uint16_t leftValue, uint16_t rightValue);
    void writeToEventCharacteristic(uint32_t millis, const String& event, std::list<uint16_t>* payload);
    void processValuesForDistanceChar(uint32_t millis, uint16_t leftValue, uint16_t rightValue);
    void processValuesForEventChar_Avg2s(uint32_t millis, uint16_t leftValue, uint16_t rightValue);
    void processValuesForEventChar_MinKalman(uint32_t millis, uint16_t leftValue, uint16_t rightValue);

    BLEService *mService;
    BLECharacteristic *mDistanceCharacteristic;
    BLECharacteristic *mEventCharacteristic;

    // Distance characteristic
    int mDistancePhase = PHASE_PRE;
    CircularBuffer<short, 200> mDistanceBuffer; // maximum 10 seconds buffer

    // Event characteristic - Avg2s
    CircularBuffer<short, 40> mEventAvg2s_Buffer;

    // Event characteristic - MinKalman
    float mEventMinKalman_ErrEstimate = 10;
    float mEventMinKalman_CurrentEstimate;
    float mEventMinKalman_LastEstimate;
    float mEventMinKalman_Gain;
    float mEventMinKalman_Min = UINT8_MAX;
    unsigned long mEventMinKalman_MinTimestamp = 0;
};

#endif
