/**
 * @file ble_manager.c
 * @brief Implementation for BLE management.
 */

#include "ble_manager.h"
#include "esp_log.h"
#include "nvs_storage.h" // For getting the device name
#include "app_task.h"    // For posting commands to the app task

// NimBLE host and controller includes
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_uuid.h"

static const char *TAG = "BLE_MANAGER";

// UUIDs for the custom service and characteristics (reversed for NimBLE)
#define SERVICE_UUID_BASE 0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e
#define CHAR_UUID_RX_BASE 0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e
#define CHAR_UUID_TX_BASE 0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e

// Module-level static variables for BLE state
static bool device_connected = false;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t tx_char_handle = 0;
static uint8_t own_addr_type;
static uint16_t negotiated_mtu = 6; // Default MTU, updated on event

// Forward declarations for local functions
static int gatt_char_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gap_event_handler(struct ble_gap_event *event, void *arg);
static void start_ble_advertising(int reason);
static void on_reset(int reason);
static void ble_on_sync(void);
static void ble_host_task(void *param);

// GATT service definition
static const ble_uuid128_t gatt_service_uuid = BLE_UUID128_INIT(SERVICE_UUID_BASE);
static const ble_uuid128_t gatt_char_tx_uuid = BLE_UUID128_INIT(CHAR_UUID_TX_BASE);
static const ble_uuid128_t gatt_char_rx_uuid = BLE_UUID128_INIT(CHAR_UUID_RX_BASE);

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (const ble_uuid_t *)&gatt_service_uuid,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = (const ble_uuid_t *)&gatt_char_tx_uuid,
                .val_handle = &tx_char_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .access_cb = gatt_char_access, // Same cb as RX, it will filter by op_type
            },
            {
                .uuid = (const ble_uuid_t *)&gatt_char_rx_uuid,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .access_cb = gatt_char_access,
            },
            {0}}, // End of characteristics
    },
    {0}}; // End of services

esp_err_t ble_manager_init(void)
{
    ESP_ERROR_CHECK(nimble_port_init());

    // Configure the BLE host
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Set a preferred MTU, the max supported is 517
    ble_att_set_preferred_mtu(517);

    // Initialize the GAP and GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Set the device name from NVS
    ble_svc_gap_device_name_set(nvs_storage_get_device_name());

    // Add our custom services
    ESP_ERROR_CHECK(ble_gatts_count_cfg(gatt_svcs));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(gatt_svcs));

    // Start the NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE Manager initialized.");
    return ESP_OK;
}

void ble_manager_send_response(const char *msg)
{
    if (!ble_manager_is_connected() || tx_char_handle == 0)
    {
        ESP_LOGW(TAG, "TX: Not connected, cannot send: %s", msg);
        return;
    }

    ESP_LOGI(TAG, "TX: %s", msg);

    size_t total_len = strlen(msg);
    uint16_t chunk_size = negotiated_mtu > 3 ? negotiated_mtu - 3 : 20; // 3 bytes for ATT header

    if (total_len <= chunk_size)
    {
        // Message fits in a single notification
        struct os_mbuf *om = ble_hs_mbuf_from_flat(msg, total_len);
        if (om)
        {
            ble_gatts_notify_custom(conn_handle, tx_char_handle, om);
        }
    }
    else
    {
        // Message needs to be chunked
        ESP_LOGI(TAG, "Chunking response of size %d into %d-byte chunks", total_len, chunk_size);
        for (size_t offset = 0; offset < total_len; offset += chunk_size)
        {
            size_t len_to_send = total_len - offset;
            if (len_to_send > chunk_size)
            {
                len_to_send = chunk_size;
            }

            struct os_mbuf *om = ble_hs_mbuf_from_flat(msg + offset, len_to_send);
            if (om)
            {
                ble_gatts_notify_custom(conn_handle, tx_char_handle, om);
                // A small delay is crucial to allow the BLE stack and client to process each chunk.
                vTaskDelay(pdMS_TO_TICKS(20)); 
            }
        }
    }
}

bool ble_manager_is_connected(void)
{
    return device_connected;
}

static void on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static int gatt_char_access(uint16_t conn_handle_param, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        char cmd_buf[APP_CMD_MAX_LEN];

        if (om_len > 0 && om_len < sizeof(cmd_buf))
        {
            ble_hs_mbuf_to_flat(ctxt->om, cmd_buf, om_len, NULL);
            cmd_buf[om_len] = '\0';
            ESP_LOGI(TAG, "RX: Queuing command: %s", cmd_buf);
            
            // Post the received command to the main application task queue
            app_task_queue_post(cmd_buf);
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            ESP_LOGI(TAG, "BLE Connected; conn_handle=%d", event->connect.conn_handle);
            device_connected = true;
            conn_handle = event->connect.conn_handle;
            // Reset MTU to default on new connection
            negotiated_mtu = 23;
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
        // Reset MTU to default
        negotiated_mtu = 23;
        start_ble_advertising(0);
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Subscribe event; cur_notify=%d, attr_handle=%d", event->subscribe.cur_notify, event->subscribe.attr_handle);
        if (event->subscribe.attr_handle == tx_char_handle)
        {
            // Client subscribed, send initial status
            app_task_queue_post("status()");
        }
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update; conn_handle=%d, mtu=%d", event->mtu.conn_handle, event->mtu.value);
        negotiated_mtu = event->mtu.value;
        break;
    }
    return 0;
}

static void start_ble_advertising(int reason)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields sr_fields; // Scan response
    const char *name = ble_svc_gap_device_name();

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    if (ble_gap_adv_set_fields(&fields) != 0)
    {
        ESP_LOGE(TAG, "Error setting advertisement data");
        return;
    }

    memset(&sr_fields, 0, sizeof(sr_fields));
    ble_uuid128_t service_uuid = BLE_UUID128_INIT(SERVICE_UUID_BASE);
    sr_fields.uuids128 = &service_uuid;
    sr_fields.num_uuids128 = 1;
    sr_fields.uuids128_is_complete = 1;

    if (ble_gap_adv_rsp_set_fields(&sr_fields) != 0)
    {
        ESP_LOGE(TAG, "Error setting scan response data");
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    if (ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event_handler, NULL) != 0)
    {
        ESP_LOGE(TAG, "Error enabling advertisement");
        return;
    }
    ESP_LOGI(TAG, "BLE advertising started. Device name: %s", name);
}

static void ble_on_sync(void)
{
    if (ble_hs_id_infer_auto(0, &own_addr_type) != 0)
    {
        ESP_LOGE(TAG, "Error determining address type");
        return;
    }
    ESP_LOGI(TAG, "BLE host synced, starting advertising.");
    start_ble_advertising(0);
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}
