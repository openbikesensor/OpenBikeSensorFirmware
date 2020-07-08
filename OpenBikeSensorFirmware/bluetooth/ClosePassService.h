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

  void newSensorValues(const std::list<uint8_t>& leftValues, const std::list<uint8_t>& rightValues) override;
  void buttonPressed() override;

private:
  void writeToDistanceCharacteristic(const std::list<uint8_t>& leftValues, const std::list<uint8_t>& rightValues);
  void writeToEventCharacteristic(const String& event, std::list<uint8_t>* payload);
  void processValuesForDistanceChar(const std::list<uint8_t>& leftValues, const std::list<uint8_t>& rightValues);
  void processValuesForEventChar(const std::list<uint8_t>& leftValues, const std::list<uint8_t>& rightValues);

  BLEService *mService;
  BLECharacteristic *mDistanceCharacteristic;
  BLECharacteristic *mEventCharacteristic;

  int mDistancePhase = PHASE_PRE;
  CircularBuffer<short, 200> mDistanceBuffer; // maximum 10 seconds buffer

  CircularBuffer<short, 40> mEventBufferAvg2s;
};

#endif
