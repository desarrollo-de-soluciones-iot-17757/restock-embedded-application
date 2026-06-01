#ifndef SMART_INVENTORY_SCALE_DEVICE_H
#define SMART_INVENTORY_SCALE_DEVICE_H

/**
 * @file SmartInventoryScaleDevice.h
 * @brief Declares the SmartInventoryScaleDevice class.
 *
 * @details
 * SmartInventoryScaleDevice represents the complete physical Restock device:
 * ESP32, environmental sensor, weight sensor, display and telemetry client.
 *
 * It coordinates sensor events using the ModestIoT event-driven style. Physical
 * outputs are handled as actuators, while Edge communication is delegated to
 * TelemetryClient as a communication component.
 *
 * @author Gabriela Shapiama
 * @date May 31, 2026
 * @version 0.2
 */

#include "Device.h"
#include "EnvironmentSensor.h"
#include "WeightSensor.h"
#include "Display.h"
#include "TelemetryClient.h"

/**
 * @brief Main device coordinator for the Restock smart inventory scale.
 */
class SmartInventoryScaleDevice : public Device {
private:
    EnvironmentSensor environmentSensor; ///< Temperature and humidity sensor.
    WeightSensor weightSensor;           ///< Weight sensor abstraction.
    Display display;                     ///< LCD display actuator.
    TelemetryClient telemetryClient;     ///< Edge communication component.

    String buildEnvironmentTelemetryPayload() const;

public:
    SmartInventoryScaleDevice();

    void begin();

    void on(Event event) override;

    void handle(Command command) override;
};

#endif // SMART_INVENTORY_SCALE_DEVICE_H