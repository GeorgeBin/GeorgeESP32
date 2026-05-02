#include "ble_provisioning.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "protocomm_ble.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "system_status.h"

#define BLE_PROVISIONING_RESTART_DELAY_MS 2000

static const char *TAG = "ble_provisioning";
static esp_event_handler_instance_t s_prov_handler_instance;
static esp_event_handler_instance_t s_ble_transport_handler_instance;

static void restart_task(void *arg)
{
    (void) arg;
    vTaskDelay(pdMS_TO_TICKS(BLE_PROVISIONING_RESTART_DELAY_MS));
    ESP_LOGI(TAG, "Restarting after provisioning success");
    esp_restart();
}

static void schedule_restart(void)
{
    static bool restart_scheduled;

    if (restart_scheduled) {
        return;
    }

    restart_scheduled = true;
    if (xTaskCreate(restart_task, "prov_restart", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create provisioning restart task");
        esp_restart();
    }
}

static void provisioning_event_handler(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data)
{
    (void) arg;

    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            system_status_set_device_state(DEVICE_STATE_PROVISIONING);
            system_status_set_ble(true, false);
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *) event_data;
            ESP_LOGI(TAG, "Received Wi-Fi credentials for SSID: %s", (const char *) wifi_sta_cfg->ssid);
            system_status_set_wifi((const char *) wifi_sta_cfg->ssid, NULL, false);
            system_status_set_device_state(DEVICE_STATE_WIFI_CONNECTING);
            break;
        }
        case WIFI_PROV_CRED_FAIL: {
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *) event_data;
            ESP_LOGE(TAG, "Provisioning failed: %s",
                     (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "auth error" : "AP not found");
            system_status_set_device_state(DEVICE_STATE_WIFI_FAILED);
            break;
        }
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            system_status_set_device_state(DEVICE_STATE_WIFI_CONNECTED);
            schedule_restart();
            break;
        case WIFI_PROV_END:
            ESP_LOGI(TAG, "Provisioning ended");
            wifi_prov_mgr_deinit();
            break;
        default:
            break;
        }
    } else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        switch (event_id) {
        case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
            ESP_LOGI(TAG, "Provisioning BLE transport connected");
            system_status_set_ble(true, true);
            break;
        case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
            ESP_LOGI(TAG, "Provisioning BLE transport disconnected");
            system_status_set_ble(true, false);
            break;
        default:
            break;
        }
    }
}

esp_err_t ble_provisioning_start(void)
{
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
        .app_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
        .wifi_prov_conn_cfg = {
            .wifi_conn_attempts = 5,
        },
    };
    wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
    wifi_prov_security1_params_t *sec_params = BLE_PROVISIONING_POP;
    uint8_t custom_service_uuid[] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x10, 0xa0, 0x00, 0x00,
    };

    if (s_prov_handler_instance == NULL) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                                provisioning_event_handler, NULL,
                                                &s_prov_handler_instance),
            TAG, "register provisioning handler failed");
    }
    if (s_ble_transport_handler_instance == NULL) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID,
                                                provisioning_event_handler, NULL,
                                                &s_ble_transport_handler_instance),
            TAG, "register provisioning BLE handler failed");
    }

    ESP_RETURN_ON_ERROR(wifi_prov_mgr_init(config), TAG, "provisioning manager init failed");
    ESP_RETURN_ON_ERROR(wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid), TAG,
                        "set provisioning BLE UUID failed");

    system_status_set_device_state(DEVICE_STATE_WAIT_PROVISIONING);
    system_status_set_ble(true, false);
    ESP_RETURN_ON_ERROR(
        wifi_prov_mgr_start_provisioning(security, (const void *) sec_params,
                                         BLE_PROVISIONING_DEVICE_NAME, NULL),
        TAG, "start provisioning failed");

    ESP_LOGI(TAG, "BLE provisioning started. Device=%s PoP=%s",
             BLE_PROVISIONING_DEVICE_NAME, BLE_PROVISIONING_POP);
    return ESP_OK;
}
