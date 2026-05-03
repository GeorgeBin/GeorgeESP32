#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ANCS_APP_ID_MAX_LEN 96
#define ANCS_TITLE_MAX_LEN 128
#define ANCS_SUBTITLE_MAX_LEN 128
#define ANCS_MESSAGE_MAX_LEN 256

typedef enum {
    ANCS_EVENT_ADDED = 0,
    ANCS_EVENT_MODIFIED,
    ANCS_EVENT_REMOVED,
    ANCS_EVENT_UNKNOWN,
} ancs_event_action_t;

typedef struct {
    uint8_t event_id;
    uint8_t event_flags;
    uint8_t category_id;
    uint8_t category_count;
    uint32_t notification_uid;
    ancs_event_action_t action;
} ancs_source_event_t;

typedef struct {
    uint32_t notification_uid;
    ancs_event_action_t action;
    char app_id[ANCS_APP_ID_MAX_LEN];
    char title[ANCS_TITLE_MAX_LEN];
    char subtitle[ANCS_SUBTITLE_MAX_LEN];
    char message[ANCS_MESSAGE_MAX_LEN];
    uint8_t category_id;
    uint8_t category_count;
    uint8_t event_flags;
    uint32_t received_time_ms;
} ancs_notification_event_t;

bool ancs_parse_notification_source(const uint8_t *data, size_t len, ancs_source_event_t *event);
bool ancs_parse_notification_attributes(const uint8_t *data, size_t len,
                                        ancs_notification_event_t *event);
void ancs_apply_source_event(ancs_notification_event_t *event, const ancs_source_event_t *source_event);
const char *ancs_event_action_to_string(ancs_event_action_t action);
