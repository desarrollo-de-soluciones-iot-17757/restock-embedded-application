#ifndef DISPLAY_MODE_H
#define DISPLAY_MODE_H

/**
 * @brief Display modes supported by the Restock embedded device.
 */
enum DisplayMode {
    DISPLAY_MODE_ENVIRONMENT,
    DISPLAY_MODE_TEMPERATURE,
    DISPLAY_MODE_HUMIDITY,
    DISPLAY_MODE_WEIGHT,
    DISPLAY_MODE_CONVERTED_UNITS
};

/**
 * @brief Converts a configuration string into a display mode.
 *
 * @param rawDisplayMode Display mode received from Edge.
 * @return Parsed display mode.
 */
static DisplayMode parseDisplayMode(const String& rawDisplayMode) {
    String normalizedMode = rawDisplayMode;
    normalizedMode.toLowerCase();

    if (normalizedMode == "temperature" || normalizedMode == "temperatura") {
        return DISPLAY_MODE_TEMPERATURE;
    }

    if (normalizedMode == "humidity" || normalizedMode == "humedad") {
        return DISPLAY_MODE_HUMIDITY;
    }

    if (normalizedMode == "weight" || normalizedMode == "peso") {
        return DISPLAY_MODE_WEIGHT;
    }

    if (normalizedMode == "converted_units" ||
        normalizedMode == "units" ||
        normalizedMode == "unit") {
        return DISPLAY_MODE_CONVERTED_UNITS;
        }

    return DISPLAY_MODE_ENVIRONMENT;
}

#endif //DISPLAY_MODE_H
