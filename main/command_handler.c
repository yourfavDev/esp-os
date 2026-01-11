/**
 * @file command_handler.c
 * @brief Implementation for the command handler.
 */

#include "command_handler.h"
#include "minmea.h"
#include "app_includes.h"
#include "nvs.h" 
#include <string.h>
#include <stdlib.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "ble_manager.h"
#include "wifi_manager.h"
#include "driver/gpio.h"
#include "nvs_storage.h"
#include "utils.h"
#include "app_task.h" 

static const char *TAG = "CMD_HANDLER";



// --- Command Handler Forward Declarations ---
static void cmd_echo(const char *arg);
static void cmd_connect(const char *ssid, const char *password, bool save);
static void cmd_reconnect(void);
static void cmd_disconnect(void);
static void cmd_led(void);
static void cmd_forget(void);
static void cmd_status(void);
static void cmd_set_auto_connect(bool value);
static void cmd_set_name(const char *name);
static void cmd_reset(void);
static void cmd_restart(void);
static void cmd_help(void);
//static void gps(void);


// static TaskHandle_t s_gps_task_handle = NULL;
// // --- U-BLOX M8N CONFIGURATION COMMANDS ---
// // 1. UBX-CFG-PRT: Set UART1 to 115200 baud, 8N1, UBX+NMEA out
// static const uint8_t UBX_SET_115200[] = {
//     0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 
//     0xD0, 0x08, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x07, 0x00, 
//     0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC4, 0x96
// };

// // 2. UBX-CFG-RATE: Set update rate to 10Hz (100ms)
// static const uint8_t UBX_SET_10HZ[] = {
//     0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0x64, 0x00, 0x01, 0x00, 
//     0x01, 0x00, 0x7A, 0x12
// };

// // 3. UBX-CFG-GNSS: Enable GPS + GLONASS (Better signal for M8N)
// // This is optional but recommended for M8N to get faster lock
// static const uint8_t UBX_ENABLE_GPS_GLONASS[] = {
//     0xB5, 0x62, 0x06, 0x3E, 0x2C, 0x00, 0x00, 0x00, 0x20, 0x05, 
//     0x00, 0x08, 0x10, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 
//     0x03, 0x00, 0x01, 0x00, 0x01, 0x01, 0x04, 0x00, 0x08, 0x00, 
//     0x00, 0x00, 0x01, 0x01, 0x05, 0x00, 0x03, 0x00, 0x01, 0x00, 
//     0x01, 0x01, 0x06, 0x08, 0x0E, 0x00, 0x01, 0x00, 0x01, 0x01, 
//     0xFC, 0x11
// };

// // ==========================================================
// // GPS BACKGROUND TASK (High Performance 10Hz Mode)
// // ==========================================================
// static void gps_task_entry(void *arg) {
//     ESP_LOGI(TAG, ">>> GPS TASK STARTED (10Hz High Performance) <<<");

//     // 1. Initial Setup at DEFAULT 9600
//     // We must start at 9600 to talk to the GPS before we configure it.
//     if (!uart_is_driver_installed(GPS_UART_NUM)) {
//         uart_config_t uart_config = {
//             .baud_rate = 9600,
//             .data_bits = UART_DATA_8_BITS,
//             .parity = UART_PARITY_DISABLE,
//             .stop_bits = UART_STOP_BITS_1,
//             .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//             .source_clk = UART_SCLK_DEFAULT,
//         };
//         uart_driver_install(GPS_UART_NUM, GPS_BUF_SIZE * 2, 0, 0, NULL, 0);
//         uart_param_config(GPS_UART_NUM, &uart_config);
//         uart_set_pin(GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
//     }

//     // --- BAUD RATE SWITCHING SEQUENCE ---
//     ESP_LOGW(TAG, "Switching GPS to 115200 baud...");

//     // A. Send command to GPS to switch to 115200 (Sent at 9600)
//     uart_write_bytes(GPS_UART_NUM, (const char*)UBX_SET_115200, sizeof(UBX_SET_115200));
    
//     // B. CRITICAL DELAY: Give GPS time to process command and change its internal clock
//     vTaskDelay(pdMS_TO_TICKS(200)); 

//     // C. Now switch ESP32 to 115200 to match
//     uart_flush(GPS_UART_NUM);
//     uart_set_baudrate(GPS_UART_NUM, 115200);
//     ESP_LOGI(TAG, "ESP32 UART switched to 115200");
//     vTaskDelay(pdMS_TO_TICKS(100)); // Let connection stabilize

//     // D. Send 10Hz Command (Sent at 115200)
//     uart_write_bytes(GPS_UART_NUM, (const char*)UBX_SET_10HZ, sizeof(UBX_SET_10HZ));
//     vTaskDelay(pdMS_TO_TICKS(50));

//     // E. (Optional) Enable GLONASS for better M8N performance
//     uart_write_bytes(GPS_UART_NUM, (const char*)UBX_ENABLE_GPS_GLONASS, sizeof(UBX_ENABLE_GPS_GLONASS));

//     uart_flush_input(GPS_UART_NUM); 

//     uint8_t *data = (uint8_t *) malloc(GPS_BUF_SIZE);
//     char line_buffer[256];
//     int line_pos = 0;
//     char response_buffer[128]; 

//     // Timers
//     uint32_t last_valid_send = 0;
//     uint32_t last_data_received_time = pdTICKS_TO_MS(xTaskGetTickCount());

//     while (1) {
//         // Read fast! 10Hz means data comes every 100ms.
//         int len = uart_read_bytes(GPS_UART_NUM, data, GPS_BUF_SIZE, pdMS_TO_TICKS(50));
//         uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

//         if (len > 0) {
//             last_data_received_time = now;

//             for (int i = 0; i < len; i++) {
//                 char c = (char)data[i];
                
//                 if (c == '\n' || c == '\r') {
//                     line_buffer[line_pos] = '\0'; 
                    
//                     if (line_pos > 5 && line_buffer[0] == '$') {
//                         // Accept GNRMC (GLONASS+GPS) or GPRMC (GPS only)
//                         if (strstr(line_buffer, "RMC") != NULL) {
                            
//                             struct minmea_sentence_rmc frame;
//                             if (minmea_parse_rmc(&frame, line_buffer)) {
                                
//                                 if (frame.valid) {
//                                     // SEND DATA AS FAST AS IT COMES (Limit to approx 10Hz max to not flood BLE)
//                                     // 80ms throttle ensures we don't choke the BLE stack
//                                     if ((now - last_valid_send) > 80) { 
//                                         float lat = minmea_tocoord(&frame.latitude);
//                                         float lon = minmea_tocoord(&frame.longitude);
//                                         float speed = minmea_tofloat(&frame.speed) * 1.852; 
                                        
//                                         snprintf(response_buffer, sizeof(response_buffer), 
//                                                  "{\"gps\":true, \"lat\":%f, \"lon\":%f, \"kph\":%.2f}", 
//                                                  lat, lon, speed);
                                        
//                                         ble_manager_send_response(response_buffer);
//                                         last_valid_send = now;
//                                     }
//                                 } else {
//                                     // Searching... don't spam this, just once per 2s
//                                     static uint32_t last_search = 0;
//                                     if (now - last_search > 2000) {
//                                         ble_manager_send_response("{\"gps\":false, \"status\":\"searching_10hz...\"}");
//                                         last_search = now;
//                                     }
//                                 }
//                             }
//                         }
//                     }
//                     line_pos = 0; 
//                 } else {
//                     if (line_pos < sizeof(line_buffer) - 1) {
//                         line_buffer[line_pos++] = c;
//                     }
//                 }
//             }
//         } else {
//             // Failsafe: If no data for 5 seconds, the baud switch might have failed.
//             if ((now - last_data_received_time) > 5000) {
//                 ESP_LOGE(TAG, "GPS TIMEOUT: Reverting to 9600 baud to attempt recovery...");
//                 uart_set_baudrate(GPS_UART_NUM, 9600);
//                 last_data_received_time = now; // Reset timer to avoid loops
//                 vTaskDelay(pdMS_TO_TICKS(1000));
//             }
//         }
//     }
//     free(data);
//     vTaskDelete(NULL);
// }

// // ==========================================================
// // GPS BACKGROUND TASK 
// // ==========================================================

// static void gps(void) {
//     ESP_LOGI(TAG, "GPS Command Triggered!");
    
//     if (s_gps_task_handle != NULL) {
//         ble_manager_send_response("{\"error\":\"already_running\"}");
//         return;
//     }

//     // Create the task with 4096 stack size (sufficient for UART/printf)
//     BaseType_t res = xTaskCreate(gps_task_entry, "gps_task", 4096, NULL, 5, &s_gps_task_handle);
    
//     if (res == pdPASS) {
//         ble_manager_send_response("{\"status\":\"gps_started\"}");
//     } else {
//         ble_manager_send_response("{\"error\":\"task_create_failed\"}");
//     }
// }


// --- STANDARD COMMANDS ---

static void cmd_echo(const char *arg)
{
    ble_manager_send_response(arg);
}

static void cmd_connect(const char *ssid, const char *password, bool save)
{
    ESP_LOGI(TAG, "Executing command: connect to %s", ssid);
    ble_manager_send_response("{\"status\":\"connecting\"}");
    wifi_manager_connect(ssid, password);
    if (save) nvs_storage_save_wifi_credentials(ssid, password);
}

static void cmd_reconnect(void)
{
    const char *ssid = nvs_storage_get_ssid();
    if (ssid[0] == '\0') {
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
    cmd_status(); 
}
#define LED_PIN 23

static void cmd_led(void) {
    // 1. Static variable to keep track of the state (persists between calls)
    static bool led_state = false;

    // 2. Initialize the pin only once
    static bool is_initialized = false;
    if (!is_initialized) {
        gpio_reset_pin(LED_PIN);
                gpio_reset_pin(22);

        gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
                gpio_set_direction(22, GPIO_MODE_OUTPUT);

        is_initialized = true;
    }

    // 3. Toggle the state
    led_state = !led_state;

    // 4. Update the physical hardware
    gpio_set_level(LED_PIN, led_state);
        gpio_set_level(22, led_state);

}

static void cmd_forget(void)
{
    ESP_LOGI(TAG, "Executing command: forget wifi");
    wifi_manager_disconnect();
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
        "\"gps()\","
        "\"echo(\\\"msg\\\")\""
        "]}";
    ble_manager_send_response(help);
}

// ==========================================================
// FIXED COMMAND PROCESSOR
// Handles both "gps" and "gps()" formats
// ==========================================================
void command_handler_process(const char *input)
{
    char buf[APP_CMD_MAX_LEN];
    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Remove any trailing newline characters
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n')) {
        buf[len - 1] = '\0';
        len--;
    }

    char *cmd = buf;
    char *args = ""; 
    char *arg1 = NULL;
    char *arg2 = NULL;

    // Check if arguments exist (look for parenthesis)
    char *paren = strchr(buf, '(');
    if (paren) {
        *paren = '\0'; // Split command from args
        args = paren + 1;
        char *end = strrchr(args, ')');
        if (end) *end = '\0';
    } 
    // If no parenthesis, cmd is just the whole buffer, and args is empty string

    // Parse arguments if they exist
    if (args[0] != '\0') {
        char *q1 = strchr(args, '"');
        if (q1) {
            arg1 = q1 + 1;
            char *q2 = strchr(arg1, '"');
            if (q2) {
                *q2 = '\0';
                char *q3 = strchr(q2 + 1, '"');
                if (q3) {
                    arg2 = q3 + 1;
                    char *q4 = strchr(arg2, '"');
                    if (q4) *q4 = '\0';
                }
            }
        }
    }

    bool bool_arg = false;
    bool has_bool_arg = (strcmp(args, "true") == 0 || strcmp(args, "false") == 0);
    if (has_bool_arg) bool_arg = (strcmp(args, "true") == 0);

    // --- EXECUTE COMMANDS ---
    if (strcmp(cmd, "echo") == 0)
        cmd_echo(arg1 ? arg1 : "");
    else if (strcmp(cmd, "connect") == 0)
        (arg1 && arg2) ? cmd_connect(arg1, arg2, true) : ble_manager_send_response("{\"error\":\"usage: connect(\\\"ssid\\\",\\\"pass\\\")\"}");
    else if (strcmp(cmd, "reconnect") == 0)
        cmd_reconnect();
    else if (strcmp(cmd, "led") == 0)
        cmd_led();
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
    // else if (strcmp(cmd, "gps") == 0)
    //  //  gps(); // <--- Now triggers even if you just typed "gps"
    else if (strcmp(cmd, "help") == 0)
        cmd_help();
    else
    {
        char resp[160];
        snprintf(resp, sizeof(resp), "{\"error\":\"unknown: %s\"}", cmd);
        ble_manager_send_response(resp);
    }
}