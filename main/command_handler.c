/**
 * @file command_handler.c
 * @brief Implementation for the command handler.
 */

#include "command_handler.h"
#include "app_includes.h"
#include "nvs.h" // For nvs_get_stats
#include <string.h>
#include <stdlib.h>

// Include all the modules this handler needs to control
#include "ble_manager.h"
#include "wifi_manager.h"
#include "nvs_storage.h"
#include "utils.h"
#include "app_task.h" // For APP_CMD_MAX_LEN

static const char *TAG = "CMD_HANDLER";

// --- Command Handler Forward Declarations ---
static void cmd_echo(const char *arg);
static void cmd_connect(const char *ssid, const char *password, bool save);
static void cmd_reconnect(void);
static void cmd_disconnect(void);
static void cmd_forget(void);
static void cmd_status(void);
static void cmd_set_auto_connect(bool value);
static void cmd_set_name(const char *name);
static void cmd_reset(void);
static void cmd_restart(void);
static void cmd_help(void);

// --- Command Implementations ---

static void cmd_echo(const char *arg)
{
    ble_manager_send_response(arg);
}

static void cmd_connect(const char *ssid, const char *password, bool save)
{
    ESP_LOGI(TAG, "Executing command: connect to %s", ssid);
    ble_manager_send_response("{\"status\":\"connecting\"}");
    wifi_manager_connect(ssid, password);
    if (save)
    {
        nvs_storage_save_wifi_credentials(ssid, password);
    }
}

static void cmd_reconnect(void)
{
    const char *ssid = nvs_storage_get_ssid();
    if (ssid[0] == '\0')
    {
        ble_manager_send_response("{\"error\":\"no saved credentials\"}");
        return;
    }
    ESP_LOGI(TAG, "Executing command: reconnect");
    cmd_connect(ssid, nvs_storage_get_password(), false);
}

static void cmd_disconnect(void)
{
    ESP_LOGI(TAG, "Executing command: disconnect");
    wifi_manager_disconnect();
    ble_manager_send_response("{\"status\":\"disconnected\"}");
    cmd_status(); // Send updated status
}

static void cmd_forget(void)
{
    ESP_LOGI(TAG, "Executing command: forget wifi");
    wifi_manager_disconnect();
    // Overwrite only wifi credentials in NVS
    nvs_storage_save_wifi_credentials("", "");
    ble_manager_send_response("{\"status\":\"credentials_cleared\"}");
    wifi_manager_start_scan();
}

static void cmd_status(void)
{
    static char json[1024];
    size_t offset = 0;

    wifi_ap_record_t ap_info;
    bool is_connected = wifi_manager_is_connected();
    
    char ssid_escaped[65];
    json_escape(nvs_storage_get_ssid(), ssid_escaped, sizeof(ssid_escaped));

    char devname_escaped[65];
    json_escape(nvs_storage_get_device_name(), devname_escaped, sizeof(devname_escaped));

    nvs_stats_t nvs_stats;
    nvs_get_stats(NULL, &nvs_stats);

    if (is_connected)
    {
        esp_netif_ip_info_t ip_info;
        wifi_manager_get_ap_info(&ap_info);
        wifi_manager_get_ip_info(&ip_info);
        offset = snprintf(json, sizeof(json),
                          "{\"wifi\":true,\"rssi\":%d,\"ip\":\"" IPSTR "\",\"heap\":%lu,"
                          "\"uptime\":%lld,\"ble\":%s,\"saved_ssid\":\"%s\","
                          "\"autoconnect\":%s,\"devname\":\"%s\",\"nvs_free\":%zu}",
                          ap_info.rssi, IP2STR(&ip_info.ip), (unsigned long)esp_get_free_heap_size(),
                          (long long)esp_timer_get_time() / 1000000, ble_manager_is_connected() ? "true" : "false",
                          ssid_escaped, nvs_storage_get_auto_connect() ? "true" : "false", devname_escaped, nvs_stats.free_entries);
    }
    else
    {
        offset = snprintf(json, sizeof(json),
                          "{\"wifi\":false,\"rssi\":0,\"ip\":\"\",\"heap\":%lu,"
                          "\"uptime\":%lld,\"ble\":%s,\"saved_ssid\":\"%s\","
                          "\"autoconnect\":%s,\"devname\":\"%s\",\"nvs_free\":%zu",
                          (unsigned long)esp_get_free_heap_size(), (long long)esp_timer_get_time() / 1000000,
                          ble_manager_is_connected() ? "true" : "false", ssid_escaped,
                          nvs_storage_get_auto_connect() ? "true" : "false", devname_escaped, nvs_stats.free_entries);
        
        static char network_json[512];
        wifi_manager_get_networks_json(network_json, sizeof(network_json));
        // Append a comma, the network status JSON fragment, and the final closing brace
        snprintf(json + offset, sizeof(json) - offset, ",%s}", network_json);
    }
    ble_manager_send_response(json);
}


static void cmd_set_auto_connect(bool value)
{
    ESP_LOGI(TAG, "Executing command: set autoconnect to %d", value);
    nvs_storage_save_auto_connect(value);
    char resp[48];
    snprintf(resp, sizeof(resp), "{\"autoconnect\":%s}", value ? "true" : "false");
    ble_manager_send_response(resp);
}

static void cmd_set_name(const char *name)
{
    ESP_LOGI(TAG, "Executing command: set device name to %s", name);
    nvs_storage_save_device_name(name);
    char resp[128];
    char name_escaped[65];
    json_escape(name, name_escaped, sizeof(name_escaped));
    snprintf(resp, sizeof(resp), "{\"devname\":\"%s\",\"note\":\"restart required\"}", name_escaped);
    ble_manager_send_response(resp);
}

static void cmd_reset(void)
{
    ESP_LOGI(TAG, "Executing command: factory reset");
    nvs_storage_clear_all_preferences();
    ble_manager_send_response("{\"status\":\"factory_reset\",\"note\":\"restarting...\"}");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static void cmd_restart(void)
{
    ESP_LOGI(TAG, "Executing command: restart");
    ble_manager_send_response("{\"status\":\"restarting...\"}");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static void cmd_help(void)
{
    const char *help =
        "{\"commands\":["
        "\"connect(\\\"ssid\\\",\\\"pass\\\")\","
        "\"reconnect()\","
        "\"disconnect()\","
        "\"forget()\","
        "\"status()\","
        "\"autoconnect(true|false)\","
        "\"setname(\\\"name\\\")\","
        "\"reset()\","
        "\"restart()\","
        "\"echo(\\\"msg\\\")\""
        "]}";
    ble_manager_send_response(help);
}

void command_handler_process(const char *input)
{
    char buf[APP_CMD_MAX_LEN];
    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *paren = strchr(buf, '(');
    if (!paren)
        return;

    *paren = '\0';
    char *cmd = buf;
    char *args = paren + 1;

    char *end = strrchr(args, ')');
    if (end)
        *end = '\0';

    char *arg1 = NULL;
    char *arg2 = NULL;
    bool bool_arg = false;
    bool has_bool_arg = (strcmp(args, "true") == 0 || strcmp(args, "false") == 0);
    if (has_bool_arg)
        bool_arg = (strcmp(args, "true") == 0);

    char *q1 = strchr(args, '"');
    if (q1)
    {
        arg1 = q1 + 1;
        char *q2 = strchr(arg1, '"');
        if (q2)
        {
            *q2 = '\0';
            char *q3 = strchr(q2 + 1, '"');
            if (q3)
            {
                arg2 = q3 + 1;
                char *q4 = strchr(arg2, '"');
                if (q4)
                    *q4 = '\0';
            }
        }
    }

    if (strcmp(cmd, "echo") == 0)
        cmd_echo(arg1 ? arg1 : "");
    else if (strcmp(cmd, "connect") == 0)
        (arg1 && arg2) ? cmd_connect(arg1, arg2, true) : ble_manager_send_response("{\"error\":\"usage: connect(\\\"ssid\\\",\\\"pass\\\")\"}");
    else if (strcmp(cmd, "reconnect") == 0)
        cmd_reconnect();
    else if (strcmp(cmd, "disconnect") == 0)
        cmd_disconnect();
    else if (strcmp(cmd, "forget") == 0)
        cmd_forget();
    else if (strcmp(cmd, "status") == 0)
        cmd_status();
    else if (strcmp(cmd, "autoconnect") == 0)
        has_bool_arg ? cmd_set_auto_connect(bool_arg) : ble_manager_send_response("{\"error\":\"usage: autoconnect(true|false)\"}");
    else if (strcmp(cmd, "setname") == 0)
        arg1 ? cmd_set_name(arg1) : ble_manager_send_response("{\"error\":\"usage: setname(\\\"name\\\")\"}");
    else if (strcmp(cmd, "reset") == 0)
        cmd_reset();
    else if (strcmp(cmd, "restart") == 0)
        cmd_restart();
    else if (strcmp(cmd, "help") == 0)
        cmd_help();
    else
    {
        char resp[160];
        snprintf(resp, sizeof(resp), "{\"error\":\"unknown: %s\"}", cmd);
        ble_manager_send_response(resp);
    }
}