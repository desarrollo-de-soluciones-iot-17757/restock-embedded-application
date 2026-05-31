
# Restock Embedded Application

Restock Embedded Application is the firmware project for the smart inventory scale device used in the Restock IoT ecosystem. It runs on an ESP32 microcontroller and coordinates sensor readings, local device behavior, display output, and telemetry delivery to the Restock Edge Service.

The device monitors physical and environmental conditions from inventory storage areas, allowing Restock to collect real-time data about stock weight, temperature, humidity, and device status.

## Overview

The embedded application is designed for a smart inventory scale device that supports automated stock monitoring. It integrates sensors and actuators to capture relevant inventory data and communicate it to the Restock platform.

The device is responsible for:

* Reading environmental conditions.
* Reading stock weight.
* Displaying local information.
* Managing device events and commands.
* Sending telemetry data to the Edge Service.
* Supporting real-time inventory monitoring.

## Main Features

* Temperature and humidity monitoring.
* Weight sensing through load cell integration.
* Local information display.
* Event-driven device behavior.
* Periodic telemetry generation.
* Serial monitoring for debugging.
* ESP32-based execution.
* Wokwi simulation support.
* PlatformIO-based development workflow.

## Hardware Components

The embedded device is based on the following hardware components:

| Component    | Purpose                          |
| ------------ | -------------------------------- |
| ESP32 DevKit | Main microcontroller             |
| DHT22        | Temperature and humidity sensing |
| HX711        | Load cell signal amplification   |
| Load cells   | Weight measurement               |
| LCD Display  | Local visualization              |
| Breadboard   | Circuit prototyping              |

## Software Stack

This project uses:

* C++
* Arduino framework
* PlatformIO
* Wokwi Simulator
* ModestIoT Nano-framework

## Architecture

The firmware follows an object-oriented design based on the ModestIoT Nano-framework. The application is organized around device, sensor, actuator, command, and event abstractions.

The main device class coordinates the behavior of the smart inventory scale and delegates responsibilities to specific components such as environmental sensing, weight sensing, and display handling.

Core responsibilities include:

* Device lifecycle management.
* Sensor abstraction.
* Actuator abstraction.
* Event handling.
* Command handling.
* Telemetry preparation.

## PlatformIO Configuration

The project uses PlatformIO with the ESP32 DevKit board and the Arduino framework.

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
```

## Build

To compile the firmware, run:

```bash
pio run
```

The generated firmware files are located in:

```txt
.pio/build/esp32dev/
```

## Wokwi Simulation

The project includes Wokwi support through `diagram.json` and `wokwi.toml`.

Recommended workflow:

1. Build the project with PlatformIO.
2. Start the Wokwi simulation.
3. Verify the device behavior using the simulator and serial monitor.

The `wokwi.toml` file points Wokwi to the PlatformIO firmware output:

```toml
[wokwi]
version = 1
firmware = ".pio/build/esp32dev/firmware.bin"
elf = ".pio/build/esp32dev/firmware.elf"
```

## Run Serial Monitor

To monitor device logs, run:

```bash
pio device monitor
```

## Project Purpose

This embedded application is part of the Restock Smart Inventory solution. Its purpose is to connect the physical inventory environment with the software platform by collecting sensor data and delivering it to the Edge Service for processing, monitoring, and decision support.

## Credits

This project uses the ModestIoT Nano-framework created by Angel Velasquez as the base framework for device, sensor, actuator, command, and event abstractions.

Restock-specific components and embedded application behavior are developed by the Restock team.

## License

This project is intended for academic use as part of the Restock IoT solution.
