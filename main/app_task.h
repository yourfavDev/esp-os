/**
 * @file app_task.h
 * @brief Defines and manages the main application task.
 *
 * This module contains the primary task that drives the application logic.
 * It is built around a FreeRTOS queue that receives commands from other
 * modules (like the BLE manager) and dispatches them to the command handler.
 */

#ifndef APP_TASK_H
#define APP_TASK_H

#include "app_includes.h"

// The maximum length of a command string that can be queued.
#define APP_CMD_MAX_LEN 128

// The maximum number of commands that can be held in the queue.
#define APP_TASK_QUEUE_SIZE 10

/**
 * @brief Structure for commands passed into the application task queue.
 */
typedef struct
{
    char cmd[APP_CMD_MAX_LEN];
} app_cmd_t;

/**
 * @brief Starts the main application task.
 *
 * This function creates the FreeRTOS task that runs the main application loop.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t app_task_start(void *init_done_sem);

/**
 * @brief Posts a command string to the application task queue.
 *
 * This is a thread-safe way to send a command to be processed by the main
 * application task.
 *
 * @param cmd The null-terminated command string to post.
 * @return pdTRUE if the command was successfully posted, pdFALSE otherwise.
 */
BaseType_t app_task_queue_post(const char *cmd);

#endif // APP_TASK_H
