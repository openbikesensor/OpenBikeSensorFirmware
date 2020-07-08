#include "ClosePassService.h"

uint8_t lastValue = UINT8_MAX;

void ClosePassService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_CLOSEPASS_UUID);

  mDistanceCharacteristic = mService->createCharacteristic(SERVICE_CLOSEPASS_CHAR_DISTANCE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  mEventCharacteristic = mService->createCharacteristic(SERVICE_CLOSEPASS_CHAR_EVENT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
}

bool ClosePassService::shouldAdvertise() {
  return true;
}

BLEService* ClosePassService::getService() {
  return mService;
}

void ClosePassService::newSensorValues(const std::list<uint8_t>& leftValues, const std::list<uint8_t>& rightValues) {
  processValuesForDistanceChar(leftValues, rightValues);
  processValuesForEventChar(leftValues, rightValues);
}

void ClosePassService::buttonPressed() {
  auto *payload = new std::list<uint8_t>();
  payload->push_back(lastValue);

  writeToEventCharacteristic("button", payload);
}

void ClosePassService::writeToDistanceCharacteristic(const std::list<uint8_t>& leftValues, const std::list<uint8_t>& rightValues) {
    auto transmitValue = String(millis()) + ";";
    transmitValue += joinList(leftValues, ",") + ";";
    transmitValue += joinList(rightValues, ",");

    mDistanceCharacteristic->setValue(transmitValue.c_str());
    mDistanceCharacteristic->notify();
}

void ClosePassService::writeToEventCharacteristic(const String& event, std::list<uint8_t>* payload) {
  auto transmitValue = String(millis()) + ";";
  transmitValue += event + ";";
  if (payload) transmitValue += joinList(*payload, ",");

  mEventCharacteristic->setValue(transmitValue.c_str());
  mEventCharacteristic->notify();
}

void ClosePassService::processValuesForDistanceChar(const std::list<uint8_t>& leftValues, const std::list<uint8_t>& rightValues) {
  auto value = leftValues.front();

  if (mDistancePhase == PHASE_PRE && value < THRESHOLD_CLOSEPASS) {
    mDistancePhase = PHASE_TRANSMITTING;
  }

  if (mDistancePhase == PHASE_TRANSMITTING) {
    // Transmitting the value
    writeToDistanceCharacteristic(leftValues, rightValues);
    mDistanceBuffer.push(value);

    // Either the buffer is full (10s)
    if (mDistanceBuffer.isFull()) {
      mDistancePhase = PHASE_PRE;
    } else {
      int nValuesAboveThreshold = 0;
      const int offset = max(mDistanceBuffer.size() - 40, 0);
      for (decltype(mDistanceBuffer)::index_t i = offset; i < mDistanceBuffer.size(); i++) {
        nValuesAboveThreshold += mDistanceBuffer[i] >= THRESHOLD_CLOSEPASS ? 1 : 0;
      }

      // ... or the last 2s were above the threshold
      if ((float) nValuesAboveThreshold / (float) (mDistanceBuffer.size() - offset) >= 0.9f) {
        mDistancePhase = PHASE_PRE;
      }
    }
  }
}

void ClosePassService::processValuesForEventChar(const std::list<uint8_t>& leftValues, const std::list<uint8_t>& rightValues) {
  auto value = leftValues.front();
  lastValue = value;
  mEventBufferAvg2s.push(value);

  if (!mEventBufferAvg2s.isFull()) return;

  // Calculate average
  float distanceAvg = 0.0;
  uint8_t distanceMin = UINT8_MAX;
  const uint8_t size = mEventBufferAvg2s.size();
  using index_t = decltype(mEventBufferAvg2s)::index_t;
  for (index_t i = 0; i < size; i++) {
    uint8_t v = mEventBufferAvg2s[i];
    distanceAvg += (float) v / (float) size;
    distanceMin = min(distanceMin, v);
  }

  if (distanceAvg <= THRESHOLD_CLOSEPASS) {
    // Clear buffer
    mEventBufferAvg2s.clear();

    // Create payload
    auto *payload = new std::list<uint8_t>();
    payload->push_back((uint8_t) distanceAvg);
    payload->push_back(distanceMin);

    writeToEventCharacteristic("avg2s", payload);
  }
}
