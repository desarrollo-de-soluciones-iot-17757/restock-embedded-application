#ifndef TELEMETRY_CLIENT_H
#define TELEMETRY_CLIENT_H

/**
 * @file TelemetryClient.h
 * @brief Declares the TelemetryClient communication component.
 *
 * @details
 * TelemetryClient is responsible for sending telemetry data from the Restock
 * embedded device to the Edge Service.
 *
 * This class is not modeled as an actuator because it does not modify the
 * physical environment. It is a communication component used by the device
 * coordinator when an environmental event must be reported.
 *
 * @author Gabriela Shapiama
 * @date May 31, 2026
 * @version 0.1
 */

#include <Arduino.h>

/**
 * @brief Communication component for Edge Service telemetry delivery.
 */
class TelemetryClient {
private:
    const char* ssid;      ///< WiFi SSID.
    const char* password;  ///< WiFi password.
    const char* baseUrl;   ///< Edge Service base URL.
    const char* apiKey;    ///< Device API key.

    bool lastSendSucceeded; ///< Result of the latest telemetry request.
    int lastStatusCode;     ///< HTTP status code of the latest telemetry request.

    String buildUrl(const String& endpoint) const;

public:
    TelemetryClient(
        const char* ssid,
        const char* password,
        const char* baseUrl,
        const char* apiKey
    );

    void begin();

    bool isConnected() const;

    bool sendTelemetry(const String& endpoint, const String& payload);

    bool wasLastSendSuccessful() const;

    int getLastStatusCode() const;
};

#endif // TELEMETRY_CLIENT_H