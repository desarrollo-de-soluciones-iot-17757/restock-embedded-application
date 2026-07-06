#ifndef WEIGHT_TELEMETRY_PACKAGE_H
#define WEIGHT_TELEMETRY_PACKAGE_H

#pragma once

#include <Arduino.h>
#include <ModestIoT.h>

/**
 * @brief Telemetry payload for Restock weight readings.
 *
 * @details
 * Serializes significant weight changes using the standard device unit:
 * grams. Business unit conversion is handled by the Edge service.
 *
 * Expected JSON body:
 * {
 *   "device_id": "...",
 *   "branch_id": "...",
 *   "weight_grams": 0.0,
 *   "measured_at_ms": 0
 * }
 */
class WeightTelemetryPackage : public TelemetryPackage {
private:
    const char* deviceId;                  ///< Unique embedded device identifier.
    const char* branchId;                  ///< Branch/store/restaurant identifier.
    float weightInGrams;                   ///< Current weight in grams.
    unsigned long measuredAtMilliseconds;  ///< Reading timestamp from device startup.

public:
    /**
     * @brief Creates a weight telemetry payload.
     *
     * @param deviceId Unique embedded device identifier.
     * @param branchId Branch/store/restaurant identifier.
     * @param weightInGrams Current measured weight in grams.
     * @param measuredAtMilliseconds Reading timestamp in milliseconds.
     */
    WeightTelemetryPackage(
        const char* deviceId,
        const char* branchId,
        float weightInGrams,
        unsigned long measuredAtMilliseconds
    )
        : deviceId(deviceId),
          branchId(branchId),
          weightInGrams(weightInGrams),
          measuredAtMilliseconds(measuredAtMilliseconds) {
    }

    /**
     * @brief Serializes the weight telemetry payload into JSON.
     *
     * @param serializationDestination Destination JSON document.
     */
    void serialize(JsonDocument& serializationDestination) const override {
        serializationDestination["device_id"] = deviceId;
        serializationDestination["branch_id"] = branchId;
        serializationDestination["weight_grams"] = weightInGrams;
        serializationDestination["measured_at_ms"] = measuredAtMilliseconds;
    }
};

#endif // WEIGHT_TELEMETRY_PACKAGE_H