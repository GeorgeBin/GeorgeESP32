#include "system_status.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_status_mutex;
static system_status_snapshot_t s_status = {
    .device_state = DEVICE_STATE_BOOTING,
    .wifi_connected = false,
    .ble_enabled = false,
    .ble_connected = false,
    .wifi_ssid = "",
    .ip_address = "waiting...",
    .led = {
        .color_r = 255,
        .color_g = 255,
        .color_b = 255,
        .brightness = 100,
        .mode = LED_MODE_SOLID,
        .period_ms = 2000,
        .on_ms = 500,
        .off_ms = 500,
        .source = CONTROL_SOURCE_NONE,
    },
    .last_source = CONTROL_SOURCE_NONE,
    .last_result_code = 0,
    .last_result_msg = "ok",
};

static void system_status_copy_string(char *dest, size_t dest_size, const char *src)
{
    if (dest_size == 0) {
        return;
    }

    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

esp_err_t system_status_init(void)
{
    if (s_status_mutex != NULL) {
        return ESP_OK;
    }

    s_status_mutex = xSemaphoreCreateMutex();
    return s_status_mutex != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

void system_status_set_device_state(device_state_t state)
{
    if (s_status_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_status_mutex, portMAX_DELAY) == pdTRUE) {
        s_status.device_state = state;
        xSemaphoreGive(s_status_mutex);
    }
}

void system_status_set_wifi(const char *ssid, const char *ip_or_null, bool connected)
{
    if (s_status_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_status_mutex, portMAX_DELAY) == pdTRUE) {
        system_status_copy_string(s_status.wifi_ssid, sizeof(s_status.wifi_ssid), ssid);
        if (ip_or_null != NULL) {
            system_status_copy_string(s_status.ip_address, sizeof(s_status.ip_address), ip_or_null);
        } else {
            system_status_copy_string(s_status.ip_address, sizeof(s_status.ip_address),
                                      connected ? "waiting..." : "unavailable");
        }
        s_status.wifi_connected = connected;
        xSemaphoreGive(s_status_mutex);
    }
}

void system_status_set_ble(bool enabled, bool connected)
{
    if (s_status_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_status_mutex, portMAX_DELAY) == pdTRUE) {
        s_status.ble_enabled = enabled;
        s_status.ble_connected = connected;
        xSemaphoreGive(s_status_mutex);
    }
}

void system_status_set_led_command(const led_command_t *command)
{
    if (s_status_mutex == NULL || command == NULL) {
        return;
    }

    if (xSemaphoreTake(s_status_mutex, portMAX_DELAY) == pdTRUE) {
        s_status.led = *command;
        s_status.last_source = command->source;
        xSemaphoreGive(s_status_mutex);
    }
}

void system_status_set_last_result(int code, const char *msg)
{
    if (s_status_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_status_mutex, portMAX_DELAY) == pdTRUE) {
        s_status.last_result_code = code;
        system_status_copy_string(s_status.last_result_msg, sizeof(s_status.last_result_msg), msg);
        xSemaphoreGive(s_status_mutex);
    }
}

void system_status_get_snapshot(system_status_snapshot_t *snapshot)
{
    if (s_status_mutex == NULL || snapshot == NULL) {
        return;
    }

    if (xSemaphoreTake(s_status_mutex, portMAX_DELAY) == pdTRUE) {
        *snapshot = s_status;
        xSemaphoreGive(s_status_mutex);
    }
}

const char *system_status_led_mode_to_string(led_mode_t mode)
{
    switch (mode) {
    case LED_MODE_OFF:
        return "off";
    case LED_MODE_SOLID:
        return "solid";
    case LED_MODE_BREATH:
        return "breath";
    case LED_MODE_BLINK:
        return "blink";
    default:
        return "unknown";
    }
}

const char *system_status_control_source_to_string(control_source_t source)
{
    switch (source) {
    case CONTROL_SOURCE_HTTP:
        return "http";
    case CONTROL_SOURCE_BLE:
        return "ble";
    case CONTROL_SOURCE_NONE:
    default:
        return "none";
    }
}

const char *system_status_device_state_to_string(device_state_t state)
{
    switch (state) {
    case DEVICE_STATE_BOOTING:
        return "booting";
    case DEVICE_STATE_WAIT_PROVISIONING:
        return "wait_provisioning";
    case DEVICE_STATE_PROVISIONING:
        return "provisioning";
    case DEVICE_STATE_WIFI_CONNECTING:
        return "wifi_connecting";
    case DEVICE_STATE_WIFI_CONNECTED:
        return "wifi_connected";
    case DEVICE_STATE_WIFI_FAILED:
        return "wifi_failed";
    case DEVICE_STATE_NORMAL_RUNNING:
        return "normal_running";
    case DEVICE_STATE_RESETTING_WIFI:
        return "resetting_wifi";
    default:
        return "unknown";
    }
}
