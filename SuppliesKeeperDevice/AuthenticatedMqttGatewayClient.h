#ifndef AUTHENTICATED_MQTT_GATEWAY_CLIENT_H
#define AUTHENTICATED_MQTT_GATEWAY_CLIENT_H

/**
 * @file AuthenticatedMqttGatewayClient.h
 * @brief Authenticated MQTT gateway for Restock device telemetry and responses.
 *
 * @details
 * The ModestIoT framework already includes an MQTT gateway for publishing
 * TelemetryPackage payloads. This Restock adapter keeps that framework idea but
 * adds the behavior required by the project:
 * - broker username/password authentication;
 * - publishing to different telemetry topics depending on payload type;
 * - subscribing to Edge response topics;
 * - invoking a device callback when Edge sends display/stock updates.
 *
 * @author Gabriela Shapiama
 * @date Jul 06, 2026
 * @version 0.6
 */

#include <Arduino.h>
#include <ModestIoT.h>
#include <PubSubClient.h>

#include "Config.h"

/**
 * @brief Function pointer contract for incoming MQTT messages.
 *
 * @param topic MQTT topic where the message was received.
 * @param payload Message body as a String.
 */
typedef void (*MqttMessageHandler)(const String& topic, const String& payload);

/**
 * @brief MQTT gateway that publishes telemetry and listens to Edge responses.
 */
class AuthenticatedMqttGatewayClient {
private:
    ConnectivityDriver& communicationTransportDriver; ///< Network connectivity driver.
    PubSubClient mqttClient;                           ///< MQTT client implementation.

    String brokerHost;             ///< MQTT broker host.
    uint16_t brokerPort;           ///< MQTT broker port.
    String defaultPublishTopic;    ///< Backward-compatible default telemetry topic.
    String clientId;               ///< MQTT client identifier.
    String mqttUsername;           ///< MQTT username.
    String mqttPassword;           ///< MQTT password/token.
    String responseTopic;          ///< Canonical Edge response subscription topic.
    String legacyResponseTopic;    ///< Temporary legacy Edge response subscription topic.
    MqttMessageHandler handler;    ///< Application callback for incoming messages.

    /**
     * @brief Processes a raw PubSubClient callback and converts it to Strings.
     *
     * @param topic Raw MQTT topic.
     * @param payload Raw MQTT payload buffer.
     * @param length Payload byte length.
     */
    void handleIncomingMqttMessage(char* topic, byte* payload, unsigned int length) {
        String message;
        message.reserve(length);

        for (unsigned int index = 0; index < length; ++index) {
            message += static_cast<char>(payload[index]);
        }

        Serial.print("[MQTT] Incoming topic: ");
        Serial.println(topic);
        Serial.print("[MQTT] Incoming payload: ");
        Serial.println(message);

        if (handler != nullptr) {
            handler(String(topic), message);
        }
    }

    /**
     * @brief Subscribes to configured Edge response topics after connection.
     */
    void subscribeToConfiguredResponseTopics() {
        if (!mqttClient.connected()) {
            return;
        }

        if (responseTopic.length() > 0) {
            bool subscribed = mqttClient.subscribe(responseTopic.c_str());
            Serial.printf(
                "[MQTT] Subscribe %s => %s\n",
                responseTopic.c_str(),
                subscribed ? "ok" : "failed"
            );
        }

        if (legacyResponseTopic.length() > 0 && legacyResponseTopic != responseTopic) {
            bool subscribed = mqttClient.subscribe(legacyResponseTopic.c_str());
            Serial.printf(
                "[MQTT] Subscribe %s => %s\n",
                legacyResponseTopic.c_str(),
                subscribed ? "ok" : "failed"
            );
        }
    }

    /**
     * @brief Ensures that the MQTT broker session is connected.
     */
    void ensureBrokerSessionConnection() {
        if (!communicationTransportDriver.canTransmit()) {
            communicationTransportDriver.connect();
            return;
        }

        if (mqttClient.connected()) {
            return;
        }

        Serial.println("[MQTT] Connecting to broker...");

        bool connected = false;

        if (MQTT_AUTHENTICATION_ENABLED) {
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

        if (connected) {
            subscribeToConfiguredResponseTopics();
        }
    }

public:
    /**
     * @brief Creates an authenticated MQTT telemetry gateway.
     *
     * @param transportDriver Network connectivity driver.
     * @param networkClient Arduino network client socket.
     * @param brokerHost MQTT broker host.
     * @param brokerPort MQTT broker port.
     * @param defaultPublishTopic Backward-compatible default telemetry topic.
     * @param clientId MQTT client identifier.
     * @param mqttUsername MQTT username.
     * @param mqttPassword MQTT password/token.
     */
    AuthenticatedMqttGatewayClient(
        ConnectivityDriver& transportDriver,
        Client& networkClient,
        const String& brokerHost,
        uint16_t brokerPort,
        const String& defaultPublishTopic,
        const String& clientId,
        const String& mqttUsername,
        const String& mqttPassword
    )
        : communicationTransportDriver(transportDriver),
          mqttClient(networkClient),
          brokerHost(brokerHost),
          brokerPort(brokerPort),
          defaultPublishTopic(defaultPublishTopic),
          clientId(clientId),
          mqttUsername(mqttUsername),
          mqttPassword(mqttPassword),
          responseTopic(""),
          legacyResponseTopic(""),
          handler(nullptr) {

        mqttClient.setServer(this->brokerHost.c_str(), this->brokerPort);
        mqttClient.setBufferSize(MQTT_PAYLOAD_BUFFER_SIZE);
        mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->handleIncomingMqttMessage(topic, payload, length);
        });
    }

    /**
     * @brief Registers the application callback for incoming MQTT messages.
     *
     * @param messageHandler Callback executed when Edge publishes a response.
     */
    void setMessageHandler(MqttMessageHandler messageHandler) {
        handler = messageHandler;
    }

    /**
     * @brief Configures Edge response topics and subscribes when possible.
     *
     * @param canonicalTopic Canonical response topic, usually stores/<device>/response/#.
     * @param legacyTopic Temporary legacy topic, usually store/<device>/response/#.
     */
    void configureResponseSubscriptions(
        const String& canonicalTopic,
        const String& legacyTopic
    ) {
        responseTopic = canonicalTopic;
        legacyResponseTopic = legacyTopic;
        subscribeToConfiguredResponseTopics();
    }

    /**
     * @brief Subscribes to an arbitrary MQTT topic.
     *
     * @param topic MQTT topic or wildcard subscription.
     * @return true when the broker accepted the subscription.
     */
    bool subscribeToTopic(const char* topic) {
        if (topic == nullptr || strlen(topic) == 0) {
            return false;
        }

        ensureBrokerSessionConnection();

        if (!mqttClient.connected()) {
            Serial.println("[MQTT] Broker unavailable. Subscription skipped.");
            return false;
        }

        bool subscribed = mqttClient.subscribe(topic);
        Serial.printf("[MQTT] Subscribe %s => %s\n", topic, subscribed ? "ok" : "failed");
        return subscribed;
    }

    /**
     * @brief Runs the MQTT network loop required to receive subscribed messages.
     */
    void loop() {
        ensureBrokerSessionConnection();

        if (mqttClient.connected()) {
            mqttClient.loop();
        }
    }

    /**
     * @brief Serializes and publishes a telemetry payload to a target topic.
     *
     * @param telemetryPayload Telemetry payload.
     * @param targetTopic MQTT topic selected by the application.
     * @return true if the broker accepted the published payload.
     */
    bool publishTelemetryRecord(
        const TelemetryPackage& telemetryPayload,
        const char* targetTopic
    ) {
        if (targetTopic == nullptr || strlen(targetTopic) == 0) {
            Serial.println("[MQTT] Missing target topic. Telemetry not published.");
            return false;
        }

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

        Serial.print("[MQTT] Publishing to topic: ");
        Serial.println(targetTopic);
        Serial.println(serializedPayload);

        bool published = mqttClient.publish(
            targetTopic,
            serializedPayload.c_str()
        );

        mqttClient.loop();
        return published;
    }

    /**
     * @brief Backward-compatible publishing method using the default topic.
     *
     * @param telemetryPayload Telemetry payload.
     * @return true if the broker accepted the published payload.
     */
    bool sendTelemetryRecord(const TelemetryPackage& telemetryPayload) {
        return publishTelemetryRecord(telemetryPayload, defaultPublishTopic.c_str());
    }
};

#endif // AUTHENTICATED_MQTT_GATEWAY_CLIENT_H
