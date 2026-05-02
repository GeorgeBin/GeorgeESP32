#pragma once

#include "esp_err.h"

#define BLE_PROVISIONING_DEVICE_NAME "George_LED_Device"
#define BLE_PROVISIONING_POP "abcd1234"

esp_err_t ble_provisioning_start(void);
