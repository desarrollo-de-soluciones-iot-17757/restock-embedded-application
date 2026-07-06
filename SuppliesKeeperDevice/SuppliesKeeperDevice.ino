/**
 * @file SuppliesKeeperDevice.ino
 * @brief Restock embedded application using Modest-IoT Nano Framework.
 *
 * @details
 * Defines a reactive ESP32-based Supplies Keeper device that reads temperature,
 * humidity and weight, publishes telemetry through MQTT to the Edge-connected
 * broker, receives Edge responses and updates a 16x2 I2C LCD dynamically.
 *
 * Current sprint scope:
 * - Environmental telemetry to Edge topic stores/<device_id>/telemetry/environment.
 * - Weight telemetry to Edge topic stores/<device_id>/telemetry/weight.
 * - Health telemetry to Edge topic stores/<device_id>/health.
 * - Dynamic LCD updates from both canonical and legacy response topics.
 * - One active HX711 load cell channel; the remaining three cells are deferred.
 *
 * @author Gabriela Shapiama
 * @date Jul 06, 2026
 * @version 0.7
 */

#include <Arduino.h>
#include <esp_system.h>
#include <math.h>
#include <string.h>
#include <time.h>
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

/**
 * @brief Writes the current UTC timestamp using ISO-8601 format.
 *
 * @details
 * The Edge service requires created_at as an ISO-8601 string. If NTP time is
 * not available yet, a deterministic fallback timestamp is generated using
 * millis() so the payload remains parseable during local tests.
 *
 * @param destination Output buffer.
 * @param size Output buffer size.
 */
static void formatCurrentUtcTimestamp(char* destination, size_t size) {
    if (destination == nullptr || size == 0U) {
        return;
    }

    struct tm timeInfo;

    if (getLocalTime(&timeInfo, 50U)) {
        strftime(destination, size, "%Y-%m-%dT%H:%M:%SZ", &timeInfo);
        return;
    }

    const unsigned long elapsedSeconds = millis() / 1000UL;
    snprintf(
        destination,
        size,
        "1970-01-01T00:%02lu:%02luZ",
        (elapsedSeconds / 60UL) % 60UL,
        elapsedSeconds % 60UL
    );
}

/**
 * @brief Main application mediator for the Restock Supplies Keeper device.
 *
 * @details
 * Coordinates framework components:
 * - DhtSensor for temperature and humidity.
 * - LoadCellAmplifier for weight readings.
 * - CharacterLcdDisplay for local visualization.
 * - AuthenticatedMqttGatewayClient for authenticated MQTT publishing/listening.
 *
 * Sensors emit events, this mediator applies Restock rules, and telemetry is
 * handed to ModestIoT's asynchronous queue before being sent to MQTT.
 */
class SuppliesKeeperDevice : public Device {
private:
    DhtSensor environmentSensor;                   ///< DHT22 sensor adapter from ModestIoT.
    LoadCellAmplifier frontLeftLoadCell;           ///< HX711 load cell adapter from ModestIoT.
    CharacterLcdDisplay statusDisplay;             ///< I2C LCD actuator from ModestIoT.
    AuthenticatedMqttGatewayClient& gatewayClient; ///< Authenticated MQTT gateway.

    String environmentTelemetryTopic; ///< MQTT topic for environmental telemetry.
    String weightTelemetryTopic;      ///< MQTT topic for weight telemetry.
    String healthTelemetryTopic;      ///< MQTT topic for health telemetry.

    DisplayMode displayMode;        ///< Current LCD display mode.
    String productUnitLabel;        ///< Product/inventory unit label returned by Edge.
    float convertedProductQuantity; ///< Product quantity converted by Edge for display.

    float currentTemperatureInCelsius;             ///< Latest temperature reading.
    float currentRelativeHumidityInPercentage;     ///< Latest humidity reading.
    float stableTemperatureInCelsius;              ///< Baseline temperature for change detection.
    float stableRelativeHumidityInPercentage;      ///< Baseline humidity for change detection.
    unsigned long environmentMeasuredAtMilliseconds; ///< Timestamp of latest environment reading.
    bool stableEnvironmentReadingRegistered;       ///< Whether environment baseline exists.

    bool heapAlertActive;    ///< Track state of active memory alerts.
    bool cpuAlertActive;     ///< Track state of active CPU alerts.
    bool voltageAlertActive; ///< Track state of active voltage alerts.
    bool tempAlertActive;    ///< Track state of active temperature alerts.

    float currentTotalWeightInGrams;            ///< Latest total weight reading in grams.
    float stableTotalWeightInGrams;             ///< Baseline total weight for change detection.
    unsigned long weightMeasuredAtMilliseconds; ///< Timestamp of latest weight reading.
    bool stableWeightReadingRegistered;         ///< Whether weight baseline exists.

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

    /** @brief Shows the startup message. */
    void showStartupMessage() {
        renderDisplayLines("Restock IoT", "Starting...");
    }

    /**
     * @brief Copies a String to a fixed LCD buffer safely.
     *
     * @param destination Destination buffer.
     * @param size Destination size.
     * @param source Source text.
     */
    static void copyLcdLine(char* destination, size_t size, const String& source) {
        if (destination == nullptr || size == 0U) {
            return;
        }

        snprintf(destination, size, "%s", source.c_str());
    }

    /**
     * @brief Renders the latest reading according to the current display mode.
     *
     * @details
     * Temperature is always shown in Celsius. The configurable unit refers to
     * product/inventory quantities, for example kg, bottles, boxes or units.
     */
    void updateConfiguredDisplay() {
        char firstLine[LCD_COLUMNS + 1];
        char secondLine[LCD_COLUMNS + 1];

        switch (displayMode) {
            case DISPLAY_MODE_TEMPERATURE:
                snprintf(firstLine, sizeof(firstLine), "Temperature");
                snprintf(secondLine, sizeof(secondLine), "%.1f C", currentTemperatureInCelsius);
                break;

            case DISPLAY_MODE_HUMIDITY:
                snprintf(firstLine, sizeof(firstLine), "Humidity");
                snprintf(secondLine, sizeof(secondLine), "%.1f %%", currentRelativeHumidityInPercentage);
                break;

            case DISPLAY_MODE_WEIGHT:
                snprintf(firstLine, sizeof(firstLine), "Current weight");
                snprintf(secondLine, sizeof(secondLine), "%.1f g", currentTotalWeightInGrams);
                break;

            case DISPLAY_MODE_CONVERTED_UNITS: {
                snprintf(firstLine, sizeof(firstLine), "Current stock");
                String stockLine = String(convertedProductQuantity, 1) + " " + productUnitLabel;
                copyLcdLine(secondLine, sizeof(secondLine), stockLine);
                break;
            }

            case DISPLAY_MODE_ENVIRONMENT:
            default:
                snprintf(firstLine, sizeof(firstLine), "Temp: %.1f C", currentTemperatureInCelsius);
                snprintf(secondLine, sizeof(secondLine), "Hum: %.1f %%", currentRelativeHumidityInPercentage);
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
        const float temperatureDifference = fabsf(
            currentTemperatureInCelsius - stableTemperatureInCelsius
        );

        const float humidityDifference = fabsf(
            currentRelativeHumidityInPercentage - stableRelativeHumidityInPercentage
        );

        return temperatureDifference >= SENSOR_TEMPERATURE_CHANGE_TOLERANCE_C ||
               humidityDifference >= SENSOR_HUMIDITY_CHANGE_TOLERANCE_RH;
    }

    /** @brief Registers the current values as the new stable environment baseline. */
    void registerStableEnvironmentReading() {
        stableTemperatureInCelsius = currentTemperatureInCelsius;
        stableRelativeHumidityInPercentage = currentRelativeHumidityInPercentage;
        stableEnvironmentReadingRegistered = true;
    }

    /** @brief Creates and enqueues the environmental telemetry payload. */
    void enqueueEnvironmentTelemetry() {
        char createdAt[CREATED_AT_BUFFER_SIZE];
        formatCurrentUtcTimestamp(createdAt, sizeof(createdAt));

        TelemetryPackage* telemetryPayload = new EnvironmentTelemetryPackage(
            DEVICE_ID,
            currentTemperatureInCelsius,
            currentRelativeHumidityInPercentage,
            String(createdAt)
        );

        if (!enqueueTelemetryPayload(&telemetryPayload)) {
            delete telemetryPayload;
            Serial.println("[SuppliesKeeperDevice] Telemetry queue full. Environment payload discarded.");
            return;
        }

        Serial.println("[SuppliesKeeperDevice] Environment telemetry enqueued.");
    }

    /**
     * @brief Reads the microcontroller internal temperature in Celsius.
     *
     * @return Internal ESP32 temperature or a Wokwi-safe simulated value.
     */
    float readInternalTemperature() {
        const float temp = temperatureRead();
        if (temp == 0.0f || temp < -100.0f || temp > 150.0f) {
            static float simulatedTemperature = 42.0f;
            simulatedTemperature += (static_cast<float>(rand() % 5) - 2.0f) * 0.1f;
            return simulatedTemperature;
        }

        return temp;
    }

    /**
     * @brief Reads or estimates the microcontroller internal voltage.
     *
     * @return Simulated ESP32 voltage in volts.
     */
    float readInternalVoltage() {
        static float simulatedVoltage = 3.3f;
        simulatedVoltage += (static_cast<float>(rand() % 3) - 1.0f) * 0.01f;

        if (simulatedVoltage < 3.1f) {
            simulatedVoltage = 3.1f;
        }

        if (simulatedVoltage > 3.5f) {
            simulatedVoltage = 3.5f;
        }

        return simulatedVoltage;
    }

    /**
     * @brief Simulates/calculates CPU usage percentage.
     *
     * @return Simulated CPU usage percentage.
     */
    float getCpuUsage() {
        static float cpuUsage = 35.0f;
        cpuUsage += static_cast<float>(rand() % 11) - 5.0f;

        if (cpuUsage < 10.0f) {
            cpuUsage = 10.0f;
        }

        if (cpuUsage > 100.0f) {
            cpuUsage = 100.0f;
        }

        return cpuUsage;
    }

    /**
     * @brief Maps esp_reset_reason_t to a string name.
     *
     * @param reason ESP32 reset reason enum.
     * @return Human-readable reset reason.
     */
    String getResetReasonString(esp_reset_reason_t reason) {
        switch (reason) {
            case ESP_RST_POWERON:  return "POWERON_RESET";
            case ESP_RST_SW:       return "SOFTWARE_RESET";
            case ESP_RST_PANIC:    return "PANIC_RESET";
            case ESP_RST_INT_WDT:  return "INTERRUPT_WATCHDOG_RESET";
            case ESP_RST_TASK_WDT: return "TASK_WATCHDOG_RESET";
            case ESP_RST_WDT:      return "OTHER_WATCHDOG_RESET";
            case ESP_RST_DEEPSLEEP:return "DEEPSLEEP_RESET";
            case ESP_RST_BROWNOUT: return "BROWNOUT_RESET";
            case ESP_RST_SDIO:     return "SDIO_RESET";
            case ESP_RST_UNKNOWN:
            default:               return "UNKNOWN_RESET";
        }
    }

    /** @brief Checks the reset reason and enqueues the startup alert immediately. */
    void checkAndSendResetReason() {
        const esp_reset_reason_t reason = esp_reset_reason();
        const String reasonStr = getResetReasonString(reason);

        Serial.printf("[HealthMonitor] Boot Reset Reason: %s (code: %d)\n", reasonStr.c_str(), static_cast<int>(reason));

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
            return;
        }

        Serial.println("[HealthMonitor] Boot reset reason alert enqueued.");
    }

    /** @brief Evaluates all health metrics against their thresholds. */
    void evaluateHealth() {
        const uint32_t freeHeap = ESP.getFreeHeap();
        const float cpuUsage = getCpuUsage();
        const unsigned long uptimeMs = millis();
        const float voltage = readInternalVoltage();
        const float tempC = readInternalTemperature();

        Serial.println("--- [Health Status] ---");
        Serial.printf("  Free Heap: %u bytes\n", freeHeap);
        Serial.printf("  CPU Usage: %.1f %%\n", cpuUsage);
        Serial.printf("  Uptime: %lu ms\n", uptimeMs);
        Serial.printf("  Internal Voltage: %.2f V\n", voltage);
        Serial.printf("  Internal Temp: %.1f C\n", tempC);
        Serial.println("-----------------------");

        if (freeHeap < HEALTH_THRESHOLD_MIN_FREE_HEAP_BYTES) {
            if (!heapAlertActive) {
                heapAlertActive = true;
                TelemetryPackage* alert = new HealthTelemetryPackage(
                    DEVICE_ID,
                    BRANCH_ID,
                    "HEALTH_ANOMALY",
                    "heap",
                    String(freeHeap),
                    String(HEALTH_THRESHOLD_MIN_FREE_HEAP_BYTES),
                    "Low memory threshold breached. Available heap is critically low.",
                    uptimeMs
                );

                if (!enqueueTelemetryPayload(&alert)) {
                    delete alert;
                }
            }
        } else if (heapAlertActive) {
            heapAlertActive = false;
            Serial.println("[HealthMonitor] INFO: Heap memory recovered to safe levels.");
        }

        if (cpuUsage > HEALTH_THRESHOLD_MAX_CPU_USAGE_PERCENT) {
            if (!cpuAlertActive) {
                cpuAlertActive = true;
                TelemetryPackage* alert = new HealthTelemetryPackage(
                    DEVICE_ID,
                    BRANCH_ID,
                    "HEALTH_ANOMALY",
                    "cpu",
                    String(cpuUsage, 1),
                    String(HEALTH_THRESHOLD_MAX_CPU_USAGE_PERCENT, 1),
                    "High CPU usage threshold breached. Device processor load is high.",
                    uptimeMs
                );

                if (!enqueueTelemetryPayload(&alert)) {
                    delete alert;
                }
            }
        } else if (cpuAlertActive) {
            cpuAlertActive = false;
            Serial.println("[HealthMonitor] INFO: CPU load recovered to safe levels.");
        }

        if (voltage < HEALTH_THRESHOLD_MIN_VOLTAGE_V || voltage > HEALTH_THRESHOLD_MAX_VOLTAGE_V) {
            if (!voltageAlertActive) {
                voltageAlertActive = true;
                const String message = "Voltage anomaly detected. Safe bounds: " +
                                       String(HEALTH_THRESHOLD_MIN_VOLTAGE_V, 1) + "V - " +
                                       String(HEALTH_THRESHOLD_MAX_VOLTAGE_V, 1) + "V.";

                TelemetryPackage* alert = new HealthTelemetryPackage(
                    DEVICE_ID,
                    BRANCH_ID,
                    "HEALTH_ANOMALY",
                    "voltage",
                    String(voltage, 2),
                    String(HEALTH_THRESHOLD_MIN_VOLTAGE_V, 1) + "/" + String(HEALTH_THRESHOLD_MAX_VOLTAGE_V, 1),
                    message,
                    uptimeMs
                );

                if (!enqueueTelemetryPayload(&alert)) {
                    delete alert;
                }
            }
        } else if (voltageAlertActive) {
            voltageAlertActive = false;
            Serial.println("[HealthMonitor] INFO: Internal voltage returned to safe levels.");
        }

        if (tempC > HEALTH_THRESHOLD_MAX_TEMP_C) {
            if (!tempAlertActive) {
                tempAlertActive = true;
                TelemetryPackage* alert = new HealthTelemetryPackage(
                    DEVICE_ID,
                    BRANCH_ID,
                    "HEALTH_ANOMALY",
                    "temperature",
                    String(tempC, 1),
                    String(HEALTH_THRESHOLD_MAX_TEMP_C, 1),
                    "High internal temperature threshold breached. Microcontroller is running hot.",
                    uptimeMs
                );

                if (!enqueueTelemetryPayload(&alert)) {
                    delete alert;
                }
            }
        } else if (tempAlertActive) {
            tempAlertActive = false;
            Serial.println("[HealthMonitor] INFO: Internal temperature returned to safe levels.");
        }
    }

    /**
     * @brief Calculates the current total weight from active load cells.
     *
     * @details
     * Current prototype uses one 5 kg load cell. Future versions can add
     * front-right, rear-left and rear-right readings here while keeping the rest
     * of the weight change detection and Edge contract unchanged.
     *
     * @return Current total weight in grams.
     */
    float calculateTotalWeightInGrams() const {
        const float weight = frontLeftLoadCell.getWeightInGrams();

        if (fabsf(weight) <= LOAD_CELL_ZERO_DEADBAND_GRAMS) {
            return 0.0f;
        }

        if (weight < 0.0f) {
            return 0.0f;
        }

        if (weight > LOAD_CELL_MAXIMUM_WEIGHT_IN_GRAMS) {
            return LOAD_CELL_MAXIMUM_WEIGHT_IN_GRAMS;
        }

        return weight;
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

    /** @brief Registers the current weight as the stable comparison baseline. */
    void registerStableWeightReading() {
        stableTotalWeightInGrams = currentTotalWeightInGrams;
        stableWeightReadingRegistered = true;
    }

    /** @brief Creates and enqueues the weight telemetry payload. */
    void enqueueWeightTelemetry() {
        char createdAt[CREATED_AT_BUFFER_SIZE];
        formatCurrentUtcTimestamp(createdAt, sizeof(createdAt));

        TelemetryPackage* telemetryPayload = new WeightTelemetryPackage(
            DEVICE_ID,
            currentTotalWeightInGrams,
            String(createdAt)
        );

        if (!enqueueTelemetryPayload(&telemetryPayload)) {
            delete telemetryPayload;
            Serial.println("[SuppliesKeeperDevice] Weight telemetry queue full. Payload discarded.");
            return;
        }

        Serial.printf("[SuppliesKeeperDevice] Weight telemetry enqueued: %.2f g\n", currentTotalWeightInGrams);
    }

    /**
     * @brief Tares the load cell and prepares the initial weight baseline.
     *
     * @details
     * The scale should be empty when this method is executed.
     */
    void calibrateWeightSensor() {
        Serial.println("[WeightSensor] Stabilizing load cell. Keep the scale empty.");
        renderDisplayLines("Weight sensor", "Stabilizing...");

        delay(LOAD_CELL_STARTUP_STABILIZATION_MS);

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
     */
    void handleWeightDataReadEvent() {
        currentTotalWeightInGrams = calculateTotalWeightInGrams();
        weightMeasuredAtMilliseconds = millis();

        Serial.printf("[WeightSensor] Reading: %.2f g\n", currentTotalWeightInGrams);
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

    /**
     * @brief Selects the MQTT topic for a serialized telemetry document.
     *
     * @param telemetryDocument Serialized telemetry document.
     * @return Topic configured for the detected telemetry type.
     */
    const char* resolveTelemetryTopic(JsonDocument& telemetryDocument) const {
        if (!telemetryDocument["temperature"].isNull() && !telemetryDocument["humidity"].isNull()) {
            return environmentTelemetryTopic.c_str();
        }

        if (!telemetryDocument["weight_grams"].isNull()) {
            return weightTelemetryTopic.c_str();
        }

        if (!telemetryDocument["alert_type"].isNull() || !telemetryDocument["metric"].isNull()) {
            return healthTelemetryTopic.c_str();
        }

        return environmentTelemetryTopic.c_str();
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

        JsonDocument telemetryDocument;
        rawQueueItemPayload->serialize(telemetryDocument);

        const char* destinationTopic = resolveTelemetryTopic(telemetryDocument);
        const bool sent = gatewayClient.publishTelemetryRecord(*rawQueueItemPayload, destinationTopic);

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
     * @param environmentTelemetryTopic MQTT topic for environmental telemetry.
     * @param weightTelemetryTopic MQTT topic for weight telemetry.
     * @param healthTelemetryTopic MQTT topic for health telemetry.
     * @param displayMode Display mode received from Edge provisioning.
     * @param productUnitLabel Product/inventory unit label received from Edge.
     * @param convertedProductQuantity Product quantity already converted by Edge.
     * @param samplingIntervalInMilliseconds Sensor sampling interval.
     * @param timerChannel ESP32 timer channel used by the framework scheduler.
     */
    SuppliesKeeperDevice(
        AuthenticatedMqttGatewayClient& gatewayClient,
        const String& environmentTelemetryTopic,
        const String& weightTelemetryTopic,
        const String& healthTelemetryTopic,
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
          environmentTelemetryTopic(environmentTelemetryTopic),
          weightTelemetryTopic(weightTelemetryTopic),
          healthTelemetryTopic(healthTelemetryTopic),
          displayMode(displayMode),
          productUnitLabel(productUnitLabel.length() == 0 ? String(PRODUCT_UNIT_LABEL_FALLBACK) : productUnitLabel),
          convertedProductQuantity(convertedProductQuantity),
          currentTemperatureInCelsius(0.0f),
          currentRelativeHumidityInPercentage(0.0f),
          stableTemperatureInCelsius(0.0f),
          stableRelativeHumidityInPercentage(0.0f),
          environmentMeasuredAtMilliseconds(0UL),
          stableEnvironmentReadingRegistered(false),
          heapAlertActive(false),
          cpuAlertActive(false),
          voltageAlertActive(false),
          tempAlertActive(false),
          currentTotalWeightInGrams(0.0f),
          stableTotalWeightInGrams(0.0f),
          weightMeasuredAtMilliseconds(0UL),
          stableWeightReadingRegistered(false) {

        Serial.println("[SuppliesKeeperDevice] Initializing device...");

        initializeAsynchronousEngine(TELEMETRY_QUEUE_LENGTH);

        const bool environmentSchedulerRegistered = appendSensorToScheduler(
            &environmentSensor,
            Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER
        );

        if (!environmentSchedulerRegistered) {
            Serial.println("[SuppliesKeeperDevice] Failed to register DHT sensor in scheduler.");
        }

        const bool weightSchedulerRegistered = appendSensorToScheduler(
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
        checkAndSendResetReason();
        updateConfiguredDisplay();

        Serial.println("[SuppliesKeeperDevice] Device ready.");
    }

    /**
     * @brief Handles MQTT responses sent by the Edge service.
     *
     * @details
     * The Edge returns the processed telemetry record through response topics.
     * This method intentionally follows the current Edge response contracts and
     * only adds the LCD fields agreed for the sprint: display_mode and
     * product_unit_label.
     *
     * Expected environment response:
     * @code{.json}
     * {
     *   "id": "environment-record-id",
     *   "device_id": "supplies-keeper-001",
     *   "temperature": 25.0,
     *   "humidity": 60.0,
     *   "temperature_is_anomaly": false,
     *   "humidity_is_anomaly": false,
     *   "created_at": "2026-08-14T06:19:12",
     *   "average_temperature": 24.8,
     *   "average_humidity": 59.7,
     *   "display_mode": "environment",
     *   "product_unit_label": "units"
     * }
     * @endcode
     *
     * Expected weight response:
     * @code{.json}
     * {
     *   "id": "weight-record-id",
     *   "device_id": "supplies-keeper-001",
     *   "raw_weight": 500.0,
     *   "physical_stock": 5.0,
     *   "created_at": "2026-08-14T06:19:12",
     *   "average_physical_stock": 4.8,
     *   "display_mode": "converted_units",
     *   "product_unit_label": "units"
     * }
     * @endcode
     *
     * @param topic MQTT response topic.
     * @param payload JSON response payload.
     */
    void handleMqttResponse(const String& topic, const String& payload) {
        (void) topic;

        JsonDocument document;
        const DeserializationError error = deserializeJson(document, payload);

        if (error) {
            Serial.print("[MQTT] Invalid response payload: ");
            Serial.println(error.c_str());
            return;
        }

        if (!document["display_mode"].isNull()) {
            displayMode = parseDisplayMode(String(document["display_mode"].as<const char*>()));
        }

        if (!document["product_unit_label"].isNull()) {
            const char* receivedProductUnitLabel = document["product_unit_label"].as<const char*>();
            productUnitLabel = (receivedProductUnitLabel != nullptr && strlen(receivedProductUnitLabel) > 0)
                ? String(receivedProductUnitLabel)
                : String(PRODUCT_UNIT_LABEL_FALLBACK);
        } else {
            productUnitLabel = PRODUCT_UNIT_LABEL_FALLBACK;
        }

        if (!document["temperature"].isNull()) {
            currentTemperatureInCelsius = document["temperature"].as<float>();
        }

        if (!document["humidity"].isNull()) {
            currentRelativeHumidityInPercentage = document["humidity"].as<float>();
        }

        if (!document["raw_weight"].isNull()) {
            currentTotalWeightInGrams = document["raw_weight"].as<float>();
        }

        if (!document["weight_grams"].isNull()) {
            currentTotalWeightInGrams = document["weight_grams"].as<float>();
        }

        if (!document["physical_stock"].isNull()) {
            convertedProductQuantity = document["physical_stock"].as<float>();
        } else if (!document["average_physical_stock"].isNull()) {
            convertedProductQuantity = document["average_physical_stock"].as<float>();
        }

        if (!document["temperature_is_anomaly"].isNull() || !document["humidity_is_anomaly"].isNull()) {
            Serial.printf(
                "[Edge] Environment anomaly flags - temperature: %s | humidity: %s\n",
                document["temperature_is_anomaly"].as<bool>() ? "true" : "false",
                document["humidity_is_anomaly"].as<bool>() ? "true" : "false"
            );
        }

        Serial.println("[LCD] Display updated from Edge response.");
        updateConfiguredDisplay();
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
            Serial.println("[SuppliesKeeperDevice] Environment change below tolerance. Telemetry skipped.");
            return;
        }

        Serial.printf(
            "[SuppliesKeeperDevice] Significant environment change detected. Previous: %.2f C / %.2f %%RH | Current: %.2f C / %.2f %%RH\n",
            stableTemperatureInCelsius,
            stableRelativeHumidityInPercentage,
            currentTemperatureInCelsius,
            currentRelativeHumidityInPercentage
        );

        registerStableEnvironmentReading();
        enqueueEnvironmentTelemetry();
    }
};

/** @brief Global WiFi connectivity driver instance. */
static WiFiConnectivityDriver* wifiConnectivityDriver = nullptr;

/** @brief Global authenticated MQTT gateway instance. */
static AuthenticatedMqttGatewayClient* mqttGatewayClient = nullptr;

/** @brief Global SuppliesKeeperDevice instance. */
static SuppliesKeeperDevice* suppliesKeeperDevice = nullptr;

/**
 * @brief Forwards MQTT responses to the active device instance.
 *
 * @param topic MQTT topic where the response was received.
 * @param payload MQTT payload received from Edge.
 */
static void onMqttMessageReceived(const String& topic, const String& payload) {
    if (suppliesKeeperDevice != nullptr) {
        suppliesKeeperDevice->handleMqttResponse(topic, payload);
    }
}

/**
 * @brief Keeps the MQTT client alive without adding logic to Arduino loop().
 *
 * @details
 * PubSubClient requires a frequent loop call to receive subscribed messages.
 * This task preserves the event-driven ModestIoT style while allowing the main
 * loop() to remain as a simple delay.
 *
 * @param parameter Unused FreeRTOS task parameter.
 */
static void runMqttClientLoopTask(void* parameter) {
    (void) parameter;

    for (;;) {
        if (mqttGatewayClient != nullptr) {
            mqttGatewayClient->loop();
        }

        vTaskDelay(pdMS_TO_TICKS(MQTT_LOOP_DELAY_MS));
    }
}

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
    const unsigned long startTime = millis();

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

    if (provisioningResult.mqttBrokerPort == 0U) {
        provisioningResult.mqttBrokerPort = MQTT_BROKER_PORT_FALLBACK;
    }

    if (provisioningResult.environmentTelemetryTopic.length() == 0) {
        provisioningResult.environmentTelemetryTopic = MQTT_ENVIRONMENT_TELEMETRY_TOPIC_FALLBACK;
    }

    if (provisioningResult.weightTelemetryTopic.length() == 0) {
        provisioningResult.weightTelemetryTopic = MQTT_WEIGHT_TELEMETRY_TOPIC_FALLBACK;
    }

    if (provisioningResult.healthTelemetryTopic.length() == 0) {
        provisioningResult.healthTelemetryTopic = MQTT_HEALTH_TELEMETRY_TOPIC_FALLBACK;
    }

    if (provisioningResult.telemetryTopic.length() == 0) {
        provisioningResult.telemetryTopic = provisioningResult.environmentTelemetryTopic;
    }

    if (provisioningResult.responseTopic.length() == 0) {
        provisioningResult.responseTopic = MQTT_RESPONSE_TOPIC_FALLBACK;
    }

    if (provisioningResult.legacyResponseTopic.length() == 0) {
        provisioningResult.legacyResponseTopic = MQTT_LEGACY_RESPONSE_TOPIC_FALLBACK;
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
 * 2. Configure NTP for created_at timestamps.
 * 3. Ask Edge for MQTT credentials/display configuration when available.
 * 4. Apply development fallbacks if Edge provisioning is not available.
 * 5. Create authenticated MQTT gateway.
 * 6. Start the event-driven device.
 * 7. Subscribe to Edge response topics and start the MQTT listener task.
 */
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);

    Serial.println();
    Serial.println("[System Boot] Restock Embedded Application");
    Serial.println("[System Boot] SuppliesKeeperDevice starting...");

    wifiConnectivityDriver = new RestockWiFiConnectivityDriver(WIFI_SSID, WIFI_PASSWORD);

    const bool networkReady = waitForNetworkConnection(
        *wifiConnectivityDriver,
        WIFI_CONNECTION_TIMEOUT_MS
    );

    if (networkReady) {
        Serial.println("[System Boot] WiFi connected.");
        configTime(
            NTP_GMT_OFFSET_SECONDS,
            NTP_DAYLIGHT_OFFSET_SECONDS,
            NTP_SERVER
        );
        Serial.println("[System Boot] NTP time sync requested.");
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
    Serial.printf("  MQTT broker: %s:%u\n", provisioningResult.mqttBrokerHost.c_str(), provisioningResult.mqttBrokerPort);
    Serial.printf("  Environment topic: %s\n", provisioningResult.environmentTelemetryTopic.c_str());
    Serial.printf("  Weight topic: %s\n", provisioningResult.weightTelemetryTopic.c_str());
    Serial.printf("  Health topic: %s\n", provisioningResult.healthTelemetryTopic.c_str());
    Serial.printf("  Response topic: %s\n", provisioningResult.responseTopic.c_str());
    Serial.printf("  Legacy response topic: %s\n", provisioningResult.legacyResponseTopic.c_str());
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

    mqttGatewayClient->setMessageHandler(onMqttMessageReceived);

    suppliesKeeperDevice = new SuppliesKeeperDevice(
        *mqttGatewayClient,
        provisioningResult.environmentTelemetryTopic,
        provisioningResult.weightTelemetryTopic,
        provisioningResult.healthTelemetryTopic,
        parseDisplayMode(provisioningResult.displayMode),
        provisioningResult.productUnitLabel,
        provisioningResult.convertedQuantity,
        DEVICE_SAMPLING_INTERVAL_MS,
        CORE_TIMER_CHANNEL
    );

    mqttGatewayClient->configureResponseSubscriptions(
        provisioningResult.responseTopic,
        provisioningResult.legacyResponseTopic
    );

    xTaskCreate(
        runMqttClientLoopTask,
        "RestockMqttLoop",
        MQTT_LOOP_TASK_STACK_SIZE,
        nullptr,
        MQTT_LOOP_TASK_PRIORITY,
        nullptr
    );

    Serial.println("[System Boot] SuppliesKeeperDevice active.");
}

/**
 * @brief Arduino loop entry point.
 *
 * @details
 * ModestIoT schedules sensors and a dedicated FreeRTOS task keeps MQTT alive.
 * The main Arduino loop is intentionally minimal for the course framework.
 */
void loop() {
    delay(MQTT_LOOP_DELAY_MS);
}
