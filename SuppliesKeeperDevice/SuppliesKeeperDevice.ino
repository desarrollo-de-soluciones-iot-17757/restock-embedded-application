/**
 * @file SuppliesKeeperDevice.ino
 * @brief Restock embedded application using Modest-IoT Nano Framework.
 *
 * @details
 * This sketch defines the SuppliesKeeperDevice, a reactive ESP32-based device
 * that reads temperature, humidity and weight, displays the latest values on a
 * 16x2 I2C LCD, and sends telemetry to the Restock Edge Service only when a
 * significant data change is detected.
 *
 * The implementation uses the Modest-IoT Nano Framework components:
 * - DhtSensor for temperature and humidity acquisition.
 * - LoadCellAmplifier for weight acquisition through HX711.
 * - CharacterLcdDisplay for LCD output.
 * - WiFiConnectivityDriver for WiFi provisioning.
 * - AuthenticatedMqttGatewayClient for MQTT telemetry delivery.
 * - TelemetryPackage for JSON payload serialization.
 *
 * @author Gabriela Shapiama
 * @date Jul 01, 2026
 * @version 0.5
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
#include "WeightTelemetryPackage.h"

/**
 * @brief Temporary WiFi driver adapter for the current framework version.
 *
 * @details
 * The current WiFiConnectivityDriver implementation does not implement the
 * abstract transmit method required by ConnectivityDriver. This adapter keeps
 * the original WiFi behavior and only fulfills that missing contract without
 * modifying the framework files.
 */
class RestockWiFiConnectivityDriver : public WiFiConnectivityDriver {
public:
    /**
     * @brief Creates the Restock WiFi connectivity adapter.
     *
     * @param ssid Wireless network identifier.
     * @param password Wireless network password.
     */
    RestockWiFiConnectivityDriver(const char* ssid, const char* password)
        : WiFiConnectivityDriver(ssid, password) {
    }

    /**
     * @brief Fulfills the ConnectivityDriver transmit contract.
     *
     * @details
     * Restock telemetry is sent through AuthenticatedMqttGatewayClient, while
     * Edge provisioning uses HTTPClient directly. This method only reports
     * whether the WiFi transport is ready.
     *
     * @param target Ignored target endpoint.
     * @param data Ignored payload data.
     * @return true when the WiFi transport can transmit, otherwise false.
     */
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
 * - LoadCellAmplifier for weight readings.
 * - CharacterLcdDisplay for local visualization.
 * - AuthenticatedMqttGatewayClient for authenticated MQTT publishing.
 *
 * The device decides whether a sensor reading represents a significant change.
 * Sensors only read values; business/device-side tolerance is handled here.
 */
class SuppliesKeeperDevice : public Device {
private:
    DhtSensor environmentSensor;                         ///< DHT22 sensor adapter from ModestIoT.
    LoadCellAmplifier frontLeftLoadCell;                 ///< HX711 load cell adapter from ModestIoT.
    CharacterLcdDisplay statusDisplay;                   ///< I2C LCD actuator from ModestIoT.
    AuthenticatedMqttGatewayClient& gatewayClient;       ///< Authenticated MQTT telemetry gateway.

    DisplayMode displayMode;             ///< Display mode returned by Edge provisioning.
    String productUnitLabel;             ///< Product/inventory unit label returned by Edge provisioning.
    float convertedProductQuantity;      ///< Product quantity already converted by Edge for display.

    float currentTemperatureInCelsius;          ///< Latest temperature reading.
    float currentRelativeHumidityInPercentage;  ///< Latest humidity reading.
    float stableTemperatureInCelsius;           ///< Baseline temperature used for change detection.
    float stableRelativeHumidityInPercentage;   ///< Baseline humidity used for change detection.
    unsigned long environmentMeasuredAtMilliseconds; ///< Timestamp of latest environment reading.
    bool stableEnvironmentReadingRegistered;    ///< Indicates whether an environment baseline already exists.

    bool heapAlertActive;     ///< Track state of active memory alerts.
    bool cpuAlertActive;      ///< Track state of active CPU alerts.
    bool voltageAlertActive;  ///< Track state of active voltage alerts.
    bool tempAlertActive;     ///< Track state of active temperature alerts.

    float currentTotalWeightInGrams;              ///< Latest total weight reading in grams.
    float stableTotalWeightInGrams;               ///< Baseline total weight used for change detection.
    unsigned long weightMeasuredAtMilliseconds;   ///< Timestamp of latest weight reading.
    bool stableWeightReadingRegistered;           ///< Indicates whether a weight baseline already exists.

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
                snprintf(secondLine, sizeof(secondLine), "%.1f %%RH", currentRelativeHumidityInPercentage);
                break;

            case DISPLAY_MODE_WEIGHT:
                snprintf(firstLine, sizeof(firstLine), "Weight:");
                snprintf(secondLine, sizeof(secondLine), "%.0f g", currentTotalWeightInGrams);
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
                    currentRelativeHumidityInPercentage
                );
                break;

            case DISPLAY_MODE_ENVIRONMENT:
            default:
                snprintf(firstLine, sizeof(firstLine), "Temp: %.1f C", currentTemperatureInCelsius);
                snprintf(secondLine, sizeof(secondLine), "Hum:  %.1f %%", currentRelativeHumidityInPercentage);
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
            currentRelativeHumidityInPercentage - stableRelativeHumidityInPercentage
        );

        return temperatureDifference >= SENSOR_TEMPERATURE_CHANGE_TOLERANCE_C ||
               humidityDifference >= SENSOR_HUMIDITY_CHANGE_TOLERANCE_RH;
    }

    /**
     * @brief Registers the current values as the new stable baseline.
     */
    void registerStableEnvironmentReading() {
        stableTemperatureInCelsius = currentTemperatureInCelsius;
        stableRelativeHumidityInPercentage = currentRelativeHumidityInPercentage;
        stableEnvironmentReadingRegistered = true;
    }

    /**
     * @brief Creates and enqueues the environmental telemetry payload.
     */
    void enqueueEnvironmentTelemetry() {
        TelemetryPackage* telemetryPayload = new EnvironmentTelemetryPackage(
            DEVICE_ID,
            BRANCH_ID,
            currentTemperatureInCelsius,
            currentRelativeHumidityInPercentage,
            environmentMeasuredAtMilliseconds
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

    /**
     * @brief Calculates the current total weight from active load cells.
     *
     * @details
     * Current prototype uses one 5 kg load cell. Future versions can add
     * front-right, rear-left and rear-right readings here while keeping the
     * rest of the weight change detection logic unchanged.
     *
     * @return Current total weight in grams.
     */
    float calculateTotalWeightInGrams() const {
        return frontLeftLoadCell.getWeightInGrams();
    }

    /**
     * @brief Determines whether current weight changed significantly.
     *
     * @return true when the configured weight tolerance was exceeded.
     */
    bool hasSignificantWeightChange() const {
        return fabsf(currentTotalWeightInGrams - stableTotalWeightInGrams) >=
               WEIGHT_CHANGE_TOLERANCE_IN_GRAMS;
    }

    /**
     * @brief Registers the current weight as the stable comparison baseline.
     */
    void registerStableWeightReading() {
        stableTotalWeightInGrams = currentTotalWeightInGrams;
        stableWeightReadingRegistered = true;
    }

    /**
     * @brief Creates and enqueues the weight telemetry payload.
     */
    void enqueueWeightTelemetry() {
        TelemetryPackage* telemetryPayload = new WeightTelemetryPackage(
            DEVICE_ID,
            BRANCH_ID,
            currentTotalWeightInGrams,
            weightMeasuredAtMilliseconds
        );

        if (!enqueueTelemetryPayload(&telemetryPayload)) {
            delete telemetryPayload;
            Serial.println("[SuppliesKeeperDevice] Weight telemetry queue full. Payload discarded.");
            return;
        }

        Serial.printf(
            "[SuppliesKeeperDevice] Weight telemetry enqueued: %.2f g\n",
            currentTotalWeightInGrams
        );
    }

    /**
     * @brief Tares the load cell and prepares the initial weight baseline.
     *
     * @details
     * The scale should be empty when this method is executed.
     */
    void calibrateWeightSensor() {
        Serial.println("[WeightSensor] Taring load cell. Keep the scale empty.");
        renderDisplayLines("Weight sensor", "Taring...");

        frontLeftLoadCell.tare();

        currentTotalWeightInGrams = 0.0f;
        stableTotalWeightInGrams = 0.0f;
        weightMeasuredAtMilliseconds = millis();
        stableWeightReadingRegistered = false;

        Serial.println("[WeightSensor] Tare completed.");
    }

    /**
     * @brief Handles weight readings emitted by the framework load cell adapter.
     *
     * @details
     * The framework generates a data-read event when the load cell reading changes.
     * This method applies the Restock significant-change rule before sending
     * telemetry to Edge.
     */
    void handleWeightDataReadEvent() {
        currentTotalWeightInGrams = calculateTotalWeightInGrams();
        weightMeasuredAtMilliseconds = millis();

        Serial.printf(
            "[WeightSensor] Reading: %.2f g\n",
            currentTotalWeightInGrams
        );

        updateConfiguredDisplay();

        if (!stableWeightReadingRegistered) {
            registerStableWeightReading();

            Serial.println("[WeightSensor] Initial weight baseline registered.");

            if (SEND_INITIAL_WEIGHT_READING_TO_EDGE) {
                enqueueWeightTelemetry();
            }

            return;
        }

        if (!hasSignificantWeightChange()) {
            Serial.println("[WeightSensor] Change below tolerance. Telemetry skipped.");
            return;
        }

        Serial.printf(
            "[WeightSensor] Significant change detected. Previous: %.2f g | Current: %.2f g\n",
            stableTotalWeightInGrams,
            currentTotalWeightInGrams
        );

        registerStableWeightReading();
        enqueueWeightTelemetry();
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
          frontLeftLoadCell(
              FRONT_LEFT_LOAD_CELL_DATA_PIN,
              FRONT_LEFT_LOAD_CELL_CLOCK_PIN,
              LOAD_CELL_MINIMUM_WEIGHT_IN_GRAMS,
              LOAD_CELL_MAXIMUM_WEIGHT_IN_GRAMS,
              LOAD_CELL_MINIMUM_RAW_VALUE,
              LOAD_CELL_MAXIMUM_RAW_VALUE,
              LOAD_CELL_FILTER_DEPTH,
              this
          ),
          statusDisplay(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS, true, this),
          gatewayClient(gatewayClient),
          displayMode(displayMode),
          productUnitLabel(productUnitLabel),
          convertedProductQuantity(convertedProductQuantity),
          currentTemperatureInCelsius(0.0f),
          currentRelativeHumidityInPercentage(0.0f),
          stableTemperatureInCelsius(0.0f),
          stableRelativeHumidityInPercentage(0.0f),
          environmentMeasuredAtMilliseconds(0),
          stableEnvironmentReadingRegistered(false),
          heapAlertActive(false),
          cpuAlertActive(false),
          voltageAlertActive(false),
          tempAlertActive(false),
          currentTotalWeightInGrams(0.0f),
          stableTotalWeightInGrams(0.0f),
          weightMeasuredAtMilliseconds(0),
          stableWeightReadingRegistered(false) {

        Serial.println("[SuppliesKeeperDevice] Initializing device...");

        initializeAsynchronousEngine(TELEMETRY_QUEUE_LENGTH);

        bool environmentSchedulerRegistered = appendSensorToScheduler(
            &environmentSensor,
            Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER
        );

        if (!environmentSchedulerRegistered) {
            Serial.println("[SuppliesKeeperDevice] Failed to register DHT sensor in scheduler.");
        }

        bool weightSchedulerRegistered = appendSensorToScheduler(
            &frontLeftLoadCell,
            Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER
        );

        if (!weightSchedulerRegistered) {
            Serial.println("[SuppliesKeeperDevice] Failed to register load cell in scheduler.");
        }

        if (!statusDisplay.isBacklightOn()) {
            statusDisplay.handle(CharacterLcdDisplay::TURN_BACKLIGHT_ON_COMMAND);
        }

        showStartupMessage();

        calibrateWeightSensor();

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
        if (event.identifier != Sensor::DATA_READ_EVENT_IDENTIFIER) {
            Serial.printf("[SuppliesKeeperDevice] Unhandled event identifier: %d\n", event.identifier);
            return;
        }

        if (event.sourceId == FRONT_LEFT_LOAD_CELL_DATA_PIN) {
            handleWeightDataReadEvent();
            return;
        }

        currentTemperatureInCelsius = environmentSensor.getTemperatureInCelsius();
        currentRelativeHumidityInPercentage = environmentSensor.getRelativeHumidityInPercentage();
        environmentMeasuredAtMilliseconds = millis();

        updateConfiguredDisplay();

        Serial.printf(
            "[SuppliesKeeperDevice] DHT reading: %.2f C, %.2f %%RH\n",
            currentTemperatureInCelsius,
            currentRelativeHumidityInPercentage
        );

        evaluateHealth();

        if (!stableEnvironmentReadingRegistered) {
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
        DEVICE_SAMPLING_INTERVAL_MS,
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
