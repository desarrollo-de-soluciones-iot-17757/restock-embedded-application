/**
 * @file SuppliesKeeperDevice.ino
 * @brief Restock embedded application using Modest-IoT Nano Framework.
 *
 * @details
 * This sketch defines the SuppliesKeeperDevice, a reactive ESP32-based device
 * that reads temperature and humidity from a DHT22 sensor, displays the latest
 * environmental values on a 16x2 I2C LCD, and sends telemetry to the Restock
 * Edge Service only when a significant environmental change is detected.
 *
 * The implementation uses the Modest-IoT Nano Framework components:
 * - DhtSensor for temperature and humidity acquisition.
 * - CharacterLcdDisplay for LCD output.
 * - WiFiConnectivityDriver for WiFi provisioning.
 * - AuthenticatedMqttGatewayClient for MQTT telemetry delivery.
 * - TelemetryPackage for JSON payload serialization.
 *
 * @author Gabriela Shapiama
 * @date Jun 15, 2026
 * @version 0.4
 */

#include <Arduino.h>
#include <esp_system.h>
#include <ModestIoT.h>


#include "Config.h"
#include "EnvironmentTelemetryPackage.h"
#include "HealthTelemetryPackage.h"
#include "DisplayMode.h"
#include "AuthenticatedMqttGatewayClient.h"
#include "EdgeProvisioningClient.h"

//Temporal class for missing method in WiFiConnectivityDriver of the framework
class RestockWiFiConnectivityDriver : public WiFiConnectivityDriver {
public:
    RestockWiFiConnectivityDriver(const char* ssid, const char* password)
        : WiFiConnectivityDriver(ssid, password) {
    }

    bool transmit(const char* target, const char* data) override {
        (void) target;
        (void) data;

        return canTransmit();
    }
};


// --- 1. EVENT-DRIVEN APPLICATION MEDIATOR ---

/**
 * @brief Main application mediator for the Restock Supplies Keeper device.
 *
 * @details
 * Coordinates framework components:
 * - DhtSensor for temperature and humidity.
 * - CharacterLcdDisplay for local visualization.
 * - AuthenticatedMqttGatewayClient for authenticated MQTT publishing.
 *
 * The device decides whether a DHT reading represents a significant change.
 * Sensors only read values; business/device-side tolerance is handled here.
 */
class SuppliesKeeperDevice : public Device {
private:
    DhtSensor environmentSensor;                         ///< DHT22 sensor adapter from ModestIoT.
    CharacterLcdDisplay statusDisplay;                   ///< I2C LCD actuator from ModestIoT.
    AuthenticatedMqttGatewayClient& gatewayClient;///< Authenticated MQTT telemetry gateway.

    DisplayMode displayMode;              ///< Display mode returned by Edge provisioning.
    String productUnitLabel;             ///< Product/inventory unit label returned by Edge provisioning.
    float convertedProductQuantity;      ///< Product quantity already converted by Edge for display.

    float currentTemperatureInCelsius;       ///< Latest temperature reading.
    float currentRelativeHumidityPercentage; ///< Latest humidity reading.
    float stableTemperatureInCelsius;        ///< Baseline temperature used for change detection.
    float stableRelativeHumidityPercentage;  ///< Baseline humidity used for change detection.
    unsigned long measuredAtMilliseconds;    ///< Timestamp of latest valid reading.
    bool stableReadingRegistered;            ///< Indicates whether a baseline already exists.
    bool heapAlertActive;                    ///< Track state of active memory alerts.
    bool cpuAlertActive;                     ///< Track state of active CPU alerts.
    bool voltageAlertActive;                 ///< Track state of active voltage alerts.
    bool tempAlertActive;                    ///< Track state of active temperature alerts.


    /**
     * @brief Shows a two-line message on the LCD.
     *
     * @param firstLine First LCD row.
     * @param secondLine Second LCD row.
     */
    void renderDisplayLines(const char* firstLine, const char* secondLine) {
        statusDisplay.setLineBuffer(0, firstLine);
        statusDisplay.setLineBuffer(1, secondLine);
        statusDisplay.handle(CharacterLcdDisplay::UPDATE_TEXT_COMMAND);
    }

    /**
     * @brief Shows the startup message.
     */
    void showStartupMessage() {
        renderDisplayLines("Restock IoT", "Starting...");
    }

    /**
     * @brief Shows that the device was not authorized by the Edge.
     */
    void showProvisioningErrorMessage() {
        renderDisplayLines("Edge auth error", "Check config");
    }

    /**
     * @brief Renders the latest reading according to boot-time display/product configuration.
     *
     * @details
     * Temperature is always displayed in Celsius. The configurable unit refers
     * to product/inventory quantities, for example kg, bottles, boxes or units.
     */
    void updateConfiguredDisplay() {
        char firstLine[LCD_COLUMNS + 1];
        char secondLine[LCD_COLUMNS + 1];

        switch (displayMode) {
            case DISPLAY_MODE_TEMPERATURE:
                snprintf(firstLine, sizeof(firstLine), "Temp: %.1f C", currentTemperatureInCelsius);
                snprintf(secondLine, sizeof(secondLine), "Mode: Temp");
                break;

            case DISPLAY_MODE_HUMIDITY:
                snprintf(firstLine, sizeof(firstLine), "Humidity:");
                snprintf(secondLine, sizeof(secondLine), "%.1f %%RH", currentRelativeHumidityPercentage);
                break;

            case DISPLAY_MODE_WEIGHT:
                snprintf(firstLine, sizeof(firstLine), "Weight: N/A");
                snprintf(secondLine, sizeof(secondLine), "Temp: %.1f C", currentTemperatureInCelsius);
                break;

            case DISPLAY_MODE_CONVERTED_UNITS:
                snprintf(
                    firstLine,
                    sizeof(firstLine),
                    "%.1f %s",
                    convertedProductQuantity,
                    productUnitLabel.c_str()
                );
                snprintf(
                    secondLine,
                    sizeof(secondLine),
                    "T%.1fC H%.0f%%",
                    currentTemperatureInCelsius,
                    currentRelativeHumidityPercentage
                );
                break;

            case DISPLAY_MODE_ENVIRONMENT:
            default:
                snprintf(firstLine, sizeof(firstLine), "Temp: %.1f C", currentTemperatureInCelsius);
                snprintf(secondLine, sizeof(secondLine), "Hum:  %.1f %%", currentRelativeHumidityPercentage);
                break;
        }

        renderDisplayLines(firstLine, secondLine);
    }

    /**
     * @brief Determines whether current environmental data changed significantly.
     *
     * @return true when temperature or humidity exceeded configured tolerances.
     */
    bool hasSignificantEnvironmentChange() const {
        float temperatureDifference = fabsf(
            currentTemperatureInCelsius - stableTemperatureInCelsius
        );

        float humidityDifference = fabsf(
            currentRelativeHumidityPercentage - stableRelativeHumidityPercentage
        );

        return temperatureDifference >= SENSOR_TEMPERATURE_CHANGE_TOLERANCE_C ||
               humidityDifference >= SENSOR_HUMIDITY_CHANGE_TOLERANCE_RH;
    }

    /**
     * @brief Registers the current values as the new stable baseline.
     */
    void registerStableEnvironmentReading() {
        stableTemperatureInCelsius = currentTemperatureInCelsius;
        stableRelativeHumidityPercentage = currentRelativeHumidityPercentage;
        stableReadingRegistered = true;
    }

    /**
     * @brief Creates and enqueues the environmental telemetry payload.
     */
    void enqueueEnvironmentTelemetry() {
        TelemetryPackage* telemetryPayload = new EnvironmentTelemetryPackage(
            DEVICE_ID,
            BRANCH_ID,
            currentTemperatureInCelsius,
            currentRelativeHumidityPercentage,
            measuredAtMilliseconds
        );

        if (!enqueueTelemetryPayload(&telemetryPayload)) {
            delete telemetryPayload;
            Serial.println("[SuppliesKeeperDevice] Telemetry queue full. Payload discarded.");
            return;
        }

        Serial.println("[SuppliesKeeperDevice] Environment telemetry enqueued.");
    }

    /**
     * @brief Reads the microcontroller internal temperature in Celsius.
     */
    float readInternalTemperature() {
        float temp = temperatureRead();
        if (temp == 0.0f || temp < -100.0f || temp > 150.0f) {
            // Fallback simulated reading for Wokwi compatibility/stability
            static float simTemp = 42.0f;
            simTemp += ((float)(rand() % 5) - 2.0f) * 0.1f; // drift slightly
            return simTemp;
        }
        return temp;
    }

    /**
     * @brief Reads/estimates the microcontroller internal voltage.
     */
    float readInternalVoltage() {
        // ESP32 nominal VCC is 3.3V. Simulate a small realistic drift around 3.3V.
        static float simVoltage = 3.3f;
        simVoltage += ((float)(rand() % 3) - 1.0f) * 0.01f;
        if (simVoltage < 3.1f) simVoltage = 3.1f;
        if (simVoltage > 3.5f) simVoltage = 3.5f;
        return simVoltage;
    }

    /**
     * @brief Simulates/calculates CPU usage percentage.
     */
    float getCpuUsage() {
        static float cpuUsage = 35.0f;
        cpuUsage += ((float)(rand() % 11) - 5.0f);
        if (cpuUsage < 10.0f) cpuUsage = 10.0f;
        if (cpuUsage > 100.0f) cpuUsage = 100.0f;
        return cpuUsage;
    }

    /**
     * @brief Map esp_reset_reason_t to a string name.
     */
    String getResetReasonString(esp_reset_reason_t reason) {
        switch (reason) {
            case ESP_RST_POWERON:   return "POWERON_RESET";
            case ESP_RST_SW:        return "SOFTWARE_RESET";
            case ESP_RST_PANIC:      return "PANIC_RESET";
            case ESP_RST_INT_WDT:    return "INTERRUPT_WATCHDOG_RESET";
            case ESP_RST_TASK_WDT:   return "TASK_WATCHDOG_RESET";
            case ESP_RST_WDT:        return "OTHER_WATCHDOG_RESET";
            case ESP_RST_DEEPSLEEP:  return "DEEPSLEEP_RESET";
            case ESP_RST_BROWNOUT:  return "BROWNOUT_RESET";
            case ESP_RST_SDIO:      return "SDIO_RESET";
            case ESP_RST_UNKNOWN:
            default:                return "UNKNOWN_RESET";
        }
    }

    /**
     * @brief Checks the reset reason and enqueues the startup alert immediately.
     */
    void checkAndSendResetReason() {
        esp_reset_reason_t reason = esp_reset_reason();
        String reasonStr = getResetReasonString(reason);

        Serial.printf("[HealthMonitor] Boot Reset Reason: %s (code: %d)\n", reasonStr.c_str(), (int)reason);

        TelemetryPackage* alertPayload = new HealthTelemetryPackage(
            DEVICE_ID,
            BRANCH_ID,
            "BOOT_RESET_REASON",
            "reset_reason",
            reasonStr,
            "N/A",
            "Device booted. Reset reason: " + reasonStr,
            millis()
        );

        if (!enqueueTelemetryPayload(&alertPayload)) {
            delete alertPayload;
            Serial.println("[HealthMonitor] Failed to enqueue boot reset reason alert.");
        } else {
            Serial.println("[HealthMonitor] Boot reset reason alert enqueued.");
        }
    }

    /**
     * @brief Evaluates all health metrics against their thresholds.
     */
    void evaluateHealth() {
        uint32_t freeHeap = ESP.getFreeHeap();
        float cpuUsage = getCpuUsage();
        unsigned long uptimeMs = millis();
        float voltage = readInternalVoltage();
        float tempC = readInternalTemperature();

        // Print health status to Serial console for local debugging
        Serial.println("--- [Health Status] ---");
        Serial.printf("  Free Heap: %u bytes\n", freeHeap);
        Serial.printf("  CPU Usage: %.1f %%\n", cpuUsage);
        Serial.printf("  Uptime: %lu ms\n", uptimeMs);
        Serial.printf("  Internal Voltage: %.2f V\n", voltage);
        Serial.printf("  Internal Temp: %.1f C\n", tempC);
        Serial.println("-----------------------");

        // Heap Memory Check
        if (freeHeap < HEALTH_THRESHOLD_MIN_FREE_HEAP_BYTES) {
            if (!heapAlertActive) {
                heapAlertActive = true;
                Serial.printf("[HealthMonitor] WARNING: Low memory! Heap: %u bytes\n", freeHeap);
                
                TelemetryPackage* alert = new HealthTelemetryPackage(
                    DEVICE_ID, BRANCH_ID, "HEALTH_ANOMALY", "heap",
                    String(freeHeap), String(HEALTH_THRESHOLD_MIN_FREE_HEAP_BYTES),
                    "Low memory threshold breached. Available heap is critically low.", uptimeMs
                );
                if (!enqueueTelemetryPayload(&alert)) {
                    delete alert;
                }
            }
        } else {
            if (heapAlertActive) {
                heapAlertActive = false;
                Serial.println("[HealthMonitor] INFO: Heap memory recovered to safe levels.");
            }
        }

        // CPU Usage Check
        if (cpuUsage > HEALTH_THRESHOLD_MAX_CPU_USAGE_PERCENT) {
            if (!cpuAlertActive) {
                cpuAlertActive = true;
                Serial.printf("[HealthMonitor] WARNING: High CPU! CPU: %.1f %%\n", cpuUsage);

                TelemetryPackage* alert = new HealthTelemetryPackage(
                    DEVICE_ID, BRANCH_ID, "HEALTH_ANOMALY", "cpu",
                    String(cpuUsage, 1), String(HEALTH_THRESHOLD_MAX_CPU_USAGE_PERCENT, 1),
                    "High CPU usage threshold breached. Device processor load is high.", uptimeMs
                );
                if (!enqueueTelemetryPayload(&alert)) {
                    delete alert;
                }
            }
        } else {
            if (cpuAlertActive) {
                cpuAlertActive = false;
                Serial.println("[HealthMonitor] INFO: CPU load recovered to safe levels.");
            }
        }

        // Internal Voltage Check
        if (voltage < HEALTH_THRESHOLD_MIN_VOLTAGE_V || voltage > HEALTH_THRESHOLD_MAX_VOLTAGE_V) {
            if (!voltageAlertActive) {
                voltageAlertActive = true;
                Serial.printf("[HealthMonitor] WARNING: Voltage anomaly! Voltage: %.2f V\n", voltage);

                String msg = "Voltage anomaly detected. Safe bounds: " +
                             String(HEALTH_THRESHOLD_MIN_VOLTAGE_V, 1) + "V - " +
                             String(HEALTH_THRESHOLD_MAX_VOLTAGE_V, 1) + "V.";
                TelemetryPackage* alert = new HealthTelemetryPackage(
                    DEVICE_ID, BRANCH_ID, "HEALTH_ANOMALY", "voltage",
                    String(voltage, 2),
                    String(HEALTH_THRESHOLD_MIN_VOLTAGE_V, 1) + "/" + String(HEALTH_THRESHOLD_MAX_VOLTAGE_V, 1),
                    msg, uptimeMs
                );
                if (!enqueueTelemetryPayload(&alert)) {
                    delete alert;
                }
            }
        } else {
            if (voltageAlertActive) {
                voltageAlertActive = false;
                Serial.println("[HealthMonitor] INFO: Internal voltage returned to safe levels.");
            }
        }

        // Internal Temperature Check
        if (tempC > HEALTH_THRESHOLD_MAX_TEMP_C) {
            if (!tempAlertActive) {
                tempAlertActive = true;
                Serial.printf("[HealthMonitor] WARNING: High internal temperature! Temp: %.1f C\n", tempC);

                TelemetryPackage* alert = new HealthTelemetryPackage(
                    DEVICE_ID, BRANCH_ID, "HEALTH_ANOMALY", "temperature",
                    String(tempC, 1), String(HEALTH_THRESHOLD_MAX_TEMP_C, 1),
                    "High internal temperature threshold breached. Microcontroller is running hot.", uptimeMs
                );
                if (!enqueueTelemetryPayload(&alert)) {
                    delete alert;
                }
            }
        } else {
            if (tempAlertActive) {
                tempAlertActive = false;
                Serial.println("[HealthMonitor] INFO: Internal temperature returned to safe levels.");
            }
        }
    }

protected:
    /**
     * @brief Sends queued telemetry using the authenticated MQTT gateway.
     *
     * @param rawQueueItemPayload Telemetry payload received from the framework queue.
     */
    void processQueuedTelemetryData(const TelemetryPackage* rawQueueItemPayload) const override {
        if (rawQueueItemPayload == nullptr) {
            return;
        }

        bool sent = gatewayClient.sendTelemetryRecord(*rawQueueItemPayload);

        Serial.printf(
            "[SuppliesKeeperDevice] MQTT publish result: %s\n",
            sent ? "published" : "failed"
        );
    }

public:
    /**
     * @brief Creates the SuppliesKeeperDevice.
     *
     * @param gatewayClient Authenticated MQTT gateway.
     * @param displayMode Display mode received from Edge provisioning.
     * @param productUnitLabel Product/inventory unit label received from Edge provisioning.
     * @param convertedProductQuantity Product quantity already converted by Edge.
     * @param samplingIntervalInMilliseconds Sensor sampling interval.
     * @param timerChannel ESP32 timer channel used by the framework scheduler.
     */
    SuppliesKeeperDevice(
        AuthenticatedMqttGatewayClient& gatewayClient,
        DisplayMode displayMode,
        const String& productUnitLabel,
        float convertedProductQuantity,
        unsigned long samplingIntervalInMilliseconds,
        uint8_t timerChannel = CORE_TIMER_CHANNEL
    )
        : Device(samplingIntervalInMilliseconds, timerChannel),
          environmentSensor(ENVIRONMENT_SENSOR_PIN, DHT22, this),
          statusDisplay(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS, true, this),
          gatewayClient(gatewayClient),
          displayMode(displayMode),
          productUnitLabel(productUnitLabel),
          convertedProductQuantity(convertedProductQuantity),
          currentTemperatureInCelsius(0.0f),
          currentRelativeHumidityPercentage(0.0f),
          stableTemperatureInCelsius(0.0f),
          stableRelativeHumidityPercentage(0.0f),
          measuredAtMilliseconds(0),
          stableReadingRegistered(false),
          heapAlertActive(false),
          cpuAlertActive(false),
          voltageAlertActive(false),
          tempAlertActive(false) {

        Serial.println("[SuppliesKeeperDevice] Initializing device...");

        initializeAsynchronousEngine(TELEMETRY_QUEUE_LENGTH);

        bool schedulerRegistered = appendSensorToScheduler(
            &environmentSensor,
            Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER
        );

        if (!schedulerRegistered) {
            Serial.println("[SuppliesKeeperDevice] Failed to register DHT sensor in scheduler.");
        }

        if (!statusDisplay.isBacklightOn()) {
            statusDisplay.handle(CharacterLcdDisplay::TURN_BACKLIGHT_ON_COMMAND);
        }

        showStartupMessage();

        // Enqueue reset reason alert immediately upon startup
        checkAndSendResetReason();

        Serial.println("[SuppliesKeeperDevice] Device ready.");
    }

    /**
     * @brief Handles events emitted by framework sensors.
     *
     * @param event Event emitted by a component.
     */
    void on(Event event) override {
        if (event.identifier != DhtSensor::DATA_READ_EVENT_IDENTIFIER) {
            Serial.printf("[SuppliesKeeperDevice] Unhandled event identifier: %d\n", event.identifier);
            return;
        }

        currentTemperatureInCelsius = environmentSensor.getTemperatureInCelsius();
        currentRelativeHumidityPercentage = environmentSensor.getRelativeHumidityInPercentage();
        measuredAtMilliseconds = millis();

        updateConfiguredDisplay();

        Serial.printf(
            "[SuppliesKeeperDevice] DHT reading: %.2f C, %.2f %%RH\n",
            currentTemperatureInCelsius,
            currentRelativeHumidityPercentage
        );

        // Evaluate health telemetry on every sensor reading interval
        evaluateHealth();

        if (!stableReadingRegistered) {
            registerStableEnvironmentReading();

            Serial.println("[SuppliesKeeperDevice] Initial environment baseline registered.");

            if (SEND_INITIAL_ENVIRONMENT_READING_TO_EDGE) {
                enqueueEnvironmentTelemetry();
            }

            return;
        }

        if (!hasSignificantEnvironmentChange()) {
            Serial.println("[SuppliesKeeperDevice] Change below tolerance. Telemetry skipped.");
            return;
        }

        registerStableEnvironmentReading();
        enqueueEnvironmentTelemetry();
    }
};

// --- 2. DECLARATIVE RUNTIME CANVAS ---

/**
 * @brief Global WiFi connectivity driver instance.
 */
static WiFiConnectivityDriver* wifiConnectivityDriver = nullptr;

/**
 * @brief Global authenticated MQTT gateway instance.
 */
static AuthenticatedMqttGatewayClient* mqttGatewayClient = nullptr;

/**
 * @brief Global SuppliesKeeperDevice instance.
 */
static SuppliesKeeperDevice* suppliesKeeperDevice = nullptr;

/**
 * @brief Waits until the connectivity driver reports an active connection.
 *
 * @param connectivityDriver Connectivity driver.
 * @param timeoutInMilliseconds Maximum wait time.
 * @return true when connected before timeout.
 */
static bool waitForNetworkConnection(
    ConnectivityDriver& connectivityDriver,
    unsigned long timeoutInMilliseconds
) {
    unsigned long startTime = millis();

    connectivityDriver.connect();

    while (millis() - startTime < timeoutInMilliseconds) {
        if (connectivityDriver.isConnected()) {
            return true;
        }

        delay(250);
        Serial.print(".");
    }

    Serial.println();
    return connectivityDriver.isConnected();
}


/**
 * @brief Applies local fallback values when Edge provisioning is unavailable.
 *
 * @param provisioningResult Provisioning result to complete.
 */
static void applyProvisioningFallbacks(ProvisioningResult& provisioningResult) {
    if (provisioningResult.mqttClientId.length() == 0) {
        provisioningResult.mqttClientId = DEVICE_ID;
    }

    if (provisioningResult.mqttUsername.length() == 0) {
        provisioningResult.mqttUsername = MQTT_USERNAME_FALLBACK;
    }

    if (provisioningResult.mqttPassword.length() == 0) {
        provisioningResult.mqttPassword = MQTT_PASSWORD_FALLBACK;
    }

    if (provisioningResult.mqttBrokerHost.length() == 0) {
        provisioningResult.mqttBrokerHost = MQTT_BROKER_HOST_FALLBACK;
    }

    if (provisioningResult.mqttBrokerPort == 0) {
        provisioningResult.mqttBrokerPort = MQTT_BROKER_PORT_FALLBACK;
    }

    if (provisioningResult.telemetryTopic.length() == 0) {
        provisioningResult.telemetryTopic = MQTT_TELEMETRY_TOPIC_FALLBACK;
    }

    if (provisioningResult.displayMode.length() == 0) {
        provisioningResult.displayMode = DISPLAY_MODE_FALLBACK;
    }

    if (provisioningResult.productUnitLabel.length() == 0) {
        provisioningResult.productUnitLabel = PRODUCT_UNIT_LABEL_FALLBACK;
    }

    if (provisioningResult.convertedQuantity < 0.0f) {
        provisioningResult.convertedQuantity = CONVERTED_QUANTITY_FALLBACK;
    }
}

/**
 * @brief Arduino setup entry point.
 *
 * @details
 * Boot sequence:
 * 1. Connect WiFi.
 * 2. Ask Edge for MQTT credentials and display configuration.
 * 3. Create authenticated MQTT gateway.
 * 4. Start the event-driven device.
 */
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);

    Serial.println();
    Serial.println("[System Boot] Restock Embedded Application");
    Serial.println("[System Boot] SuppliesKeeperDevice starting...");

    wifiConnectivityDriver = new RestockWiFiConnectivityDriver(WIFI_SSID, WIFI_PASSWORD);

    bool networkReady = waitForNetworkConnection(
        *wifiConnectivityDriver,
        WIFI_CONNECTION_TIMEOUT_MS
    );

    if (networkReady) {
        Serial.println("[System Boot] WiFi connected.");
    } else {
        Serial.println("[System Boot] WiFi connection timeout.");
    }

    ProvisioningResult provisioningResult;

    if (networkReady) {
        EdgeProvisioningClient provisioningClient(
            *wifiConnectivityDriver,
            EDGE_DEVICE_PROVISIONING_URL,
            EDGE_PROVISIONING_TIMEOUT_MS
        );

        provisioningResult = provisioningClient.requestDeviceProvisioning(
            DEVICE_ID,
            DEVICE_API_KEY
        );
    }

    if (!provisioningResult.authorized) {
        Serial.println("[System Boot] Edge provisioning failed or device was not authorized.");

        if (!ALLOW_DEVELOPMENT_PROVISIONING_FALLBACK) {
            CharacterLcdDisplay errorDisplay(
                LCD_I2C_ADDRESS,
                LCD_COLUMNS,
                LCD_ROWS,
                true,
                nullptr
            );

            errorDisplay.setLineBuffer(0, "Edge auth error");
            errorDisplay.setLineBuffer(1, "Device stopped");
            errorDisplay.handle(CharacterLcdDisplay::UPDATE_TEXT_COMMAND);

            return;
        }

        Serial.println("[System Boot] Development fallback enabled. Using local MQTT defaults.");
    }

    applyProvisioningFallbacks(provisioningResult);

    Serial.println("[System Boot] Effective provisioning:");
    Serial.printf("  MQTT clientId: %s\n", provisioningResult.mqttClientId.c_str());
    Serial.printf("  MQTT user: %s\n", provisioningResult.mqttUsername.c_str());
    Serial.printf("  MQTT broker: %s:%u\n",
                  provisioningResult.mqttBrokerHost.c_str(),
                  provisioningResult.mqttBrokerPort);
    Serial.printf("  Telemetry topic: %s\n", provisioningResult.telemetryTopic.c_str());
    Serial.printf("  Display mode: %s\n", provisioningResult.displayMode.c_str());
    Serial.printf("  Product unit: %s\n", provisioningResult.productUnitLabel.c_str());
    Serial.printf("  Converted quantity: %.2f\n", provisioningResult.convertedQuantity);

    mqttGatewayClient = new AuthenticatedMqttGatewayClient(
        *wifiConnectivityDriver,
        wifiConnectivityDriver->getNetworkSocket(),
        provisioningResult.mqttBrokerHost,
        provisioningResult.mqttBrokerPort,
        provisioningResult.telemetryTopic,
        provisioningResult.mqttClientId,
        provisioningResult.mqttUsername,
        provisioningResult.mqttPassword
    );

    suppliesKeeperDevice = new SuppliesKeeperDevice(
        *mqttGatewayClient,
        parseDisplayMode(provisioningResult.displayMode),
        provisioningResult.productUnitLabel,
        provisioningResult.convertedQuantity,
        ENVIRONMENT_MONITORING_INTERVAL_MS,
        CORE_TIMER_CHANNEL
    );

    Serial.println("[System Boot] SuppliesKeeperDevice active.");
}

/**
 * @brief Arduino loop entry point.
 *
 * @details
 * Business logic is handled by the ModestIoT asynchronous engine.
 */
void loop() {
    vTaskDelay(pdMS_TO_TICKS(60000));
}