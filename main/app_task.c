/**
 * @file app_task.c
 * @brief Implementation for the main application task.
 */

#include "app_task.h"
#include "app_includes.h"

#include "command_handler.h"
#include "nvs_storage.h"
#include "wifi_manager.h"

#include "freertos/semphr.h"

static const char *TAG = "APP_TASK";

// The queue handle for commands
static QueueHandle_t app_task_queue;

// The main application task function
static void app_task(void *pvParameters)
{
    SemaphoreHandle_t init_done_sem = (SemaphoreHandle_t)pvParameters;

    ESP_LOGI(TAG, "Application task waiting for system initialization...");
    // Wait for the signal that system initialization is complete
    xSemaphoreTake(init_done_sem, portMAX_DELAY);
    // The semaphore is no longer needed
    vSemaphoreDelete(init_done_sem);

    ESP_LOGI(TAG, "Application task started.");

    // Perform initial actions based on stored preferences
    if (nvs_storage_get_auto_connect() && nvs_storage_get_ssid()[0] != '\0')
    {
        ESP_LOGI(TAG, "Auto-connecting to: %s", nvs_storage_get_ssid());
        wifi_manager_connect(nvs_storage_get_ssid(), nvs_storage_get_password());
    }
    else
    {
        ESP_LOGI(TAG, "WiFi auto-connect disabled or no credentials, starting a scan.");
        wifi_manager_start_scan();
    }

    app_cmd_t received_cmd;
    while (1)
    {
        // Wait indefinitely for a command to arrive in the queue
        if (xQueueReceive(app_task_queue, &received_cmd, portMAX_DELAY) == pdPASS)
        {
            ESP_LOGI(TAG, "Dequeued command: %s", received_cmd.cmd);
            command_handler_process(received_cmd.cmd);
        }
    }
}

esp_err_t app_task_start(void *init_done_sem)
{
    app_task_queue = xQueueCreate(APP_TASK_QUEUE_SIZE, sizeof(app_cmd_t));
    if (app_task_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create application task queue.");
        return ESP_FAIL;
    }

    BaseType_t result = xTaskCreate(app_task, "app_task", 4096, init_done_sem, 5, NULL);
    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create application task.");
        return ESP_FAIL;
    }

    return ESP_OK;
}

BaseType_t app_task_queue_post(const char *cmd)
{
    if (app_task_queue == NULL)
    {
        ESP_LOGE(TAG, "Cannot post to queue, it has not been initialized.");
        return pdFAIL;
    }

    app_cmd_t cmd_to_queue;
    strncpy(cmd_to_queue.cmd, cmd, APP_CMD_MAX_LEN - 1);
    cmd_to_queue.cmd[APP_CMD_MAX_LEN - 1] = '\0';

    if (xQueueSend(app_task_queue, &cmd_to_queue, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to queue command '%s', queue might be full.", cmd);
        return pdFAIL;
    }
    return pdTRUE;
}
