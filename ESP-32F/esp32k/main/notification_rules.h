#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ancs_notification_parser.h"
#include "cJSON.h"
#include "esp_err.h"
#include "message_center.h"

#define NOTIFICATION_RULE_MAX_COUNT 16
#define NOTIFICATION_RULE_LABEL_LEN 32
#define NOTIFICATION_RULE_APP_ID_LEN 64

typedef enum {
    NOTIFICATION_RULE_KIND_ANY = 0,
    NOTIFICATION_RULE_KIND_MESSAGE,
    NOTIFICATION_RULE_KIND_INCOMING_CALL,
} notification_rule_kind_t;

typedef struct {
    bool enabled;
    char label[NOTIFICATION_RULE_LABEL_LEN];
    char app_id[NOTIFICATION_RULE_APP_ID_LEN];
    notification_rule_kind_t kind;
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    uint8_t brightness;
    led_mode_t mode;
    uint32_t period_ms;
    uint32_t on_ms;
    uint32_t off_ms;
} notification_rule_t;

esp_err_t notification_rules_init(void);
void notification_rules_reset_defaults(void);
esp_err_t notification_rules_get(notification_rule_t *rules, size_t max_rules, size_t *rule_count);
esp_err_t notification_rules_set(const notification_rule_t *rules, size_t rule_count);
bool notification_rules_match_event(const ancs_notification_event_t *event,
                                    notification_rule_t *matched_rule);
esp_err_t notification_rules_apply_event(const ancs_notification_event_t *event);
bool notification_rules_parse_color(const char *text, uint8_t *red, uint8_t *green, uint8_t *blue);
bool notification_rules_parse_mode(const char *mode_str, led_mode_t *mode);
const char *notification_rules_mode_to_string(led_mode_t mode);
bool notification_rules_parse_kind(const char *kind_str, notification_rule_kind_t *kind);
const char *notification_rules_kind_to_string(notification_rule_kind_t kind);
void notification_rules_add_json(cJSON *root);
