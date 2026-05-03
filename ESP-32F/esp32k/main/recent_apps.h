#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cJSON.h"
#include "esp_err.h"

#define RECENT_APPS_MAX_COUNT 10

typedef struct {
    char app_id[64];
    char display_name[32];
    uint8_t category;
    char event_type[16];
    char summary[128];
    uint32_t time_ms;
} recent_app_entry_t;

esp_err_t recent_apps_init(void);
void recent_apps_start(void);
void recent_apps_stop(void);
bool recent_apps_is_recording(void);
void recent_apps_add(const char *app_id, uint8_t category, const char *event_type, const char *summary);
void recent_apps_clear(void);
void recent_apps_get_json(cJSON *root);
void recent_apps_get_list_json(cJSON *root);
