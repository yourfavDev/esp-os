# Technical Documentation: esp-os

This document provides a technical overview of the `esp-os` project, detailing its architecture, components, and operational procedures.

## 1. Project Overview

`esp-os` is a firmware for the ESP32 platform designed to be a personal operating system. It provides a command-based interface for managing device functionalities such as Wi-Fi connectivity and BLE communication. The system is modular, with distinct components for handling different aspects of the device's operation. Configuration data, such as Wi-Fi credentials, is persisted in Non-Volatile Storage (NVS).

## 2. Architecture

The firmware is built on top of the ESP-IDF and FreeRTOS. The architecture is centered around a main application task (`app_task`) that processes commands from a message queue. This design decouples the command source (e.g., BLE) from the command execution, allowing for a flexible and extensible system.

The main components are:
- **Application Task (`app_task`):** The core of the application, responsible for orchestrating command processing.
- **BLE Manager (`ble_manager`):** Manages all Bluetooth Low Energy (BLE) operations, including advertising and GATT services for communication.
- **Wi-Fi Manager (`wifi_manager`):** Handles Wi-Fi scanning, connection, and status reporting.
- **NVS Storage (`nvs_storage`):** Provides an abstraction layer for reading from and writing to the ESP32's Non-Volatile Storage.
- **Command Handler (`command_handler`):** Parses and executes the string-based commands received by the `app_task`.
- **Utilities (`utils`):** A collection of helper functions used across the project.

## 3. Components

### 3.1. Application Task (`app_task`)

- **Source:** `main/app_task.c`, `main/app_task.h`
- **Description:** This component runs the main application logic in a dedicated FreeRTOS task. It waits for commands to be posted to a queue and dispatches them to the `command_handler` for processing.
- **Public Functions:**
    - `esp_err_t app_task_start(void *init_done_sem)`: Starts the main application task.
    - `BaseType_t app_task_queue_post(const char *cmd)`: Posts a command string to the application task's queue.

### 3.2. BLE Manager (`ble_manager`)

- **Source:** `main/ble_manager.c`, `main/ble_manager.h`
- **Description:** Encapsulates the NimBLE stack for BLE functionality. It initializes the BLE service, handles advertising, and manages client connections and communication. It provides a simple interface for sending notifications to a connected client.
- **Public Functions:**
    - `esp_err_t ble_manager_init(void)`: Initializes the BLE manager, sets up GATT services, and starts advertising.
    - `void ble_manager_send_response(const char *msg)`: Sends a message to the connected BLE client via a GATT notification.
    - `bool ble_manager_is_connected(void)`: Checks if a BLE client is currently connected.

### 3.3. Wi-Fi Manager (`wifi_manager`)

- **Source:** `main/wifi_manager.c`, `main/wifi_manager.h`
- **Description:** Provides a centralized API for all Wi-Fi related operations. It handles the underlying ESP-IDF Wi-Fi events and provides functions to scan for networks, connect to an access point, and query the current connection status.
- **Public Functions:**
    - `esp_err_t wifi_manager_init(void)`: Initializes the Wi-Fi manager.
    - `esp_err_t wifi_manager_connect(const char *ssid, const char *password)`: Connects to a Wi-Fi access point.
    - `esp_err_t wifi_manager_disconnect(void)`: Disconnects from the current Wi-Fi access point.
    - `bool wifi_manager_start_scan(void)`: Starts an asynchronous Wi-Fi scan.
    - `void wifi_manager_get_networks_json(char *json_out, size_t max_size)`: Gets the list of available networks from the last scan as a JSON string.
    - `bool wifi_manager_is_connected(void)`: Checks if the device is connected to a Wi-Fi network.
    - `esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t *ip_info)`: Gets the IP information of the STA interface.
    - `esp_err_t wifi_manager_get_ap_info(wifi_ap_record_t *ap_info)`: Gets the AP information of the STA interface.

### 3.4. NVS Storage (`nvs_storage`)

- **Source:** `main/nvs_storage.c`, `main/nvs_storage.h`
- **Description:** Manages persistent configuration data using the ESP32's Non-Volatile Storage. This module is used to store Wi-Fi credentials, the device name, and other settings.
- **Public Functions:**
    - `esp_err_t nvs_storage_init(void)`: Initializes the NVS flash and loads all preferences.
    - `void nvs_storage_save_wifi_credentials(const char *ssid, const char *password)`: Saves Wi-Fi credentials to NVS.
    - `void nvs_storage_save_auto_connect(bool value)`: Saves the auto-connect preference.
    - `void nvs_storage_save_device_name(const char *name)`: Saves the device name.
    - `void nvs_storage_clear_all_preferences(void)`: Erases all stored preferences.
    - `const char *nvs_storage_get_ssid(void)`: Gets the stored Wi-Fi SSID.
    - `const char *nvs_storage_get_password(void)`: Gets the stored Wi-Fi password.
    - `bool nvs_storage_get_auto_connect(void)`: Gets the auto-connect preference.
    - `const char *nvs_storage_get_device_name(void)`: Gets the device name.

### 3.5. Command Handler (`command_handler`)

- **Source:** `main/command_handler.c`, `main/command_handler.h`
- **Description:** This module is responsible for parsing and executing the string-based commands received from the `app_task`. It acts as a dispatcher, calling the appropriate functions from other modules based on the command.
- **Public Functions:**
    - `void command_handler_process(const char *command)`: Parses and processes a command string.

### 3.6. Utilities (`utils`)

- **Source:** `main/utils.c`, `main/utils.h`
- **Description:** A collection of utility functions used throughout the project.
- **Public Functions:**
    - `void json_escape(const char *str, char *out, size_t out_size)`: Escapes a string for inclusion in a JSON document.

## 4. Building and Flashing

To build and flash the project, you need to have the ESP-IDF installed and configured.

1.  **Set up ESP-IDF environment variables.**
    (Refer to the official ESP-IDF documentation for instructions.)

2.  **Build the project:**
    ```bash
    idf.py build
    ```

3.  **Flash to your ESP32 device:**
    ```bash
    idf.py -p /dev/ttyUSB0 flash
    ```
    *(Replace `/dev/ttyUSB0` with your ESP32's serial port.)*

4.  **Monitor serial output:**
    ```bash
    idf.py -p /dev/ttyUSB0 monitor
    ```

This command will display the serial output from the ESP32, which is useful for debugging.
