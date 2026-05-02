#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "message_center.h"

typedef enum {
    DEVICE_STATE_BOOTING = 0,
    DEVICE_STATE_WAIT_PROVISIONING,
    DEVICE_STATE_PROVISIONING,
    DEVICE_STATE_WIFI_CONNECTING,
    DEVICE_STATE_WIFI_CONNECTED,
    DEVICE_STATE_WIFI_FAILED,
    DEVICE_STATE_NORMAL_RUNNING,
    DEVICE_STATE_RESETTING_WIFI,
} device_state_t;

typedef struct {
    device_state_t device_state;
    bool wifi_connected;
    bool ble_enabled;
    bool ble_connected;
    char wifi_ssid[33];
    char ip_address[16];
    led_command_t led;
    control_source_t last_source;
    int last_result_code;
    char last_result_msg[32];
} system_status_snapshot_t;

esp_err_t system_status_init(void);
void system_status_set_device_state(device_state_t state);
void system_status_set_wifi(const char *ssid, const char *ip_or_null, bool connected);
void system_status_set_ble(bool enabled, bool connected);
void system_status_set_led_command(const led_command_t *command);
void system_status_set_last_result(int code, const char *msg);
void system_status_get_snapshot(system_status_snapshot_t *snapshot);
const char *system_status_led_mode_to_string(led_mode_t mode);
const char *system_status_control_source_to_string(control_source_t source);
const char *system_status_device_state_to_string(device_state_t state);
