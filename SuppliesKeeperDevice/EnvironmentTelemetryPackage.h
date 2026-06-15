#ifndef ENVIRONMENT_TELEMETRY_PACKAGE_H
#define ENVIRONMENT_TELEMETRY_PACKAGE_H

#pragma once

/**
 * @brief Telemetry payload for Restock environmental readings.
 *
 * @details
 * Preserves the previous telemetry body used by the Restock Embedded App:
 *
 * {
 *   "device_id": "...",
 *   "branch_id": "...",
 *   "temperature": 0.0,
 *   "humidity": 0.0,
 *   "measured_at_ms": 0
 * }
 *
 * The implementation now uses the ModestIoT TelemetryPackage abstraction.
 */
class EnvironmentTelemetryPackage : public TelemetryPackage {
private:
    const char* deviceId;                 ///< Unique identifier of the embedded device.
    const char* branchId;                 ///< Identifier of the branch/store/restaurant.
    float temperatureInCelsius;           ///< Temperature reading in Celsius.
    float relativeHumidityPercentage;     ///< Relative humidity percentage.
    unsigned long measuredAtMilliseconds; ///< Reading timestamp from device startup.

public:
    /**
     * @brief Creates an environmental telemetry payload.
     *
     * @param deviceId Unique embedded device identifier.
     * @param branchId Branch or store identifier.
     * @param temperatureInCelsius Temperature in Celsius.
     * @param relativeHumidityPercentage Relative humidity percentage.
     * @param measuredAtMilliseconds Reading timestamp in milliseconds.
     */
    EnvironmentTelemetryPackage(
        const char* deviceId,
        const char* branchId,
        float temperatureInCelsius,
        float relativeHumidityPercentage,
        unsigned long measuredAtMilliseconds
    )
        : deviceId(deviceId),
          branchId(branchId),
          temperatureInCelsius(temperatureInCelsius),
          relativeHumidityPercentage(relativeHumidityPercentage),
          measuredAtMilliseconds(measuredAtMilliseconds) {}

    /**
     * @brief Serializes the telemetry payload into a JSON document.
     *
     * @param serializationDestination Destination JSON document.
     */
    void serialize(JsonDocument& serializationDestination) const override {
        serializationDestination["device_id"] = deviceId;
        serializationDestination["branch_id"] = branchId;
        serializationDestination["temperature"] = temperatureInCelsius;
        serializationDestination["humidity"] = relativeHumidityPercentage;
        serializationDestination["measured_at_ms"] = measuredAtMilliseconds;
    }
};


#endif //ENVIRONMENT_TELEMETRY_PACKAGE_H
