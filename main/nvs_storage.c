/**
 * @file nvs_storage.c
 * @brief Implementation for NVS management.
 */

#include "nvs_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS_STORAGE";

// Static variables to hold the configuration in memory
static char stored_ssid[33] = {0};
static char stored_password[65] = {0};
static bool auto_connect = true;
static char device_name[33] = "ESP32-BLE";

/**
 * @brief Loads all preferences from NVS into memory.
 *
 * This is an internal function called by nvs_storage_init.
 */
static void load_preferences(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READONLY, &handle);
    if (err == ESP_OK)
    {
        size_t len;

        len = sizeof(stored_ssid);
        nvs_get_str(handle, "ssid", stored_ssid, &len);

        len = sizeof(stored_password);
        nvs_get_str(handle, "password", stored_password, &len);

        uint8_t ac = 1;
        nvs_get_u8(handle, "autoconnect", &ac);
        auto_connect = (ac != 0);

        len = sizeof(device_name);
        if (nvs_get_str(handle, "devname", device_name, &len) != ESP_OK)
        {
            strcpy(device_name, "ESP32-BLE");
        }

        nvs_close(handle);
        ESP_LOGI(TAG, "Preferences loaded from NVS.");
    }
    else
    {
        ESP_LOGW(TAG, "Could not open NVS to load preferences. Using defaults.");
    }

    ESP_LOGI(TAG, "  SSID: %s", stored_ssid[0] ? stored_ssid : "(not set)");
    ESP_LOGI(TAG, "  Auto-connect: %s", auto_connect ? "true" : "false");
    ESP_LOGI(TAG, "  Device name: %s", device_name);
}

esp_err_t nvs_storage_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS partition was corrupt, erasing and re-initializing.");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    load_preferences();
    return ret;
}

void nvs_storage_save_wifi_credentials(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        nvs_set_str(handle, "ssid", ssid);
        nvs_set_str(handle, "password", password);
        nvs_commit(handle);
        nvs_close(handle);

        strncpy(stored_ssid, ssid, sizeof(stored_ssid) - 1);
        stored_ssid[sizeof(stored_ssid) - 1] = '\0';
        strncpy(stored_password, password, sizeof(stored_password) - 1);
        stored_password[sizeof(stored_password) - 1] = '\0';

        ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to open NVS for writing credentials.");
    }
}

void nvs_storage_save_auto_connect(bool value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        nvs_set_u8(handle, "autoconnect", value ? 1 : 0);
        nvs_commit(handle);
        nvs_close(handle);

        auto_connect = value;
        ESP_LOGI(TAG, "Auto-connect set to: %s", value ? "true" : "false");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to open NVS for writing auto-connect.");
    }
}

void nvs_storage_save_device_name(const char *name)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        nvs_set_str(handle, "devname", name);
        nvs_commit(handle);
        nvs_close(handle);

        strncpy(device_name, name, sizeof(device_name) - 1);
        device_name[sizeof(device_name) - 1] = '\0';
        ESP_LOGI(TAG, "Device name set to: %s (restart required)", name);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to open NVS for writing device name.");
    }
}

void nvs_storage_clear_all_preferences(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);

        // Reset in-memory values to defaults
        stored_ssid[0] = '\0';
        stored_password[0] = '\0';
        auto_connect = true;
        strcpy(device_name, "ESP32-BLE");

        ESP_LOGI(TAG, "All preferences cleared from NVS.");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to open NVS to clear preferences.");
    }
}

const char *nvs_storage_get_ssid(void)
{
    return stored_ssid;
}

const char *nvs_storage_get_password(void)
{
    return stored_password;
}

bool nvs_storage_get_auto_connect(void)
{
    return auto_connect;
}

const char *nvs_storage_get_device_name(void)
{
    return device_name;
}
