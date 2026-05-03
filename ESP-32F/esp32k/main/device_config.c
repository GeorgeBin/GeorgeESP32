#include "device_config.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define CFG_NAMESPACE "device_cfg"
#define CFG_KEY "config"
#define CFG_MAGIC 0x44434647u
#define CFG_VERSION 2u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    device_config_t config;
} device_config_store_t;

static const char *TAG = "device_config";
static device_config_t s_config;

static esp_err_t init_nvs_once(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase failed");
        ret = nvs_flash_init();
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return ret;
}

static void load_defaults(void)
{
    memset(&s_config, 0, sizeof(s_config));
    strncpy(s_config.device_name, "George Light", sizeof(s_config.device_name) - 1);
    s_config.default_brightness = 60;
    s_config.privacy_mode = 0;
    s_config.clear_behavior = 0;

    // boot: #1fb7a6 pulse 50 3000ms
    s_config.phase_boot.color_r = 31;
    s_config.phase_boot.color_g = 183;
    s_config.phase_boot.color_b = 166;
    s_config.phase_boot.mode = LED_MODE_BREATH;
    s_config.phase_boot.brightness = 50;
    s_config.phase_boot.duration_ms = 3000;
    s_config.phase_boot.repeat = 1;

    // disconnected: #f0a92e breath 35 0ms
    s_config.phase_disconnected.color_r = 240;
    s_config.phase_disconnected.color_g = 169;
    s_config.phase_disconnected.color_b = 46;
    s_config.phase_disconnected.mode = LED_MODE_BREATH;
    s_config.phase_disconnected.brightness = 35;
    s_config.phase_disconnected.duration_ms = 0;
    s_config.phase_disconnected.repeat = 0;

    // standby: #16343d breath 8 0ms
    s_config.phase_standby.color_r = 22;
    s_config.phase_standby.color_g = 52;
    s_config.phase_standby.color_b = 61;
    s_config.phase_standby.mode = LED_MODE_BREATH;
    s_config.phase_standby.brightness = 8;
    s_config.phase_standby.duration_ms = 0;
    s_config.phase_standby.repeat = 0;

    // unmatchedCall: #ff3b30 blink 85 15000ms
    s_config.phase_unmatched_call.color_r = 255;
    s_config.phase_unmatched_call.color_g = 59;
    s_config.phase_unmatched_call.color_b = 48;
    s_config.phase_unmatched_call.mode = LED_MODE_BLINK;
    s_config.phase_unmatched_call.brightness = 85;
    s_config.phase_unmatched_call.duration_ms = 15000;
    s_config.phase_unmatched_call.repeat = 8;

    // unmatchedMessage: #1fb7a6 breath 55 8000ms
    s_config.phase_unmatched_message.color_r = 31;
    s_config.phase_unmatched_message.color_g = 183;
    s_config.phase_unmatched_message.color_b = 166;
    s_config.phase_unmatched_message.mode = LED_MODE_BREATH;
    s_config.phase_unmatched_message.brightness = 55;
    s_config.phase_unmatched_message.duration_ms = 8000;
    s_config.phase_unmatched_message.repeat = 1;
}

static esp_err_t save_config(void)
{
    nvs_handle_t handle;
    device_config_store_t store = {
        .magic = CFG_MAGIC,
        .version = CFG_VERSION,
        .reserved = 0,
        .config = s_config,
    };

    esp_err_t ret = nvs_open(CFG_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "open NVS failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_blob(handle, CFG_KEY, &store, sizeof(store));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);
    return ret;
}

esp_err_t device_config_init(void)
{
    nvs_handle_t handle;
    device_config_store_t store;
    size_t length = sizeof(store);

    esp_err_t ret = init_nvs_once();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs init failed: %s", esp_err_to_name(ret));
        load_defaults();
        return ret;
    }

    ret = nvs_open(CFG_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        load_defaults();
        return save_config();
    }

    ret = nvs_get_blob(handle, CFG_KEY, &store, &length);
    nvs_close(handle);
    if (ret != ESP_OK || length != sizeof(store) || store.magic != CFG_MAGIC ||
        store.version != CFG_VERSION) {
        ESP_LOGI(TAG, "loading default device config");
        load_defaults();
        return save_config();
    }

    s_config = store.config;
    ESP_LOGI(TAG, "loaded device config: name=%s brightness=%u privacy=%u clear=%u",
             s_config.device_name, s_config.default_brightness,
             s_config.privacy_mode, s_config.clear_behavior);
    return ESP_OK;
}

esp_err_t device_config_get(device_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *config = s_config;
    return ESP_OK;
}

esp_err_t device_config_set(const device_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config = *config;
    return save_config();
}
