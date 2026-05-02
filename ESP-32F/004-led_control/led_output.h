#pragma once

#include "esp_err.h"
#include "message_center.h"

esp_err_t led_output_init(const led_command_t *default_command);
