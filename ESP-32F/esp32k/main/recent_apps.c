#include "recent_apps.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "notification_rules.h"

static const char *TAG = "recent_apps";

static recent_app_entry_t s_entries[RECENT_APPS_MAX_COUNT];
static size_t s_count = 0;
static bool s_recording_enabled = false;

static const char *category_to_display_name(uint8_t category)
{
    switch (category) {
    case 0:
        return "Other";
    case 1:
        return "Incoming Call";
    case 2:
        return "Missed Call";
    case 3:
        return "Voicemail";
    case 4:
        return "Social";
    case 5:
        return "Schedule";
    case 6:
        return "Email";
    case 7:
        return "News";
    case 8:
        return "Health & Fitness";
    case 9:
        return "Business & Finance";
    case 10:
        return "Location";
    case 11:
        return "Entertainment";
    default:
        return "Unknown";
    }
}

static void extract_display_name(const char *app_id, char *out, size_t out_size)
{
    if (app_id == NULL || out_size == 0) {
        return;
    }

    const char *preset = notification_rules_get_preset_label(app_id);
    if (preset != NULL) {
        strncpy(out, preset, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    const char *last_dot = strrchr(app_id, '.');
    if (last_dot != NULL && last_dot[1] != '\0') {
        strncpy(out, last_dot + 1, out_size - 1);
    } else {
        strncpy(out, app_id, out_size - 1);
    }
    out[out_size - 1] = '\0';
}

esp_err_t recent_apps_init(void)
{
    memset(s_entries, 0, sizeof(s_entries));
    s_count = 0;
    s_recording_enabled = false;
    return ESP_OK;
}

void recent_apps_start(void)
{
    s_recording_enabled = true;
    ESP_LOGI(TAG, "recording started");
}

void recent_apps_stop(void)
{
    s_recording_enabled = false;
    ESP_LOGI(TAG, "recording stopped");
}

bool recent_apps_is_recording(void)
{
    return s_recording_enabled;
}

void recent_apps_add(const char *app_id, uint8_t category, const char *event_type,
                     const char *summary)
{
    if (!s_recording_enabled) {
        return;
    }
    if (app_id == NULL || app_id[0] == '\0') {
        return;
    }

    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    for (size_t i = 0; i < s_count; ++i) {
        if (strcmp(s_entries[i].app_id, app_id) == 0) {
            s_entries[i].category = category;
            if (event_type != NULL) {
                strncpy(s_entries[i].event_type, event_type,
                        sizeof(s_entries[i].event_type) - 1);
                s_entries[i].event_type[sizeof(s_entries[i].event_type) - 1] = '\0';
            }
            if (summary != NULL) {
                strncpy(s_entries[i].summary, summary,
                        sizeof(s_entries[i].summary) - 1);
                s_entries[i].summary[sizeof(s_entries[i].summary) - 1] = '\0';
            }
            s_entries[i].time_ms = now_ms;

            recent_app_entry_t temp = s_entries[i];
            for (size_t j = i; j > 0; --j) {
                s_entries[j] = s_entries[j - 1];
            }
            s_entries[0] = temp;
            return;
        }
    }

    if (s_count < RECENT_APPS_MAX_COUNT) {
        s_count++;
    }

    for (size_t i = s_count - 1; i > 0; --i) {
        s_entries[i] = s_entries[i - 1];
    }

    memset(&s_entries[0], 0, sizeof(s_entries[0]));
    strncpy(s_entries[0].app_id, app_id, sizeof(s_entries[0].app_id) - 1);
    s_entries[0].app_id[sizeof(s_entries[0].app_id) - 1] = '\0';
    s_entries[0].category = category;
    extract_display_name(app_id, s_entries[0].display_name,
                         sizeof(s_entries[0].display_name));
    if (event_type != NULL) {
        strncpy(s_entries[0].event_type, event_type,
                sizeof(s_entries[0].event_type) - 1);
        s_entries[0].event_type[sizeof(s_entries[0].event_type) - 1] = '\0';
    }
    if (summary != NULL) {
        strncpy(s_entries[0].summary, summary, sizeof(s_entries[0].summary) - 1);
        s_entries[0].summary[sizeof(s_entries[0].summary) - 1] = '\0';
    }
    s_entries[0].time_ms = now_ms;

    ESP_LOGI(TAG, "added recent app: %s (%s)", app_id, s_entries[0].display_name);
}

void recent_apps_clear(void)
{
    memset(s_entries, 0, sizeof(s_entries));
    s_count = 0;
    ESP_LOGI(TAG, "cleared all entries");
}

void recent_apps_get_json(cJSON *root)
{
    cJSON *array = cJSON_AddArrayToObject(root, "recentApps");
    if (array == NULL) {
        return;
    }

    for (size_t i = 0; i < s_count; ++i) {
        cJSON *item = cJSON_CreateObject();
        if (item == NULL) {
            continue;
        }
        cJSON_AddStringToObject(item, "appId", s_entries[i].app_id);
        cJSON_AddStringToObject(item, "displayName", s_entries[i].display_name);
        cJSON_AddStringToObject(item, "category",
                                notification_rules_category_to_string(s_entries[i].category));
        cJSON_AddStringToObject(item, "eventType", s_entries[i].event_type);
        cJSON_AddStringToObject(item, "summary", s_entries[i].summary);
        cJSON_AddNumberToObject(item, "timeMs", s_entries[i].time_ms);
        cJSON_AddItemToArray(array, item);
    }
}

void recent_apps_get_list_json(cJSON *root)
{
    cJSON *array = cJSON_AddArrayToObject(root, "items");
    if (array == NULL) {
        return;
    }

    for (size_t i = 0; i < s_count; ++i) {
        cJSON *item = cJSON_CreateObject();
        if (item == NULL) {
            continue;
        }
        cJSON_AddStringToObject(item, "appId", s_entries[i].app_id);
        cJSON_AddStringToObject(item, "displayName", s_entries[i].display_name);
        cJSON_AddStringToObject(item, "category",
                                notification_rules_category_to_string(s_entries[i].category));
        cJSON_AddStringToObject(item, "eventType", s_entries[i].event_type);
        cJSON_AddStringToObject(item, "summary", s_entries[i].summary);
        cJSON_AddNumberToObject(item, "timeMs", s_entries[i].time_ms);
        cJSON_AddItemToArray(array, item);
    }
}
