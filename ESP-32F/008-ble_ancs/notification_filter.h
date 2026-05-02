#pragma once

#include <stdbool.h>

#include "ancs_notification_parser.h"

typedef enum {
    LED_NOTIFY_TYPE_WECHAT = 0,
    LED_NOTIFY_TYPE_PHONE,
    LED_NOTIFY_TYPE_SMS,
    LED_NOTIFY_TYPE_MAIL,
    LED_NOTIFY_TYPE_OTHER,
} led_notify_type_t;

bool notification_filter_is_wechat(const ancs_notification_event_t *event);
led_notify_type_t notification_filter_map_to_led_type(const ancs_notification_event_t *event);
const char *notification_filter_type_to_string(led_notify_type_t type);
