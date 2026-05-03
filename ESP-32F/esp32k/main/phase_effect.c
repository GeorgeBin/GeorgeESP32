#include "phase_effect.h"

#include <string.h>

#include "notification_rules.h"

void phase_effect_add_json(cJSON *root, const char *name, const phase_effect_t *effect)
{
    if (root == NULL || name == NULL || effect == NULL) {
        return;
    }
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return;
    }
    char color[8];
    snprintf(color, sizeof(color), "#%02X%02X%02X",
             effect->color_r, effect->color_g, effect->color_b);
    cJSON_AddStringToObject(obj, "color", color);
    cJSON_AddStringToObject(obj, "mode", notification_rules_mode_to_string(effect->mode));
    cJSON_AddNumberToObject(obj, "brightness", effect->brightness);
    cJSON_AddNumberToObject(obj, "durationMs", effect->duration_ms);
    cJSON_AddNumberToObject(obj, "repeat", effect->repeat);
    cJSON_AddItemToObject(root, name, obj);
}

bool phase_effect_parse_json(const cJSON *obj, phase_effect_t *effect)
{
    if (obj == NULL || effect == NULL || !cJSON_IsObject(obj)) {
        return false;
    }

    const cJSON *color = cJSON_GetObjectItemCaseSensitive(obj, "color");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(obj, "mode");
    const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(obj, "brightness");
    const cJSON *durationMs = cJSON_GetObjectItemCaseSensitive(obj, "durationMs");
    const cJSON *repeat = cJSON_GetObjectItemCaseSensitive(obj, "repeat");

    if (cJSON_IsString(color)) {
        notification_rules_parse_color(color->valuestring,
                                       &effect->color_r, &effect->color_g, &effect->color_b);
    }
    if (cJSON_IsString(mode)) {
        led_mode_t m;
        if (notification_rules_parse_mode(mode->valuestring, &m)) {
            effect->mode = m;
        }
    }
    if (cJSON_IsNumber(brightness)) {
        int b = brightness->valueint;
        effect->brightness = (uint8_t)((b < 0) ? 0 : (b > 100) ? 100 : b);
    }
    if (cJSON_IsNumber(durationMs) && durationMs->valueint >= 0) {
        effect->duration_ms = (uint32_t)durationMs->valueint;
    }
    if (cJSON_IsNumber(repeat)) {
        int r = repeat->valueint;
        effect->repeat = (uint8_t)((r < 0) ? 0 : (r > 99) ? 99 : r);
    }
    return true;
}

void phase_effect_apply_defaults(phase_effect_t *effect)
{
    if (effect == NULL) {
        return;
    }
    memset(effect, 0, sizeof(*effect));
    effect->brightness = 50;
    effect->mode = LED_MODE_BREATH;
    effect->duration_ms = 3000;
    effect->repeat = 1;
}
