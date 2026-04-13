#include "message_center.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/queue.h"

static const char *TAG = "message_center";
static QueueHandle_t s_led_queue;

esp_err_t message_center_init(size_t queue_length)
{
    if (s_led_queue != NULL) {
        return ESP_OK;
    }

    s_led_queue = xQueueCreate(queue_length, sizeof(led_command_t));
    if (s_led_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create message queue");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t message_center_submit(const led_command_t *command)
{
    if (s_led_queue == NULL || command == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueOverwrite(s_led_queue, command) != pdPASS) {
        ESP_LOGE(TAG, "Failed to submit LED command");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Command queued: rgb=(%u,%u,%u) mode=%d",
             command->color_r, command->color_g, command->color_b, command->mode);
    return ESP_OK;
}

bool message_center_receive(led_command_t *command, TickType_t timeout_ticks)
{
    if (s_led_queue == NULL || command == NULL) {
        return false;
    }

    return xQueueReceive(s_led_queue, command, timeout_ticks) == pdPASS;
}
