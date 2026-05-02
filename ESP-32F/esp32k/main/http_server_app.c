#include "http_server_app.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble_led_service.h"
#include "cJSON.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "message_center.h"
#include "system_status.h"
#include "wifi_manager.h"

static const char *TAG = "http_server";

static httpd_handle_t s_server;

static const char *INDEX_HTML =
    "<!DOCTYPE html>"
    "<html lang=\"zh-CN\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>ESP32 LED Control</title>"
    "<style>"
    ":root{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;color:#0f172a;background:#e2e8f0;}"
    "body{margin:0;min-height:100vh;display:grid;place-items:center;background:linear-gradient(135deg,#dbeafe,#f8fafc 55%,#bfdbfe);}"
    ".panel{width:min(92vw,420px);background:rgba(255,255,255,.92);padding:24px;border-radius:20px;box-shadow:0 18px 40px rgba(15,23,42,.16);}"
    "h1{margin:0 0 16px;font-size:1.5rem;}label{display:block;margin:14px 0 8px;font-weight:600;}"
    "input,select,button{width:100%;border:none;border-radius:12px;padding:12px;font-size:1rem;}"
    "button{margin-top:18px;background:#0f172a;color:#fff;font-weight:700;cursor:pointer;}"
    ".status{margin-top:16px;padding:12px;border-radius:12px;background:#eff6ff;color:#1d4ed8;min-height:24px;}"
    ".tips{margin-top:16px;color:#475569;font-size:.92rem;line-height:1.5;}"
    "</style>"
    "</head>"
    "<body><main class=\"panel\"><h1>ESP32 RGB 控灯</h1>"
    "<label for=\"color\">颜色</label><input id=\"color\" type=\"color\" value=\"#ffffff\">"
    "<label for=\"mode\">模式</label><select id=\"mode\">"
    "<option value=\"solid\">常亮</option>"
    "<option value=\"breath\">呼吸</option>"
    "<option value=\"blink\">闪烁</option>"
    "</select>"
    "<button id=\"submit\">发送命令</button>"
    "<div class=\"status\" id=\"status\">等待发送</div>"
    "<div class=\"tips\">接口：POST /api/led，JSON 字段为 color、mode 和 brightness。</div>"
    "</main><script>"
    "const statusEl=document.getElementById('status');"
    "document.getElementById('submit').addEventListener('click',async()=>{"
    "const payload={color:document.getElementById('color').value,mode:document.getElementById('mode').value};"
    "statusEl.textContent='发送中...';"
    "try{const res=await fetch('/api/led',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});"
    "const data=await res.json();"
    "statusEl.textContent=data.ok?`已应用 ${data.color} / ${data.mode}`:(data.error||'请求失败');"
    "}catch(err){statusEl.textContent='请求失败: '+err.message;}"
    "});"
    "</script></body></html>";

static esp_err_t send_json_error(httpd_req_t *req, const char *status, const char *message)
{
    char response[128];

    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    snprintf(response, sizeof(response), "{\"ok\":false,\"error\":\"%s\"}", message);
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

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
    unsigned long value;

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

    value = strtoul(hex, &endptr, 16);
    if (endptr == NULL || *endptr != '\0') {
        return false;
    }

    *red = (uint8_t) ((value >> 16) & 0xFF);
    *green = (uint8_t) ((value >> 8) & 0xFF);
    *blue = (uint8_t) (value & 0xFF);
    return true;
}

static void command_apply_defaults(led_command_t *command)
{
    command->brightness = 100;
    command->period_ms = 2000;
    command->on_ms = 500;
    command->off_ms = 500;
    command->source = CONTROL_SOURCE_HTTP;
}

static void append_state_json(char *response, size_t response_size, const system_status_snapshot_t *snapshot)
{
    snprintf(response, response_size,
             "{\"device_state\":\"%s\",\"ble_connected\":%s,\"wifi_connected\":%s,"
             "\"led\":{\"color\":\"#%02X%02X%02X\",\"mode\":\"%s\",\"brightness\":%u,"
             "\"period_ms\":%" PRIu32 ",\"on_ms\":%" PRIu32 ",\"off_ms\":%" PRIu32 "},"
             "\"last_source\":\"%s\",\"last_result\":{\"code\":%d,\"msg\":\"%s\"}}",
             system_status_device_state_to_string(snapshot->device_state),
             snapshot->ble_connected ? "true" : "false",
             snapshot->wifi_connected ? "true" : "false",
             snapshot->led.color_r, snapshot->led.color_g, snapshot->led.color_b,
             system_status_led_mode_to_string(snapshot->led.mode),
             snapshot->led.brightness,
             snapshot->led.period_ms,
             snapshot->led.on_ms,
             snapshot->led.off_ms,
             system_status_control_source_to_string(snapshot->last_source),
             snapshot->last_result_code,
             snapshot->last_result_msg);
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t led_post_handler(httpd_req_t *req)
{
    char body[256];
    int remaining = req->content_len;
    int offset = 0;

    if (remaining <= 0 || remaining >= (int) sizeof(body)) {
        return send_json_error(req, "400 Bad Request", "request body too large");
    }

    while (remaining > 0) {
        const int received = httpd_req_recv(req, body + offset, remaining);
        if (received <= 0) {
            return send_json_error(req, "400 Bad Request", "failed to read request body");
        }
        remaining -= received;
        offset += received;
    }
    body[offset] = '\0';

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json_error(req, "400 Bad Request", "invalid json");
    }

    led_command_t command = {0};
    command_apply_defaults(&command);

    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(root, "mode");
    if (!cJSON_IsString(mode) || !parse_led_mode(mode->valuestring, &command.mode)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid mode");
    }

    if (command.mode != LED_MODE_OFF) {
        const cJSON *color = cJSON_GetObjectItemCaseSensitive(root, "color");
        if (!cJSON_IsString(color) ||
            !parse_hex_color(color->valuestring, &command.color_r, &command.color_g, &command.color_b)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid color format");
        }
    }

    const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(root, "brightness");
    if (brightness != NULL) {
        if (!cJSON_IsNumber(brightness) || brightness->valueint < 0 || brightness->valueint > 100) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid brightness");
        }
        command.brightness = (uint8_t) brightness->valueint;
    }

    const cJSON *period_ms = cJSON_GetObjectItemCaseSensitive(root, "period_ms");
    if (period_ms != NULL && cJSON_IsNumber(period_ms) && period_ms->valueint > 0) {
        command.period_ms = (uint32_t) period_ms->valueint;
    }
    const cJSON *on_ms = cJSON_GetObjectItemCaseSensitive(root, "on_ms");
    if (on_ms != NULL && cJSON_IsNumber(on_ms) && on_ms->valueint > 0) {
        command.on_ms = (uint32_t) on_ms->valueint;
    }
    const cJSON *off_ms = cJSON_GetObjectItemCaseSensitive(root, "off_ms");
    if (off_ms != NULL && cJSON_IsNumber(off_ms) && off_ms->valueint > 0) {
        command.off_ms = (uint32_t) off_ms->valueint;
    }

    cJSON_Delete(root);

    if (message_center_submit(&command) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue LED command");
        system_status_set_last_result(3001, "led queue failed");
        return send_json_error(req, "500 Internal Server Error", "failed to queue LED command");
    }

    system_status_set_led_command(&command);
    system_status_set_last_result(0, "ok");
    ble_led_service_notify_state(0, 0, "ok");

    char response[128];
    httpd_resp_set_type(req, "application/json");
    snprintf(response, sizeof(response),
             "{\"ok\":true,\"color\":\"#%02X%02X%02X\",\"mode\":\"%s\",\"brightness\":%u}",
             command.color_r, command.color_g, command.color_b,
             system_status_led_mode_to_string(command.mode), command.brightness);
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t state_get_handler(httpd_req_t *req)
{
    system_status_snapshot_t snapshot = {0};
    char response[384];

    system_status_get_snapshot(&snapshot);
    append_state_json(response, sizeof(response), &snapshot);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static void wifi_reset_restart_task(void *arg)
{
    (void) arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static esp_err_t wifi_reset_post_handler(httpd_req_t *req)
{
    char response[64];

    if (wifi_manager_reset_config() != ESP_OK) {
        return send_json_error(req, "500 Internal Server Error", "failed to reset wifi");
    }

    system_status_set_last_result(0, "wifi reset");
    httpd_resp_set_type(req, "application/json");
    snprintf(response, sizeof(response), "{\"ok\":true,\"restarting\":true}");
    esp_err_t ret = httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    if (xTaskCreate(wifi_reset_restart_task, "wifi_reset_restart", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi reset restart task");
        esp_restart();
    }

    return ret;
}

esp_err_t http_server_app_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;

    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "start server failed");

    const httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t led_uri = {
        .uri = "/api/led",
        .method = HTTP_POST,
        .handler = led_post_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t state_uri = {
        .uri = "/api/state",
        .method = HTTP_GET,
        .handler = state_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t wifi_reset_uri = {
        .uri = "/api/wifi/reset",
        .method = HTTP_POST,
        .handler = wifi_reset_post_handler,
        .user_ctx = NULL,
    };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &index_uri), TAG, "register / failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &led_uri), TAG, "register /api/led failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &state_uri), TAG, "register /api/state failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &wifi_reset_uri), TAG,
                        "register /api/wifi/reset failed");

    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}
