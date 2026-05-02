#include "ble_manager.h"

#include <string.h>

#include "ble_led_service.h"
#include "esp_check.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "system_status.h"

#define BLE_DEVICE_NAME "George_LED_Device"

static const char *TAG = "ble_manager";
static uint8_t s_own_addr_type;

static const ble_uuid128_t BLE_LED_SERVICE_UUID =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x01, 0xa0, 0x00, 0x00);

static int ble_gap_event(struct ble_gap_event *event, void *arg);

static esp_err_t init_nvs_once(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase failed");
        ret = nvs_flash_init();
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return ret;
}

static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *) BLE_DEVICE_NAME;
    fields.name_len = strlen(BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "set advertising fields failed: %d", rc);
        return;
    }

    rsp_fields.uuids128 = &BLE_LED_SERVICE_UUID;
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 1;
    rsp_fields.tx_pwr_lvl_is_present = 1;
    rsp_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "set scan response fields failed: %d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "start advertising failed: %d", rc);
        return;
    }

    system_status_set_ble(true, false);
    ESP_LOGI(TAG, "BLE advertising as %s", BLE_DEVICE_NAME);
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    (void) arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "BLE connected, handle=%d", event->connect.conn_handle);
            ble_led_service_set_connection(event->connect.conn_handle, true);
            system_status_set_ble(true, true);
            ble_led_service_notify_state(0, 0, "ok");
        } else {
            ESP_LOGW(TAG, "BLE connection failed: %d", event->connect.status);
            ble_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected, reason=%d", event->disconnect.reason);
        ble_led_service_set_connection(BLE_HS_CONN_HANDLE_NONE, false);
        system_status_set_ble(true, false);
        ble_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE advertising complete");
        ble_advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ble_led_service_set_notify_enabled(event->subscribe.attr_handle,
                                           event->subscribe.cur_notify);
        ESP_LOGI(TAG, "BLE subscribe attr=%d notify=%d",
                 event->subscribe.attr_handle, event->subscribe.cur_notify);
        break;

    default:
        break;
    }

    return 0;
}

static void ble_on_sync(void)
{
    const int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer BLE address failed: %d", rc);
        return;
    }
    ble_advertise();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE host reset: %d", reason);
    system_status_set_ble(false, false);
}

static void ble_host_task(void *param)
{
    (void) param;
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_manager_start(void)
{
    ESP_RETURN_ON_ERROR(init_nvs_once(), TAG, "nvs init failed");

    esp_err_t ret = nimble_port_init();
    ESP_RETURN_ON_ERROR(ret, TAG, "nimble init failed");

    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    ESP_RETURN_ON_ERROR(ble_led_service_init(), TAG, "LED service init failed");

    int rc = ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
    ESP_RETURN_ON_FALSE(rc == 0, ESP_FAIL, TAG, "set device name failed: %d", rc);

    system_status_set_ble(true, false);
    nimble_port_freertos_init(ble_host_task);
    return ESP_OK;
}
