#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"

#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_WIFI_MGR";

// UUIDs are reversed for NimBLE!
#define SERVICE_UUID_BASE 0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e
#define CHAR_UUID_RX_BASE 0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e
#define CHAR_UUID_TX_BASE 0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e

#define MAX_NETWORKS 5
#define SCAN_CACHE_DURATION_MS 30000
#define APP_TASK_QUEUE_SIZE 10
#define APP_CMD_MAX_LEN 128

// App command structure for the queue
typedef struct
{
    char cmd[APP_CMD_MAX_LEN];
} app_cmd_t;

// Global state
static bool device_connected = false;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t tx_char_handle = 0;
static uint8_t own_addr_type;

static char stored_ssid[33] = {0};
static char stored_password[65] = {0};
static bool auto_connect = true;
static char device_name[33] = "ESP32-BLE";

static bool scan_in_progress = false;
static bool resend_status_after_scan = false;
static int64_t last_scan_time = 0;
static char cached_networks_json[512] = {0};

static QueueHandle_t app_task_queue;

// Function declarations
static void send_response(const char *msg);
static void process_command(const char *input);
static void cmd_status(void);
static bool start_async_scan(void);
static void start_ble_advertising(int reason);
static void app_task(void *pvParameters);
static void build_networks_json(char *json_out, size_t max_size);

// ============ NVS Preferences Management ============

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
    }

    ESP_LOGI(TAG, "Preferences loaded:");
    ESP_LOGI(TAG, "  SSID: %s", stored_ssid[0] ? stored_ssid : "(not set)");
    ESP_LOGI(TAG, "  Auto-connect: %s", auto_connect ? "true" : "false");
    ESP_LOGI(TAG, "  Device name: %s", device_name);
}

static void save_wifi_credentials(const char *ssid, const char *password)
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
        strncpy(stored_password, password, sizeof(stored_password) - 1);

        ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    }
}

static void save_auto_connect(bool value)
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
}

static void save_device_name(const char *name)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        nvs_set_str(handle, "devname", name);
        nvs_commit(handle);
        nvs_close(handle);

        strncpy(device_name, name, sizeof(device_name) - 1);
        ESP_LOGI(TAG, "Device name set to: %s (restart required)", name);
    }
}

static void clear_all_preferences(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);

        stored_ssid[0] = '\0';
        stored_password[0] = '\0';
        auto_connect = true;
        strcpy(device_name, "ESP32-BLE");

        ESP_LOGI(TAG, "All preferences cleared");
    }
}

// ============ BLE Helper ============

static void send_response(const char *msg)
{
    if (device_connected && tx_char_handle != 0)
    {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(msg, strlen(msg));
        if (om)
        {
            // Using notify for simplicity, matches iOS app expectation
            ble_gatts_notify_custom(conn_handle, tx_char_handle, om);
        }
    }
    ESP_LOGI(TAG, "TX: %s", msg);
}

// ============ JSON Helper ============

static void json_escape(const char *str, char *out, size_t out_size)
{
    size_t j = 0;
    for (size_t i = 0; str[i] && j < out_size - 6; i++)
    {
        char c = str[i];
        switch (c)
        {
        case '"':
            out[j++] = '\\';
            out[j++] = '"';
            break;
        case '\\':
            out[j++] = '\\';
            out[j++] = '\\';
            break;
        case '\b':
            out[j++] = '\\';
            out[j++] = 'b';
            break;
        case '\f':
            out[j++] = '\\';
            out[j++] = 'f';
            break;
        case '\n':
            out[j++] = '\\';
            out[j++] = 'n';
            break;
        case '\r':
            out[j++] = '\\';
            out[j++] = 'r';
            break;
        case '\t':
            out[j++] = '\\';
            out[j++] = 't';
            break;
        default:
            if (c < 32)
            {
                j += snprintf(&out[j], out_size - j, "\\u%04x", (unsigned int)c);
            }
            else
            {
                out[j++] = c;
            }
        }
    }
    out[j] = '\0';
}

// ============ WiFi Event Handler ============

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
            ESP_LOGI(TAG, "WiFi disconnected. Reason: %d", ((wifi_event_sta_disconnected_t *)event_data)->reason);
        }
        else if (event_id == WIFI_EVENT_SCAN_DONE)
        {
            ESP_LOGI(TAG, "WiFi scan done, building networks list...");
            build_networks_json(cached_networks_json, sizeof(cached_networks_json));
            scan_in_progress = false;
            app_cmd_t cmd_to_queue = {.cmd = "status()"};
            if (xQueueSend(app_task_queue, &cmd_to_queue, 0) != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to queue status() command after scan");
            }
            else
            {
                ESP_LOGI(TAG, "Queued status() command to send scan results");
            }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        app_cmd_t cmd_to_queue = {.cmd = "status()"};
        xQueueSend(app_task_queue, &cmd_to_queue, pdMS_TO_TICKS(100));    
    }
}

// ============ WiFi Scan ============

static bool start_async_scan(void)
{
    if (scan_in_progress)
    {
        ESP_LOGI(TAG, "Scan already in progress.");
        return true;
    }

    ESP_LOGI(TAG, "Attempting to start async WiFi scan...");

    wifi_scan_config_t scan_config = {
        .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = false, .scan_type = WIFI_SCAN_TYPE_ACTIVE, .scan_time.active = {.min = 100, .max = 300}};
    esp_err_t err = esp_wifi_scan_start(&scan_config, false); // Non-blocking scan

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

static void build_networks_json(char *json_out, size_t max_size)
{
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count == 0)
    {
        snprintf(json_out, max_size, "\"available_networks\":[]");
        return;
    }

    // Limit to a reasonable number to avoid oversized packets
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

    // Cache results
    strncpy(cached_networks_json, json_out, sizeof(cached_networks_json) - 1);
    last_scan_time = esp_timer_get_time() / 1000;
}

// ============ Command Handlers (run in app_task context) ============

static void cmd_echo(const char *arg)
{
    send_response(arg);
}

static void cmd_connect(const char *ssid, const char *password, bool save)
{
    ESP_LOGI(TAG, "Connecting to: %s", ssid);
    send_response("{\"status\":\"connecting\"}");

    cached_networks_json[0] = '\0';
    last_scan_time = 0;

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    // Disconnect from any previous network first
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    // NOTE: We don't block here. The result is handled by the event handlers.
    // A better implementation would use an event group to wait for connection status.
    // For now, the user can poll with the 'status' command.

    if (save)
    {
        save_wifi_credentials(ssid, password);
    }
}

static void cmd_reconnect(void)
{
    if (stored_ssid[0] == '\0')
    {
        send_response("{\"error\":\"no saved credentials\"}");
        return;
    }
    cmd_connect(stored_ssid, stored_password, false);
}

static void cmd_disconnect(void)
{
    esp_wifi_disconnect();
    cached_networks_json[0] = '\0';
    last_scan_time = 0;
    send_response("{\"status\":\"disconnected\"}");
    cmd_status();
}

static void cmd_forget(void)
{
    esp_wifi_disconnect();

    nvs_handle_t handle;
    if (nvs_open("config", NVS_READWRITE, &handle) == ESP_OK)
    {
        nvs_erase_key(handle, "ssid");
        nvs_erase_key(handle, "password");
        nvs_commit(handle);
        nvs_close(handle);
    }

    stored_ssid[0] = '\0';
    stored_password[0] = '\0';
    cached_networks_json[0] = '\0';
    last_scan_time = 0;

    send_response("{\"status\":\"credentials_cleared\"}");
    start_async_scan();
}

static void cmd_status(void)
{
    char json[1024]; // Increased size for networks
    size_t offset = 0;

    wifi_ap_record_t ap_info;
    bool is_connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);

    char ssid_escaped[65];
    json_escape(stored_ssid, ssid_escaped, sizeof(ssid_escaped));

    char devname_escaped[65];
    json_escape(device_name, devname_escaped, sizeof(devname_escaped));

    nvs_stats_t nvs_stats;
    nvs_get_stats(NULL, &nvs_stats);

    if (is_connected)
    {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);

        offset = snprintf(json, sizeof(json),
                          "{\"wifi\":true,\"rssi\":%d,\"ip\":\"" IPSTR "\",\"heap\":%lu,"
                          "\"uptime\":%lld,\"ble\":%s,\"saved_ssid\":\"%s\","
                          "\"autoconnect\":%s,\"devname\":\"%s\",\"nvs_free\":%zu}",
                          ap_info.rssi, IP2STR(&ip_info.ip), (unsigned long)esp_get_free_heap_size(),
                          esp_timer_get_time() / 1000000, device_connected ? "true" : "false",
                          ssid_escaped, auto_connect ? "true" : "false", devname_escaped, nvs_stats.free_entries);
    }
    else
    {
        offset = snprintf(json, sizeof(json),
                          "{\"wifi\":false,\"rssi\":0,\"ip\":\"\",\"heap\":%lu,"
                          "\"uptime\":%lld,\"ble\":%s,\"saved_ssid\":\"%s\","
                          "\"autoconnect\":%s,\"devname\":\"%s\",\"nvs_free\":%zu,",
                          (unsigned long)esp_get_free_heap_size(), esp_timer_get_time() / 1000000,
                          device_connected ? "true" : "false", ssid_escaped,
                          auto_connect ? "true" : "false", devname_escaped, nvs_stats.free_entries);

        int64_t time_since_scan = (esp_timer_get_time() / 1000) - last_scan_time;

        if (scan_in_progress)
        {
            offset += snprintf(json + offset, sizeof(json) - offset, "\"scanning\":true}");
        }
        else if (cached_networks_json[0] != '\0' && time_since_scan < SCAN_CACHE_DURATION_MS)
        {
            offset += snprintf(json + offset, sizeof(json) - offset, "%s}", cached_networks_json);
        }
        else
        {
            // Scan results are stale. Try to start a new one.
            bool scan_started = start_async_scan();
            if (scan_started)
            {
                resend_status_after_scan = true;
                offset += snprintf(json + offset, sizeof(json) - offset, "\"scanning\":true}");
            }
            else
            {
                // Scan didn't start, probably because we are connecting.
                // Report that we are not scanning and provide an empty network list.
                resend_status_after_scan = false;
                offset += snprintf(json + offset, sizeof(json) - offset, "\"scanning\":false, \"available_networks\":[]}");
            }
        }
    }
    send_response(json);
}

static void cmd_set_auto_connect(bool value)
{
    save_auto_connect(value);
    char resp[48];
    snprintf(resp, sizeof(resp), "{\"autoconnect\":%s}", value ? "true" : "false");
    send_response(resp);
}

static void cmd_set_name(const char *name)
{
    save_device_name(name);
    char resp[128];
    char name_escaped[65];
    json_escape(name, name_escaped, sizeof(name_escaped));
    snprintf(resp, sizeof(resp), "{\"devname\":\"%s\",\"note\":\"restart required\"}", name_escaped);
    send_response(resp);
}

static void cmd_reset(void)
{
    clear_all_preferences();
    send_response("{\"status\":\"factory_reset\",\"note\":\"restarting...\"}");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static void cmd_restart(void)
{
    send_response("{\"status\":\"restarting...\"}");
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
    send_response(help);
}

// ============ Command Parser ============

static void process_command(const char *input)
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
        (arg1 && arg2) ? cmd_connect(arg1, arg2, true) : send_response("{\"error\":\"usage: connect(\\\"ssid\\\",\\\"pass\\\")\"}");
    else if (strcmp(cmd, "reconnect") == 0)
        cmd_reconnect();
    else if (strcmp(cmd, "disconnect") == 0)
        cmd_disconnect();
    else if (strcmp(cmd, "forget") == 0)
        cmd_forget();
    else if (strcmp(cmd, "status") == 0)
        cmd_status();
    else if (strcmp(cmd, "autoconnect") == 0)
        has_bool_arg ? cmd_set_auto_connect(bool_arg) : send_response("{\"error\":\"usage: autoconnect(true|false)\"}");
    else if (strcmp(cmd, "setname") == 0)
        arg1 ? cmd_set_name(arg1) : send_response("{\"error\":\"usage: setname(\\\"name\\\")\"}");
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
        send_response(resp);
    }
}

// ============ BLE GATT Handlers ============

static int gatt_char_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(SERVICE_UUID_BASE),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID128_DECLARE(CHAR_UUID_TX_BASE),
                .val_handle = &tx_char_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .access_cb = gatt_char_access,
            },
            {
                .uuid = BLE_UUID128_DECLARE(CHAR_UUID_RX_BASE),
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .access_cb = gatt_char_access,
            },
            {0}},
    },
    {0}};

static int gatt_char_access(uint16_t conn_handle_param, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len > 0 && om_len < APP_CMD_MAX_LEN)
        {
            app_cmd_t cmd_to_queue;
            ble_hs_mbuf_to_flat(ctxt->om, cmd_to_queue.cmd, om_len, NULL);
            cmd_to_queue.cmd[om_len] = '\0';
            ESP_LOGI(TAG, "RX: Queuing command: %s", cmd_to_queue.cmd);
            if (xQueueSend(app_task_queue, &cmd_to_queue, pdMS_TO_TICKS(100)) != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to queue command, queue full");
            }
        }
        return 0;
    }
    // All other ops are not used in this simple example
    return BLE_ATT_ERR_UNLIKELY;
}

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            ESP_LOGI(TAG, "BLE Connected; conn_handle=%d", event->connect.conn_handle);
            device_connected = true;
            conn_handle = event->connect.conn_handle;
            if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0)
            {
                struct ble_gap_upd_params params = {
                    .itvl_min = desc.conn_itvl,
                    .itvl_max = desc.conn_itvl,
                    .latency = desc.conn_latency,
                    .supervision_timeout = desc.supervision_timeout};
                ble_gap_update_params(event->connect.conn_handle, &params);
            }
        }
        else
        {
            ESP_LOGE(TAG, "BLE Connection failed; status=%d, restarting advertising", event->connect.status);
            start_ble_advertising(0);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE Disconnected; reason=%d", event->disconnect.reason);
        device_connected = false;
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        start_ble_advertising(0);
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Subscribe event; cur_notify=%d", event->subscribe.cur_notify);
        if (event->subscribe.attr_handle == tx_char_handle)
        {
            app_cmd_t cmd_to_queue = {.cmd = "status()"};
            xQueueSend(app_task_queue, &cmd_to_queue, pdMS_TO_TICKS(100));
        }
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update; conn_handle=%d, mtu=%d", event->mtu.conn_handle, event->mtu.value);
        break;
    }
    return 0;
}

static void start_ble_advertising(int reason)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields sr_fields; // Scan response fields
    const char *name;
    int rc;

    // --- Advertisement data ---
    memset(&fields, 0, sizeof(fields));

    // Flags: General discoverable, BR/EDR not supported.
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Device name
    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return;
    }

    // --- Scan response data ---
    memset(&sr_fields, 0, sizeof(sr_fields));

    // Service UUID
    ble_uuid128_t service_uuid = BLE_UUID128_INIT(SERVICE_UUID_BASE);
    sr_fields.uuids128 = &service_uuid;
    sr_fields.num_uuids128 = 1;
    sr_fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&sr_fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error setting scan response data; rc=%d", rc);
        return;
    }

    /* Begin advertising */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
        return;
    }
}

static void ble_on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "BLE host synced, starting advertising");
    start_ble_advertising(0);
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// ============ App Task ============
static void app_task(void *pvParameters)
{
    app_cmd_t received_cmd;
    ESP_LOGI(TAG, "Application task started");

    // Initial actions
    if (auto_connect && stored_ssid[0] != '\0')
    {
        ESP_LOGI(TAG, "Auto-connecting to: %s", stored_ssid);
        wifi_config_t wifi_config = {0};
        strncpy((char *)wifi_config.sta.ssid, stored_ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *)wifi_config.sta.password, stored_password, sizeof(wifi_config.sta.password) - 1);
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_connect();
    }
    else
    {
        ESP_LOGI(TAG, "WiFi ready (disconnected)");
        start_async_scan();
    }

    while (1)
    {
        if (xQueueReceive(app_task_queue, &received_cmd, portMAX_DELAY) == pdPASS)
        {
            ESP_LOGI(TAG, "APP_TASK: Dequeued command: %s", received_cmd.cmd);
            process_command(received_cmd.cmd);
        }
    }
}

// ============ Main ============

void app_main(void)
{
    ESP_LOGI(TAG, "\n=== ESP32 BLE WiFi Manager ===");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    load_preferences();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Create the queue for the application task
    app_task_queue = xQueueCreate(APP_TASK_QUEUE_SIZE, sizeof(app_cmd_t));

    // Create the application task
    xTaskCreate(app_task, "app_task", 4096, NULL, 5, NULL);

    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = start_ble_advertising; // Restart advertising on reset
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Set MTU to the maximum value
    ble_att_set_preferred_mtu(517);

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(device_name);

    ESP_ERROR_CHECK(ble_gatts_count_cfg(gatt_svcs));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(gatt_svcs));

    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "Setup complete.");
}
