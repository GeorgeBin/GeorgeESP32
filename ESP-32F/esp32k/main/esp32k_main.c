#include "esp_check.h"
#include "esp_log.h"

#include "ble_manager.h"
#include "display_service.h"
#include "http_server_app.h"
#include "led_output.h"
#include "message_center.h"
#include "system_status.h"
#include "wifi_service.h"

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
    ESP_ERROR_CHECK(led_output_init(&default_command));
    ESP_ERROR_CHECK(message_center_submit(&default_command));
    ESP_ERROR_CHECK(display_service_init());
    ESP_ERROR_CHECK(ble_manager_start());
    ESP_ERROR_CHECK(wifi_service_init_sta());

    if (!wifi_service_wait_connected(15000)) {
        ESP_LOGW(TAG, "Wi-Fi not connected, HTTP server will not start; BLE remains available");
    } else {
        ESP_ERROR_CHECK(http_server_app_start());
    }

    ESP_LOGI(TAG, "System ready");
}
