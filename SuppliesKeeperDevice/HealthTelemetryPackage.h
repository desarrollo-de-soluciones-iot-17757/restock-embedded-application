#ifndef HEALTH_TELEMETRY_PACKAGE_H
#define HEALTH_TELEMETRY_PACKAGE_H

#pragma once

#include <Arduino.h>
#include <ModestIoT.h>

/**
 * @brief Telemetry payload for Restock device health alerts and reset events.
 *
 * @details
 * Serializes health anomalies (threshold breaches) and boot reset reason events:
 * {
 *   "device_id": "...",
 *   "branch_id": "...",
 *   "alert_type": "HEALTH_ANOMALY" | "BOOT_RESET_REASON",
 *   "metric": "heap" | "cpu" | "voltage" | "temperature" | "reset_reason",
 *   "value": "...",
 *   "threshold": "...",
 *   "message": "...",
 *   "timestamp_ms": 0
 * }
 */
class HealthTelemetryPackage : public TelemetryPackage {
private:
    const char* deviceId;          ///< Unique identifier of the embedded device.
    const char* branchId;          ///< Identifier of the branch.
    String alertType;              ///< Type of alert (e.g., HEALTH_ANOMALY, BOOT_RESET_REASON).
    String metric;                 ///< The specific metric names (e.g., heap, cpu, temperature, voltage).
    String value;                  ///< Current reading value formatted as a string.
    String threshold;              ///< Configured limit breached, or "N/A".
    String message;                ///< Human-readable description.
    unsigned long timestampMs;     ///< Device time of reading in milliseconds.

public:
    /**
     * @brief Creates a health telemetry package.
     */
    HealthTelemetryPackage(
        const char* deviceId,
        const char* branchId,
        const String& alertType,
        const String& metric,
        const String& value,
        const String& threshold,
        const String& message,
        unsigned long timestampMs
    )
        : deviceId(deviceId),
          branchId(branchId),
          alertType(alertType),
          metric(metric),
          value(value),
          threshold(threshold),
          message(message),
          timestampMs(timestampMs) {}

    /**
     * @brief Serializes the health alert payload into a JSON document.
     */
    void serialize(JsonDocument& serializationDestination) const override {
        serializationDestination["device_id"] = deviceId;
        serializationDestination["branch_id"] = branchId;
        serializationDestination["alert_type"] = alertType;
        serializationDestination["metric"] = metric;
        serializationDestination["value"] = value;
        serializationDestination["threshold"] = threshold;
        serializationDestination["message"] = message;
        serializationDestination["timestamp_ms"] = timestampMs;
    }
};

#endif // HEALTH_TELEMETRY_PACKAGE_H
