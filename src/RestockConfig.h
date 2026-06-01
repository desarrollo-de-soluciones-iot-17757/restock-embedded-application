#ifndef RESTOCK_CONFIG_H
#define RESTOCK_CONFIG_H

/**
 * @file RestockConfig.h
 * @brief Defines hardware, network and telemetry configuration for Restock.
 *
 * @details
 * Centralizes device identifiers, sensor pins, LCD settings, WiFi credentials,
 * Edge Service endpoint configuration and environment change detection values.
 *
 * The API key is a development placeholder. In a real deployment, the key
 * should be provisioned during device registration and validated by the Edge
 * Service.
 *
 * @author Gabriela Shapiama
 * @date May 31, 2026
 * @version 0.1
 */

#define DEVICE_ID "restock-scale-001"
#define BRANCH_ID "branch-001"

#define ENVIRONMENT_SENSOR_PIN 15

/**
 * @brief Internal interval used by EnvironmentSensor to check environmental values.
 *
 * @details
 * This interval does not represent periodic telemetry delivery. It is only used
 * by the sensor to detect whether temperature or humidity changed enough to
 * emit an event.
 */
#define ENVIRONMENT_MONITORING_INTERVAL_MS 5000

/**
 * @brief Minimum temperature variation required to emit an environmental event.
 *
 * @details
 * This value is a device-side tolerance used to avoid emitting events caused by
 * sensor noise. It is not a business threshold for product safety.
 */
#define SENSOR_TEMPERATURE_CHANGE_TOLERANCE_C 0.5f

/**
 * @brief Minimum humidity variation required to emit an environmental event.
 *
 * @details
 * This value is a device-side tolerance used to avoid emitting events caused by
 * sensor noise. It is not a business threshold for product safety.
 */
#define SENSOR_HUMIDITY_CHANGE_TOLERANCE_RH 2.0f

#define LCD_I2C_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

#define EDGE_SERVICE_BASE_URL "http://example.com"
#define ENVIRONMENT_TELEMETRY_ENDPOINT "/api/v1/environment-readings"

#define DEVICE_API_KEY "dev-restock-api-key"

#endif // RESTOCK_CONFIG_H