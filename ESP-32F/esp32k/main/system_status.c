#include "system_status.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_status_mutex;
static system_status_snapshot_t s_status = {
    .wifi_connected = false,
    .wifi_ssid = "",
    .ip_address = "waiting...",
    .led_mode = LED_MODE_SOLID,
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

void system_status_set_led_mode(led_mode_t mode)
{
    if (s_status_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_status_mutex, portMAX_DELAY) == pdTRUE) {
        s_status.led_mode = mode;
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
