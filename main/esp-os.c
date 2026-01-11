/**
 * @file esp-os.c
 * @brief Main application entry point for esp-os.
 *
 * This file contains the main entry point of the application, `app_main`.
 * Its primary responsibility is to initialize all the application modules
 * in the correct order.
 */

#include "esp_log.h"
#include "nvs_storage.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "app_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "ESP-OS_MAIN";

/**
 * @brief Main application entry point.
 *
 * This function is called by the ESP-IDF framework after the second-stage
 * bootloader has finished.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "===== Starting ESP-OS =====");

    // Create a semaphore to signal when system initialization is complete.
    SemaphoreHandle_t init_done_sem = xSemaphoreCreateBinary();

    // 1. Initialize Non-Volatile Storage
    ESP_ERROR_CHECK(nvs_storage_init());

    // 2. Initialize the application task and its command queue.
    //    The task will block until the init_done_sem is given.
    ESP_ERROR_CHECK(app_task_start(init_done_sem));

    // 3. Initialize the WiFi Manager
    ESP_ERROR_CHECK(wifi_manager_init());

    // 4. Initialize the BLE Manager (which starts advertising)
    ESP_ERROR_CHECK(ble_manager_init());

    // 5. Signal the application task that it can now proceed.
    xSemaphoreGive(init_done_sem);

    ESP_LOGI(TAG, "===== ESP-OS Startup Complete =====");
}