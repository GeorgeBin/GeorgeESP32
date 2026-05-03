#include "led_output.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_status.h"

#define LED_R_GPIO GPIO_NUM_27
#define LED_G_GPIO GPIO_NUM_33
#define LED_B_GPIO GPIO_NUM_32

#define LEDC_TIMER_RESOLUTION LEDC_TIMER_10_BIT
#define LEDC_MAX_DUTY ((1 << 10) - 1)
#define BREATH_MIN_BRIGHTNESS 16
#define BREATH_MAX_BRIGHTNESS 255
#define BREATH_STEP_SIZE 5
#define DEFAULT_BREATH_PERIOD_MS 2000
#define DEFAULT_BLINK_ON_MS 500
#define DEFAULT_BLINK_OFF_MS 500

static const char *TAG = "led_output";

static led_command_t s_current_command;

static void led_output_apply_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    const uint32_t duty_r = LEDC_MAX_DUTY - ((uint32_t) red * LEDC_MAX_DUTY / 255);
    const uint32_t duty_g = LEDC_MAX_DUTY - ((uint32_t) green * LEDC_MAX_DUTY / 255);
    const uint32_t duty_b = LEDC_MAX_DUTY - ((uint32_t) blue * LEDC_MAX_DUTY / 255);

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_r));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_g));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty_b));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2));
}

static void led_output_apply_scaled(const led_command_t *command, uint8_t brightness)
{
    const uint8_t command_brightness = command->brightness <= 100 ? command->brightness : 100;
    const uint8_t effective_brightness = (uint8_t) (((uint16_t) brightness * command_brightness) / 100);
    const uint8_t red = (uint8_t) (((uint16_t) command->color_r * effective_brightness) / 255);
    const uint8_t green = (uint8_t) (((uint16_t) command->color_g * effective_brightness) / 255);
    const uint8_t blue = (uint8_t) (((uint16_t) command->color_b * effective_brightness) / 255);

    led_output_apply_rgb(red, green, blue);
}

static bool led_output_drain_command(led_command_t *command)
{
    led_command_t latest_command;
    bool updated = false;

    while (message_center_receive(&latest_command, 0)) {
        *command = latest_command;
        updated = true;
    }

    return updated;
}

static void led_output_task(void *arg)
{
    (void) arg;

    led_command_t command = s_current_command;
    led_mode_t active_mode = command.mode;
    int brightness = BREATH_MIN_BRIGHTNESS;
    int8_t breath_step = 1;
    bool blink_on = true;
    uint8_t blink_completed = 0;
    TickType_t last_wake = xTaskGetTickCount();

    led_output_apply_scaled(&command, 255);

    while (1) {
        if (led_output_drain_command(&command)) {
            s_current_command = command;
            system_status_set_led_command(&command);
            active_mode = command.mode;
            brightness = BREATH_MIN_BRIGHTNESS;
            breath_step = 1;
            blink_on = true;
            blink_completed = 0;
            last_wake = xTaskGetTickCount();
            ESP_LOGI(TAG, "Applying new command: rgb=(%u,%u,%u) brightness=%u mode=%d source=%d",
                     command.color_r, command.color_g, command.color_b, command.brightness,
                     command.mode, command.source);
        }

        switch (command.mode) {
        case LED_MODE_OFF:
            active_mode = LED_MODE_OFF;
            led_output_apply_scaled(&command, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        case LED_MODE_SOLID:
            active_mode = LED_MODE_SOLID;
            led_output_apply_scaled(&command, 255);
            vTaskDelay(pdMS_TO_TICKS(50));
            break;
        case LED_MODE_BREATH:
        {
            if (active_mode != LED_MODE_BREATH) {
                active_mode = LED_MODE_BREATH;
                brightness = BREATH_MIN_BRIGHTNESS;
                breath_step = 1;
                last_wake = xTaskGetTickCount();
            }

            led_output_apply_scaled(&command, (uint8_t) brightness);
            brightness += breath_step * BREATH_STEP_SIZE;
            if (brightness >= BREATH_MAX_BRIGHTNESS) {
                brightness = BREATH_MAX_BRIGHTNESS;
                breath_step = -1;
            } else if (brightness <= BREATH_MIN_BRIGHTNESS) {
                brightness = BREATH_MIN_BRIGHTNESS;
                breath_step = 1;
            }
            const uint32_t period_ms = command.period_ms > 0 ? command.period_ms : DEFAULT_BREATH_PERIOD_MS;
            const uint32_t steps_per_cycle =
                ((BREATH_MAX_BRIGHTNESS - BREATH_MIN_BRIGHTNESS) / BREATH_STEP_SIZE) * 2;
            uint32_t step_delay_ms = period_ms / steps_per_cycle;
            if (step_delay_ms < 10) {
                step_delay_ms = 10;
            }
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(step_delay_ms));
            break;
        }
        case LED_MODE_BLINK:
        {
            active_mode = LED_MODE_BLINK;
            led_output_apply_scaled(&command, blink_on ? 255 : 0);
            const uint32_t delay_ms = blink_on ?
                                      (command.on_ms > 0 ? command.on_ms : DEFAULT_BLINK_ON_MS) :
                                      (command.off_ms > 0 ? command.off_ms : DEFAULT_BLINK_OFF_MS);
            if (!blink_on && command.repeat > 0) {
                blink_completed++;
                if (blink_completed >= command.repeat) {
                    led_output_apply_scaled(&command, 0);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
                }
            }
            blink_on = !blink_on;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            break;
        }
        default:
            active_mode = command.mode;
            led_output_apply_scaled(&command, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        }
    }
}

esp_err_t led_output_init(const led_command_t *default_command)
{
    ESP_RETURN_ON_FALSE(default_command != NULL, ESP_ERR_INVALID_ARG, TAG, "default command is null");

    const ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_RESOLUTION,
        .freq_hz = 25000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&ledc_timer), TAG, "timer config failed");

    const ledc_channel_config_t channels[] = {
        {
            .gpio_num = LED_R_GPIO,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = LEDC_MAX_DUTY,
            .hpoint = 0,
        },
        {
            .gpio_num = LED_G_GPIO,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = LEDC_MAX_DUTY,
            .hpoint = 0,
        },
        {
            .gpio_num = LED_B_GPIO,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_2,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = LEDC_MAX_DUTY,
            .hpoint = 0,
        },
    };

    for (size_t i = 0; i < sizeof(channels) / sizeof(channels[0]); ++i) {
        ESP_RETURN_ON_ERROR(ledc_channel_config(&channels[i]), TAG, "channel config failed");
    }

    s_current_command = *default_command;
    system_status_set_led_command(default_command);
    led_output_apply_scaled(default_command, 255);
    BaseType_t task_result = xTaskCreate(led_output_task, "led_output", 4096, NULL, 5, NULL);
    ESP_RETURN_ON_FALSE(task_result == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create LED task");

    ESP_LOGI(TAG, "LED output initialized. R=%d G=%d B=%d", LED_R_GPIO, LED_G_GPIO, LED_B_GPIO);
    return ESP_OK;
}
