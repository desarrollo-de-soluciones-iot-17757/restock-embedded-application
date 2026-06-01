#ifndef ENVIRONMENT_SENSOR_H
#define ENVIRONMENT_SENSOR_H

/**
 * @file EnvironmentSensor.h
 * @brief Declares the EnvironmentSensor class.
 *
 * @details
 * EnvironmentSensor represents the DHT22 sensor used by the Restock smart
 * inventory scale to capture temperature and humidity values.
 *
 * In this implementation, EnvironmentSensor acts as the event source of the
 * environmental flow. It encapsulates environmental monitoring and emits an
 * event only when it detects a significant change in temperature or humidity.
 *
 * @author Gabriela Shapiama
 * @date May 31, 2026
 * @version 0.1
 */

#include <Arduino.h>
#include <DHT.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Sensor.h"

/**
 * @brief Sensor responsible for detecting environmental changes.
 */
class EnvironmentSensor : public Sensor {
private:
    DHT dht;                         ///< DHT22 sensor driver instance.
    float temperature;               ///< Last measured temperature in Celsius.
    float humidity;                  ///< Last measured relative humidity percentage.
    float stableTemperature;         ///< Last stable temperature used for change comparison.
    float stableHumidity;            ///< Last stable humidity used for change comparison.
    unsigned long measuredAtMs;      ///< Timestamp in milliseconds of the last accepted reading.
    bool validReading;               ///< Indicates whether the last reading was valid.
    bool hasStableReading;           ///< Indicates whether a stable baseline already exists.
    unsigned long monitoringIntervalMs; ///< Interval used internally to check sensor values.
    float temperatureThreshold;      ///< Minimum temperature change required to emit an event.
    float humidityThreshold;         ///< Minimum humidity change required to emit an event.
    TaskHandle_t monitoringTaskHandle; ///< FreeRTOS task handle for internal monitoring.

    static void monitoringTask(void* parameter);

    bool hasSignificantChange(float currentTemperature, float currentHumidity) const;

public:
    static const int ENVIRONMENT_CHANGE_DETECTED_EVENT_ID = 2001;
    static const int ENVIRONMENT_READING_FAILED_EVENT_ID = 2002;

    static const Event ENVIRONMENT_CHANGE_DETECTED_EVENT;
    static const Event ENVIRONMENT_READING_FAILED_EVENT;

    EnvironmentSensor(
        int pin,
        unsigned long monitoringIntervalMs,
        float temperatureThreshold,
        float humidityThreshold,
        EventHandler* eventHandler = nullptr
    );

    void begin();

    void checkForChange();

    float getTemperature() const;

    float getHumidity() const;

    unsigned long getMeasuredAtMs() const;

    bool hasValidReading() const;
};

#endif // ENVIRONMENT_SENSOR_H