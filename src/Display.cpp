#include "Display.h"
#include <Arduino.h>

const Command Display::SHOW_STARTUP_MESSAGE_COMMAND(
    Display::SHOW_STARTUP_MESSAGE_COMMAND_ID
);

const Command Display::SHOW_ENVIRONMENT_READING_COMMAND(
    Display::SHOW_ENVIRONMENT_READING_COMMAND_ID
);

const Command Display::SHOW_SENSOR_ERROR_COMMAND(
    Display::SHOW_SENSOR_ERROR_COMMAND_ID
);

const Command Display::SHOW_TELEMETRY_RESULT_COMMAND(
    Display::SHOW_TELEMETRY_RESULT_COMMAND_ID
);

Display::Display(uint8_t address, int columns, int rows, CommandHandler* commandHandler)
    : Actuator(-1, commandHandler),
      lcd(address, columns, rows),
      temperature(0.0f),
      humidity(0.0f),
      telemetrySent(false),
      telemetryStatusCode(0) {}

void Display::begin() {
    lcd.init();
    lcd.backlight();
    handle(SHOW_STARTUP_MESSAGE_COMMAND);

    Serial.println("Display ready");
}

void Display::setEnvironmentValues(float temperature, float humidity) {
    this->temperature = temperature;
    this->humidity = humidity;
}

void Display::setTelemetryResult(bool sent, int statusCode) {
    telemetrySent = sent;
    telemetryStatusCode = statusCode;
}

void Display::handle(Command command) {
    if (command == SHOW_STARTUP_MESSAGE_COMMAND) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Restock Scale");
        lcd.setCursor(0, 1);
        lcd.print("Starting...");
        return;
    }

    if (command == SHOW_ENVIRONMENT_READING_COMMAND) {
        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(temperature, 1);
        lcd.print(" C");

        lcd.setCursor(0, 1);
        lcd.print("Hum:  ");
        lcd.print(humidity, 1);
        lcd.print(" %");

        return;
    }

    if (command == SHOW_SENSOR_ERROR_COMMAND) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DHT22 error");
        lcd.setCursor(0, 1);
        lcd.print("Check sensor");
        return;
    }

    if (command == SHOW_TELEMETRY_RESULT_COMMAND) {
        Serial.printf(
            "Telemetry result: %s, status code: %d\n",
            telemetrySent ? "sent" : "failed",
            telemetryStatusCode
        );
        return;
    }

    Serial.printf("Display ignored command ID: %d\n", command.id);
}