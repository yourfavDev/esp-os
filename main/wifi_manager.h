/**
 * @file wifi_manager.h
 * @brief Manages WiFi connectivity, including scanning, connecting, and event handling.
 *
 * This module provides a centralized API for all WiFi-related operations. It handles
 * the underlying ESP-IDF WiFi events and provides functions to scan for networks,

 * connect to an access point, and query the current connection status.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi_types.h"
#include "esp_netif_types.h"
#include <stdbool.h>

/**
 * @brief Initializes the WiFi manager.
 *
 * This function sets up the WiFi station, registers event handlers, and prepares
 * the module for use. It must be called before any other function in this module.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t wifi_manager_init(void);
/**
 * @brief Connects to a WiFi access point.
 *
 * @param ssid The SSID of the network to connect to.
 * @param password The password for the network.
 * @return ESP_OK if the connection process is initiated successfully.
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

/**
 * @brief Disconnects from the currently connected WiFi access point.
 *
 * @return ESP_OK on success.
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * @brief Starts an asynchronous (non-blocking) WiFi scan.
 *
 * The results of the scan will be cached internally. Use
 * wifi_manager_get_networks_json to retrieve them.
 *
 * @return True if the scan was started successfully, false otherwise (e.g., if a
 *         scan is already in progress).
 */
bool wifi_manager_start_scan(void);

/**
 * @brief Gets the list of available networks from the last scan as a JSON string.
 *
 * This function returns cached results if a scan was performed recently.
 * If the cache is stale, it may trigger a new scan implicitly.
 *
 * @param[out] json_out Buffer to write the JSON string to.
 * @param[in]  max_size The maximum size of the output buffer.
 */
void wifi_manager_get_networks_json(char *json_out, size_t max_size);

/**
 * @brief Checks if the device is currently connected to a WiFi network.
 *
 * @return True if connected, false otherwise.
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Gets the IP information of the STA interface.
 *
 * @param[out] ip_info Pointer to a structure to store the IP information.
 * @return ESP_OK on success, ESP_FAIL if not connected.
 */
esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t *ip_info);


/**
 * @brief Gets the AP information of the STA interface.
 *
 * @param[out] ap_info Pointer to a structure to store the AP information.
 * @return ESP_OK on success, ESP_FAIL if not connected.
 */
esp_err_t wifi_manager_get_ap_info(wifi_ap_record_t *ap_info);


#endif // WIFI_MANAGER_H
