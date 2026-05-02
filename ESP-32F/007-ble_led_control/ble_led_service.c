#include "ble_led_service.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "message_center.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "system_status.h"

#define BLE_LED_MAX_WRITE_LEN 256
#define BLE_LED_STATE_JSON_LEN 384

#define BLE_LED_ERR_JSON 1001
#define BLE_LED_ERR_MISSING_CMD 1002
#define BLE_LED_ERR_UNSUPPORTED_CMD 1003
#define BLE_LED_ERR_UNSUPPORTED_MODE 1004
#define BLE_LED_ERR_COLOR 1005
#define BLE_LED_ERR_BRIGHTNESS 1006
#define BLE_LED_ERR_PARAMS 1007
#define BLE_LED_ERR_CONTROL 3001
#define BLE_LED_ERR_INTERNAL 9001

static const char *TAG = "ble_led_service";

static uint16_t s_state_value_handle;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_connected;
static bool s_notify_enabled;

static int ble_led_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);

static const ble_uuid128_t BLE_LED_SERVICE_UUID =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x01, 0xa0, 0x00, 0x00);
static const ble_uuid128_t BLE_LED_COMMAND_UUID =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x02, 0xa0, 0x00, 0x00);
static const ble_uuid128_t BLE_LED_STATE_UUID =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x03, 0xa0, 0x00, 0x00);
static const ble_uuid128_t BLE_LED_INFO_UUID =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x04, 0xa0, 0x00, 0x00);

static const struct ble_gatt_svc_def BLE_LED_SERVICES[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &BLE_LED_SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &BLE_LED_COMMAND_UUID.u,
                .access_cb = ble_led_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &BLE_LED_STATE_UUID.u,
                .access_cb = ble_led_chr_access,
                .val_handle = &s_state_value_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid = &BLE_LED_INFO_UUID.u,
                .access_cb = ble_led_chr_access,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {0},
        },
    },
    {0},
};

static bool parse_led_mode(const char *mode_str, led_mode_t *mode)
{
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

static bool parse_hex_color(const char *text, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    const char *hex = text;
    char *endptr = NULL;

    if (hex == NULL) {
        return false;
    }
    if (hex[0] == '#') {
        hex++;
    }
    if (strlen(hex) != 6) {
        return false;
    }
    for (size_t i = 0; i < 6; ++i) {
        if (!isxdigit((unsigned char) hex[i])) {
            return false;
        }
    }

    const unsigned long value = strtoul(hex, &endptr, 16);
    if (endptr == NULL || *endptr != '\0') {
        return false;
    }

    *red = (uint8_t) ((value >> 16) & 0xFF);
    *green = (uint8_t) ((value >> 8) & 0xFF);
    *blue = (uint8_t) (value & 0xFF);
    return true;
}

static void command_defaults(led_command_t *command)
{
    command->brightness = 100;
    command->period_ms = 2000;
    command->on_ms = 500;
    command->off_ms = 500;
    command->source = CONTROL_SOURCE_BLE;
}

static void format_state_json(char *out, size_t out_size, uint32_t seq, int code, const char *msg)
{
    system_status_snapshot_t snapshot = {0};

    system_status_get_snapshot(&snapshot);
    snprintf(out, out_size,
             "{\"seq\":%" PRIu32 ",\"code\":%d,\"msg\":\"%s\","
             "\"state\":{\"color\":\"#%02X%02X%02X\",\"mode\":\"%s\",\"brightness\":%u,"
             "\"period_ms\":%" PRIu32 ",\"on_ms\":%" PRIu32 ",\"off_ms\":%" PRIu32 ","
             "\"source\":\"%s\",\"ble_connected\":%s,\"wifi_connected\":%s}}",
             seq, code, msg,
             snapshot.led.color_r, snapshot.led.color_g, snapshot.led.color_b,
             system_status_led_mode_to_string(snapshot.led.mode),
             snapshot.led.brightness,
             snapshot.led.period_ms,
             snapshot.led.on_ms,
             snapshot.led.off_ms,
             system_status_control_source_to_string(snapshot.last_source),
             snapshot.ble_connected ? "true" : "false",
             snapshot.wifi_connected ? "true" : "false");
}

static int append_text(struct os_mbuf *om, const char *text)
{
    const int rc = os_mbuf_append(om, text, strlen(text));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int parse_set_led(cJSON *root, led_command_t *command, int *code, const char **msg)
{
    command_defaults(command);

    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(root, "mode");
    if (!cJSON_IsString(mode)) {
        *code = BLE_LED_ERR_PARAMS;
        *msg = "missing mode";
        return -1;
    }
    if (!parse_led_mode(mode->valuestring, &command->mode)) {
        *code = BLE_LED_ERR_UNSUPPORTED_MODE;
        *msg = "unsupported mode";
        return -1;
    }

    if (command->mode != LED_MODE_OFF) {
        const cJSON *color = cJSON_GetObjectItemCaseSensitive(root, "color");
        if (!cJSON_IsString(color) ||
            !parse_hex_color(color->valuestring, &command->color_r, &command->color_g, &command->color_b)) {
            *code = BLE_LED_ERR_COLOR;
            *msg = "invalid color";
            return -1;
        }
    }

    const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(root, "brightness");
    if (brightness != NULL) {
        if (!cJSON_IsNumber(brightness) || brightness->valueint < 0 || brightness->valueint > 100) {
            *code = BLE_LED_ERR_BRIGHTNESS;
            *msg = "invalid brightness";
            return -1;
        }
        command->brightness = (uint8_t) brightness->valueint;
    }

    const cJSON *period_ms = cJSON_GetObjectItemCaseSensitive(root, "period_ms");
    if (period_ms != NULL) {
        if (!cJSON_IsNumber(period_ms) || period_ms->valueint <= 0) {
            *code = BLE_LED_ERR_PARAMS;
            *msg = "invalid period";
            return -1;
        }
        command->period_ms = (uint32_t) period_ms->valueint;
    }

    const cJSON *on_ms = cJSON_GetObjectItemCaseSensitive(root, "on_ms");
    if (on_ms != NULL) {
        if (!cJSON_IsNumber(on_ms) || on_ms->valueint <= 0) {
            *code = BLE_LED_ERR_PARAMS;
            *msg = "invalid on_ms";
            return -1;
        }
        command->on_ms = (uint32_t) on_ms->valueint;
    }

    const cJSON *off_ms = cJSON_GetObjectItemCaseSensitive(root, "off_ms");
    if (off_ms != NULL) {
        if (!cJSON_IsNumber(off_ms) || off_ms->valueint <= 0) {
            *code = BLE_LED_ERR_PARAMS;
            *msg = "invalid off_ms";
            return -1;
        }
        command->off_ms = (uint32_t) off_ms->valueint;
    }

    *code = 0;
    *msg = "ok";
    return 0;
}

static void handle_command_json(const char *json)
{
    uint32_t seq = 0;
    int code = 0;
    const char *msg = "ok";
    cJSON *root = cJSON_Parse(json);

    if (root == NULL) {
        code = BLE_LED_ERR_JSON;
        msg = "invalid json";
        goto done;
    }

    const cJSON *seq_item = cJSON_GetObjectItemCaseSensitive(root, "seq");
    if (cJSON_IsNumber(seq_item) && seq_item->valueint >= 0) {
        seq = (uint32_t) seq_item->valueint;
    }

    const cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    if (!cJSON_IsString(cmd)) {
        code = BLE_LED_ERR_MISSING_CMD;
        msg = "missing cmd";
        goto done;
    }

    if (strcmp(cmd->valuestring, "set_led") == 0) {
        led_command_t command = {0};
        if (parse_set_led(root, &command, &code, &msg) == 0) {
            if (message_center_submit(&command) != ESP_OK) {
                code = BLE_LED_ERR_CONTROL;
                msg = "led control failed";
            } else {
                system_status_set_led_command(&command);
            }
        }
    } else if (strcmp(cmd->valuestring, "get_state") == 0) {
        code = 0;
        msg = "ok";
    } else {
        code = BLE_LED_ERR_UNSUPPORTED_CMD;
        msg = "unsupported cmd";
    }

done:
    system_status_set_last_result(code, msg);
    ble_led_service_notify_state(seq, code, msg);
    if (root != NULL) {
        cJSON_Delete(root);
    }
}

static int access_command(struct ble_gatt_access_ctxt *ctxt)
{
    char body[BLE_LED_MAX_WRITE_LEN];
    uint16_t copied_len = 0;

    const int rc = ble_hs_mbuf_to_flat(ctxt->om, body, sizeof(body) - 1, &copied_len);
    if (rc != 0) {
        system_status_set_last_result(BLE_LED_ERR_INTERNAL, "read failed");
        ble_led_service_notify_state(0, BLE_LED_ERR_INTERNAL, "read failed");
        return BLE_ATT_ERR_UNLIKELY;
    }

    body[copied_len] = '\0';
    ESP_LOGI(TAG, "BLE command: %s", body);
    handle_command_json(body);
    return 0;
}

static int access_state(struct ble_gatt_access_ctxt *ctxt)
{
    char response[BLE_LED_STATE_JSON_LEN];
    system_status_snapshot_t snapshot = {0};

    system_status_get_snapshot(&snapshot);
    format_state_json(response, sizeof(response), 0,
                      snapshot.last_result_code, snapshot.last_result_msg);
    return append_text(ctxt->om, response);
}

static int access_device_info(struct ble_gatt_access_ctxt *ctxt)
{
    static const char DEVICE_INFO[] =
        "{\"name\":\"George_LED_Device\",\"fw\":\"0.1.0\",\"hw\":\"ESP-32F\",\"protocol\":1}";
    return append_text(ctxt->om, DEVICE_INFO);
}

static int ble_led_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void) conn_handle;
    (void) attr_handle;
    (void) arg;

    if (ble_uuid_cmp(ctxt->chr->uuid, &BLE_LED_COMMAND_UUID.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            return access_command(ctxt);
        }
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }
    if (ble_uuid_cmp(ctxt->chr->uuid, &BLE_LED_STATE_UUID.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return access_state(ctxt);
        }
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }
    if (ble_uuid_cmp(ctxt->chr->uuid, &BLE_LED_INFO_UUID.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return access_device_info(ctxt);
        }
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

esp_err_t ble_led_service_init(void)
{
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(BLE_LED_SERVICES);
    if (rc != 0) {
        ESP_LOGE(TAG, "count GATT config failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(BLE_LED_SERVICES);
    if (rc != 0) {
        ESP_LOGE(TAG, "add GATT services failed: %d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void ble_led_service_set_connection(uint16_t conn_handle, bool connected)
{
    s_connected = connected;
    s_notify_enabled = false;
    s_conn_handle = connected ? conn_handle : BLE_HS_CONN_HANDLE_NONE;
}

void ble_led_service_set_notify_enabled(uint16_t attr_handle, bool enabled)
{
    if (attr_handle == s_state_value_handle) {
        s_notify_enabled = enabled;
    }
}

void ble_led_service_notify_state(uint32_t seq, int code, const char *msg)
{
    char response[BLE_LED_STATE_JSON_LEN];
    struct os_mbuf *om;
    int rc;

    if (!s_connected || !s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }

    format_state_json(response, sizeof(response), seq, code, msg);
    om = ble_hs_mbuf_from_flat(response, strlen(response));
    if (om == NULL) {
        ESP_LOGE(TAG, "failed to allocate notify buffer");
        return;
    }

    rc = ble_gatts_notify_custom(s_conn_handle, s_state_value_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "notify failed: %d", rc);
    }
}
