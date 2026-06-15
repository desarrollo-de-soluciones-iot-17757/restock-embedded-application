#ifndef AUTHENTICATED_MQTT_GATEWAY_CLIENT_H
#define AUTHENTICATED_MQTT_GATEWAY_CLIENT_H

/**
 * @file AuthenticatedMqttGatewayClient.h
 * @brief Authenticated MQTT telemetry gateway for Restock devices.
 *
 * @details
 * The current ModestIoT MqttGatewayClient publishes TelemetryPackage payloads,
 * but it connects to the broker only with clientId. This Restock-specific
 * adapter keeps the same TelemetryPackage-based publishing idea while allowing
 * MQTT username/password credentials returned by the Edge provisioning flow.
 *
 * This class should be replaced by the framework MqttGatewayClient once the
 * framework supports authenticated broker sessions.
 *
 * @author Gabriela Shapiama
 * @date Jun 15, 2026
 * @version 0.5
 */

#include <Arduino.h>
#include <ModestIoT.h>

#include "Config.h"

/**
 * @brief MQTT gateway that publishes telemetry using Edge-provisioned credentials.
 */
class AuthenticatedMqttGatewayClient {
private:
    ConnectivityDriver& communicationTransportDriver; ///< Network connectivity driver.
    PubSubClient mqttClient;                           ///< MQTT client implementation.

    String brokerHost;     ///< MQTT broker host.
    uint16_t brokerPort;   ///< MQTT broker port.
    String publishTopic;   ///< Telemetry topic.
    String clientId;       ///< MQTT client identifier.
    String mqttUsername;   ///< MQTT username.
    String mqttPassword;   ///< MQTT password/token.

    /**
     * @brief Ensures that the MQTT broker session is connected.
     */
    void ensureBrokerSessionConnection() {
        if (!communicationTransportDriver.canTransmit()) {
            communicationTransportDriver.connect();
            return;
        }

        if (mqttClient.connected()) {
            mqttClient.loop();
            return;
        }

        Serial.println("[MQTT] Connecting to broker...");

        bool connected = false;

        if (mqttUsername.length() > 0 || mqttPassword.length() > 0) {
            connected = mqttClient.connect(
                clientId.c_str(),
                mqttUsername.c_str(),
                mqttPassword.c_str()
            );
        } else {
            connected = mqttClient.connect(clientId.c_str());
        }

        Serial.printf(
            "[MQTT] Broker connection result: %s\n",
            connected ? "connected" : "failed"
        );
    }

public:
    /**
     * @brief Creates an authenticated MQTT telemetry gateway.
     *
     * @param transportDriver Network connectivity driver.
     * @param networkClient Arduino network client socket.
     * @param brokerHost MQTT broker host.
     * @param brokerPort MQTT broker port.
     * @param publishTopic Telemetry topic.
     * @param clientId MQTT client identifier.
     * @param mqttUsername MQTT username.
     * @param mqttPassword MQTT password/token.
     */
    AuthenticatedMqttGatewayClient(
        ConnectivityDriver& transportDriver,
        Client& networkClient,
        const String& brokerHost,
        uint16_t brokerPort,
        const String& publishTopic,
        const String& clientId,
        const String& mqttUsername,
        const String& mqttPassword
    )
        : communicationTransportDriver(transportDriver),
          mqttClient(networkClient),
          brokerHost(brokerHost),
          brokerPort(brokerPort),
          publishTopic(publishTopic),
          clientId(clientId),
          mqttUsername(mqttUsername),
          mqttPassword(mqttPassword) {

        mqttClient.setServer(this->brokerHost.c_str(), this->brokerPort);
        mqttClient.setBufferSize(MQTT_PAYLOAD_BUFFER_SIZE);
    }

    /**
     * @brief Serializes and publishes a telemetry payload.
     *
     * @param telemetryPayload Telemetry payload.
     * @return true if the broker accepted the published payload.
     */
    bool sendTelemetryRecord(const TelemetryPackage& telemetryPayload) {
        if (!communicationTransportDriver.canTransmit()) {
            Serial.println("[MQTT] Network unavailable. Telemetry not published.");
            communicationTransportDriver.connect();
            return false;
        }

        ensureBrokerSessionConnection();

        if (!mqttClient.connected()) {
            Serial.println("[MQTT] Broker unavailable. Telemetry not published.");
            return false;
        }

        JsonDocument serializationDocument;
        telemetryPayload.serialize(serializationDocument);

        String serializedPayload;
        serializeJson(serializationDocument, serializedPayload);

        Serial.println("[MQTT] Publishing telemetry:");
        Serial.println(serializedPayload);

        bool published = mqttClient.publish(
            publishTopic.c_str(),
            serializedPayload.c_str()
        );

        mqttClient.loop();

        return published;
    }
};

#endif // AUTHENTICATED_MQTT_GATEWAY_CLIENT_H