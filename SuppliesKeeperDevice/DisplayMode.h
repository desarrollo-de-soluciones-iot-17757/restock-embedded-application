#ifndef DISPLAY_MODE_H
#define DISPLAY_MODE_H

/**
 * @file DisplayMode.h
 * @brief Display mode contract supported by the Restock embedded LCD.
 *
 * @details
 * Keeps the original five display options expected by the embedded application.
 * Edge responses may select one option through the display_mode field.
 *
 * @author Gabriela Shapiama
 * @date Jul 06, 2026
 * @version 0.7
 */

#include <Arduino.h>

/**
 * @brief Display modes supported by the Restock embedded device.
 */
enum DisplayMode {
    DISPLAY_MODE_ENVIRONMENT,      ///< Shows temperature and humidity together.
    DISPLAY_MODE_TEMPERATURE,      ///< Shows temperature only.
    DISPLAY_MODE_HUMIDITY,         ///< Shows humidity only.
    DISPLAY_MODE_WEIGHT,           ///< Shows current weight in grams.
    DISPLAY_MODE_CONVERTED_UNITS   ///< Shows converted stock/product units from Edge.
};

/**
 * @brief Converts an Edge configuration string into one of the five display modes.
 *
 * @param rawDisplayMode Display mode received from Edge.
 * @return Parsed display mode. Defaults to DISPLAY_MODE_ENVIRONMENT.
 */
static DisplayMode parseDisplayMode(const String& rawDisplayMode) {
    String normalizedMode = rawDisplayMode;
    normalizedMode.trim();
    normalizedMode.toUpperCase();

    if (normalizedMode == "DISPLAY_MODE_ENVIRONMENT") {
        return DISPLAY_MODE_TEMPERATURE;
    }

    if (normalizedMode == "DISPLAY_MODE_HUMIDITY") {
        return DISPLAY_MODE_HUMIDITY;
    }

    if (normalizedMode == "DISPLAY_MODE_WEIGHT") {
        return DISPLAY_MODE_WEIGHT;
    }

    if (normalizedMode == "DISPLAY_MODE_CONVERTED_UNITS") {
        return DISPLAY_MODE_CONVERTED_UNITS;
    }

    return DISPLAY_MODE_ENVIRONMENT;
}

#endif // DISPLAY_MODE_H
