#include "WeightSensor.h"
#include <Arduino.h>

const Event WeightSensor::WEIGHT_READING_TAKEN_EVENT(
    WeightSensor::WEIGHT_READING_TAKEN_EVENT_ID
);

WeightSensor::WeightSensor(int pin, EventHandler* eventHandler)
    : Sensor(pin, eventHandler) {}

void WeightSensor::begin() {
    Serial.println("WeightSensor ready");
}