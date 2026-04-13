#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "message_center.h"

typedef struct {
    bool wifi_connected;
    char wifi_ssid[33];
    char ip_address[16];
    led_mode_t led_mode;
} system_status_snapshot_t;

esp_err_t system_status_init(void);
void system_status_set_wifi(const char *ssid, const char *ip_or_null, bool connected);
void system_status_set_led_mode(led_mode_t mode);
void system_status_get_snapshot(system_status_snapshot_t *snapshot);
