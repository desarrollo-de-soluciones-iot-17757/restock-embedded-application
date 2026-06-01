/**
 * @file sketch.ino
 * @brief Entry point for the Restock Embedded Application.
 *
 * @details
 * Initializes the Restock smart inventory scale device.
 *
 * This sketch follows the event-driven style promoted by the ModestIoT
 * Nano-framework. The Arduino loop does not contain polling logic. In the
 * environmental telemetry flow, EnvironmentSensor acts as the event source:
 * it encapsulates environmental monitoring and emits events only when a
 * significant change in temperature or humidity is detected.
 *
 * @author Gabriela Shapiama
 * @date May 31, 2026
 * @version 0.2
 */

#include <Arduino.h>
#include "SmartInventoryScaleDevice.h"

SmartInventoryScaleDevice device; ///< Main Restock smart inventory scale device.

/**
 * @brief Initializes the embedded application.
 */
void setup() {
    Serial.begin(115200);
    delay(500);

    device.begin();

    Serial.println("EnvironmentSensor change detection enabled");
}

/**
 * @brief Keeps the Arduino loop free of business logic.
 *
 * @details
 * No polling is needed. EnvironmentSensor emits change events and the device
 * reacts through the ModestIoT event-driven flow.
 */
void loop() {
    // No polling needed; EnvironmentSensor and framework handle behavior.
} 