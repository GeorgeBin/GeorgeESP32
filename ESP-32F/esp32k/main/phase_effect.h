#pragma once

#include <stdint.h>

#include "cJSON.h"
#include "message_center.h"

typedef struct {
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    uint8_t brightness;
    led_mode_t mode;
    uint32_t duration_ms;
    uint8_t repeat;
} phase_effect_t;

void phase_effect_add_json(cJSON *root, const char *name, const phase_effect_t *effect);
bool phase_effect_parse_json(const cJSON *obj, phase_effect_t *effect);
void phase_effect_apply_defaults(phase_effect_t *effect);
