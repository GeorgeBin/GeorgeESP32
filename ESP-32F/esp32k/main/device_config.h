#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "phase_effect.h"

typedef struct {
    char device_name[32];
    uint8_t default_brightness;
    uint8_t privacy_mode;
    uint8_t clear_behavior;
    phase_effect_t phase_boot;
    phase_effect_t phase_disconnected;
    phase_effect_t phase_standby;
    phase_effect_t phase_unmatched_call;
    phase_effect_t phase_unmatched_message;
} device_config_t;

esp_err_t device_config_init(void);
esp_err_t device_config_get(device_config_t *config);
esp_err_t device_config_set(const device_config_t *config);
