#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t wifi_service_init_sta(void);
bool wifi_service_wait_connected(uint32_t timeout_ms);
