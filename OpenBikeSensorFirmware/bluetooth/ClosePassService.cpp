#include "ClosePassService.h"
#include <math.h>

uint16_t lastValue = UINT8_MAX;

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

void ClosePassService::newSensorValues(const uint16_t leftValue, const uint16_t rightValue) {
  processValuesForDistanceChar(leftValue, rightValue);
  processValuesForEventChar_Avg2s(leftValue, rightValue);
  processValuesForEventChar_MinKalman(leftValue, rightValue);
}

void ClosePassService::buttonPressed() {
  auto *payload = new std::list<uint16_t>();
  payload->push_back(lastValue);

  writeToEventCharacteristic("button", payload);
  delete payload;
}

void ClosePassService::writeToDistanceCharacteristic(const uint16_t leftValue, const uint16_t rightValue) {
  auto transmitValue = String(millis()) + ";";
  transmitValue += valueAsString(leftValue) + ";";
  transmitValue += valueAsString(rightValue);

  mDistanceCharacteristic->setValue(transmitValue.c_str());
  mDistanceCharacteristic->notify();
}

void ClosePassService::writeToEventCharacteristic(const String& event, std::list<uint16_t>* payload) {
  auto transmitValue = String(millis()) + ";";
  transmitValue += event + ";";
  if (payload) {
    transmitValue += joinList(*payload, ",");
  }

  mEventCharacteristic->setValue(transmitValue.c_str());
  mEventCharacteristic->notify();
}

void ClosePassService::processValuesForDistanceChar(const uint16_t leftValue, const uint16_t rightValue) {
  if (mDistancePhase == PHASE_PRE && leftValue < THRESHOLD_CLOSEPASS) {
    mDistancePhase = PHASE_TRANSMITTING;
  }

  if (mDistancePhase == PHASE_TRANSMITTING) {
    // Transmitting the value
    writeToDistanceCharacteristic(leftValue, rightValue);
    mDistanceBuffer.push(leftValue);

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

void ClosePassService::processValuesForEventChar_Avg2s(const uint16_t leftValue, const uint16_t rightValue) {
  mEventAvg2s_Buffer.push(leftValue);

  if (!mEventAvg2s_Buffer.isFull()) {
    return;
  }

  // Calculate average
  float distanceAvg = 0.0;
  uint16_t distanceMin = UINT8_MAX;
  const uint8_t size = mEventAvg2s_Buffer.size();
  using index_t = decltype(mEventAvg2s_Buffer)::index_t;
  for (index_t i = 0; i < size; i++) {
    auto v = (uint16_t) mEventAvg2s_Buffer[i];
    distanceAvg += (float) v / (float) size;
    distanceMin = min(distanceMin, v);
  }

  if (distanceAvg <= THRESHOLD_CLOSEPASS) {
    // Clear buffer
    mEventAvg2s_Buffer.clear();

    // Create payload
    auto *payload = new std::list<uint16_t>();
    payload->push_back((uint16_t) distanceAvg);
    payload->push_back(distanceMin);

    writeToEventCharacteristic("avg2s", payload);
    delete payload;
  }
}

void ClosePassService::processValuesForEventChar_MinKalman(const uint16_t leftValue, const uint16_t rightValue) {
  // TODO: test these parameters!!!
  float errMeasure = 10;
  float q = 0.01;

  mEventMinKalman_Gain = mEventMinKalman_ErrEstimate / (mEventMinKalman_ErrEstimate + errMeasure);
  mEventMinKalman_CurrentEstimate = mEventMinKalman_LastEstimate + mEventMinKalman_Gain * ((float) leftValue - mEventMinKalman_LastEstimate);
  mEventMinKalman_ErrEstimate = (1.0f - mEventMinKalman_Gain) * mEventMinKalman_ErrEstimate + fabsf(mEventMinKalman_LastEstimate - mEventMinKalman_CurrentEstimate) * q;
  mEventMinKalman_LastEstimate = mEventMinKalman_CurrentEstimate;

  if (mEventMinKalman_CurrentEstimate < mEventMinKalman_Min) {
    mEventMinKalman_MinTimestamp = millis();
    mEventMinKalman_Min = mEventMinKalman_CurrentEstimate;
  }

  // Below close pass threshold and last minimum value was more than one second ago
  if (mEventMinKalman_Min < THRESHOLD_CLOSEPASS && (millis() - mEventMinKalman_MinTimestamp) >= 1000) {
    auto *payload = new std::list<uint16_t>();
    payload->push_back((uint16_t) mEventMinKalman_Min);

    writeToEventCharacteristic("min_kalman", payload);
    mEventMinKalman_Min = UINT8_MAX;
    delete payload;
  }
}
