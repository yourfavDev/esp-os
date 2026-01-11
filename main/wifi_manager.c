/**
 * @file wifi_manager.c
 * @brief Implementation for WiFi management.
 */

#include "wifi_manager.h"
#include "app_includes.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "app_task.h" // For app_task_queue_post
#include "utils.h"    // For json_escape

static const char *TAG = "WIFI_MANAGER";

#define MAX_NETWORKS 5
#define SCAN_CACHE_DURATION_MS 30000

// Module-level static variables
static bool scan_in_progress = false;
static int64_t last_scan_time = 0;
static char cached_networks_json[512] = {0};

// Forward declaration for the event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void build_networks_json(char *json_out, size_t max_size);

esp_err_t wifi_manager_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi Manager initialized.");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);

    cached_networks_json[0] = '\0';
    last_scan_time = 0;

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_RETURN_ON_ERROR(esp_wifi_disconnect(), TAG, "Failed to disconnect before connecting");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Failed to set WiFi config");
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "Failed to start WiFi connection");

    return ESP_OK;
}

esp_err_t wifi_manager_disconnect(void)
{
    ESP_LOGI(TAG, "Disconnecting from WiFi.");
    cached_networks_json[0] = '\0';
    last_scan_time = 0;
    return esp_wifi_disconnect();
}

bool wifi_manager_start_scan(void)
{
    if (scan_in_progress)
    {
        ESP_LOGI(TAG, "Scan already in progress.");
        return true; // Not an error, just busy
    }

    ESP_LOGI(TAG, "Starting asynchronous WiFi scan...");

    wifi_scan_config_t scan_config = {
        .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = false, .scan_type = WIFI_SCAN_TYPE_ACTIVE, .scan_time.active = {.min = 100, .max = 300}};

    esp_err_t err = esp_wifi_scan_start(&scan_config, false); // Non-blocking

    if (err == ESP_OK)
    {
        scan_in_progress = true;
        ESP_LOGI(TAG, "Scan started successfully.");
        return true;
    }
    else
    {
        ESP_LOGW(TAG, "Failed to start scan: %s. Device might be connecting.", esp_err_to_name(err));
        return false;
    }
}

void wifi_manager_get_networks_json(char *json_out, size_t max_size)
{
    int64_t time_since_scan = (esp_timer_get_time() / 1000) - last_scan_time;

    if (scan_in_progress)
    {
        snprintf(json_out, max_size, "\"scanning\":true");
    }
    else if (cached_networks_json[0] != '\0' && time_since_scan < SCAN_CACHE_DURATION_MS)
    {
        snprintf(json_out, max_size, "%s", cached_networks_json);
    }
    else
    {
        // Cache is stale or empty, start a new scan
        bool scan_started = wifi_manager_start_scan();
        if (scan_started)
        {
            snprintf(json_out, max_size, "\"scanning\":true");
        }
        else
        {
            // Scan didn't start (maybe connecting), return empty list
            snprintf(json_out, max_size, "\"scanning\":false, \"available_networks\":[]");
        }
    }
}

bool wifi_manager_is_connected(void)
{
    wifi_ap_record_t ap_info;
    return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t *ip_info)
{
    if (wifi_manager_is_connected())
    {
        return esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), ip_info);
    }
    return ESP_FAIL;
}

esp_err_t wifi_manager_get_ap_info(wifi_ap_record_t *ap_info)
{
    if (wifi_manager_is_connected())
    {
        return esp_wifi_sta_get_ap_info(ap_info);
    }
    return ESP_FAIL;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            ESP_LOGI(TAG, "WiFi STA Started");
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            ESP_LOGI(TAG, "WiFi disconnected.");
            app_task_queue_post("status()");

            // Optionally attempt to reconnect if auto-connect is enabled
        }
        else if (event_id == WIFI_EVENT_SCAN_DONE)
        {
            ESP_LOGI(TAG, "WiFi scan done, building networks list...");
            build_networks_json(cached_networks_json, sizeof(cached_networks_json));
            scan_in_progress = false;

            // Notify the main app task to send a status update
            app_task_queue_post("status()");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        // Notify the main app task to send a status update
        app_task_queue_post("status()");
    }
}

static void build_networks_json(char *json_out, size_t max_size)
{
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count == 0)
    {
        snprintf(json_out, max_size, "\"available_networks\":[]");
        return;
    }

    uint16_t num_to_get = (ap_count < MAX_NETWORKS) ? ap_count : MAX_NETWORKS;
    wifi_ap_record_t ap_records[MAX_NETWORKS];

    esp_wifi_scan_get_ap_records(&num_to_get, ap_records);

    size_t offset = snprintf(json_out, max_size, "\"available_networks\":[");

    for (int i = 0; i < num_to_get && offset < max_size - 128; i++)
    {
        char ssid_escaped[65];
        json_escape((char *)ap_records[i].ssid, ssid_escaped, sizeof(ssid_escaped));

        offset += snprintf(json_out + offset, max_size - offset,
                           "%s{\"ssid\":\"%s\",\"rssi\":%d,\"encryption\":%d}",
                           i > 0 ? "," : "",
                           ssid_escaped,
                           ap_records[i].rssi,
                           ap_records[i].authmode == WIFI_AUTH_OPEN ? 0 : 1);
    }

    snprintf(json_out + offset, max_size - offset, "]");

    last_scan_time = esp_timer_get_time() / 1000;
}
