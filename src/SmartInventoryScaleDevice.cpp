#include "SmartInventoryScaleDevice.h"
#include <Arduino.h>

SmartInventoryScaleDevice::SmartInventoryScaleDevice()
    : lastUpdateTime(0) {}

void SmartInventoryScaleDevice::begin() {
    Serial.println("Restock Embedded Application");
    Serial.println("SmartInventoryScaleDevice initial setup ready");
    Serial.println("Waiting for environmental sensor integration...");
}

void SmartInventoryScaleDevice::update() {
    unsigned long now = millis();

    if (now - lastUpdateTime >= UPDATE_INTERVAL_MS) {
        Serial.println("SmartInventoryScaleDevice running...");
        Serial.println("Base lifecycle OK");

        lastUpdateTime = now;
    }
}

void SmartInventoryScaleDevice::on(Event event) {
    Serial.printf("Event received. Event ID: %d\n", event.id);
}

void SmartInventoryScaleDevice::handle(Command command) {
    Serial.printf("Command received. Command ID: %d\n", command.id);
}


