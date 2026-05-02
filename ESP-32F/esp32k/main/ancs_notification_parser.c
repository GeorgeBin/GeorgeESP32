#include "ancs_notification_parser.h"

#include <string.h>

#include "esp_timer.h"

#define ANCS_COMMAND_GET_NOTIFICATION_ATTRIBUTES 0
#define ANCS_ATTRIBUTE_APP_IDENTIFIER 0
#define ANCS_ATTRIBUTE_TITLE 1
#define ANCS_ATTRIBUTE_SUBTITLE 2
#define ANCS_ATTRIBUTE_MESSAGE 3

static void copy_attribute(char *dest, size_t dest_size, const uint8_t *src, size_t src_len)
{
    size_t copy_len;

    if (dest_size == 0) {
        return;
    }

    copy_len = src_len < (dest_size - 1) ? src_len : (dest_size - 1);
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
}

static ancs_event_action_t action_from_event_id(uint8_t event_id)
{
    switch (event_id) {
    case 0:
        return ANCS_EVENT_ADDED;
    case 1:
        return ANCS_EVENT_MODIFIED;
    case 2:
        return ANCS_EVENT_REMOVED;
    default:
        return ANCS_EVENT_UNKNOWN;
    }
}

bool ancs_parse_notification_source(const uint8_t *data, size_t len, ancs_source_event_t *event)
{
    if (data == NULL || event == NULL || len < 8) {
        return false;
    }

    memset(event, 0, sizeof(*event));
    event->event_id = data[0];
    event->event_flags = data[1];
    event->category_id = data[2];
    event->category_count = data[3];
    event->notification_uid = ((uint32_t)data[4]) |
                              ((uint32_t)data[5] << 8) |
                              ((uint32_t)data[6] << 16) |
                              ((uint32_t)data[7] << 24);
    event->action = action_from_event_id(event->event_id);
    return true;
}

bool ancs_parse_notification_attributes(const uint8_t *data, size_t len,
                                        ancs_notification_event_t *event)
{
    size_t offset = 5;

    if (data == NULL || event == NULL || len < 5 ||
        data[0] != ANCS_COMMAND_GET_NOTIFICATION_ATTRIBUTES) {
        return false;
    }

    memset(event, 0, sizeof(*event));
    event->notification_uid = ((uint32_t)data[1]) |
                              ((uint32_t)data[2] << 8) |
                              ((uint32_t)data[3] << 16) |
                              ((uint32_t)data[4] << 24);
    event->action = ANCS_EVENT_ADDED;
    event->received_time_ms = (uint32_t)(esp_timer_get_time() / 1000);

    while ((offset + 3) <= len) {
        const uint8_t attribute_id = data[offset];
        const uint16_t attribute_len = (uint16_t)data[offset + 1] |
                                       ((uint16_t)data[offset + 2] << 8);
        const uint8_t *attribute_data = &data[offset + 3];

        offset += 3;
        if ((offset + attribute_len) > len) {
            return false;
        }

        switch (attribute_id) {
        case ANCS_ATTRIBUTE_APP_IDENTIFIER:
            copy_attribute(event->app_id, sizeof(event->app_id), attribute_data, attribute_len);
            break;
        case ANCS_ATTRIBUTE_TITLE:
            copy_attribute(event->title, sizeof(event->title), attribute_data, attribute_len);
            break;
        case ANCS_ATTRIBUTE_SUBTITLE:
            copy_attribute(event->subtitle, sizeof(event->subtitle), attribute_data, attribute_len);
            break;
        case ANCS_ATTRIBUTE_MESSAGE:
            copy_attribute(event->message, sizeof(event->message), attribute_data, attribute_len);
            break;
        default:
            break;
        }

        offset += attribute_len;
    }

    return true;
}

const char *ancs_event_action_to_string(ancs_event_action_t action)
{
    switch (action) {
    case ANCS_EVENT_ADDED:
        return "added";
    case ANCS_EVENT_MODIFIED:
        return "modified";
    case ANCS_EVENT_REMOVED:
        return "removed";
    case ANCS_EVENT_UNKNOWN:
    default:
        return "unknown";
    }
}
