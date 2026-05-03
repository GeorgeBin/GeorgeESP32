#include "notification_rules.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "system_status.h"

#define RULES_NAMESPACE "notify_rules"
#define RULES_KEY "rules"
#define RULES_MAGIC 0x414E4353u
#define RULES_VERSION 2u
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
static uint32_t s_active_led_notification_uid;
static bool s_has_active_led_notification;

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
                                     uint8_t category,
                                     uint8_t red, uint8_t green, uint8_t blue,
                                     led_mode_t mode)
{
    notification_rule_t rule = {
        .enabled = true,
        .category = category,
        .event_type = 0,
        .color_r = red,
        .color_g = green,
        .color_b = blue,
        .brightness = 100,
        .mode = mode,
        .priority = 50,
        .period_ms = 2000,
        .on_ms = 300,
        .off_ms = 300,
        .repeat = 1,
    };
    copy_string(rule.label, sizeof(rule.label), label);
    copy_string(rule.app_id, sizeof(rule.app_id), app_id);
    rule.keyword[0] = '\0';
    return rule;
}

static void load_defaults(void)
{
    s_rule_count = 0;
    s_rules[s_rule_count++] = make_rule("Wechat Call", "com.tencent.xin",
                                        1, 0, 255, 0, LED_MODE_BLINK);
    s_rules[s_rule_count++] = make_rule("Wechat Message", "com.tencent.xin",
                                        255, 0, 255, 0, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("System Call", "",
                                        1, 255, 0, 0, LED_MODE_BLINK);
    s_rules[s_rule_count++] = make_rule("SMS", "com.apple.MobileSMS",
                                        255, 0, 64, 255, LED_MODE_BLINK);
    s_rules[s_rule_count++] = make_rule("WeCom", "com.tencent.WeWork",
                                        255, 0, 180, 255, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("QQ", "com.tencent.mqq",
                                        255, 0, 128, 255, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("DingTalk", "com.laiwang.DingTalk",
                                        255, 0, 64, 255, LED_MODE_BREATH);
    s_rules[s_rule_count++] = make_rule("Alipay", "com.alipay.iphoneclient",
                                        255, 0, 160, 255, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("Taobao", "com.taobao.taobao4iphone",
                                        255, 255, 120, 0, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("Douyin", "com.ss.iphone.ugc.Aweme",
                                        255, 255, 255, 255, LED_MODE_BLINK);
    s_rules[s_rule_count++] = make_rule("Xiaohongshu", "com.xingin.discover",
                                        255, 255, 0, 80, LED_MODE_SOLID);
    s_rules[s_rule_count++] = make_rule("Bilibili", "tv.danmaku.bilianime",
                                        255, 255, 80, 160, LED_MODE_SOLID);
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

static bool rule_category_matches(uint8_t category, const ancs_notification_event_t *event)
{
    if (category == 255) {
        return true;
    }
    return category == event->category_id;
}

static bool rule_event_type_matches(uint8_t event_type, const ancs_notification_event_t *event)
{
    if (event_type == 0) {
        return true;
    }
    return event_type == (uint8_t)(event->action + 1);
}

static bool rule_keyword_matches(const char *keyword, const ancs_notification_event_t *event)
{
    if (keyword == NULL || keyword[0] == '\0') {
        return true;
    }
    if (event->title[0] != '\0' && strstr(event->title, keyword) != NULL) {
        return true;
    }
    if (event->message[0] != '\0' && strstr(event->message, keyword) != NULL) {
        return true;
    }
    return false;
}

static void generate_rule_id(const char *label, char *id_buf, size_t buf_size)
{
    size_t j = 0;
    for (size_t i = 0; label[i] != '\0' && j < buf_size - 1; ++i) {
        char c = label[i];
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            id_buf[j++] = c;
        } else if (c == ' ' || c == '-') {
            if (j == 0 || id_buf[j - 1] != '-') {
                id_buf[j++] = '-';
            }
        }
    }
    if (j > 0 && id_buf[j - 1] == '-') {
        j--;
    }
    id_buf[j] = '\0';
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
        ESP_LOGI(TAG, "loading default notification rules (version mismatch or invalid data)");
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

    const notification_rule_t *best_rule = NULL;
    int best_priority = -1;

    for (size_t i = 0; i < s_rule_count; ++i) {
        const notification_rule_t *rule = &s_rules[i];
        if (!rule->enabled) {
            continue;
        }
        if (!rule_category_matches(rule->category, event)) {
            continue;
        }
        if (!rule_event_type_matches(rule->event_type, event)) {
            continue;
        }
        if (rule->app_id[0] != '\0' && strcmp(rule->app_id, event->app_id) != 0) {
            continue;
        }
        if (!rule_keyword_matches(rule->keyword, event)) {
            continue;
        }
        if ((int)rule->priority > best_priority) {
            best_priority = (int)rule->priority;
            best_rule = &s_rules[i];
        }
    }

    if (best_rule != NULL) {
        if (matched_rule != NULL) {
            *matched_rule = *best_rule;
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
    esp_err_t ret = message_center_submit(&command);
    if (ret == ESP_OK) {
        s_active_led_notification_uid = event->notification_uid;
        s_has_active_led_notification = true;
        ESP_LOGI(TAG, "active ANCS LED notification uid=%" PRIu32,
                 s_active_led_notification_uid);
    }
    return ret;
}

esp_err_t notification_rules_handle_removed(uint32_t notification_uid)
{
    if (!s_has_active_led_notification || s_active_led_notification_uid != notification_uid) {
        ESP_LOGI(TAG, "removed notification uid=%" PRIu32 " does not control current LED",
                 notification_uid);
        return ESP_ERR_NOT_FOUND;
    }

    system_status_snapshot_t snapshot = {0};
    system_status_get_snapshot(&snapshot);
    s_has_active_led_notification = false;
    s_active_led_notification_uid = 0;

    if (snapshot.last_source != CONTROL_SOURCE_ANCS) {
        ESP_LOGI(TAG, "removed ANCS uid matched, but LED source is %s; not turning off",
                 system_status_control_source_to_string(snapshot.last_source));
        return ESP_ERR_INVALID_STATE;
    }

    device_config_t cfg;
    if (device_config_get(&cfg) == ESP_OK && cfg.clear_behavior == 1) {
        ESP_LOGI(TAG, "removed active ANCS notification uid=%" PRIu32 "; keeping LED on (clear_behavior=keep)",
                 notification_uid);
        return ESP_OK;
    }

    const led_command_t command = {
        .color_r = 0,
        .color_g = 0,
        .color_b = 0,
        .brightness = 0,
        .mode = LED_MODE_OFF,
        .period_ms = 0,
        .on_ms = 0,
        .off_ms = 0,
        .source = CONTROL_SOURCE_ANCS,
    };

    ESP_LOGI(TAG, "removed active ANCS notification uid=%" PRIu32 "; turning LED off",
             notification_uid);
    return message_center_submit(&command);
}

void notification_rules_clear_active(void)
{
    if (s_has_active_led_notification) {
        ESP_LOGI(TAG, "clearing active ANCS notification uid=%" PRIu32,
                 s_active_led_notification_uid);
    }
    s_has_active_led_notification = false;
    s_active_led_notification_uid = 0;
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
    if (strcmp(mode_str, "breath") == 0 || strcmp(mode_str, "pulse") == 0) {
        *mode = LED_MODE_BREATH;
        return true;
    }
    if (strcmp(mode_str, "blink") == 0 || strcmp(mode_str, "rainbow") == 0) {
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

bool notification_rules_parse_category(const char *str, uint8_t *category)
{
    if (str == NULL || category == NULL) {
        return false;
    }
    if (strcmp(str, "any") == 0) {
        *category = 255;
        return true;
    }
    if (strcmp(str, "other") == 0) {
        *category = 0;
        return true;
    }
    if (strcmp(str, "incoming_call") == 0) {
        *category = 1;
        return true;
    }
    if (strcmp(str, "missed_call") == 0) {
        *category = 2;
        return true;
    }
    if (strcmp(str, "voicemail") == 0) {
        *category = 3;
        return true;
    }
    if (strcmp(str, "social") == 0) {
        *category = 4;
        return true;
    }
    if (strcmp(str, "schedule") == 0) {
        *category = 5;
        return true;
    }
    if (strcmp(str, "email") == 0) {
        *category = 6;
        return true;
    }
    if (strcmp(str, "news") == 0) {
        *category = 7;
        return true;
    }
    if (strcmp(str, "health_fitness") == 0) {
        *category = 8;
        return true;
    }
    if (strcmp(str, "business_finance") == 0) {
        *category = 9;
        return true;
    }
    if (strcmp(str, "location") == 0) {
        *category = 10;
        return true;
    }
    if (strcmp(str, "entertainment") == 0) {
        *category = 11;
        return true;
    }
    return false;
}

const char *notification_rules_category_to_string(uint8_t category)
{
    switch (category) {
    case 255:
        return "any";
    case 0:
        return "other";
    case 1:
        return "incoming_call";
    case 2:
        return "missed_call";
    case 3:
        return "voicemail";
    case 4:
        return "social";
    case 5:
        return "schedule";
    case 6:
        return "email";
    case 7:
        return "news";
    case 8:
        return "health_fitness";
    case 9:
        return "business_finance";
    case 10:
        return "location";
    case 11:
        return "entertainment";
    default:
        return "any";
    }
}

bool notification_rules_parse_event_type(const char *str, uint8_t *event_type)
{
    if (str == NULL || event_type == NULL) {
        return false;
    }
    if (strcmp(str, "any") == 0) {
        *event_type = 0;
        return true;
    }
    if (strcmp(str, "added") == 0) {
        *event_type = 1;
        return true;
    }
    if (strcmp(str, "modified") == 0) {
        *event_type = 2;
        return true;
    }
    if (strcmp(str, "removed") == 0) {
        *event_type = 3;
        return true;
    }
    return false;
}

const char *notification_rules_event_type_to_string(uint8_t event_type)
{
    switch (event_type) {
    case 0:
        return "any";
    case 1:
        return "added";
    case 2:
        return "modified";
    case 3:
        return "removed";
    default:
        return "any";
    }
}

void notification_rules_add_json(cJSON *root)
{
    cJSON *rules = cJSON_AddArrayToObject(root, "rules");
    cJSON *presets = cJSON_AddArrayToObject(root, "presets");

    for (size_t i = 0; rules != NULL && i < s_rule_count; ++i) {
        char color[8];
        char id[32];
        cJSON *item = cJSON_CreateObject();
        cJSON *match = cJSON_CreateObject();
        cJSON *led = cJSON_CreateObject();
        if (item == NULL || match == NULL || led == NULL) {
            cJSON_Delete(item);
            cJSON_Delete(match);
            cJSON_Delete(led);
            continue;
        }

        snprintf(color, sizeof(color), "#%02X%02X%02X",
                 s_rules[i].color_r, s_rules[i].color_g, s_rules[i].color_b);
        generate_rule_id(s_rules[i].label, id, sizeof(id));

        cJSON_AddStringToObject(item, "id", id);
        cJSON_AddStringToObject(item, "name", s_rules[i].label);
        cJSON_AddBoolToObject(item, "enabled", s_rules[i].enabled);
        cJSON_AddNumberToObject(item, "priority", s_rules[i].priority);

        cJSON_AddStringToObject(match, "appId", s_rules[i].app_id);
        cJSON_AddStringToObject(match, "category",
                                notification_rules_category_to_string(s_rules[i].category));
        cJSON_AddStringToObject(match, "eventType",
                                notification_rules_event_type_to_string(s_rules[i].event_type));
        cJSON_AddStringToObject(match, "keyword", s_rules[i].keyword);
        cJSON_AddItemToObject(item, "match", match);

        cJSON_AddStringToObject(led, "color", color);
        cJSON_AddStringToObject(led, "mode",
                                notification_rules_mode_to_string(s_rules[i].mode));
        cJSON_AddNumberToObject(led, "brightness", s_rules[i].brightness);
        cJSON_AddNumberToObject(led, "durationMs", s_rules[i].period_ms);
        cJSON_AddNumberToObject(led, "repeat", s_rules[i].repeat);
        cJSON_AddItemToObject(item, "led", led);

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

const char *notification_rules_get_preset_label(const char *app_id)
{
    if (app_id == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(APP_PRESETS) / sizeof(APP_PRESETS[0]); ++i) {
        if (strcmp(APP_PRESETS[i].app_id, app_id) == 0) {
            return APP_PRESETS[i].label;
        }
    }
    return NULL;
}
