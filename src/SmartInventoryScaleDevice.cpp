/**
 * @file SmartInventoryScaleDevice.cpp
 * @brief Implements the SmartInventoryScaleDevice class.
 *
 * @details
 * Coordinates the Restock smart inventory scale components using the ModestIoT
 * event-driven style.
 *
 * The device receives events from sensors and converts them into actions for
 * the corresponding components. Physical outputs, such as the LCD display, are
 * handled through actuator commands. Communication with the Edge Service is
 * delegated to TelemetryClient, which is modeled as a communication component
 * instead of a physical actuator.
 *
 * For the environmental telemetry flow, EnvironmentSensor acts as the event
 * source. When it detects a significant temperature or humidity change, the
 * device updates the display and sends the environmental data to the Edge
 * Service.
 *
 * @author Gabriela Shapiama
 * @date Jun 1, 2026
 * @version 0.3
 */

#include "SmartInventoryScaleDevice.h"
#include "RestockConfig.h"
#include <Arduino.h>

/**
 * @brief Constructs the smart inventory scale device and its components.
 */
SmartInventoryScaleDevice::SmartInventoryScaleDevice()
    : environmentSensor(
          ENVIRONMENT_SENSOR_PIN,
          ENVIRONMENT_MONITORING_INTERVAL_MS,
          SENSOR_TEMPERATURE_CHANGE_TOLERANCE_C,
          SENSOR_HUMIDITY_CHANGE_TOLERANCE_RH,
          this
      ),
      weightSensor(-1, this),
      display(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS),
      telemetryClient(
          WIFI_SSID,
          WIFI_PASSWORD,
          EDGE_SERVICE_BASE_URL,
          DEVICE_API_KEY
      ) {}

/**
 * @brief Initializes device components.
 */
void SmartInventoryScaleDevice::begin() {
    Serial.println("Restock Embedded Application");
    Serial.println("Smart inventory scale device starting...");

    display.begin();
    environmentSensor.begin();
    weightSensor.begin();
    telemetryClient.begin();

    Serial.println("Smart inventory scale device ready");
}

/**
 * @brief Handles events emitted by sensors.
 *
 * @param event Event received by the device.
 */
void SmartInventoryScaleDevice::on(Event event) {
    if (event == EnvironmentSensor::ENVIRONMENT_INITIAL_READING_TAKEN_EVENT ||
        event == EnvironmentSensor::ENVIRONMENT_CHANGE_DETECTED_EVENT) {
        
        display.setEnvironmentValues(
            environmentSensor.getTemperature(),
            environmentSensor.getHumidity()
        );

        handle(Display::SHOW_ENVIRONMENT_READING_COMMAND);

        telemetryClient.sendTelemetry(
            ENVIRONMENT_TELEMETRY_ENDPOINT,
            buildEnvironmentTelemetryPayload()
        );

        display.setTelemetryResult(
            telemetryClient.wasLastSendSuccessful(),
            telemetryClient.getLastStatusCode()
        );

        handle(Display::SHOW_TELEMETRY_RESULT_COMMAND);
        return;
    }

    if (event == EnvironmentSensor::ENVIRONMENT_READING_FAILED_EVENT) {
        handle(Display::SHOW_SENSOR_ERROR_COMMAND);
        return;
    }

    Serial.printf("Unhandled event ID: %d\n", event.id);
}

/**
 * @brief Handles commands and delegates them to physical actuators.
 *
 * @details
 * This method is used for components that follow the ModestIoT command-driven
 * actuator model. In this implementation, Display is a physical actuator and
 * therefore receives commands. TelemetryClient is not handled here because it
 * is a communication component, not a physical actuator.
 *
 * @param command Command received by the device.
 */
void SmartInventoryScaleDevice::handle(Command command) {
    if (command == Display::SHOW_STARTUP_MESSAGE_COMMAND ||
        command == Display::SHOW_ENVIRONMENT_READING_COMMAND ||
        command == Display::SHOW_SENSOR_ERROR_COMMAND ||
        command == Display::SHOW_TELEMETRY_RESULT_COMMAND) {
        display.handle(command);
        return;
    }

    Serial.printf("Unhandled command ID: %d\n", command.id);
}

/**
 * @brief Builds the environmental telemetry JSON payload.
 *
 * @return JSON payload containing device, branch, temperature, humidity and timestamp.
 */
String SmartInventoryScaleDevice::buildEnvironmentTelemetryPayload() const {
    String payload = "{";
    payload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
    payload += "\"branch_id\":\"" + String(BRANCH_ID) + "\",";
    payload += "\"temperature\":" + String(environmentSensor.getTemperature(), 2) + ",";
    payload += "\"humidity\":" + String(environmentSensor.getHumidity(), 2) + ",";
    payload += "\"measured_at_ms\":" + String(environmentSensor.getMeasuredAtMs());
    payload += "}";

    return payload;
}