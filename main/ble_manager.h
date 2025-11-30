/**
 * @file ble_manager.h
 * @brief Manages BLE services, advertising, and communication.
 *
 * This module encapsulates the NimBLE stack initialization, GATT service
 * creation, and GAP event handling. It provides a simple interface for
 * sending notifications to a connected client and for checking the
 * connection status.
 */

#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initializes the BLE manager.
 *
 * This function initializes the NimBLE stack, sets up the required GATT
 * services and characteristics, and starts BLE advertising. It must be
 * called before any other function in this module.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t ble_manager_init(void);

/**
 * @brief Sends a message to the connected BLE client via GATT notification.
 *
 * If a client is connected and has subscribed to notifications, this function
 * sends the provided string. If not, the message is logged but not sent.
 *
 * @param msg The null-terminated string to send.
 */
void ble_manager_send_response(const char *msg);

/**
 * @brief Checks if a BLE client is currently connected.
 *
 * @return True if a client is connected, false otherwise.
 */
bool ble_manager_is_connected(void);

#endif // BLE_MANAGER_H
