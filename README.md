# ESP32-BLE-Controller: Building Blocks for Mobile Control

This project provides the foundational firmware for controlling an ESP32 device from any mobile device or other platform that can communicate via Bluetooth Low Energy (BLE). It's designed to be a flexible and extensible starting point for a wide range of IoT applications.

## Project Overview

`esp-os` is a modular and command-driven firmware for the ESP32, built on top of the ESP-IDF and FreeRTOS. It exposes a command interface over BLE, allowing a connected client (like a mobile app) to manage the device's functionalities, such as Wi-Fi connectivity. The system is designed to be easily extended with new commands and features.

## Key Features

- **BLE Command Interface:** Control the ESP32 using simple string-based commands sent over a BLE GATT characteristic.
- **Wi-Fi Management:** Scan for Wi-Fi networks, connect, disconnect, and check the connection status.
- **Persistent Configuration:** Wi-Fi credentials and other settings are stored in Non-Volatile Storage (NVS), so they're not lost after a reboot.
- **Modular Architecture:** The code is organized into logical components, making it easy to understand, maintain, and extend.
- **Asynchronous Command Processing:** A message queue decouples command reception from execution, ensuring that the system remains responsive.

## Getting Started

To build and flash this project, you'll need to have the ESP-IDF installed and configured.

1.  **Set up your ESP-IDF environment:**
    Follow the official ESP-IDF documentation to install the toolchain and set up the required environment variables.

2.  **Clone the repository:**
    ```bash
    git clone https://github.com/yourfavDev/esp-os.git
    cd esp-os
    ```

3.  **Build the project:**
    ```bash
    idf.py build
    ```

4.  **Flash the firmware to your ESP32:**
    Connect your ESP32 device to your computer and run:
    ```bash
    idf.py -p /dev/ttyUSB0 flash
    ```
    *(Replace `/dev/ttyUSB0` with the serial port of your ESP32.)*

5.  **Monitor the device output:**
    To see the log messages from the ESP32, run:
    ```bash
    idf.py -p /dev/ttyUSB0 monitor
    ```

## Architecture

The firmware's architecture is centered around a main application task (`app_task`) that processes commands from a message queue. This design decouples the command source (e.g., BLE) from the command execution, allowing for a flexible and extensible system.

The main components are:

- **Application Task (`app_task`):** The core of the application, responsible for orchestrating command processing.
- **BLE Manager (`ble_manager`):** Manages all Bluetooth Low Energy (BLE) operations, including advertising and GATT services for communication.
- **Wi-Fi Manager (`wifi_manager`):** Handles Wi-Fi scanning, connection, and status reporting.
- **NVS Storage (`nvs_storage`):** Provides an abstraction layer for reading from and writing to the ESP32's Non-Volatile Storage.
- **Command Handler (`command_handler`):** Parses and executes the string-based commands received by the `app_task`.
- **Utilities (`utils`):** A collection of helper functions used across the project.

## Contributing

Contributions are welcome! If you have any ideas, suggestions, or bug reports, please open an issue or submit a pull request.
