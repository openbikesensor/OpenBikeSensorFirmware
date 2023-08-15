#include "tofmanager.h"
#include "sensor.h"

/* Value of HCSR04SensorInfo::end during an ongoing measurement. */
#define MEASUREMENT_IN_PROGRESS 0

/* To avoid that we receive a echo from a former measurement, we do not
 * start a new measurement within the given time from the start of the
 * former measurement of the opposite sensor.
 * High values can lead to the situation that we only poll the
 * primary sensor for a while!?
 */
#define SENSOR_QUIET_PERIOD_AFTER_OPPOSITE_START_MICRO_SEC (30 * 1000)

/* The last end (echo goes to low) of a measurement must be this far
 * away before a new measurement is started.
 */
#define SENSOR_QUIET_PERIOD_AFTER_END_MICRO_SEC (10 * 1000)

/* The last start of a new measurement must be as long ago as given here
    away, until we start a new measurement.
   - With 35ms I could get stable readings down to 19cm (new sensor)
   - With 30ms I could get stable readings down to 25/35cm only (new sensor)
   - It looked fine with the old sensor for all values
 */
#define SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC (35 * 1000)

/* This time is maximum echo pin high time if the sensor gets no response
 * HC-SR04: observed 71ms, docu 60ms
 * JSN-SR04T: observed 58ms
 */
#define MAX_TIMEOUT_MICRO_SEC 75000

/* The core to run the RT component on */
#define RT_CORE 0

/* Delay to call when we're waiting for work */
#define TICK_DELAY 1

/* Delay in ticks for when we run out of memory */
#define OOM_TICK_DELAY 10000

/* Enable init code for running alone */
#define ALONE_ON_CORE 1

/* default interrupt flags */
#define OBS_INTR_FLAG 0

/* Some calculations:
 *
 * Assumption:
 *  Small car length: 4m (average 2017 4,4m, really smallest is 2.3m )
 *  Speed difference: 100km/h = 27.8m/s
 *  Max interesting distance: MAX_DISTANCE_MEASURED_CM = 250cm
 *
 * Measured:
 *  MAX_TIMEOUT_MICRO_SEC == ~ 75_000 micro seconds, if the sensor gets no echo
 *
 * Time of the overtake process:
 *  4m / 27.8m/s = 0.143 sec
 *
 * We need to have time for 3 measures to get the car
 *
 * 0.148 sec / 3 measures = 48 milli seconds pre measure = 48_000 micro_sec
 *
 * -> If we wait till MAX_TIMEOUT_MICRO_SEC (75ms) because the right
 *    sensor has no measure we are to slow. (3x75 = 225ms > 143ms)
 *
 *    If we only waif tor the primary (left) sensor we are good again,
 *    and so trigger one sensor while the other is still in "timeout-state"
 *    we save some time. Assuming 1st measure is just before the car appears
 *    besides the sensor:
 *
 *    75ms open sensor + 2x SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC (35ms) = 145ms which is
 *    close to the needed 148ms
 */

/* Send echo trigger to the selected sensor and prepare measurement data
 * structures.
 */
void TOFManager::startMeasurement(uint8_t sensorId) {
  LLSensor* sensor = sensors(sensorId);

  sensor->started_measurements += 1;

  sensor->end = MEASUREMENT_IN_PROGRESS; // will be updated with LOW signal
  unsigned long now = esp_timer_get_time();

  sensor->trigger = 0; // will be updated with HIGH signal
  sensor->start = now;

  gpio_set_level(sensor->triggerPin, 1);
  // 10us are specified but some sensors are more stable with 20us according
  // to internet reports
  usleep(20);
  gpio_set_level(sensor->triggerPin, 0);

  attachSensorInterrupt(sensorId);
}

/* Checks if the given sensor is ready for a new measurement cycle.
 */
boolean TOFManager::isSensorReady(uint8_t sensorId) {
  LLSensor* sensor = sensors(sensorId);
  boolean ready = false;
  const uint32_t now = micros();
  const uint32_t start = sensor->start;
  const uint32_t end = sensor->end;
  if (end != MEASUREMENT_IN_PROGRESS) { // no measurement in flight or just finished
    const uint32_t startOther = sensors(1 - sensorId)->start;
    if (   (HCSR04SensorManager::microsBetween(now, end) > SENSOR_QUIET_PERIOD_AFTER_END_MICRO_SEC)
        && (HCSR04SensorManager::microsBetween(now, start) > SENSOR_QUIET_PERIOD_AFTER_START_MICRO_SEC)
        && (HCSR04SensorManager::microsBetween(now, startOther) > SENSOR_QUIET_PERIOD_AFTER_OPPOSITE_START_MICRO_SEC)) {
      ready = true;
    sensors(sensorId)->successful_measurements += 1;
    }
  } else if (HCSR04SensorManager::microsBetween(now, start) > MAX_TIMEOUT_MICRO_SEC) {
    // signal or interrupt was lost altogether this is an error,
    // should we raise a  it?? Now pretend the sensor is ready, hope it helps to give it a trigger.
    ready = true;
    sensors(sensorId)->successful_measurements += 1;
  }
  return ready;
}

boolean TOFManager::isMeasurementComplete(uint8_t sensor_idx) {
  if (sensors(sensor_idx)->end != MEASUREMENT_IN_PROGRESS) {
    return true;
  }
  uint32_t now = esp_timer_get_time();
  if (sensors(sensor_idx)->start + MAX_TIMEOUT_MICRO_SEC < now) {
    return true;
  }

  return false;
}

void TOFManager::startupManager(QueueHandle_t queue) {
  esp_log_level_set("tofmanager", ESP_LOG_INFO);
  ESP_LOGI("tofmanager","Currently on core %i", xPortGetCoreID());
  ESP_LOGI("tofmanager","Switching to core %i", RT_CORE);

  xTaskCreatePinnedToCore(
    TOFManager::sensorTaskFunction,          /* Task function. */
    "TOFManager",        /* String with name of task. */
    16 * 1024,            /* Stack size in bytes. */
    queue,             /* Parameter passed as input of the task */
    1,                /* Priority of the task. */
    NULL,             /* Task handle. */
    RT_CORE           /* the core to run on */
  );
}

void TOFManager::sensorTaskFunction(void *parameter) {
  esp_log_level_set("tofmanager", ESP_LOG_INFO);
  isr_log_i("tofmanager", "Hello from core %i!", xPortGetCoreID());

  LLSensor* sensors = new LLSensor[2];

  TOFManager* sensorManager = new TOFManager((QueueHandle_t)parameter, sensors);

  if (sensors == 0 || sensorManager == 0) {
    ESP_LOGE("tofmanager","Error, apparently we've run out of memory (sensors=%x, sensorManager=%x).",
      sensors, sensorManager);
  }

  sensorManager->init();
  sensorManager->loop();
}

void TOFManager::init() {
  LLSensor* sensorManaged1 = sensors(0);
  sensorManaged1->triggerPin = GPIO_NUM_25;
  sensorManaged1->echoPin = GPIO_NUM_26;
  setupSensor(sensorManaged1, 0);

  LLSensor* sensorManaged2 = sensors(1);
  sensorManaged2->triggerPin = GPIO_NUM_15;
  sensorManaged2->echoPin = GPIO_NUM_4;
  setupSensor(sensorManaged2, 1);

#if ALONE_ON_CORE
  esp_timer_init();
  gpio_install_isr_service(OBS_INTR_FLAG);
#endif
  ESP_LOGI("tofmanager", "TOFManager init complete");
}

void TOFManager::sendData(uint8_t sensor_idx) {
  LLSensor* sensor = sensors(sensor_idx);

  LLMessage message = {0};

  message.sensor_index = sensor_idx;
  message.trigger = sensor->trigger;
  message.start = sensor->start;
  message.end = sensor->end;
  xQueueSendToBack(sensor_queue, &message, portMAX_DELAY);
  //delete message;
}

void TOFManager::loop() {
  ESP_LOGI("tofmanager","Entering TOFManager::loop");
  currentSensor = primarySensor;
  startMeasurement(primarySensor);

  int statLog = 0;

  while ( true ) {
    if (statLog > 2048) {
      ESP_LOGI("tofmanager", "statlog: sensor1->irq_cnt=%u, sensor2->irq_cnt=%u",
        sensors(0)->interrupt_count, sensors(1)->interrupt_count);
      ESP_LOGI("tofmanager", "statlog: success=(%u, %u), started=(%u,%u)",
        sensors(0)->successful_measurements,
        sensors(1)->successful_measurements,
        sensors(0)->started_measurements,
        sensors(1)->started_measurements);
      statLog = 0;
    }
    statLog += 1;

    // wait for sensor
    if (!isMeasurementComplete(currentSensor)) {
      vTaskDelay(TICK_DELAY);
    } else {
      sendData(currentSensor);

      // switch to next sensor
      if (currentSensor == primarySensor && isSensorReady(1 - primarySensor)) {
        disableMeasurements();
        currentSensor = 1 - primarySensor;
        startMeasurement(currentSensor);
      } else if (isSensorReady(primarySensor)) {
        disableMeasurements();
        currentSensor = primarySensor;
        startMeasurement(primarySensor);
      }
    }
  }
}

static void IRAM_ATTR sensorInterrupt(void *isrParam) {
  LLSensor* sensor = (LLSensor*) isrParam;
  // since the measurement of start and stop use the same interrupt
  // mechanism we should see a similar delay.
  if (sensor->end == MEASUREMENT_IN_PROGRESS) {
    sensor->interrupt_count += 1;

    if (sensor->trigger == 0) {
      sensor->trigger = esp_timer_get_time();
    } else {
      sensor->end = esp_timer_get_time();
    }
  }
}

void TOFManager::setupSensor(LLSensor* sensor, uint8_t idx) {
  gpio_config_t io_conf_trigger = {};
  io_conf_trigger.intr_type = GPIO_INTR_DISABLE;
  io_conf_trigger.mode = GPIO_MODE_OUTPUT;
  io_conf_trigger.pin_bit_mask = 1ULL << sensor->triggerPin;
  io_conf_trigger.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf_trigger.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf_trigger);
  // gpio_pad_select_gpio(sensor->triggerPin);
  // gpio_set_direction(sensor->triggerPin, GPIO_MODE_OUTPUT);
  // gpio_pulldown_dis(sensor->triggerPin);
  // gpio_pullup_dis(sensor->triggerPin);

  gpio_config_t io_conf_echo = {};
  io_conf_echo.intr_type = GPIO_INTR_ANYEDGE;
  io_conf_echo.mode = GPIO_MODE_INPUT;
  io_conf_echo.pin_bit_mask = 1ULL << sensor->echoPin;
  io_conf_echo.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf_echo.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf_echo);

  // gpio_pad_select_gpio(sensor->echoPin);
  // gpio_set_direction(sensor->echoPin, GPIO_MODE_INPUT);
  // gpio_pulldown_dis(sensor->echoPin);
  // gpio_pullup_en(sensor->echoPin); // hint from https://youtu.be/xwsT-e1D9OY?t=354
  // gpio_intr_enable(sensor->echoPin);

  // gpio_set_intr_type(sensor->echoPin, GPIO_INTR_NEGEDGE);
}

void TOFManager::attachSensorInterrupt(uint8_t idx) {
  gpio_isr_handler_add(sensors(idx)->echoPin, sensorInterrupt, sensors(idx));
}

void TOFManager::detachInterrupts() {
  for (int i = 0; i < NUMBER_OF_TOF_SENSORS; i++) {
    gpio_isr_handler_remove(sensors(i)->echoPin);
  }
}

void TOFManager::disableMeasurements() {
  for (int i = 0; i < NUMBER_OF_TOF_SENSORS; i ++) {
    gpio_set_level(sensors(i)->triggerPin, 0);
  }
}
