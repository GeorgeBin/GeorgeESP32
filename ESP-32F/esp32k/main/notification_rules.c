#include "notification_rules.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "system_status.h"

#define RULES_NAMESPACE "notify_rules"
#define RULES_KEY "rules"
#define RULES_MAGIC 0x414E4353u
#define RULES_VERSION 1u
#define ANCS_CATEGORY_INCOMING_CALL 1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    notification_rule_t rules[NOTIFICATION_RULE_MAX_COUNT];
} notification_rules_store_t;

typedef struct {
    const char *label;
    const char *app_id;
} app_preset_t;

static const char *TAG = "notification_rules";
static notification_rule_t s_rules[NOTIFICATION_RULE_MAX_COUNT];
static size_t s_rule_count;

static const app_preset_t APP_PRESETS[] = {
    {"Wechat", "com.tencent.xin"},
    {"WeCom", "com.tencent.WeWork"},
    {"QQ", "com.tencent.mqq"},
    {"DingTalk", "com.laiwang.DingTalk"},
    {"Alipay", "com.alipay.iphoneclient"},
    {"Taobao", "com.taobao.taobao4iphone"},
    {"Douyin", "com.ss.iphone.ugc.Aweme"},
    {"Xiaohongshu", "com.xingin.discover"},
    {"Bilibili", "tv.danmaku.bilianime"},
    {"Phone", "com.apple.mobilephone"},
    {"SMS", "com.apple.MobileSMS"},
};

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

static void copy_string(char *dest, size_t dest_size, const char *src)
{
    if (dest_size == 0) {
        return;
    }
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

static notification_rule_t make_rule(const char *label, const char *app_id,
                                     notification_rule_kind_t kind,
                                     uint8_t red, uint8_t green, uint8_t blue,
                                     led_mode_t mode)
{
    notification_rule_t rule = {
        .enabled = true,
        .kind = kind,
        .color_r = red,
        .color_g = green,
        .color_b = blue,
        .brightness = 100,
        .mode = mode,
        .period_ms = 2000,
        .on_ms = 300,
        .off_ms = 300,
    };
    copy_string(rule.label, sizeof(rule.label), label);
    copy_string(rule.app_id, sizeof(rule.app_id), app_id);
    return rule;
}

static void load_defaults(void)
{
    s_rule_count = 0;
    s_rules[s_rule_count++] = make_rule("Wechat Call", "com.tencent.xin",
                                        NOTIFICATION_RULE_KIND_INCOMING_CALL,
                                        0, 255, 0, LED_MODE_BLINK);
    s_rules[s_rule_count++] = make_rule("Wechat Message", "com.tencent.xin",
                                        NOTIFICATION_RULE_KIND_MESSAGE,
                                        0, 255, 0, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("System Call", "",
                                        NOTIFICATION_RULE_KIND_INCOMING_CALL,
                                        255, 0, 0, LED_MODE_BLINK);
    s_rules[s_rule_count++] = make_rule("SMS", "com.apple.MobileSMS",
                                        NOTIFICATION_RULE_KIND_ANY,
                                        0, 64, 255, LED_MODE_BLINK);
    s_rules[s_rule_count++] = make_rule("WeCom", "com.tencent.WeWork",
                                        NOTIFICATION_RULE_KIND_ANY,
                                        0, 180, 255, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("QQ", "com.tencent.mqq",
                                        NOTIFICATION_RULE_KIND_ANY,
                                        0, 128, 255, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("DingTalk", "com.laiwang.DingTalk",
                                        NOTIFICATION_RULE_KIND_ANY,
                                        0, 64, 255, LED_MODE_BREATH);
    s_rules[s_rule_count++] = make_rule("Alipay", "com.alipay.iphoneclient",
                                        NOTIFICATION_RULE_KIND_ANY,
                                        0, 160, 255, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("Taobao", "com.taobao.taobao4iphone",
                                        NOTIFICATION_RULE_KIND_ANY,
                                        255, 120, 0, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("Douyin", "com.ss.iphone.ugc.Aweme",
                                        NOTIFICATION_RULE_KIND_ANY,
                                        255, 255, 255, LED_MODE_BLINK);
    s_rules[s_rule_count++] = make_rule("Xiaohongshu", "com.xingin.discover",
                                        NOTIFICATION_RULE_KIND_ANY,
                                        255, 0, 80, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("Bilibili", "tv.danmaku.bilianime",
                                        NOTIFICATION_RULE_KIND_ANY,
                                        255, 80, 160, LED_MODE_SOLID);
}

static esp_err_t save_rules(void)
{
    nvs_handle_t handle;
    notification_rules_store_t *store = calloc(1, sizeof(*store));
    if (store == NULL) {
        return ESP_ERR_NO_MEM;
    }

    store->magic = RULES_MAGIC;
    store->version = RULES_VERSION;
    store->count = (uint16_t)s_rule_count;
    memcpy(store->rules, s_rules, sizeof(s_rules));

    esp_err_t ret = nvs_open(RULES_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        free(store);
        ESP_LOGE(TAG, "open NVS failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_blob(handle, RULES_KEY, store, sizeof(*store));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);
    free(store);
    return ret;
}

static bool rule_kind_matches(notification_rule_kind_t kind, const ancs_notification_event_t *event)
{
    const bool incoming_call = event->category_id == ANCS_CATEGORY_INCOMING_CALL;

    switch (kind) {
    case NOTIFICATION_RULE_KIND_ANY:
        return true;
    case NOTIFICATION_RULE_KIND_INCOMING_CALL:
        return incoming_call;
    case NOTIFICATION_RULE_KIND_MESSAGE:
        return !incoming_call;
    default:
        return false;
    }
}

esp_err_t notification_rules_init(void)
{
    nvs_handle_t handle;
    notification_rules_store_t *store = calloc(1, sizeof(*store));
    if (store == NULL) {
        return ESP_ERR_NO_MEM;
    }
    size_t length = sizeof(*store);

    esp_err_t ret = init_nvs_once();
    if (ret != ESP_OK) {
        free(store);
        ESP_LOGE(TAG, "nvs init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_open(RULES_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        load_defaults();
        free(store);
        return save_rules();
    }

    ret = nvs_get_blob(handle, RULES_KEY, store, &length);
    nvs_close(handle);
    if (ret != ESP_OK || length != sizeof(*store) || store->magic != RULES_MAGIC ||
        store->version != RULES_VERSION || store->count > NOTIFICATION_RULE_MAX_COUNT) {
        ESP_LOGI(TAG, "loading default notification rules");
        load_defaults();
        free(store);
        return save_rules();
    }

    s_rule_count = store->count;
    memcpy(s_rules, store->rules, sizeof(s_rules));
    free(store);
    ESP_LOGI(TAG, "loaded %u notification rules", (unsigned)s_rule_count);
    return ESP_OK;
}

void notification_rules_reset_defaults(void)
{
    load_defaults();
    ESP_ERROR_CHECK(save_rules());
}

esp_err_t notification_rules_get(notification_rule_t *rules, size_t max_rules, size_t *rule_count)
{
    if (rule_count != NULL) {
        *rule_count = s_rule_count;
    }
    if (rules != NULL && max_rules > 0) {
        size_t count = s_rule_count < max_rules ? s_rule_count : max_rules;
        memcpy(rules, s_rules, count * sizeof(rules[0]));
    }
    return ESP_OK;
}

esp_err_t notification_rules_set(const notification_rule_t *rules, size_t rule_count)
{
    if (rules == NULL || rule_count > NOTIFICATION_RULE_MAX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_rules, 0, sizeof(s_rules));
    memcpy(s_rules, rules, rule_count * sizeof(rules[0]));
    s_rule_count = rule_count;
    return save_rules();
}

bool notification_rules_match_event(const ancs_notification_event_t *event,
                                    notification_rule_t *matched_rule)
{
    if (event == NULL) {
        return false;
    }

    for (size_t i = 0; i < s_rule_count; ++i) {
        const notification_rule_t *rule = &s_rules[i];
        if (!rule->enabled || !rule_kind_matches(rule->kind, event)) {
            continue;
        }
        if (rule->app_id[0] != '\0' && strcmp(rule->app_id, event->app_id) != 0) {
            continue;
        }
        if (matched_rule != NULL) {
            *matched_rule = *rule;
        }
        return true;
    }

    return false;
}

esp_err_t notification_rules_apply_event(const ancs_notification_event_t *event)
{
    notification_rule_t rule = {0};

    if (!notification_rules_match_event(event, &rule)) {
        system_status_set_ancs_rule_result(false, "");
        ESP_LOGI(TAG, "no notification rule matched app_id=%s category=%u",
                 event != NULL ? event->app_id : "", event != NULL ? event->category_id : 0);
        return ESP_ERR_NOT_FOUND;
    }

    const led_command_t command = {
        .color_r = rule.color_r,
        .color_g = rule.color_g,
        .color_b = rule.color_b,
        .brightness = rule.brightness,
        .mode = rule.mode,
        .period_ms = rule.period_ms,
        .on_ms = rule.on_ms,
        .off_ms = rule.off_ms,
        .source = CONTROL_SOURCE_ANCS,
    };

    system_status_set_ancs_rule_result(true, rule.label);
    ESP_LOGI(TAG, "matched rule %s for app_id=%s", rule.label, event->app_id);
    return message_center_submit(&command);
}

bool notification_rules_parse_color(const char *text, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    const char *hex = text;
    char *endptr = NULL;

    if (text == NULL || red == NULL || green == NULL || blue == NULL) {
        return false;
    }
    if (hex[0] == '#') {
        hex++;
    }
    if (strlen(hex) != 6) {
        return false;
    }
    for (size_t i = 0; i < 6; ++i) {
        if (!isxdigit((unsigned char)hex[i])) {
            return false;
        }
    }

    unsigned long value = strtoul(hex, &endptr, 16);
    if (endptr == NULL || *endptr != '\0') {
        return false;
    }

    *red = (uint8_t)((value >> 16) & 0xFF);
    *green = (uint8_t)((value >> 8) & 0xFF);
    *blue = (uint8_t)(value & 0xFF);
    return true;
}

bool notification_rules_parse_mode(const char *mode_str, led_mode_t *mode)
{
    if (mode_str == NULL || mode == NULL) {
        return false;
    }
    if (strcmp(mode_str, "off") == 0) {
        *mode = LED_MODE_OFF;
        return true;
    }
    if (strcmp(mode_str, "solid") == 0) {
        *mode = LED_MODE_SOLID;
        return true;
    }
    if (strcmp(mode_str, "breath") == 0) {
        *mode = LED_MODE_BREATH;
        return true;
    }
    if (strcmp(mode_str, "blink") == 0) {
        *mode = LED_MODE_BLINK;
        return true;
    }
    return false;
}

const char *notification_rules_mode_to_string(led_mode_t mode)
{
    switch (mode) {
    case LED_MODE_OFF:
        return "off";
    case LED_MODE_SOLID:
        return "solid";
    case LED_MODE_BREATH:
        return "breath";
    case LED_MODE_BLINK:
        return "blink";
    default:
        return "unknown";
    }
}

bool notification_rules_parse_kind(const char *kind_str, notification_rule_kind_t *kind)
{
    if (kind_str == NULL || kind == NULL) {
        return false;
    }
    if (strcmp(kind_str, "any") == 0) {
        *kind = NOTIFICATION_RULE_KIND_ANY;
        return true;
    }
    if (strcmp(kind_str, "message") == 0) {
        *kind = NOTIFICATION_RULE_KIND_MESSAGE;
        return true;
    }
    if (strcmp(kind_str, "incoming_call") == 0) {
        *kind = NOTIFICATION_RULE_KIND_INCOMING_CALL;
        return true;
    }
    return false;
}

const char *notification_rules_kind_to_string(notification_rule_kind_t kind)
{
    switch (kind) {
    case NOTIFICATION_RULE_KIND_ANY:
        return "any";
    case NOTIFICATION_RULE_KIND_MESSAGE:
        return "message";
    case NOTIFICATION_RULE_KIND_INCOMING_CALL:
        return "incoming_call";
    default:
        return "unknown";
    }
}

void notification_rules_add_json(cJSON *root)
{
    cJSON *rules = cJSON_AddArrayToObject(root, "rules");
    cJSON *presets = cJSON_AddArrayToObject(root, "presets");

    for (size_t i = 0; rules != NULL && i < s_rule_count; ++i) {
        char color[8];
        cJSON *item = cJSON_CreateObject();
        if (item == NULL) {
            continue;
        }
        snprintf(color, sizeof(color), "#%02X%02X%02X",
                 s_rules[i].color_r, s_rules[i].color_g, s_rules[i].color_b);
        cJSON_AddBoolToObject(item, "enabled", s_rules[i].enabled);
        cJSON_AddStringToObject(item, "label", s_rules[i].label);
        cJSON_AddStringToObject(item, "app_id", s_rules[i].app_id);
        cJSON_AddStringToObject(item, "kind", notification_rules_kind_to_string(s_rules[i].kind));
        cJSON_AddStringToObject(item, "color", color);
        cJSON_AddStringToObject(item, "mode", notification_rules_mode_to_string(s_rules[i].mode));
        cJSON_AddNumberToObject(item, "brightness", s_rules[i].brightness);
        cJSON_AddNumberToObject(item, "period_ms", s_rules[i].period_ms);
        cJSON_AddNumberToObject(item, "on_ms", s_rules[i].on_ms);
        cJSON_AddNumberToObject(item, "off_ms", s_rules[i].off_ms);
        cJSON_AddItemToArray(rules, item);
    }

    for (size_t i = 0; presets != NULL && i < sizeof(APP_PRESETS) / sizeof(APP_PRESETS[0]); ++i) {
        cJSON *item = cJSON_CreateObject();
        if (item == NULL) {
            continue;
        }
        cJSON_AddStringToObject(item, "label", APP_PRESETS[i].label);
        cJSON_AddStringToObject(item, "app_id", APP_PRESETS[i].app_id);
        cJSON_AddItemToArray(presets, item);
    }
}
