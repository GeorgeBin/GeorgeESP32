#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t ble_led_service_init(void);
void ble_led_service_set_connection(uint16_t conn_handle, bool connected);
void ble_led_service_set_notify_enabled(uint16_t attr_handle, bool enabled);
void ble_led_service_notify_state(uint32_t seq, int code, const char *msg);

