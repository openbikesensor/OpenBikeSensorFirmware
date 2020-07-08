#include "ConnectionService.h"

// This code was developed to let the ESP be interactively paired with a client.
// Once a client tries to pair with this ESP, the user needs to cover the sensor
// for at least three seconds (see isSensorCovered()). Otherwise the connection
// is refused and the client device disconnected. This feature was once
// implemented using an LED to show the current pairing status (RadmesserS).
// For backwards compatibility with the SimRa Android App, this service always
// responds with the value "1" meaning that the device was successfully paired.

unsigned const long maxConnectionTimeMs = 15 * 1000L;
unsigned const long minCoveredTimeMs = 3 * 1000L;
const float maxCoveredDistanceCm = 4.0f;

void ConnectionService::setup(BLEServer *pServer) {
  mService = pServer->createService(SERVICE_CONNECTION_UUID);

  mCharacteristic = mService->createCharacteristic(SERVICE_CONNECTION_CHAR_CONNECTED_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  uint8_t value = 1;
  mCharacteristic->setValue(&value, 1);
}

bool ConnectionService::shouldAdvertise() {
  return true;
}

BLEService *ConnectionService::getService() {
  return mService;
}

void ConnectionService::newSensorValues(const std::list<uint8_t> &leftValues, const std::list<uint8_t> &rightValues) {
}

void ConnectionService::buttonPressed() {
}

//void ConnectionService::newSensorValue(short value) {
//    if (gConnectionStatus != CONN_CONNECTING) return;
//
//    mSensorValues.push(value);
//
//    if (isSensorCovered()) {
//        // Telling device that connection was established
//        writeToConnectionService(1);
//        gConnectionStatus = CONN_CONNECTED;
//        gConnectionLED.Reset().On().Forever().Update();
//    } else if (millis() - mConnectionStartTime >= maxConnectionTimeMs) {
//        // Disconnect device
//        gConnectionLED.Reset().Blink(200, 200).Repeat(4).Update();
//        delay(800);
//        disconnectDevice();
//    }
//}

//void ConnectionService::deviceConnected() {
//    gConnectionStatus = CONN_CONNECTING;
//    gConnectionLED.Reset().Blink(500, 500).Forever().Update();
//    mConnectionStartTime = millis();
//}

//void ConnectionService::deviceDisconnected() {
//    gConnectionStatus = CONN_READY;
//    writeToConnectionService(0);
//}

/**
 * The sensor has to covered for 3 seconds with less than 2 cm distance on average.
 * A running average using a circular buffer is used.
 */
bool ConnectionService::isSensorCovered() {
  float distanceAvg = 0.0;
  using index_t = decltype(mSensorValues)::index_t;
  for (index_t i = 0; i < mSensorValues.size(); i++) {
    distanceAvg += mSensorValues[i] / mSensorValues.size();
  }

  // Average distance wasn't close enough; aborting
  if (distanceAvg > maxCoveredDistanceCm) {
    mSensorValues.clear();
    mSensorCoveredStartTime = millis();
    return false;
  }

  // Sensor was covered long enough
  if ((millis() - mSensorCoveredStartTime) >= minCoveredTimeMs) {
    mSensorValues.clear();
    mSensorCoveredStartTime = millis();
    return true;
  }

  return false;
}
