#ifndef CONFIG_H
#define CONFIG_H

/**
 * @file Config.h
 * @brief Central configuration for the Restock SuppliesKeeperDevice embedded app.
 *
 * @details
 * Groups device identity, WiFi, Edge provisioning, MQTT contracts, LCD settings,
 * sensor calibration and device-side detection thresholds. Constants are used to
 * avoid magic numbers in the application code.
 *
 * Current prototype scope:
 * - One DHT22 environmental sensor.
 * - One HX711 load cell channel.
 * - MQTT telemetry through the broker consumed by the Edge service.
 * - Dynamic LCD updates from Edge response topics.
 *
 * @author Gabriela Shapiama
 * @date Jul 06, 2026
 * @version 0.6
 */

#include <Arduino.h>

/** @brief Serial monitor baud rate. */
static const unsigned long SERIAL_BAUD_RATE = 115200UL;

/** @brief Device identifier literal used to compose static MQTT topics. */
#define RESTOCK_DEVICE_ID_LITERAL "supplies-keeper-001"

/** @brief Unique logical identifier of this embedded device. */
static const char* const DEVICE_ID = RESTOCK_DEVICE_ID_LITERAL;

/** @brief Branch/store/restaurant identifier associated with the device. */
static const char* const BRANCH_ID = "branch-001";

/** @brief Factory API key used only for the initial HTTP provisioning request. */
static const char* const DEVICE_API_KEY = "dev-restock-api-key";

/** @brief WiFi SSID used in Wokwi or local testing. */
static const char* const WIFI_SSID = "Gaby";

/** @brief WiFi password used in Wokwi or local testing. */
static const char* const WIFI_PASSWORD = "evangelion";

/** @brief Maximum time to wait for WiFi connection during setup. */
static const unsigned long WIFI_CONNECTION_TIMEOUT_MS = 15000UL;

/**
 * @brief Edge provisioning endpoint used by the device at boot time.
 *
 * @details
 * Do not use http://localhost:5000 from the ESP32 because localhost would point
 * to the ESP32 itself. Replace this IP with the host or VM IP where the Edge is
 * reachable from the ESP32 network. Provisioning remains optional while the Edge
 * endpoint is not ready because development fallbacks are enabled below.
 */
static const char* const EDGE_DEVICE_PROVISIONING_URL =
    "http://192.168.1.50:5000/api/v1/edge/devices/provision";

/** @brief HTTP timeout for Edge provisioning requests. */
static const unsigned long EDGE_PROVISIONING_TIMEOUT_MS = 10000UL;

/**
 * @brief Allows the device to continue using local values when provisioning fails.
 *
 * @details
 * Set to false when the real Edge provisioning flow returns MQTT credentials.
 */
static const bool ALLOW_DEVELOPMENT_PROVISIONING_FALLBACK = true;

/**
 * @brief Fallback MQTT broker host for local development.
 *
 * @details
 * Replace this value with the IP of the Alpine VM or machine that runs Mosquitto.
 */
static const char* const MQTT_BROKER_HOST_FALLBACK = "10.193.31.93";

/**
 * @brief Enables MQTT username/password authentication.
 *
 * For the current local integration, the broker is running without security.
 * Therefore, the device must be able to connect without credentials.
 */
static const bool MQTT_AUTHENTICATION_ENABLED = true;

/** @brief Fallback MQTT broker port for local development. */
static const uint16_t MQTT_BROKER_PORT_FALLBACK = 1883;

/** @brief Fallback MQTT username configured in Mosquitto password_file.txt. */
static const char* const MQTT_USERNAME_FALLBACK = "restock_user";

/** @brief Fallback MQTT password configured in Mosquitto password_file.txt. */
static const char* const MQTT_PASSWORD_FALLBACK = "restock_pass_123";

/** @brief MQTT topic for environmental telemetry consumed by Edge. */
static const char* const MQTT_ENVIRONMENT_TELEMETRY_TOPIC_FALLBACK =
    "stores/" RESTOCK_DEVICE_ID_LITERAL "/telemetry/environment";

/** @brief MQTT topic for weight telemetry consumed by Edge. */
static const char* const MQTT_WEIGHT_TELEMETRY_TOPIC_FALLBACK =
    "stores/" RESTOCK_DEVICE_ID_LITERAL "/telemetry/weight";

/** @brief MQTT topic for device health events consumed by Edge. */
static const char* const MQTT_HEALTH_TELEMETRY_TOPIC_FALLBACK =
    "stores/" RESTOCK_DEVICE_ID_LITERAL "/health";

/**
 * @brief Canonical Edge-to-embedded response topic.
 *
 * @details
 * The embedded subscribes to this topic so Edge can return display configuration,
 * converted stock values and unit labels after processing telemetry.
 */
static const char* const MQTT_RESPONSE_TOPIC_FALLBACK =
    "stores/" RESTOCK_DEVICE_ID_LITERAL "/response/#";

/**
 * @brief Temporary legacy response topic supported while Edge is corrected.
 *
 * @details
 * Some current Edge code publishes to store/<device_id>/response/... without the
 * plural "stores" prefix. Subscribing to both topics lets the prototype advance
 * without blocking the embedded application.
 */
static const char* const MQTT_LEGACY_RESPONSE_TOPIC_FALLBACK =
    "store/" RESTOCK_DEVICE_ID_LITERAL "/response/#";

/** @brief MQTT loop cadence required by PubSubClient to receive response messages. */
static const unsigned long MQTT_LOOP_DELAY_MS = 100UL;

/** @brief FreeRTOS stack size reserved for the MQTT listener task. */
static const uint32_t MQTT_LOOP_TASK_STACK_SIZE = 4096UL;

/** @brief FreeRTOS priority used by the MQTT listener task. */
static const uint8_t MQTT_LOOP_TASK_PRIORITY = 1U;

/** @brief Buffer size for UTC timestamps formatted as yyyy-MM-ddTHH:mm:ssZ. */
static const size_t CREATED_AT_BUFFER_SIZE = 25U;

/** @brief NTP server used to populate Edge-compatible created_at timestamps. */
static const char* const NTP_SERVER = "pool.ntp.org";

/** @brief UTC offset in seconds. Edge payloads are generated in UTC. */
static const long NTP_GMT_OFFSET_SECONDS = 0L;

/** @brief Daylight saving offset in seconds. UTC payloads do not apply DST. */
static const int NTP_DAYLIGHT_OFFSET_SECONDS = 0;

/** @brief Default display mode if Edge does not return one. */
static const char* const DISPLAY_MODE_FALLBACK = "CONVERTED_UNITS";

/** @brief Default product unit label if Edge does not return one. */
static const char* const PRODUCT_UNIT_LABEL_FALLBACK = "units";

/** @brief Default converted product quantity if Edge does not return one. */
static const float CONVERTED_QUANTITY_FALLBACK = 0.0f;

/** @brief GPIO pin connected to the DHT22 data line. */
static const int ENVIRONMENT_SENSOR_PIN = 15;

/** @brief Sensor sampling interval used by the ModestIoT scheduler. */
static const unsigned long DEVICE_SAMPLING_INTERVAL_MS = 2000UL;

/** @brief Minimum temperature variation required to enqueue telemetry. */
static const float SENSOR_TEMPERATURE_CHANGE_TOLERANCE_C = 0.5f;

/** @brief Minimum humidity variation required to enqueue telemetry. */
static const float SENSOR_HUMIDITY_CHANGE_TOLERANCE_RH = 2.0f;

/** @brief Indicates whether the first valid environment reading should be published. */
static const bool SEND_INITIAL_ENVIRONMENT_READING_TO_EDGE = true;

/** @brief GPIO pin connected to the HX711 data line for the active load cell. */
static const int FRONT_LEFT_LOAD_CELL_DATA_PIN = 4;

/**
 * @brief GPIO pin connected to the HX711 clock line for the active load cell.
 *
 * @details
 * Keep this value aligned with the physical wiring. If Wokwi still uses GPIO 2,
 * either change the diagram to GPIO 5 or set this constant to 2 for simulation.
 */
static const int FRONT_LEFT_LOAD_CELL_CLOCK_PIN = 5;

/** @brief Minimum supported weight for one load cell mapping. */
static const float LOAD_CELL_MINIMUM_WEIGHT_IN_GRAMS = 0.0f;

/** @brief Maximum supported weight for one 5 kg load cell. */
static const float LOAD_CELL_MAXIMUM_WEIGHT_IN_GRAMS = 5000.0f;

/** @brief Raw HX711 value mapped to the minimum weight. */
static const long LOAD_CELL_MINIMUM_RAW_VALUE = 0L;

/**
 * @brief Raw HX711 value mapped to the maximum weight.
 *
 * @details
 * Use 2100L for Wokwi-like tests. For the physical prototype, replace this with
 * the calibrated maximum raw value measured with a known 5 kg reference or with
 * your chosen calibration weight.
 */
//static const long LOAD_CELL_MAXIMUM_RAW_VALUE = 2100L; //WOKWI RAW
//PHYSICAL PROTOTYPE RAW
static const long LOAD_CELL_MAXIMUM_RAW_VALUE = 1740000L;

/** @brief Values within this range are treated as zero to reduce HX711 noise. */
static const float LOAD_CELL_ZERO_DEADBAND_GRAMS = 5.0f;

/** @brief Startup delay before taring the load cell. */
static const unsigned long LOAD_CELL_STARTUP_STABILIZATION_MS = 10000UL;

/** @brief Moving average depth used by the framework load cell adapter. */
static const int LOAD_CELL_FILTER_DEPTH = 10;

/** @brief Minimum weight variation required to send telemetry to Edge. */
static const float WEIGHT_CHANGE_TOLERANCE_IN_GRAMS = 50.0f;

/** @brief Indicates whether the first valid weight reading should be sent. */
static const bool SEND_INITIAL_WEIGHT_READING_TO_EDGE = true;

/** @brief I2C address of the LCD1602 display. */
static const uint8_t LCD_I2C_ADDRESS = 0x27;

/** @brief Number of LCD columns. */
static const uint8_t LCD_COLUMNS = 16;

/** @brief Number of LCD rows. */
static const uint8_t LCD_ROWS = 2;

/** @brief ESP32 hardware timer channel used by the ModestIoT scheduler. */
static const uint8_t CORE_TIMER_CHANNEL = 0;

/** @brief Maximum number of telemetry payloads waiting in the framework queue. */
static const uint8_t TELEMETRY_QUEUE_LENGTH = 8;

/** @brief MQTT payload buffer size used by the authenticated Restock client. */
static const uint16_t MQTT_PAYLOAD_BUFFER_SIZE = 512;

/** @brief Health monitoring thresholds. */
static const uint32_t HEALTH_THRESHOLD_MIN_FREE_HEAP_BYTES = 50000UL;
static const float HEALTH_THRESHOLD_MAX_CPU_USAGE_PERCENT = 85.0f;
static const float HEALTH_THRESHOLD_MIN_VOLTAGE_V = 3.0f;
static const float HEALTH_THRESHOLD_MAX_VOLTAGE_V = 3.6f;
static const float HEALTH_THRESHOLD_MAX_TEMP_C = 80.0f;

#endif // CONFIG_H
