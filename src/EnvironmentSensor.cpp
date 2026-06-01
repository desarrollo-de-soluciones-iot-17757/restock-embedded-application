/**
 * @file EnvironmentSensor.cpp
 * @brief Implements the EnvironmentSensor class.
 *
 * @details
 * Manages environmental change detection using a DHT22 sensor. The sensor keeps
 * its latest accepted temperature and humidity values internally and emits an
 * event only when the current measurement differs from the stable reading by at
 * least the configured threshold.
 *
 * This implementation follows the ModestIoT reactive style from the device
 * perspective: the Arduino loop does not poll the sensor. EnvironmentSensor
 * acts as the event source and notifies the assigned EventHandler when a
 * significant environmental change is detected.
 *
 * @author Gabriela Shapiama
 * @date Jun 1, 2026
 * @version 0.2
 */

#include "EnvironmentSensor.h"
#include <math.h>

const Event EnvironmentSensor::ENVIRONMENT_CHANGE_DETECTED_EVENT =
    Event(ENVIRONMENT_CHANGE_DETECTED_EVENT_ID);

const Event EnvironmentSensor::ENVIRONMENT_READING_FAILED_EVENT =
    Event(ENVIRONMENT_READING_FAILED_EVENT_ID);

const Event EnvironmentSensor::ENVIRONMENT_INITIAL_READING_TAKEN_EVENT =
    Event(ENVIRONMENT_INITIAL_READING_TAKEN_EVENT_ID);

/**
 * @brief Constructs an EnvironmentSensor instance.
 *
 * @param pin GPIO pin connected to the DHT22 data line.
 * @param monitoringIntervalMs Internal interval used to check the sensor.
 * @param temperatureThreshold Minimum temperature change required to emit an event.
 * @param humidityThreshold Minimum humidity change required to emit an event.
 * @param eventHandler Event handler that receives sensor events.
 */
EnvironmentSensor::EnvironmentSensor(
    int pin,
    unsigned long monitoringIntervalMs,
    float temperatureThreshold,
    float humidityThreshold,
    EventHandler* eventHandler
)
    : Sensor(pin, eventHandler),
      dht(pin, DHT22),
      temperature(0.0f),
      humidity(0.0f),
      stableTemperature(0.0f),
      stableHumidity(0.0f),
      measuredAtMs(0),
      validReading(false),
      hasStableReading(false),
      monitoringIntervalMs(monitoringIntervalMs),
      temperatureThreshold(temperatureThreshold),
      humidityThreshold(humidityThreshold),
      monitoringTaskHandle(nullptr) {}

/**
 * @brief Initializes the DHT22 sensor and starts the internal monitoring task.
 */
void EnvironmentSensor::begin() {
    dht.begin();

    xTaskCreatePinnedToCore(
        EnvironmentSensor::monitoringTask,
        "environment_monitoring",
        4096,
        this,
        1,
        &monitoringTaskHandle,
        1
    );

    Serial.println("EnvironmentSensor ready");
}

/**
 * @brief Internal task that checks environmental values.
 *
 * @details
 * This task belongs to the sensor. It does not send telemetry directly.
 * It only checks the DHT22 and emits an event when a significant change is
 * detected.
 *
 * @param parameter Pointer to the EnvironmentSensor instance.
 */
void EnvironmentSensor::monitoringTask(void* parameter) {
    EnvironmentSensor* sensor = static_cast<EnvironmentSensor*>(parameter);

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (true) {
        sensor->checkForChange();
        vTaskDelay(pdMS_TO_TICKS(sensor->monitoringIntervalMs));
    }
}

/**
 * @brief Checks the DHT22 values and emits an event only when a significant change is detected.
 *
 * @details
 * The first valid reading is stored as the stable baseline without emitting a
 * telemetry event. After that, an event is emitted only when temperature or
 * humidity changes beyond the configured thresholds.
 */
void EnvironmentSensor::checkForChange() {
    float currentHumidity = dht.readHumidity();
    float currentTemperature = dht.readTemperature();

    if (isnan(currentHumidity) || isnan(currentTemperature)) {
        validReading = false;
        Serial.println("EnvironmentSensor reading failed");
        on(ENVIRONMENT_READING_FAILED_EVENT);
        return;
    }

    if (!hasStableReading) {
        stableTemperature = currentTemperature;
        stableHumidity = currentHumidity;
        temperature = currentTemperature;
        humidity = currentHumidity;
        measuredAtMs = millis();
        validReading = true;
        hasStableReading = true;

        Serial.printf(
            "Initial environment reading taken: %.2f C, %.2f %%\n",
            temperature,
            humidity
        );

        on(ENVIRONMENT_INITIAL_READING_TAKEN_EVENT);
        return;
    }

    if (!hasSignificantChange(currentTemperature, currentHumidity)) {
        return;
    }

    temperature = currentTemperature;
    humidity = currentHumidity;
    stableTemperature = currentTemperature;
    stableHumidity = currentHumidity;
    measuredAtMs = millis();
    validReading = true;

    Serial.printf(
        "Environment change detected: %.2f C, %.2f %%\n",
        temperature,
        humidity
    );

    on(ENVIRONMENT_CHANGE_DETECTED_EVENT);
}

/**
 * @brief Evaluates whether the current values changed significantly.
 *
 * @param currentTemperature Current temperature in Celsius.
 * @param currentHumidity Current relative humidity percentage.
 * @return true if temperature or humidity exceeded its configured threshold.
 */
bool EnvironmentSensor::hasSignificantChange(
    float currentTemperature,
    float currentHumidity
) const {
    float temperatureDifference = fabs(currentTemperature - stableTemperature);
    float humidityDifference = fabs(currentHumidity - stableHumidity);

    return temperatureDifference >= temperatureThreshold ||
           humidityDifference >= humidityThreshold;
}

/**
 * @brief Gets the latest accepted temperature.
 *
 * @return Last accepted temperature value in Celsius.
 */
float EnvironmentSensor::getTemperature() const {
    return temperature;
}

/**
 * @brief Gets the latest accepted humidity.
 *
 * @return Last accepted relative humidity value.
 */
float EnvironmentSensor::getHumidity() const {
    return humidity;
}

/**
 * @brief Gets the timestamp of the latest accepted measurement.
 *
 * @return Timestamp in milliseconds from device startup.
 */
unsigned long EnvironmentSensor::getMeasuredAtMs() const {
    return measuredAtMs;
}

/**
 * @brief Indicates whether the latest accepted reading is valid.
 *
 * @return true if the latest accepted reading is valid; otherwise false.
 */
bool EnvironmentSensor::hasValidReading() const {
    return validReading;
}