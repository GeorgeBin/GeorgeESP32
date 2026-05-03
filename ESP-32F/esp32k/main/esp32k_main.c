#include "esp_check.h"
#include "esp_log.h"

#include "ble_ancs_manager.h"
#include "device_config.h"
#include "display_service.h"
#include "http_server_app.h"
#include "led_output.h"
#include "message_center.h"
#include "notification_rules.h"
#include "recent_apps.h"
#include "system_status.h"
#include "wifi_manager.h"

static const char *TAG = "esp32k_main";

void app_main(void)
{
    const led_command_t default_command = {
        .color_r = 255,
        .color_g = 255,
        .color_b = 255,
        .brightness = 100,
        .mode = LED_MODE_SOLID,
        .period_ms = 2000,
        .on_ms = 500,
        .off_ms = 500,
        .source = CONTROL_SOURCE_NONE,
    };

    ESP_ERROR_CHECK(message_center_init(1));
    ESP_ERROR_CHECK(system_status_init());
    ESP_ERROR_CHECK(device_config_init());
    ESP_ERROR_CHECK(notification_rules_init());
    ESP_ERROR_CHECK(recent_apps_init());
    ESP_ERROR_CHECK(led_output_init(&default_command));
    ESP_ERROR_CHECK(message_center_submit(&default_command));
    ESP_ERROR_CHECK(display_service_init());

    ESP_LOGI(TAG, "Starting ANCS BLE before Wi-Fi");
    ESP_ERROR_CHECK(ble_ancs_manager_start());

    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(wifi_manager_start_sta());
    if (!wifi_manager_wait_connected(15000)) {
        ESP_LOGW(TAG, "Wi-Fi not connected; HTTP server will not start");
    } else {
        ESP_ERROR_CHECK(http_server_app_start());
        system_status_set_device_state(DEVICE_STATE_NORMAL_RUNNING);
    }

    ESP_LOGI(TAG, "System ready");
}
