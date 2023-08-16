#ifndef TOFMANAGER_H
#define TOFMANAGER_H

#include <Arduino.h>
#include <cstdint>
#include <atomic>

#include "utils/median.h"

typedef struct {
  uint8_t sensor_index; // the global index of the sensor this event occured on
  uint32_t trigger;     // the microseconds since the sensor was triggered
  uint32_t start;       // the microseconds since the sensor started the measurement
  uint32_t end;         // the microseconds since the sensor started the measurement
} LLMessage;

/* low-level TOF sensor state (private interface)
 *
 * This structs holds the low level state of each sensor channel.
 */
typedef struct {
  gpio_num_t triggerPin;
  gpio_num_t echoPin;

  /* timestamp when the trigger signal was sent in microseconds */
  std::atomic_uint_least64_t trigger{0};
  /* timestamp when the reading was started in microseconds */
  std::atomic_uint_least64_t start{0};
  /* timestamp when the reading was complete in microseconds
   * if end == 0 - a measurement is in progress */
  std::atomic_uint_least64_t end{0};

  volatile uint32_t interrupt_count = 0;
  uint32_t started_measurements = 0;
  uint32_t successful_measurements = 0;
  uint32_t lost_signals = 0;
} LLSensor;

/* low-level TOF sensor manager
 *
 * This class is responsible for all the low level plumbing: interrupts, timings, pins, etc
 * This class deals exclusively with microseconds. The companion high level interface is
 * HCSR04SensorManager.
 */
class TOFManager {
  public:
    void setupSensor(LLSensor*, uint8_t idx);
    void detachInterrupts();
    void attachInterrupts();

    void loop();
    void init();

    /*
     * startup()
     *
     * The only thing you ever need to call. This will spawn another task on a different CPU
     * and you won't have to interact with this class at all.
     *
     * The LLManager will send you LLMessages on the sensor_queue you pass in.
     */
    static void startupManager(QueueHandle_t sensor_queue);
  private:
    TOFManager(QueueHandle_t q, LLSensor* sensors) : m_sensors(sensors), sensor_queue(q) {}
    ~TOFManager();

    void startMeasurement(uint8_t sensorId);
    void disableMeasurement(uint8_t sensor_idx);
    void attachSensorInterrupt(uint8_t idx);
    boolean isSensorReady(uint8_t sensorId);
    boolean isMeasurementComplete(uint8_t sensor_idx);

    uint8_t primarySensor = 1;
    volatile uint8_t currentSensor = 0;

    void sendData(uint8_t sensor_idx);
    static void sensorTaskFunction(void *parameter);

    LLSensor* sensors(int i) {
      return &m_sensors[i];
    }
    LLSensor* m_sensors;
    QueueHandle_t sensor_queue;
};

#endif // TOFMANAGER_H
