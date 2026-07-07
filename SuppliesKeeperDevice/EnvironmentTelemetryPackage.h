#ifndef ENVIRONMENT_TELEMETRY_PACKAGE_H
#define ENVIRONMENT_TELEMETRY_PACKAGE_H

#pragma once

/**
 * @file EnvironmentTelemetryPackage.h
 * @brief MQTT telemetry payload for environmental readings sent to Edge.
 *
 * @details
 * Edge currently expects the following JSON contract:
 * {
 *   "device_id": "supplies-keeper-001",
 *   "temperature": 25.0,
 *   "humidity": 60.0,
 *   "created_at": "2026-08-14T06:19:12Z"
 * }
 *
 * The package intentionally does not include branch_id or measured_at_ms because
 * the current Edge contract provided for the embedded task does not require them.
 *
 * @author Gabriela Shapiama
 * @date Jul 06, 2026
 * @version 0.6
 */

#include <Arduino.h>
#include <ModestIoT.h>

/**
 * @brief Telemetry payload for Restock environmental readings.
 */
class EnvironmentTelemetryPackage : public TelemetryPackage {
private:
    const char* deviceId;                 ///< Unique identifier of the embedded device.
    float temperatureInCelsius;           ///< Temperature reading in Celsius.
    float relativeHumidityPercentage;     ///< Relative humidity percentage.
    String createdAt;                     ///< UTC timestamp formatted as ISO-8601.

public:
    /**
     * @brief Creates an environmental telemetry payload.
     *
     * @param deviceId Unique embedded device identifier.
     * @param temperatureInCelsius Temperature in Celsius.
     * @param relativeHumidityPercentage Relative humidity percentage.
     * @param createdAt UTC timestamp formatted as yyyy-MM-ddTHH:mm:ssZ.
     */
    EnvironmentTelemetryPackage(
        const char* deviceId,
        float temperatureInCelsius,
        float relativeHumidityPercentage,
        const String& createdAt
    )
        : deviceId(deviceId),
          temperatureInCelsius(temperatureInCelsius),
          relativeHumidityPercentage(relativeHumidityPercentage),
          createdAt(createdAt) {}

    /**
     * @brief Serializes the telemetry payload into a JSON document.
     *
     * @param serializationDestination Destination JSON document.
     */
    void serialize(JsonDocument& serializationDestination) const override {
        serializationDestination["device_id"] = deviceId;
        serializationDestination["temperature"] = temperatureInCelsius;
        serializationDestination["humidity"] = relativeHumidityPercentage;
        serializationDestination["created_at"] = createdAt;
    }
};

#endif // ENVIRONMENT_TELEMETRY_PACKAGE_H
