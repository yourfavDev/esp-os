/**
 * @file nvs_storage.h
 * @brief Handles reading from and writing to Non-Volatile Storage (NVS).
 *
 * This module encapsulates all interactions with the NVS, providing a clear
 * API for managing persistent configuration data such as WiFi credentials,
 * device name, and other settings.
 */

#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initializes the NVS flash and loads all preferences into memory.
 *
 * This function must be called once at startup before any other function in
 * this module. It handles the initialization of the underlying NVS flash
 * partition and loads all stored settings. If the NVS is corrupted, it will
 * attempt to erase and re-initialize it.
 *
 * @return ESP_OK on success, or an error code from nvs_flash_init on failure.
 */
esp_err_t nvs_storage_init(void);

/**
 * @brief Saves the WiFi credentials to NVS.
 *
 * @param ssid The WiFi network SSID.
 * @param password The WiFi network password.
 */
void nvs_storage_save_wifi_credentials(const char *ssid, const char *password);

/**
 * @brief Saves the auto-connect preference to NVS.
 *
 * @param value The auto-connect preference (true or false).
 */
void nvs_storage_save_auto_connect(bool value);

/**
 * @brief Saves the device name to NVS.
 *
 * Note: A restart is required for the new device name to be used for BLE advertising.
 *
 * @param name The new device name.
 */
void nvs_storage_save_device_name(const char *name);

/**
 * @brief Erases all stored preferences from NVS.
 *
 * This will clear WiFi credentials, auto-connect settings, and the device name,
 * reverting them to their default values.
 */
void nvs_storage_clear_all_preferences(void);

/**
 * @brief Gets the stored WiFi SSID.
 *
 * @return A pointer to the null-terminated SSID string.
 */
const char *nvs_storage_get_ssid(void);

/**
 * @brief Gets the stored WiFi password.
 *
 * @return A pointer to the null-terminated password string.
 */
const char *nvs_storage_get_password(void);

/**
 * @brief Gets the auto-connect preference.
 *
 * @return True if auto-connect is enabled, false otherwise.
 */
bool nvs_storage_get_auto_connect(void);

/**
 * @brief Gets the device name.
 *
 * @return A pointer to the null-terminated device name string.
 */
const char *nvs_storage_get_device_name(void);

#endif // NVS_STORAGE_H
