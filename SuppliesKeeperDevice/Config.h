#ifndef CONFIG_H
#define CONFIG_H

/**
 * @file Config.h
 * @brief Central configuration for the Restock SuppliesKeeperDevice.
 *
 * @details
 * Defines device identifiers, WiFi credentials, Edge provisioning endpoint,
 * MQTT fallback configuration, LCD parameters, sensor pins and device-side
 * change detection tolerances.
 *
 * This file intentionally keeps development fallback values because Wokwi and
 * local demos may not have a real Edge Gateway or broker provisioning endpoint.
 *
 * @author Gabriela Shapiama
 * @date Jun 15, 2026
 * @version 0.3
 */

#include <Arduino.h>

/**
 * @brief Serial monitor baud rate.
 */
static const unsigned long SERIAL_BAUD_RATE = 115200UL;

/**
 * @brief Unique logical identifier of this embedded device.
 *
 * @details
 * In production this could be mapped to the ESP32 MAC address, a serial number,
 * or another identifier registered in Cloud and synchronized to Edge.
 */
static const char* const DEVICE_ID = "supplies-keeper-001";

/**
 * @brief Branch/store/restaurant identifier associated with the device.
 */
static const char* const BRANCH_ID = "branch-001";

/**
 * @brief Factory API key used only for the initial HTTP provisioning request.
 *
 * @details
 * The device sends this value to the Edge Gateway together with DEVICE_ID.
 * The Edge validates the device and returns MQTT credentials.
 */
static const char* const DEVICE_API_KEY = "dev-restock-api-key";

/**
 * @brief WiFi SSID used in Wokwi or local testing.
 */
static const char* const WIFI_SSID = "Wokwi-GUEST";

/**
 * @brief WiFi password used in Wokwi or local testing.
 */
static const char* const WIFI_PASSWORD = "";

/**
 * @brief Maximum time to wait for WiFi connection during setup.
 */
static const unsigned long WIFI_CONNECTION_TIMEOUT_MS = 15000UL;

/**
 * @brief Edge provisioning endpoint used by the device at boot time.
 *
 * @details
 * Expected request body:
 * {
 *   "device_id": "...",
 *   "mac_address": "..."
 * }
 *
 * Expected request header:
 * X-API-Key: <factory api key>
 *
 * Expected response body:
 * {
 *   "authorized": true,
 *   "mqtt_client_id": "...",
 *   "mqtt_user": "...",
 *   "mqtt_pass": "...",
 *   "mqtt_broker_host": "...",
 *   "mqtt_broker_port": 1883,
 *   "telemetry_topic": "...",
 *   "display_mode": "environment",
 *   "product_unit_label": "botellas",
 *   "converted_quantity": 4
 * }
 */
static const char* const EDGE_DEVICE_PROVISIONING_URL =
    "http://example.com/api/v1/edge/devices/provision";

/**
 * @brief HTTP timeout for Edge provisioning requests.
 */
static const unsigned long EDGE_PROVISIONING_TIMEOUT_MS = 10000UL;

/**
 * @brief Allows the device to continue using local values when provisioning fails.
 *
 * @details
 * Set to false when the real Edge provisioning flow is ready.
 */
static const bool ALLOW_DEVELOPMENT_PROVISIONING_FALLBACK = true;

/**
 * @brief Fallback MQTT broker host for Wokwi/local development.
 */
static const char* const MQTT_BROKER_HOST_FALLBACK = "broker.hivemq.com";

/**
 * @brief Fallback MQTT broker port for Wokwi/local development.
 */
static const uint16_t MQTT_BROKER_PORT_FALLBACK = 1883;

/**
 * @brief Fallback MQTT username.
 *
 * @details
 * Empty value allows anonymous broker connections in development brokers.
 */
static const char* const MQTT_USERNAME_FALLBACK = "";

/**
 * @brief Fallback MQTT password.
 *
 * @details
 * Empty value allows anonymous broker connections in development brokers.
 */
static const char* const MQTT_PASSWORD_FALLBACK = "";

/**
 * @brief Fallback MQTT topic for telemetry publishing.
 */
static const char* const MQTT_TELEMETRY_TOPIC_FALLBACK =
    "restock/edges/edge-001/devices/supplies-keeper-001/telemetry";

/**
 * @brief Default display mode if Edge does not return one.
 */
static const char* const DISPLAY_MODE_FALLBACK = "environment";

/**
 * @brief Default product unit label if Edge does not return one.
 *
 * @details
 * This refers to inventory/product units such as kg, bottles, boxes or units.
 * Temperature is always displayed in Celsius.
 */
static const char* const PRODUCT_UNIT_LABEL_FALLBACK = "unidades";

/**
 * @brief Default converted product quantity if Edge does not return one.
 */
static const float CONVERTED_QUANTITY_FALLBACK = 0.0f;

/**
 * @brief GPIO pin connected to the DHT22 data line.
 */
static const int ENVIRONMENT_SENSOR_PIN = 15;

/**
 * @brief Sensor sampling interval used by the ModestIoT scheduler.
 *
 * @details
 * This controls how often the DHT sensor is asked to measure data. Telemetry
 * is still sent reactively only when the Device detects a significant change.
 */
static const unsigned long DEVICE_SAMPLING_INTERVAL_MS = 2000UL;

/**
 * @brief Minimum temperature variation required to enqueue telemetry.
 */
static const float SENSOR_TEMPERATURE_CHANGE_TOLERANCE_C = 0.5f;

/**
 * @brief Minimum humidity variation required to enqueue telemetry.
 */
static const float SENSOR_HUMIDITY_CHANGE_TOLERANCE_RH = 2.0f;

/**
 * @brief Indicates whether the first valid reading should be published.
 *
 * @details
 * false means telemetry starts only after a detected change. The first reading
 * is still used as the baseline.
 */
static const bool SEND_INITIAL_ENVIRONMENT_READING_TO_EDGE = false;


/**
 * @brief GPIO pin connected to the HX711 data line for the front-left load cell.
 *
 * @details
 * Current prototype uses only this 5 kg load cell. Future versions may add
 * three additional load cells following the same ModestIoT pattern.
 */
static const int FRONT_LEFT_LOAD_CELL_DATA_PIN = 4;

/**
 * @brief GPIO pin connected to the HX711 clock line for the front-left load cell.
 */
static const int FRONT_LEFT_LOAD_CELL_CLOCK_PIN = 5;

/**
 * @brief Minimum supported weight for one load cell mapping.
 */
static const float LOAD_CELL_MINIMUM_WEIGHT_IN_GRAMS = 0.0f;

/**
 * @brief Maximum supported weight for one 5 kg load cell.
 *
 * @details
 * This is the maximum weight of one physical cell, not the total maximum
 * of the complete scale. If four 5 kg cells are used independently, each
 * LoadCellAmplifier still uses 5000 g, and the total scale capacity becomes
 * 20000 g after summing all readings.
 */
static const float LOAD_CELL_MAXIMUM_WEIGHT_IN_GRAMS = 5000.0f;

/**
 * @brief Raw HX711 value mapped to the minimum weight.
 *
 * @details
 * For real hardware, this value should be adjusted through calibration.
 */
static const long LOAD_CELL_MINIMUM_RAW_VALUE = 0;

/**
 * @brief Raw HX711 value mapped to the maximum weight.
 *
 * @details
 * This value is acceptable for Wokwi/demo behavior. Real hardware should
 * be calibrated using a known weight.
 */
static const long LOAD_CELL_MAXIMUM_RAW_VALUE = 2100L; //WOKWI RAW
//PHYSICAL PROTOTYPE RAW
//static const long LOAD_CELL_MAXIMUM_RAW_VALUE = 1740000L;

static const long LOAD_CELL_ZERO_DEADBAND_GRAMS = 5.0f;
static const long LOAD_CELL_STARTUP_STABILIZATION_MS = 10000UL;


/**
 * @brief Moving average depth used by the framework load cell adapter.
 */
static const int LOAD_CELL_FILTER_DEPTH = 10;

/**
 * @brief Minimum weight variation required to send telemetry to Edge.
 *
 * @details
 * The framework detects physical changes. This threshold represents the
 * Restock rule for a meaningful inventory variation.
 */
static const float WEIGHT_CHANGE_TOLERANCE_IN_GRAMS = 50.0f;

/**
 * @brief Indicates whether the first valid weight reading should be sent.
 *
 * @details
 * Set to false to keep the same reactive behavior as the environment sensor:
 * the first reading only initializes the baseline.
 */
static const bool SEND_INITIAL_WEIGHT_READING_TO_EDGE = false;



/**
 * @brief I2C address of the LCD1602 display.
 */
static const uint8_t LCD_I2C_ADDRESS = 0x27;

/**
 * @brief Number of LCD columns.
 */
static const uint8_t LCD_COLUMNS = 16;

/**
 * @brief Number of LCD rows.
 */
static const uint8_t LCD_ROWS = 2;

/**
 * @brief ESP32 hardware timer channel used by the ModestIoT scheduler.
 */
static const uint8_t CORE_TIMER_CHANNEL = 0;

/**
 * @brief Maximum number of telemetry payloads waiting in the framework queue.
 */
static const uint8_t TELEMETRY_QUEUE_LENGTH = 8;

/**
 * @brief MQTT payload buffer size used by the authenticated Restock client.
 */
static const uint16_t MQTT_PAYLOAD_BUFFER_SIZE = 512;

/**
 * @brief Health monitoring thresholds.
 */
static const uint32_t HEALTH_THRESHOLD_MIN_FREE_HEAP_BYTES = 50000UL;
static const float HEALTH_THRESHOLD_MAX_CPU_USAGE_PERCENT = 85.0f;
static const float HEALTH_THRESHOLD_MIN_VOLTAGE_V = 3.0f;
static const float HEALTH_THRESHOLD_MAX_VOLTAGE_V = 3.6f;
static const float HEALTH_THRESHOLD_MAX_TEMP_C = 80.0f;

#endif // CONFIG_H