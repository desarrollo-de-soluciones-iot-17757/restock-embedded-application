#ifndef WEIGHT_SENSOR_H
#define WEIGHT_SENSOR_H

/**
 * @file WeightSensor.h
 * @brief Declares the WeightSensor class.
 *
 * @details
 * WeightSensor represents the weight sensing module of the Restock smart
 * inventory scale. It is part of the complete physical device model.
 *
 * @author Restock Team
 * @date May 2026
 * @version 0.1
 */

#include "Sensor.h"

class WeightSensor : public Sensor {
public:
    static const int WEIGHT_READING_TAKEN_EVENT_ID = 2101;
    static const Event WEIGHT_READING_TAKEN_EVENT;

    explicit WeightSensor(int pin, EventHandler* eventHandler = nullptr);

    void begin();
};

#endif