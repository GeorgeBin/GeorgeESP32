#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_SOLID,
    LED_MODE_BREATH,
    LED_MODE_BLINK,
} led_mode_t;

typedef enum {
    CONTROL_SOURCE_NONE = 0,
    CONTROL_SOURCE_HTTP,
    CONTROL_SOURCE_BLE,
    CONTROL_SOURCE_ANCS,
} control_source_t;

typedef struct {
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    uint8_t brightness;
    led_mode_t mode;
    uint32_t period_ms;
    uint32_t on_ms;
    uint32_t off_ms;
    control_source_t source;
} led_command_t;

esp_err_t message_center_init(size_t queue_length);
esp_err_t message_center_submit(const led_command_t *command);
bool message_center_receive(led_command_t *command, TickType_t timeout_ticks);
