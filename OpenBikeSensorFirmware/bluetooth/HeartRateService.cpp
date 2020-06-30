#include "HeartRateService.h"

const unsigned long measurementInterval = 1000;

void HeartRateService::setup() {
    mCollectionStartTime = millis();
}

void HeartRateService::newSensorValue(float value) {
    mDistances.push(value);

    if ((millis() - mCollectionStartTime) < measurementInterval) {
        return;
    }

    // Calculate average of measurement interval
    float distanceAvg = 0.0;
    using index_t = decltype(mDistances)::index_t;
    for (index_t i = 0; i < mDistances.size(); i++) {
        distanceAvg += mDistances[i] / (float) mDistances.size();
    }

    // Transmit average
    uint8_t buffer[2] = {
            0, // whether 8 or 16 bit
            uint8_t(distanceAvg)
    };
    writeToHeartRateService(buffer, 2);

    // Reset values
    mDistances.clear();
    mCollectionStartTime = millis();
}
