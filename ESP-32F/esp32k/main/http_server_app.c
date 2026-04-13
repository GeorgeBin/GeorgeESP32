#include "http_server_app.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "message_center.h"

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
    "<div class=\"tips\">接口：POST /api/led，JSON 字段为 color 和 mode。</div>"
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

static bool extract_json_string_value(const char *json, const char *key, char *out, size_t out_size)
{
    char pattern[32];
    const char *key_pos;
    const char *colon_pos;
    const char *value_start;
    const char *value_end;
    size_t value_len;

    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) <= 0) {
        return false;
    }

    key_pos = strstr(json, pattern);
    if (key_pos == NULL) {
        return false;
    }

    colon_pos = strchr(key_pos + strlen(pattern), ':');
    if (colon_pos == NULL) {
        return false;
    }

    value_start = colon_pos + 1;
    while (*value_start != '\0' && isspace((unsigned char) *value_start)) {
        value_start++;
    }

    if (*value_start != '"') {
        return false;
    }

    value_start++;
    value_end = strchr(value_start, '"');
    if (value_end == NULL) {
        return false;
    }

    value_len = (size_t) (value_end - value_start);
    if (value_len == 0 || value_len >= out_size) {
        return false;
    }

    memcpy(out, value_start, value_len);
    out[value_len] = '\0';
    return true;
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t led_post_handler(httpd_req_t *req)
{
    char body[128];
    char color_text[16];
    char mode_text[16];
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

    if (!extract_json_string_value(body, "color", color_text, sizeof(color_text)) ||
        !extract_json_string_value(body, "mode", mode_text, sizeof(mode_text))) {
        return send_json_error(req, "400 Bad Request", "invalid json");
    }

    led_command_t command = {0};
    if (!parse_hex_color(color_text, &command.color_r, &command.color_g, &command.color_b)) {
        return send_json_error(req, "400 Bad Request", "invalid color format");
    }
    if (!parse_led_mode(mode_text, &command.mode)) {
        return send_json_error(req, "400 Bad Request", "invalid mode");
    }

    if (message_center_submit(&command) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue LED command");
        return send_json_error(req, "500 Internal Server Error", "failed to queue LED command");
    }

    char response[96];
    httpd_resp_set_type(req, "application/json");
    snprintf(response, sizeof(response),
             "{\"ok\":true,\"color\":\"#%02X%02X%02X\",\"mode\":\"%s\"}",
             command.color_r, command.color_g, command.color_b, mode_text);
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
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

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &index_uri), TAG, "register / failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &led_uri), TAG, "register /api/led failed");

    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}
