#ifndef WEIGHT_TELEMETRY_PACKAGE_H
#define WEIGHT_TELEMETRY_PACKAGE_H

#pragma once

/**
 * @file WeightTelemetryPackage.h
 * @brief MQTT telemetry payload for weight readings sent to Edge.
 *
 * @details
 * Edge currently expects the following JSON contract:
 * {
 *   "device_id": "supplies-keeper-001",
 *   "weight_grams": 500.0,
 *   "created_at": "2026-08-14T06:19:12Z"
 * }
 *
 * The embedded device sends grams as the raw physical unit. Business conversion
 * to product units is done by Edge and returned through response topics.
 *
 * @author Gabriela Shapiama
 * @date Jul 06, 2026
 * @version 0.6
 */

#include <Arduino.h>
#include <ModestIoT.h>

/**
 * @brief Telemetry payload for Restock weight readings.
 */
class WeightTelemetryPackage : public TelemetryPackage {
private:
    const char* deviceId;      ///< Unique embedded device identifier.
    float weightInGrams;      ///< Current measured weight in grams.
    String createdAt;         ///< UTC timestamp formatted as ISO-8601.

public:
    /**
     * @brief Creates a weight telemetry payload.
     *
     * @param deviceId Unique embedded device identifier.
     * @param weightInGrams Current measured weight in grams.
     * @param createdAt UTC timestamp formatted as yyyy-MM-ddTHH:mm:ssZ.
     */
    WeightTelemetryPackage(
        const char* deviceId,
        float weightInGrams,
        const String& createdAt
    )
        : deviceId(deviceId),
          weightInGrams(weightInGrams),
          createdAt(createdAt) {}

    /**
     * @brief Serializes the weight telemetry payload into JSON.
     *
     * @param serializationDestination Destination JSON document.
     */
    void serialize(JsonDocument& serializationDestination) const override {
        serializationDestination["device_id"] = deviceId;
        serializationDestination["weight_grams"] = weightInGrams;
        serializationDestination["created_at"] = createdAt;
    }
};

#endif // WEIGHT_TELEMETRY_PACKAGE_H
