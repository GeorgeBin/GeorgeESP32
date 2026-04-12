#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_R_GPIO GPIO_NUM_27
#define LED_G_GPIO GPIO_NUM_33
#define LED_B_GPIO GPIO_NUM_32

static const char *TAG = "RGB_DEMO";

/*
 * 这块板的 RGB 灯是共阳极：
 * - 高电平 = 熄灭
 * - 低电平 = 点亮
 */
static inline void rgb_off(void)
{
    gpio_set_level(LED_R_GPIO, 1);
    gpio_set_level(LED_G_GPIO, 1);
    gpio_set_level(LED_B_GPIO, 1);
}

static inline void rgb_set(bool r_on, bool g_on, bool b_on)
{
    gpio_set_level(LED_R_GPIO, r_on ? 0 : 1);
    gpio_set_level(LED_G_GPIO, g_on ? 0 : 1);
    gpio_set_level(LED_B_GPIO, b_on ? 0 : 1);
}

static void rgb_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_R_GPIO) |
                        (1ULL << LED_G_GPIO) |
                        (1ULL << LED_B_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    rgb_off();

    ESP_LOGI(TAG, "RGB init done. R=%d G=%d B=%d",
             LED_R_GPIO, LED_G_GPIO, LED_B_GPIO);
}

void app_main(void)
{
    rgb_init();

    while (1) {
        ESP_LOGI(TAG, "RED");
        rgb_set(true, false, false);
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGI(TAG, "GREEN");
        rgb_set(false, true, false);
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGI(TAG, "BLUE");
        rgb_set(false, false, true);
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGI(TAG, "YELLOW");
        rgb_set(true, true, false);
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGI(TAG, "CYAN");
        rgb_set(false, true, true);
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGI(TAG, "MAGENTA");
        rgb_set(true, false, true);
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGI(TAG, "WHITE");
        rgb_set(true, true, true);
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGI(TAG, "OFF");
        rgb_off();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}