#include "http_server_app.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "message_center.h"
#include "notification_rules.h"
#include "system_status.h"

static const char *TAG = "http_server";

static httpd_handle_t s_server;

static const char *INDEX_HTML =
    "<!DOCTYPE html>"
    "<html lang=\"zh-CN\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>ESP32 通知灯效</title>"
    "<style>"
    ":root{--bg:#eef2f3;--card:#fff;--text:#17202a;--muted:#64748b;--border:#e2e8f0;--accent:#22313f;--accent-hover:#1a252f;--ok:#dcfce7;--ok-text:#166534;--err:#fee2e2;--err-text:#991b1b;--info:#dff3ff;--info-text:#075985;--shadow:0 1px 3px rgba(0,0,0,0.08);font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;color:var(--text);background:var(--bg);}"
    "body{margin:0;min-height:100vh;background:var(--bg);}main{max-width:1180px;margin:0 auto;padding:24px;}"
    "h1{font-size:22px;margin:0 0 14px;border-bottom:1px solid var(--border);padding-bottom:12px;}"
    "h2{font-size:16px;color:#334155;margin:0 0 12px;}"
    ".bar{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:10px 0 20px;}"
    "button{border:1px solid var(--accent);background:var(--accent);color:#fff;border-radius:8px;padding:8px 14px;font-weight:700;min-height:36px;cursor:pointer;transition:all .15s;}"
    "button:hover{background:var(--accent-hover);transform:translateY(-1px);box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    "button:active{transform:translateY(0);box-shadow:none;}"
    "button.secondary{background:var(--card);color:var(--accent);border-color:#cbd5db;}"
    "button.secondary:hover{background:#f8fafc;}"
    ".status{padding:7px 12px;border-radius:6px;background:var(--info);color:var(--info-text);font-size:13px;font-weight:600;transition:all .2s;}"
    ".status.ok{background:var(--ok);color:var(--ok-text);}"
    ".status.err{background:var(--err);color:var(--err-text);}"
    "table{width:100%;border-collapse:separate;border-spacing:0;background:var(--card);border:1px solid #ccd6dd;border-radius:8px;overflow:hidden;font-size:13px;}"
    "th,td{border-bottom:1px solid #e3e8ec;padding:9px 8px;text-align:left;}"
    "th{background:#f1f5f9;font-weight:700;color:#475569;border-bottom:2px solid #cbd5db;}"
    "tr:last-child td{border-bottom:none;}"
    "tr:nth-child(even) td{background:#fafbfc;}"
    "tr:hover td{background:#f0f7ff;}"
    "input,select{box-sizing:border-box;border:1px solid #cbd5db;border-radius:5px;padding:6px;background:var(--card);font-size:13px;}"
    "input:focus,select:focus{border-color:#3b82f6;outline:none;box-shadow:0 0 0 2px rgba(59,130,246,0.15);}"
    "input[type=color]{width:42px;padding:1px;height:32px;cursor:pointer;}"
    "input[type=number]{width:60px;text-align:center;}"
    ".enabled{width:auto;height:18px;cursor:pointer;}"
    ".grid{display:grid;grid-template-columns:1fr 1fr;gap:16px;}"
    ".panel{background:var(--card);border:1px solid #ccd6dd;padding:16px;border-radius:10px;box-shadow:var(--shadow);}"
    ".test-row{display:flex;align-items:center;gap:8px;margin-bottom:10px;}"
    ".test-row label{font-size:13px;color:var(--muted);min-width:40px;}"
    ".test-row input,.test-row select{flex:1;}"
    ".divider{height:1px;background:var(--border);margin:16px 0;}"
    "#presets th,#presets td{padding:7px 8px;font-size:12px;}"
    "@media(max-width:820px){main{padding:12px;}.grid{grid-template-columns:1fr;gap:12px;}table{display:block;overflow:auto;white-space:nowrap;scrollbar-width:thin;}.bar{gap:6px;margin-bottom:14px;}button{padding:7px 10px;}}"
    "</style>"
    "</head>"
    "<body><main><h1>ESP32 iPhone 通知灯效</h1>"
    "<div class=\"bar\"><button id=\"save\">保存规则</button><button class=\"secondary\" id=\"reset\">恢复默认</button><span class=\"status\" id=\"status\">加载中</span></div>"
    "<div class=\"grid\"><section class=\"panel\"><h2>通知规则</h2><table><thead><tr><th>启用</th><th>名称</th><th>Bundle ID</th><th>类型</th><th>颜色</th><th>模式</th><th>亮度%</th><th>周期ms</th><th>亮ms</th><th>灭ms</th></tr></thead><tbody id=\"rules\"></tbody></table></section>"
    "<section class=\"panel\"><h2>手动测试</h2><div class=\"test-row\"><label>颜色</label><input id=\"testColor\" type=\"color\" value=\"#ffffff\"></div><div class=\"test-row\"><label>模式</label><select id=\"testMode\"><option value=\"solid\">常亮</option><option value=\"breath\">呼吸</option><option value=\"blink\">闪烁</option><option value=\"off\">关闭</option></select></div><button id=\"testLed\">发送测试灯效</button><div class=\"divider\"></div><h2>常见 Bundle ID</h2><table><tbody id=\"presets\"></tbody></table></section></div>"
    "</main><script>"
    "const $=id=>document.getElementById(id),statusEl=$('status');let presets=[];"
    "const modes=['solid','breath','blink','off'],kinds=['any','message','incoming_call'];"
    "function opt(list,val){return list.map(x=>`<option value=\"${x}\" ${x===val?'selected':''}>${x}</option>`).join('')}"
    "function row(r,i){return `<tr><td><input class=\"enabled\" type=\"checkbox\" ${r.enabled?'checked':''}></td><td><input class=\"label\" maxlength=\"31\" value=\"${r.label||''}\"></td><td><input class=\"app\" maxlength=\"63\" value=\"${r.app_id||''}\"></td><td><select class=\"kind\">${opt(kinds,r.kind||'any')}</select></td><td><input class=\"color\" type=\"color\" value=\"${r.color||'#ffffff'}\"></td><td><select class=\"mode\">${opt(modes,r.mode||'solid')}</select></td><td><input class=\"brightness\" type=\"number\" min=\"0\" max=\"100\" value=\"${r.brightness??100}\"></td><td><input class=\"period\" type=\"number\" min=\"1\" value=\"${r.period_ms??2000}\"></td><td><input class=\"on\" type=\"number\" min=\"1\" value=\"${r.on_ms??300}\"></td><td><input class=\"off\" type=\"number\" min=\"1\" value=\"${r.off_ms??300}\"></td></tr>`}"
    "function collect(){return [...document.querySelectorAll('#rules tr')].map(tr=>({enabled:tr.querySelector('.enabled').checked,label:tr.querySelector('.label').value,app_id:tr.querySelector('.app').value,kind:tr.querySelector('.kind').value,color:tr.querySelector('.color').value,mode:tr.querySelector('.mode').value,brightness:+tr.querySelector('.brightness').value,period_ms:+tr.querySelector('.period').value,on_ms:+tr.querySelector('.on').value,off_ms:+tr.querySelector('.off').value}))}"
    "async function load(){const r=await fetch('/api/notification-rules');const d=await r.json();presets=d.presets||[];$('rules').innerHTML=(d.rules||[]).map(row).join('');$('presets').innerHTML=presets.map(p=>`<tr><td>${p.label}</td><td>${p.app_id}</td></tr>`).join('');statusEl.textContent='已加载';statusEl.className='status ok';}"
    "$('save').onclick=async()=>{statusEl.textContent='保存中';statusEl.className='status';const r=await fetch('/api/notification-rules',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({rules:collect()})});if(r.ok){statusEl.textContent='已保存';statusEl.className='status ok';}else{statusEl.textContent=(await r.json()).error;statusEl.className='status err';}};"
    "$('reset').onclick=async()=>{await fetch('/api/notification-rules/reset',{method:'POST'});await load();statusEl.textContent='已恢复默认';statusEl.className='status ok';};"
    "$('testLed').onclick=async()=>{await fetch('/api/led',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({color:$('testColor').value,mode:$('testMode').value})});statusEl.textContent='测试灯效已发送';statusEl.className='status ok';};"
    "load().catch(e=>{statusEl.textContent=e.message;statusEl.className='status err';});"
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
    cJSON *root = cJSON_CreateObject();
    cJSON *led = cJSON_CreateObject();
    cJSON *ancs = cJSON_CreateObject();
    cJSON *last_result = cJSON_CreateObject();
    char color[8];

    if (root == NULL || led == NULL || ancs == NULL || last_result == NULL) {
        snprintf(response, response_size, "{}");
        cJSON_Delete(root);
        cJSON_Delete(led);
        cJSON_Delete(ancs);
        cJSON_Delete(last_result);
        return;
    }

    snprintf(color, sizeof(color), "#%02X%02X%02X",
             snapshot->led.color_r, snapshot->led.color_g, snapshot->led.color_b);

    cJSON_AddStringToObject(root, "device_state",
                            system_status_device_state_to_string(snapshot->device_state));
    cJSON_AddBoolToObject(root, "ble_connected", snapshot->ble_connected);
    cJSON_AddBoolToObject(root, "ancs_connected", snapshot->ancs_connected);
    cJSON_AddBoolToObject(root, "wifi_connected", snapshot->wifi_connected);
    cJSON_AddStringToObject(root, "ip_address", snapshot->ip_address);

    cJSON_AddStringToObject(led, "color", color);
    cJSON_AddStringToObject(led, "mode", system_status_led_mode_to_string(snapshot->led.mode));
    cJSON_AddNumberToObject(led, "brightness", snapshot->led.brightness);
    cJSON_AddNumberToObject(led, "period_ms", snapshot->led.period_ms);
    cJSON_AddNumberToObject(led, "on_ms", snapshot->led.on_ms);
    cJSON_AddNumberToObject(led, "off_ms", snapshot->led.off_ms);
    cJSON_AddItemToObject(root, "led", led);

    cJSON_AddNumberToObject(ancs, "uid", snapshot->ancs_notification.notification_uid);
    cJSON_AddStringToObject(ancs, "action",
                            ancs_event_action_to_string(snapshot->ancs_notification.action));
    cJSON_AddStringToObject(ancs, "app_id", snapshot->ancs_notification.app_id);
    cJSON_AddStringToObject(ancs, "title", snapshot->ancs_notification.title);
    cJSON_AddStringToObject(ancs, "subtitle", snapshot->ancs_notification.subtitle);
    cJSON_AddStringToObject(ancs, "message", snapshot->ancs_notification.message);
    cJSON_AddNumberToObject(ancs, "category_id", snapshot->ancs_notification.category_id);
    cJSON_AddBoolToObject(ancs, "rule_matched", snapshot->last_ancs_rule_matched);
    cJSON_AddStringToObject(ancs, "rule_label", snapshot->last_ancs_rule_label);
    cJSON_AddItemToObject(root, "ancs", ancs);

    cJSON_AddStringToObject(root, "last_source",
                            system_status_control_source_to_string(snapshot->last_source));
    cJSON_AddNumberToObject(last_result, "code", snapshot->last_result_code);
    cJSON_AddStringToObject(last_result, "msg", snapshot->last_result_msg);
    cJSON_AddItemToObject(root, "last_result", last_result);

    if (!cJSON_PrintPreallocated(root, response, (int)response_size, false)) {
        snprintf(response, response_size, "{}");
    }
    cJSON_Delete(root);
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
    char response[1024];

    system_status_get_snapshot(&snapshot);
    append_state_json(response, sizeof(response), &snapshot);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t wifi_reset_post_handler(httpd_req_t *req)
{
    return send_json_error(req, "409 Conflict", "fixed wifi credentials are compiled in");
}

static esp_err_t rules_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    char *json = NULL;
    esp_err_t ret = ESP_OK;

    if (root == NULL) {
        return send_json_error(req, "500 Internal Server Error", "no memory");
    }

    notification_rules_add_json(root);
    json = cJSON_PrintUnformatted(root);
    if (json == NULL) {
        cJSON_Delete(root);
        return send_json_error(req, "500 Internal Server Error", "no memory");
    }

    httpd_resp_set_type(req, "application/json");
    ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json);
    cJSON_Delete(root);
    return ret;
}

static char *read_request_body(httpd_req_t *req, size_t max_body_size)
{
    int remaining = req->content_len;
    int offset = 0;
    char *body;

    if (remaining <= 0 || remaining > (int)max_body_size) {
        return NULL;
    }
    body = malloc((size_t)remaining + 1);
    if (body == NULL) {
        return NULL;
    }

    while (remaining > 0) {
        const int received = httpd_req_recv(req, body + offset, remaining);
        if (received <= 0) {
            free(body);
            return NULL;
        }
        remaining -= received;
        offset += received;
    }
    body[offset] = '\0';
    return body;
}

static bool parse_rule_item(const cJSON *item, notification_rule_t *rule)
{
    const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(item, "enabled");
    const cJSON *label = cJSON_GetObjectItemCaseSensitive(item, "label");
    const cJSON *app_id = cJSON_GetObjectItemCaseSensitive(item, "app_id");
    const cJSON *kind = cJSON_GetObjectItemCaseSensitive(item, "kind");
    const cJSON *color = cJSON_GetObjectItemCaseSensitive(item, "color");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(item, "mode");
    const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(item, "brightness");
    const cJSON *period_ms = cJSON_GetObjectItemCaseSensitive(item, "period_ms");
    const cJSON *on_ms = cJSON_GetObjectItemCaseSensitive(item, "on_ms");
    const cJSON *off_ms = cJSON_GetObjectItemCaseSensitive(item, "off_ms");

    if (rule == NULL || !cJSON_IsObject(item) || !cJSON_IsString(label) ||
        !cJSON_IsString(app_id) || !cJSON_IsString(kind) || !cJSON_IsString(color) ||
        !cJSON_IsString(mode)) {
        return false;
    }

    memset(rule, 0, sizeof(*rule));
    rule->enabled = enabled == NULL ? true : cJSON_IsTrue(enabled);
    strlcpy(rule->label, label->valuestring, sizeof(rule->label));
    strlcpy(rule->app_id, app_id->valuestring, sizeof(rule->app_id));

    if (!notification_rules_parse_kind(kind->valuestring, &rule->kind) ||
        !notification_rules_parse_color(color->valuestring, &rule->color_r,
                                        &rule->color_g, &rule->color_b) ||
        !notification_rules_parse_mode(mode->valuestring, &rule->mode)) {
        return false;
    }

    rule->brightness = 100;
    rule->period_ms = 2000;
    rule->on_ms = 300;
    rule->off_ms = 300;

    if (brightness != NULL) {
        if (!cJSON_IsNumber(brightness) || brightness->valueint < 0 ||
            brightness->valueint > 100) {
            return false;
        }
        rule->brightness = (uint8_t)brightness->valueint;
    }
    if (period_ms != NULL) {
        if (!cJSON_IsNumber(period_ms) || period_ms->valueint <= 0) {
            return false;
        }
        rule->period_ms = (uint32_t)period_ms->valueint;
    }
    if (on_ms != NULL) {
        if (!cJSON_IsNumber(on_ms) || on_ms->valueint <= 0) {
            return false;
        }
        rule->on_ms = (uint32_t)on_ms->valueint;
    }
    if (off_ms != NULL) {
        if (!cJSON_IsNumber(off_ms) || off_ms->valueint <= 0) {
            return false;
        }
        rule->off_ms = (uint32_t)off_ms->valueint;
    }

    return true;
}

static esp_err_t rules_post_handler(httpd_req_t *req)
{
    char *body;
    notification_rule_t rules[NOTIFICATION_RULE_MAX_COUNT];
    size_t rule_count = 0;

    body = read_request_body(req, 8192);
    if (body == NULL) {
        return send_json_error(req, "400 Bad Request", "invalid request body");
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (root == NULL) {
        return send_json_error(req, "400 Bad Request", "invalid json");
    }

    const cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "rules");
    if (!cJSON_IsArray(items) || cJSON_GetArraySize(items) > NOTIFICATION_RULE_MAX_COUNT) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid rule list");
    }

    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, items) {
        if (!parse_rule_item(item, &rules[rule_count])) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid rule item");
        }
        rule_count++;
    }
    cJSON_Delete(root);

    if (notification_rules_set(rules, rule_count) != ESP_OK) {
        return send_json_error(req, "500 Internal Server Error", "save rules failed");
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t rules_reset_post_handler(httpd_req_t *req)
{
    notification_rules_reset_defaults();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
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
    const httpd_uri_t rules_get_uri = {
        .uri = "/api/notification-rules",
        .method = HTTP_GET,
        .handler = rules_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t rules_post_uri = {
        .uri = "/api/notification-rules",
        .method = HTTP_POST,
        .handler = rules_post_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t rules_reset_uri = {
        .uri = "/api/notification-rules/reset",
        .method = HTTP_POST,
        .handler = rules_reset_post_handler,
        .user_ctx = NULL,
    };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &index_uri), TAG, "register / failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &led_uri), TAG, "register /api/led failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &state_uri), TAG, "register /api/state failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &wifi_reset_uri), TAG,
                        "register /api/wifi/reset failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &rules_get_uri), TAG,
                        "register GET /api/notification-rules failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &rules_post_uri), TAG,
                        "register POST /api/notification-rules failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &rules_reset_uri), TAG,
                        "register /api/notification-rules/reset failed");

    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}
