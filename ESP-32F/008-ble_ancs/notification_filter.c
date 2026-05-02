#include "notification_filter.h"

#include <string.h>

bool notification_filter_is_wechat(const ancs_notification_event_t *event)
{
    return event != NULL && strcmp(event->app_id, "com.tencent.xin") == 0;
}

led_notify_type_t notification_filter_map_to_led_type(const ancs_notification_event_t *event)
{
    if (event == NULL) {
        return LED_NOTIFY_TYPE_OTHER;
    }

    if (notification_filter_is_wechat(event)) {
        return LED_NOTIFY_TYPE_WECHAT;
    }
    if (strstr(event->app_id, "mobilephone") != NULL ||
        strstr(event->app_id, "MobilePhone") != NULL) {
        return LED_NOTIFY_TYPE_PHONE;
    }
    if (strstr(event->app_id, "MobileSMS") != NULL ||
        strstr(event->app_id, "sms") != NULL) {
        return LED_NOTIFY_TYPE_SMS;
    }
    if (strstr(event->app_id, "mobilemail") != NULL ||
        strstr(event->app_id, "MobileMail") != NULL) {
        return LED_NOTIFY_TYPE_MAIL;
    }

    return LED_NOTIFY_TYPE_OTHER;
}

const char *notification_filter_type_to_string(led_notify_type_t type)
{
    switch (type) {
    case LED_NOTIFY_TYPE_WECHAT:
        return "wechat";
    case LED_NOTIFY_TYPE_PHONE:
        return "phone";
    case LED_NOTIFY_TYPE_SMS:
        return "sms";
    case LED_NOTIFY_TYPE_MAIL:
        return "mail";
    case LED_NOTIFY_TYPE_OTHER:
    default:
        return "other";
    }
}
