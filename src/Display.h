#ifndef DISPLAY_H
#define DISPLAY_H

/**
 * @file Display.h
 * @brief Declares the Display actuator class.
 *
 * @details
 * Display represents the LCD module used by the Restock device. It behaves as
 * an actuator because it receives commands and performs visual output actions.
 *
 * @author Gabriela Shapiama
 * @date May 31, 2026
 * @version 0.1
 */

#include <LiquidCrystal_I2C.h>
#include "Actuator.h"

/**
 * @brief LCD actuator for local device visualization.
 */
class Display : public Actuator {
private:
    LiquidCrystal_I2C lcd;
    float temperature;
    float humidity;
    bool telemetrySent;
    int telemetryStatusCode;

public:
    static const int SHOW_STARTUP_MESSAGE_COMMAND_ID = 3001;
    static const int SHOW_ENVIRONMENT_READING_COMMAND_ID = 3002;
    static const int SHOW_SENSOR_ERROR_COMMAND_ID = 3003;
    static const int SHOW_TELEMETRY_RESULT_COMMAND_ID = 3004;

    static const Command SHOW_STARTUP_MESSAGE_COMMAND;
    static const Command SHOW_ENVIRONMENT_READING_COMMAND;
    static const Command SHOW_SENSOR_ERROR_COMMAND;
    static const Command SHOW_TELEMETRY_RESULT_COMMAND;

    Display(uint8_t address, int columns, int rows, CommandHandler* commandHandler = nullptr);

    void begin();

    void setEnvironmentValues(float temperature, float humidity);

    void setTelemetryResult(bool sent, int statusCode);

    void handle(Command command) override;
};

#endif