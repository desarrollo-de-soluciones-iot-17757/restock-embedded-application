/**
 * @file TelemetryClient.cpp
 * @brief Implements the TelemetryClient communication component.
 *
 * @details
 * Manages WiFi connectivity and HTTP telemetry delivery to the Restock Edge
 * Service. This component is used by the device coordinator when sensor events
 * require data transmission to the Edge layer.
 *
 * @author Gabriela Shapiama
 * @date May 31, 2026
 * @version 0.1
 */

#include "TelemetryClient.h"
#include <WiFi.h>
#include <HTTPClient.h>

TelemetryClient::TelemetryClient(
    const char* ssid,
    const char* password,
    const char* baseUrl,
    const char* apiKey
)
    : ssid(ssid),
      password(password),
      baseUrl(baseUrl),
      apiKey(apiKey),
      lastSendSucceeded(false),
      lastStatusCode(0) {}

void TelemetryClient::begin() {
    Serial.printf("Connecting to WiFi SSID: %s\n", ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long startTime = millis();
    const unsigned long timeoutMs = 15000;

    while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeoutMs) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi connected. IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi connection failed");
    }
}

bool TelemetryClient::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String TelemetryClient::buildUrl(const String& endpoint) const {
    String url = String(baseUrl);

    if (!url.endsWith("/") && !endpoint.startsWith("/")) {
        url += "/";
    }

    if (url.endsWith("/") && endpoint.startsWith("/")) {
        url.remove(url.length() - 1);
    }

    url += endpoint;
    return url;
}

bool TelemetryClient::sendTelemetry(const String& endpoint, const String& payload) {
    if (!isConnected()) {
        Serial.println("Telemetry skipped: WiFi is not connected");
        lastSendSucceeded = false;
        lastStatusCode = 0;
        return false;
    }

    HTTPClient http;
    String url = buildUrl(endpoint);

    Serial.println("Sending telemetry to Edge Service");
    Serial.println(url);
    Serial.println(payload);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", apiKey);

    lastStatusCode = http.POST(payload);
    String response = http.getString();

    Serial.printf("Edge response status: %d\n", lastStatusCode);
    Serial.println(response);

    http.end();

    lastSendSucceeded = lastStatusCode >= 200 && lastStatusCode < 300;
    return lastSendSucceeded;
}

bool TelemetryClient::wasLastSendSuccessful() const {
    return lastSendSucceeded;
}

int TelemetryClient::getLastStatusCode() const {
    return lastStatusCode;
}