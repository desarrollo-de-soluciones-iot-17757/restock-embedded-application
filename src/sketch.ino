/**
 * @file sketch.ino
 * @brief Entry point for the Restock Embedded Application.
 *
 * @details
 * This file initializes the base embedded application for the Restock
 * smart inventory scale device. The current version is part of the initial
 * setup and validates that the PlatformIO project, ESP32 runtime and device
 * lifecycle are working correctly.
 *
 * In this initial setup, the Arduino `loop()` function delegates execution to
 * `SmartInventoryScaleDevice::update()` only as a temporary mechanism to verify
 * recurrent behavior during development.
 *
 * In the final ModestIoT-oriented implementation, `loop()` should remain empty
 * or act only as a minimal event dispatcher. Business behavior should be
 * triggered by events generated from sensors, timers, interrupts or
 * communication callbacks, following the event-driven style of the framework.
 *
 * @author Gabriela Shapiama
 * @date May 31, 2026
 * @version 0.1
 */

#include <Arduino.h>
#include "SmartInventoryScaleDevice.h"

/**
 * @brief Global instance of the Restock smart inventory scale device.
 *
 * This object represents the physical embedded device responsible for
 * coordinating inventory sensing, environmental monitoring and telemetry
 * delivery in future iterations.
 */
SmartInventoryScaleDevice device;

/**
 * @brief Initializes the ESP32 runtime and the Restock embedded device.
 *
 * This function is executed once when the microcontroller starts. It initializes
 * serial communication and delegates device-specific initialization to the
 * SmartInventoryScaleDevice instance.
 */
void setup() {
    Serial.begin(115200);
    delay(500);

    device.begin();
}

/**
 * @brief Executes temporary recurrent behavior for the initial setup.
 *
 * @note This use of `loop()` is temporary. It exists only to validate the
 * initial lifecycle of the device while the event-driven components are still
 * being integrated.
 *
 * In the final ModestIoT-based version, this function should remain empty or
 * contain only minimal event dispatching. Sensor readings, telemetry sending
 * and device reactions should be triggered through events, timers or callbacks
 * instead of placing business logic directly inside `loop()`.
 */
void loop() {
    device.update();
}