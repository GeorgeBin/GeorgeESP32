#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

typedef enum {
    LED_MODE_SOLID = 0,
    LED_MODE_BREATH,
    LED_MODE_BLINK,
} led_mode_t;

typedef struct {
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    led_mode_t mode;
} led_command_t;

esp_err_t message_center_init(size_t queue_length);
esp_err_t message_center_submit(const led_command_t *command);
bool message_center_receive(led_command_t *command, TickType_t timeout_ticks);
