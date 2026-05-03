#include "http_server_app.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "device_config.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "message_center.h"
#include "notification_rules.h"
#include "phase_effect.h"
#include "recent_apps.h"
#include "system_status.h"

static const char *TAG = "http_server";

static httpd_handle_t s_server;

static const char *INDEX_HTML =
    "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\" /><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" /><title>George Light · ANCS 配置 v3</title><style> :root { --bg: #f5f7f8; --card: #ffffff; --text: #162026; --muted: #6b7a86; --line: #dbe3e8; --primary: #1fb7a6; --danger: #d94a4a; --warn: #f0a92e; --ok: #22a06b; --shadow: 0 10px 28px rgba(22, 32, 38, .08); --radius: 18px; } * { box-sizing: border-box; } body { margin: 0; font-family: -apple-system, BlinkMacSystemFont, \"SF Pro Text\", \"Segoe UI\", Roboto, \"Helvetica Neue\", Arial, \"PingFang SC\", \"Microsoft YaHei\", sans-serif; background: radial-gradient(circle at top left, #e8fffb 0, #f5f7f8 38%, #eef2f4 100%); color: var(--text); } header { position: sticky; top: 0; z-index: 20; backdrop-filter: blur(16px); background: rgba(245, 247, 248, .88); border-bottom: 1px solid rgba(219, 227, 232, .86"
    "); } .bar { max-width: 1040px; margin: 0 auto; padding: 14px 16px 10px; display: grid; gap: 12px; } .top-row { display: flex; gap: 12px; align-items: center; justify-content: space-between; } .brand { display: flex; align-items: center; gap: 10px; min-width: 0; } .logo { width: 36px; height: 36px; border-radius: 50%; background: linear-gradient(135deg, #79ffe1, #1fb7a6); box-shadow: inset 0 0 0 1px rgba(255,255,255,.55), 0 8px 22px rgba(31,183,166,.22); flex: 0 0 auto; } .brand h1 { margin: 0; font-size: 18px; line-height: 1.2; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; } .brand p { margin: 2px 0 0; font-size: 12px; color: var(--muted); } .pill { display: inline-flex; align-items: center; gap: 6px; padding: 7px 10px; border: 1px solid var(--line); border-radius: 999px; background: rgba(255,255,255,.78); color: var(--muted); font-size: 12px; white-space: nowrap; } .do"
    "t { width: 8px; height: 8px; border-radius: 50%; background: var(--warn); } .dot.ok { background: var(--ok); } .dot.bad { background: var(--danger); } .tabs { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; padding: 4px; border: 1px solid rgba(219, 227, 232, .9); border-radius: 16px; background: rgba(255,255,255,.62); } .tab-button { border: 0; border-radius: 12px; padding: 10px 8px; background: transparent; color: var(--muted); font-weight: 750; cursor: pointer; box-shadow: none; } .tab-button.active { background: var(--card); color: var(--text); box-shadow: 0 6px 16px rgba(22, 32, 38, .08); } main { max-width: 1040px; margin: 0 auto; padding: 18px 16px 48px; } .tab-panel { display: none; } .tab-panel.active { display: block; } .grid { display: grid; grid-template-columns: 1.08fr .92fr; gap: 16px; } .card { background: rgba(255,255,255,.92); border: 1px solid rgba(219, 2"
    "27, 232, .96); border-radius: var(--radius); box-shadow: var(--shadow); overflow: hidden; margin-bottom: 16px; } .card-head { padding: 16px 16px 10px; display: flex; justify-content: space-between; align-items: flex-start; gap: 12px; } .card h2 { margin: 0; font-size: 16px; } .card .desc { margin: 5px 0 0; color: var(--muted); font-size: 12px; line-height: 1.5; } .body { padding: 0 16px 16px; } .status-list { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 10px; } .status-item { padding: 12px; border: 1px solid var(--line); border-radius: 14px; background: #fbfcfd; } .status-item label { display: block; color: var(--muted); font-size: 12px; margin-bottom: 5px; } .status-item strong { font-size: 14px; } .summary-row { display: grid; grid-template-columns: auto 1fr auto; gap: 12px; align-items: center; padding: 13px 0; border-top: 1px solid var(--line); } .summary-row"
    ":first-child { border-top: 0; } .swatch { width: 34px; height: 34px; border-radius: 50%; border: 1px solid rgba(0,0,0,.08); box-shadow: inset 0 0 0 4px rgba(255,255,255,.45); } .row-title { margin: 0; font-weight: 750; font-size: 14px; } .row-sub { margin: 4px 0 0; color: var(--muted); font-size: 12px; line-height: 1.4; word-break: break-all; } .actions { display: flex; gap: 8px; flex-wrap: wrap; } button, .button { border: 0; border-radius: 12px; padding: 10px 12px; font-weight: 750; background: var(--primary); color: white; cursor: pointer; box-shadow: 0 8px 18px rgba(31,183,166,.2); } button.secondary { background: #edf4f5; color: var(--text); box-shadow: none; border: 1px solid var(--line); } button.danger { background: #fff0f0; color: var(--danger); box-shadow: none; border: 1px solid #ffd5d5; } button:active { transform: translateY(1px); } button:disabled { opacity: .5; cursor: not"
    "-allowed; } .form-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; } .field { display: grid; gap: 6px; } .field.full { grid-column: 1 / -1; } .field label { color: var(--muted); font-size: 12px; font-weight: 750; } input, select { width: 100%; border: 1px solid var(--line); border-radius: 12px; padding: 11px 12px; font: inherit; background: #fbfcfd; color: var(--text); outline: none; } input:focus, select:focus { border-color: var(--primary); box-shadow: 0 0 0 3px rgba(31,183,166,.12); } input[type=\"color\"] { padding: 4px; height: 44px; } .hint { font-size: 12px; color: var(--muted); line-height: 1.5; margin: 8px 0 0; } .phase-grid { display: grid; grid-template-columns: repeat(5, minmax(0, 1fr)); gap: 10px; } .phase { padding: 12px; border: 1px solid var(--line); border-radius: 15px; background: #fbfcfd; display: grid; gap: 8px; } .phase-title { displ"
    "ay: flex; align-items: center; justify-content: space-between; gap: 8px; font-size: 13px; font-weight: 800; } .phase-title span:last-child { color: var(--muted); font-size: 11px; font-weight: 700; } .preset-grid { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 10px; } .preset { border: 1px solid var(--line); border-radius: 15px; background: #fbfcfd; padding: 12px; display: grid; gap: 8px; align-content: start; } .preset strong { font-size: 14px; } .preset span { color: var(--muted); font-size: 12px; line-height: 1.45; } .tag { display: inline-flex; width: fit-content; padding: 3px 7px; border-radius: 999px; background: #edf4f5; color: var(--muted); font-size: 11px; font-weight: 800; } .recent { display: grid; gap: 8px; } .recent-item { padding: 10px; border: 1px solid var(--line); border-radius: 13px; background: #fbfcfd; display: grid; grid-template-columns: 1fr a"
    "uto; gap: 8px; align-items: center; } .recent-item p { margin: 0; font-size: 13px; font-weight: 750; } .recent-item span { display: block; margin-top: 3px; color: var(--muted); font-size: 12px; word-break: break-all; } .empty { padding: 14px; border: 1px dashed var(--line); border-radius: 14px; color: var(--muted); background: #fbfcfd; font-size: 13px; line-height: 1.5; } .footer-actions { position: sticky; bottom: 0; margin-top: 16px; padding: 12px; border: 1px solid rgba(219, 227, 232, .9); border-radius: 18px; background: rgba(255,255,255,.88); backdrop-filter: blur(14px); display: flex; gap: 10px; justify-content: flex-end; box-shadow: var(--shadow); } .toast { position: fixed; left: 50%; bottom: 22px; transform: translateX(-50%); padding: 11px 14px; border-radius: 999px; background: rgba(22,32,38,.92); color: white; font-size: 13px; opacity: 0; pointer-events: none; transition: opac"
    "ity .18s ease, transform .18s ease; z-index: 50; } .toast.show { opacity: 1; transform: translateX(-50%) translateY(-4px); } @media (max-width: 860px) { .grid { grid-template-columns: 1fr; } .phase-grid { grid-template-columns: 1fr; } .preset-grid { grid-template-columns: 1fr; } } @media (max-width: 760px) { .status-list { grid-template-columns: 1fr; } .form-grid { grid-template-columns: 1fr; } .top-row { align-items: flex-start; } .pill { display: none; } .tabs { grid-template-columns: 1fr; } .footer-actions { justify-content: stretch; } .footer-actions button { flex: 1; } .summary-row { grid-template-columns: auto 1fr; } .summary-row .actions { grid-column: 1 / -1; } } </style></head><body><header><div class=\"bar\"><div class=\"top-row\"><div class=\"brand\"><div class=\"logo\" aria-hidden=\"true\"></div><div><h1>George Light ANCS 配置</h1><p>v3 · iPhone 通知 → App/分类/关键字 → LED 灯效</p></di"
    "v></div><div class=\"pill\"><span id=\"connDot\" class=\"dot\"></span><span id=\"connText\">读取设备状态中</span></div></div><nav class=\"tabs\" aria-label=\"页面导航\"><button id=\"tabOverviewBtn\" class=\"tab-button active\" onclick=\"switchTab('overview')\">概览与已配置</button><button id=\"tabConfigBtn\" class=\"tab-button\" onclick=\"switchTab('config')\">规则配置</button><button id=\"tabRecentBtn\" class=\"tab-button\" onclick=\"switchTab('recent')\">最近通知采集</button></nav></div></header><main><section id=\"tabOverview\" class=\"tab-panel active\"><section class=\"grid\"><div class=\"card\"><div class=\"card-head\"><div><h2>设备状态</h2><p class=\"desc\">用于确认当前是否已连接 iPhone、是否已获得 ANCS 授权，以及是否存在测试灯效覆盖。</p></div><button class=\"secondary\" onclick=\"loadAll()\">刷新</button></div><div class=\"body\"><div class=\"status-list\"><div class=\"status-item\"><label>BLE 连接</label><strong id=\"bleStatus\">--</strong></di"
    "v><div class=\"status-item\"><label>ANCS 授权</label><strong id=\"ancsStatus\">--</strong></div><div class=\"status-item\"><label>配置模式</label><strong id=\"wifiStatus\">--</strong></div><div class=\"status-item\"><label>测试灯效</label><strong id=\"testStatus\">--</strong></div><div class=\"status-item\"><label>当前灯效来源</label><strong id=\"ledSource\">--</strong></div><div class=\"status-item\"><label>规则数量</label><strong id=\"ruleCount\">--</strong></div></div></div></div><div class=\"card\"><div class=\"card-head\"><div><h2>全局设置摘要</h2><p class=\"desc\">全局设置影响设备名称、默认亮度、隐私策略和通知移除后的行为。</p></div></div><div class=\"body\" id=\"globalSummary\"></div></div></section><section class=\"card\"><div class=\"card-head\"><div><h2>已配置：设备全局规则</h2><p class=\"desc\">这些规则来自设备自身状态，不依赖 ANCS App。</p></div></div><div class=\"body\" id=\"phaseSummary\"></div></section><section class=\"card\"><div class=\"card-head\"><d"
    "iv><h2>已配置：ANCS 应用规则</h2><p class=\"desc\">这些规则用于匹配 iPhone 通知，按优先级从高到低执行。</p></div><button onclick=\"switchTab('config')\">新建/编辑规则</button></div><div class=\"body\" id=\"rulesSummary\"></div></section></section><section id=\"tabConfig\" class=\"tab-panel\"><section class=\"card\"><div class=\"card-head\"><div><h2>全局设置</h2><p class=\"desc\">建议默认不保存通知正文，只保存 App ID 和分类，降低隐私风险。</p></div></div><div class=\"body form-grid\"><div class=\"field\"><label for=\"deviceName\">设备名称</label><input id=\"deviceName\" placeholder=\"George Light\" /></div><div class=\"field\"><label for=\"defaultBrightness\">默认亮度</label><input id=\"defaultBrightness\" type=\"number\" min=\"1\" max=\"100\" value=\"60\" /></div><div class=\"field\"><label for=\"privacyMode\">隐私模式</label><select id=\"privacyMode\"><option value=\"app_only\">仅记录 App ID/分类</option><option value=\"title_only\">允许临时读取标题</option><option value=\"fu"
    "ll\">允许临时读取标题和内容</option></select></div><div class=\"field\"><label for=\"clearBehavior\">通知移除后</label><select id=\"clearBehavior\"><option value=\"restore\">恢复当前状态灯效</option><option value=\"off\">关闭 LED</option><option value=\"keep\">保持当前灯效</option></select></div></div></section><section class=\"card\"><div class=\"card-head\"><div><h2>设备阶段灯效配置</h2><p class=\"desc\">这些不是 ANCS 应用规则，而是设备自身状态灯效。测试灯效会临时覆盖它们，但不会写入状态机。</p></div></div><div class=\"body\"><div class=\"phase-grid\"><div class=\"phase\"><div class=\"phase-title\"><span>开机阶段</span><span>boot</span></div><input id=\"phaseBootColor\" type=\"color\" value=\"#1fb7a6\" /><select id=\"phaseBootMode\"><option value=\"solid\">常亮</option><option value=\"breath\">呼吸</option><option value=\"blink\">闪烁</option><option value=\"pulse\">脉冲</option><option value=\"rainbow\">彩虹</option><option value=\"off\">关闭</option></select><input id=\"phaseBoo"
    "tBrightness\" type=\"number\" min=\"1\" max=\"100\" value=\"50\" placeholder=\"亮度\" /><input id=\"phaseBootDuration\" type=\"number\" min=\"0\" step=\"500\" value=\"3000\" placeholder=\"持续 ms\" /></div><div class=\"phase\"><div class=\"phase-title\"><span>未连接时</span><span>disconnected</span></div><input id=\"phaseDisconnectedColor\" type=\"color\" value=\"#f0a92e\" /><select id=\"phaseDisconnectedMode\"><option value=\"solid\">常亮</option><option value=\"breath\">呼吸</option><option value=\"blink\">闪烁</option><option value=\"pulse\">脉冲</option><option value=\"rainbow\">彩虹</option><option value=\"off\">关闭</option></select><input id=\"phaseDisconnectedBrightness\" type=\"number\" min=\"1\" max=\"100\" value=\"35\" placeholder=\"亮度\" /><input id=\"phaseDisconnectedDuration\" type=\"number\" min=\"0\" step=\"500\" value=\"0\" placeholder=\"0=持续\" /></div><div class=\"phase\"><div class=\"phase"
    "-title\"><span>待机</span><span>standby</span></div><input id=\"phaseStandbyColor\" type=\"color\" value=\"#16343d\" /><select id=\"phaseStandbyMode\"><option value=\"solid\">常亮</option><option value=\"breath\">呼吸</option><option value=\"blink\">闪烁</option><option value=\"pulse\">脉冲</option><option value=\"rainbow\">彩虹</option><option value=\"off\">关闭</option></select><input id=\"phaseStandbyBrightness\" type=\"number\" min=\"1\" max=\"100\" value=\"8\" placeholder=\"亮度\" /><input id=\"phaseStandbyDuration\" type=\"number\" min=\"0\" step=\"500\" value=\"0\" placeholder=\"0=持续\" /></div><div class=\"phase\"><div class=\"phase-title\"><span>未匹配呼叫</span><span>default.call</span></div><input id=\"phaseUnmatchedCallColor\" type=\"color\" value=\"#ff3b30\" /><select id=\"phaseUnmatchedCallMode\"><option value=\"solid\">常亮</option><option value=\"breath\">呼吸</option><option value=\"blink\" selec"
    "ted>闪烁</option><option value=\"pulse\">脉冲</option><option value=\"rainbow\">彩虹</option><option value=\"off\">关闭</option></select><input id=\"phaseUnmatchedCallBrightness\" type=\"number\" min=\"1\" max=\"100\" value=\"85\" placeholder=\"亮度\" /><input id=\"phaseUnmatchedCallDuration\" type=\"number\" min=\"0\" step=\"500\" value=\"15000\" placeholder=\"持续 ms\" /></div><div class=\"phase\"><div class=\"phase-title\"><span>未匹配消息</span><span>default.message</span></div><input id=\"phaseUnmatchedMessageColor\" type=\"color\" value=\"#1fb7a6\" /><select id=\"phaseUnmatchedMessageMode\"><option value=\"solid\">常亮</option><option value=\"breath\" selected>呼吸</option><option value=\"blink\">闪烁</option><option value=\"pulse\">脉冲</option><option value=\"rainbow\">彩虹</option><option value=\"off\">关闭</option></select><input id=\"phaseUnmatchedMessageBrightness\" type=\"number\" min=\"1\" max=\"100\" "
    "value=\"55\" placeholder=\"亮度\" /><input id=\"phaseUnmatchedMessageDuration\" type=\"number\" min=\"0\" step=\"500\" value=\"8000\" placeholder=\"持续 ms\" /></div></div></div></section><section class=\"card\"><div class=\"card-head\"><div><h2>微信高级规则模板</h2><p class=\"desc\">微信普通消息较稳定；语音/视频通话、@自己、红包、转账等只能通过通知文本关键字尽力匹配。</p></div></div><div class=\"body\"><div class=\"preset-grid\"><div class=\"preset\"><span class=\"tag\">稳定</span><strong>微信普通消息</strong><span>匹配 com.tencent.xin + Social，作为微信默认消息灯效。</span><button class=\"secondary\" onclick=\"applyWechatPreset('normal')\">套用到编辑区</button></div><div class=\"preset\"><span class=\"tag\">实验</span><strong>微信语音/视频通话</strong><span>尝试关键字：语音通话、视频通话、来电、邀请你通话。</span><button class=\"secondary\" onclick=\"applyWechatPreset('call')\">套用到编辑区</button></div><div class=\"preset\"><span class=\"tag\">实验</span><strong>微信 @ 我</strong><span>尝试关键字：@我、有人@我、提到了你。</sp"
    "an><button class=\"secondary\" onclick=\"applyWechatPreset('mention')\">套用到编辑区</button></div><div class=\"preset\"><span class=\"tag\">实验</span><strong>微信红包</strong><span>尝试关键字：红包、微信红包、给你发了一个红包。</span><button class=\"secondary\" onclick=\"applyWechatPreset('red_packet')\">套用到编辑区</button></div><div class=\"preset\"><span class=\"tag\">实验</span><strong>微信转账</strong><span>尝试关键字：转账、收款、向你转账。</span><button class=\"secondary\" onclick=\"applyWechatPreset('transfer')\">套用到编辑区</button></div><div class=\"preset\"><span class=\"tag\">建议</span><strong>微信兜底</strong><span>所有无法细分的微信通知，走统一微信灯效。</span><button class=\"secondary\" onclick=\"applyWechatPreset('fallback')\">套用到编辑区</button></div></div></div></section><section class=\"card\"><div class=\"card-head\"><div><h2 id=\"editorTitle\">ANCS 应用规则编辑器</h2><p class=\"desc\">点击“新建规则”会清空编辑区，但不会立刻创建数据；只有点击“保存到规则列表”才会加入配置。</p></div></div><div class=\"body form"
    "-grid\"><input id=\"ruleId\" type=\"hidden\" /><div class=\"field\"><label for=\"ruleName\">规则名称</label><input id=\"ruleName\" placeholder=\"例如：微信消息\" /></div><div class=\"field\"><label for=\"ruleEnabled\">启用状态</label><select id=\"ruleEnabled\"><option value=\"true\">启用</option><option value=\"false\">停用</option></select></div><div class=\"field full\"><label for=\"appId\">App Identifier</label><input id=\"appId\" placeholder=\"例如：com.tencent.xin；留空表示不按 App 匹配\" /></div><div class=\"field\"><label for=\"category\">ANCS 分类</label><select id=\"category\"><option value=\"any\">任意分类</option><option value=\"other\">Other</option><option value=\"incoming_call\">Incoming Call</option><option value=\"missed_call\">Missed Call</option><option value=\"voicemail\">Voicemail</option><option value=\"social\">Social</option><option value=\"schedule\">Schedule</option><option value=\"email\">Email</op"
    "tion><option value=\"news\">News</option><option value=\"health_fitness\">Health & Fitness</option><option value=\"business_finance\">Business & Finance</option><option value=\"location\">Location</option><option value=\"entertainment\">Entertainment</option></select></div><div class=\"field\"><label for=\"eventType\">事件类型</label><select id=\"eventType\"><option value=\"added\">新增通知</option><option value=\"modified\">通知更新</option><option value=\"removed\">通知移除</option><option value=\"any\">任意事件</option></select></div><div class=\"field full\"><label for=\"keyword\">标题/内容关键字，可选</label><input id=\"keyword\" placeholder=\"例如：报警、会议、验证码；多个关键字可用 | 分隔\" /></div><div class=\"field\"><label for=\"color\">颜色</label><input id=\"color\" type=\"color\" value=\"#1fb7a6\" /></div><div class=\"field\"><label for=\"mode\">灯效模式</label><select id=\"mode\"><option value=\"solid\">常亮</option><option value=\""
    "breath\">呼吸</option><option value=\"blink\">闪烁</option><option value=\"pulse\">脉冲</option><option value=\"rainbow\">彩虹</option><option value=\"off\">关闭</option></select></div><div class=\"field\"><label for=\"brightness\">亮度 1-100</label><input id=\"brightness\" type=\"number\" min=\"1\" max=\"100\" value=\"60\" /></div><div class=\"field\"><label for=\"durationMs\">持续时间 ms</label><input id=\"durationMs\" type=\"number\" min=\"500\" step=\"500\" value=\"8000\" /></div><div class=\"field\"><label for=\"priority\">优先级</label><input id=\"priority\" type=\"number\" min=\"0\" max=\"100\" value=\"50\" /></div><div class=\"field\"><label for=\"repeat\">重复次数</label><input id=\"repeat\" type=\"number\" min=\"0\" max=\"99\" value=\"1\" /></div><div class=\"field full actions\"><button onclick=\"saveRuleFromEditor()\">保存到规则列表</button><button class=\"secondary\" onclick=\"testRuleFromEditor()\">测试灯效"
    "</button><button class=\"secondary\" onclick=\"clearTestEffect()\">停止测试/恢复灯效</button><button class=\"secondary\" onclick=\"newRule()\">新建规则</button><button class=\"danger\" onclick=\"clearEditor()\">清空编辑区</button></div></div></section><div class=\"footer-actions\"><button class=\"secondary\" onclick=\"loadAll()\">重新读取</button><button onclick=\"saveConfig()\">保存到设备</button></div></section><section id=\"tabRecent\" class=\"tab-panel\"><section class=\"card\"><div class=\"card-head\"><div><h2>最近通知采集</h2><p class=\"desc\">进入本页后才请求设备开始记录 ANCS 通知摘要；离开本页即停止。设备端建议最多保留 10 条。</p></div><div class=\"actions\"><button class=\"secondary\" onclick=\"refreshRecent()\">刷新</button><button class=\"danger\" onclick=\"clearRecent()\">清空</button></div></div><div class=\"body\"><div class=\"status-list\" style=\"margin-bottom:12px;\"><div class=\"status-item\"><label>采集状态</label><strong id=\"recentStatus\">未开启"
    "</strong></div><div class=\"status-item\"><label>最多记录</label><strong>10 条</strong></div></div><div id=\"recent\" class=\"recent\"></div><p class=\"hint\">建议固件只保存 App ID、Category、EventType、Title/Summary 的短摘要，不长期保存完整通知正文。</p></div></section></section></main><div id=\"toast\" class=\"toast\"></div><script> const API = { status: '/api/status', config: '/api/config', test: '/api/rules/test', testStop: '/api/rules/test/stop', recentStart: '/api/recent/start', recentStop: '/api/recent/stop', recentList: '/api/recent/list', recentClear: '/api/recent/clear' }; const fallbackConfig = { version: 3, deviceName: 'George Light', defaultBrightness: 60, privacyMode: 'app_only', clearBehavior: 'restore', phaseEffects: { boot: { color: '#1fb7a6', mode: 'pulse', brightness: 50, durationMs: 3000, repeat: 1 }, disconnected: { color: '#f0a92e', mode: 'breath', brightness: 35, durationMs: 0, repeat: 0 }, stand"
    "by: { color: '#16343d', mode: 'breath', brightness: 8, durationMs: 0, repeat: 0 }, unmatchedCall: { color: '#ff3b30', mode: 'blink', brightness: 85, durationMs: 15000, repeat: 8 }, unmatchedMessage: { color: '#1fb7a6', mode: 'breath', brightness: 55, durationMs: 8000, repeat: 1 } }, rules: [ { id: 'wechat-normal', name: '微信普通消息', enabled: true, priority: 70, match: { appId: 'com.tencent.xin', category: 'social', eventType: 'added', keyword: '' }, led: { color: '#21c45d', mode: 'breath', brightness: 70, durationMs: 8000, repeat: 1 } }, { id: 'system-incoming-call', name: '系统电话呼入', enabled: true, priority: 95, match: { appId: '', category: 'incoming_call', eventType: 'added', keyword: '' }, led: { color: '#ff3b30', mode: 'blink', brightness: 90, durationMs: 15000, repeat: 8 } } ], recentApps: [] }; let config = structuredClone(fallbackConfig); let activeTab = 'overview'; let recentTimer = "
    "null; let recentEnabled = false; function $(id) { return document.getElementById(id); } function toast(msg) { const el = $('toast'); el.textContent = msg; el.classList.add('show'); setTimeout(() => el.classList.remove('show'), 1800); } async function requestJson(url, options) { const res = await fetch(url, options); if (!res.ok) throw new Error('HTTP ' + res.status); return await res.json(); } async function postNoBody(url) { const res = await fetch(url, { method: 'POST' }); if (!res.ok) throw new Error('HTTP ' + res.status); const text = await res.text(); return text ? JSON.parse(text) : { ok: true }; } function switchTab(tab) { if (tab === activeTab) return; if (activeTab === 'recent' && tab !== 'recent') stopRecentCapture(); activeTab = tab; ['overview', 'config', 'recent'].forEach(name => { $('tab' + capitalize(name)).classList.toggle('active', name === tab); $('tab' + capitalize(nam"
    "e) + 'Btn').classList.toggle('active', name === tab); }); if (tab === 'overview') renderOverview(); if (tab === 'config') renderConfigEditor(); if (tab === 'recent') startRecentCapture(); } async function loadAll() { try { const status = await requestJson(API.status); renderStatus(status); } catch (e) { renderStatus({ bleConnected: false, ancsAuthorized: false, configMode: true, testOverrideActive: false, ledSource: 'unknown' }); } try { config = await requestJson(API.config); migrateConfig(); } catch (e) { config = structuredClone(fallbackConfig); toast('使用页面内置示例数据'); } renderAll(); } function migrateConfig() { config.version = Math.max(Number(config.version || 1), 3); config.phaseEffects = config.phaseEffects || structuredClone(fallbackConfig.phaseEffects); config.rules = config.rules || []; } function renderAll() { renderOverview(); renderConfigEditor(); if (activeTab === 'recent') re"
    "nderRecent(config.recentApps || []); } function renderStatus(status) { $('bleStatus').textContent = status.bleConnected ? '已连接 iPhone' : '未连接'; $('ancsStatus').textContent = status.ancsAuthorized ? '已授权' : '未授权/未知'; $('wifiStatus').textContent = status.configMode ? 'AP 配置中' : '已关闭'; $('testStatus').textContent = status.testOverrideActive ? '测试覆盖中' : '未开启'; $('ledSource').textContent = status.ledSource || '--'; const dot = $('connDot'); dot.className = 'dot ' + (status.bleConnected && status.ancsAuthorized ? 'ok' : ''); $('connText').textContent = status.bleConnected ? '设备在线' : '配置页面可用'; } function renderOverview() { $('ruleCount').textContent = (config.rules || []).length + ' 条'; $('globalSummary').innerHTML = ` <div class=\"summary-row\"><div class=\"swatch\" style=\"background:#1fb7a6\"></div><div><p class=\"row-title\">${escapeHtml(config.deviceName || 'George Light')}</p><p class=\"r"
    "ow-sub\">默认亮度 ${config.defaultBrightness || 60}% · 隐私模式 ${escapeHtml(config.privacyMode || 'app_only')} · 移除后 ${escapeHtml(config.clearBehavior || 'restore')}</p></div><div class=\"actions\"><button class=\"secondary\" onclick=\"switchTab('config')\">编辑</button></div></div> `; renderPhaseSummary(); renderRulesSummary(); } function renderPhaseSummary() { const effects = config.phaseEffects || fallbackConfig.phaseEffects; const rows = [ ['开机阶段', 'boot', effects.boot], ['未连接时', 'disconnected', effects.disconnected], ['待机', 'standby', effects.standby], ['未匹配呼叫', 'default.call', effects.unmatchedCall], ['未匹配消息', 'default.message', effects.unmatchedMessage] ]; $('phaseSummary').innerHTML = rows.map(([title, key, e]) => ` <div class=\"summary-row\"><div class=\"swatch\" style=\"background:${e.color}\"></div><div><p class=\"row-title\">${title}</p><p class=\"row-sub\">${key} · ${escapeHtml(e.mod"
    "e)} · 亮度 ${e.brightness}% · ${e.durationMs === 0 ? '持续' : e.durationMs + 'ms'} · repeat ${e.repeat}</p></div><div class=\"actions\"><button class=\"secondary\" onclick=\"switchTab('config')\">编辑</button></div></div> `).join(''); } function renderRulesSummary() { const rules = [...(config.rules || [])].sort((a, b) => (b.priority || 0) - (a.priority || 0)); if (!rules.length) { $('rulesSummary').innerHTML = '<div class=\"empty\">暂无 ANCS 应用规则。进入“规则配置”后可以新建，或进入“最近通知采集”从真实通知生成。</div>'; return; } $('rulesSummary').innerHTML = rules.map(rule => ` <div class=\"summary-row\"><div class=\"swatch\" style=\"background:${rule.led.color}\"></div><div><p class=\"row-title\">${escapeHtml(rule.name)} ${rule.enabled ? '' : '· 已停用'}</p><p class=\"row-sub\">优先级 ${rule.priority || 0} · ${escapeHtml(rule.match.appId || '任意 App')} · ${escapeHtml(rule.match.category || 'any')} · ${escapeHtml(rule.match.eventTyp"
    "e || 'any')} · ${rule.match.keyword ? '关键字 ' + escapeHtml(rule.match.keyword) + ' · ' : ''}${escapeHtml(rule.led.mode)} · ${escapeHtml(String(rule.led.durationMs || 0))}ms</p></div><div class=\"actions\"><button class=\"secondary\" onclick=\"editRuleAndOpen('${rule.id}')\">编辑</button><button class=\"danger\" onclick=\"deleteRule('${rule.id}')\">删除</button></div></div> `).join(''); } function renderConfigEditor() { $('deviceName').value = config.deviceName || ''; $('defaultBrightness').value = config.defaultBrightness || 60; $('privacyMode').value = config.privacyMode || 'app_only'; $('clearBehavior').value = config.clearBehavior || 'restore'; renderLifecycle(); } function renderLifecycle() { const effects = config.phaseEffects || fallbackConfig.phaseEffects; setPhase('Boot', effects.boot); setPhase('Disconnected', effects.disconnected); setPhase('Standby', effects.standby); setPhase('Unm"
    "atchedCall', effects.unmatchedCall); setPhase('UnmatchedMessage', effects.unmatchedMessage); } function setPhase(name, effect) { $('phase' + name + 'Color').value = effect.color || '#1fb7a6'; $('phase' + name + 'Mode').value = effect.mode || 'breath'; $('phase' + name + 'Brightness').value = effect.brightness || 50; $('phase' + name + 'Duration').value = effect.durationMs || 0; } function collectGlobal() { config.deviceName = $('deviceName').value.trim(); config.defaultBrightness = numberValue('defaultBrightness', 60); config.privacyMode = $('privacyMode').value; config.clearBehavior = $('clearBehavior').value; config.phaseEffects = { boot: getPhase('Boot', 1), disconnected: getPhase('Disconnected', 0), standby: getPhase('Standby', 0), unmatchedCall: getPhase('UnmatchedCall', 8), unmatchedMessage: getPhase('UnmatchedMessage', 1) }; } function getPhase(name, repeat) { return { color: $('p"
    "hase' + name + 'Color').value, mode: $('phase' + name + 'Mode').value, brightness: numberValue('phase' + name + 'Brightness', 50), durationMs: numberValue('phase' + name + 'Duration', 0), repeat }; } async function saveConfig() { collectGlobal(); config.version = 3; try { await requestJson(API.config, { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(config) }); renderOverview(); toast('已保存到设备'); } catch (e) { console.warn(e); toast('保存失败：设备接口不可用'); } } function newRule() { clearEditor(); $('editorTitle').textContent = 'ANCS 应用规则编辑器：新建规则'; } function editRuleAndOpen(id) { switchTab('config'); setTimeout(() => editRule(id), 0); } function editRule(id) { const r = (config.rules || []).find(x => x.id === id); if (!r) return; $('editorTitle').textContent = 'ANCS 应用规则编辑器：' + r.name; $('ruleId').value = r.id; $('ruleName').value = r.name || ''; $('ruleEnabl"
    "ed').value = String(r.enabled !== false); $('appId').value = r.match.appId || ''; $('category').value = r.match.category || 'any'; $('eventType').value = r.match.eventType || 'added'; $('keyword').value = r.match.keyword || ''; $('color').value = r.led.color || '#1fb7a6'; $('mode').value = r.led.mode || 'breath'; $('brightness').value = r.led.brightness || 60; $('durationMs').value = r.led.durationMs || 8000; $('priority').value = r.priority || 50; $('repeat').value = r.led.repeat || 1; scrollToEditor(); } function clearEditor() { $('ruleId').value = ''; $('ruleName').value = ''; $('ruleEnabled').value = 'true'; $('appId').value = ''; $('category').value = 'any'; $('eventType').value = 'added'; $('keyword').value = ''; $('color').value = '#1fb7a6'; $('mode').value = 'breath'; $('brightness').value = config.defaultBrightness || 60; $('durationMs').value = 8000; $('priority').value = 50; $"
    "('repeat').value = 1; } function saveRuleFromEditor() { const id = $('ruleId').value || makeId(); const name = $('ruleName').value.trim() || '未命名规则'; const rule = { id, name, enabled: $('ruleEnabled').value === 'true', priority: numberValue('priority', 50), match: { appId: $('appId').value.trim(), category: $('category').value, eventType: $('eventType').value, keyword: $('keyword').value.trim() }, led: { color: $('color').value, mode: $('mode').value, brightness: numberValue('brightness', 60), durationMs: numberValue('durationMs', 8000), repeat: numberValue('repeat', 1) } }; const idx = (config.rules || []).findIndex(x => x.id === id); if (idx >= 0) config.rules[idx] = rule; else config.rules.push(rule); $('ruleId').value = id; renderOverview(); toast('规则已更新，记得保存到设备'); } async function testRuleFromEditor() { const testPayload = { color: $('color').value, mode: $('mode').value, brightness"
    ": numberValue('brightness', 60), durationMs: numberValue('durationMs', 3000), repeat: numberValue('repeat', 1), temporary: true }; try { await requestJson(API.test, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(testPayload) }); toast('已发送测试灯效'); await refreshStatusOnly(); } catch (e) { toast('测试失败：设备接口不可用'); } } async function clearTestEffect() { try { await postNoBody(API.testStop); toast('已停止测试，恢复当前灯效'); await refreshStatusOnly(); } catch (e) { toast('停止测试失败：设备接口不可用'); } } async function refreshStatusOnly() { try { const status = await requestJson(API.status); renderStatus(status); } catch (e) {} } function deleteRule(id) { config.rules = (config.rules || []).filter(x => x.id !== id); renderOverview(); toast('已删除，记得保存到设备'); } function createFromRecent(appId, displayName, category, eventType) { switchTab('config'); setTimeout(() => { clearEditor"
    "(); $('ruleName').value = (displayName || appId) + ' 通知'; $('appId').value = appId || ''; $('category').value = category || 'any'; $('eventType').value = eventType || 'added'; $('priority').value = 70; $('mode').value = category === 'incoming_call' ? 'blink' : 'breath'; $('color').value = category === 'incoming_call' ? '#ff3b30' : '#1fb7a6'; scrollToEditor(); }, 0); } function applyWechatPreset(type) { const presets = { normal: { name: '微信普通消息', keyword: '', color: '#21c45d', mode: 'breath', priority: 70, durationMs: 8000, repeat: 1 }, call: { name: '微信语音/视频通话', keyword: '语音通话|视频通话|来电|邀请你通话', color: '#ff3b30', mode: 'blink', priority: 92, durationMs: 15000, repeat: 8 }, mention: { name: '微信 @ 我', keyword: '@我|有人@我|提到了你', color: '#ffcc00', mode: 'pulse', priority: 88, durationMs: 10000, repeat: 3 }, red_packet: { name: '微信红包', keyword: '红包|微信红包|给你发了一个红包', color: '#ff2d55', mode: 'blink', "
    "priority: 90, durationMs: 12000, repeat: 6 }, transfer: { name: '微信转账', keyword: '转账|收款|向你转账', color: '#ff9500', mode: 'pulse', priority: 89, durationMs: 12000, repeat: 4 }, fallback: { name: '微信兜底通知', keyword: '', color: '#21c45d', mode: 'breath', priority: 60, durationMs: 8000, repeat: 1 } }; const p = presets[type] || presets.normal; clearEditor(); $('editorTitle').textContent = 'ANCS 应用规则编辑器：' + p.name; $('ruleName').value = p.name; $('appId').value = 'com.tencent.xin'; $('category').value = 'social'; $('eventType').value = 'added'; $('keyword').value = p.keyword; $('color').value = p.color; $('mode').value = p.mode; $('priority').value = p.priority; $('durationMs').value = p.durationMs; $('repeat').value = p.repeat; scrollToEditor(); } async function startRecentCapture() { try { await postNoBody(API.recentStart); recentEnabled = true; $('recentStatus').textContent = '采集中'; } catch ("
    "e) { recentEnabled = false; $('recentStatus').textContent = '接口不可用，显示缓存'; } await refreshRecent(); clearInterval(recentTimer); recentTimer = setInterval(refreshRecent, 3000); } async function stopRecentCapture() { clearInterval(recentTimer); recentTimer = null; if (!recentEnabled) { $('recentStatus').textContent = '未开启'; return; } try { await postNoBody(API.recentStop); } catch (e) {} recentEnabled = false; $('recentStatus').textContent = '已停止'; } async function refreshRecent() { let items = []; try { const data = await requestJson(API.recentList); items = Array.isArray(data) ? data : (data.items || []); } catch (e) { items = (config.recentApps || []).slice(0, 10); } renderRecent(items.slice(0, 10)); } async function clearRecent() { try { await postNoBody(API.recentClear); } catch (e) {} renderRecent([]); toast('最近通知记录已清空'); } function renderRecent(items) { const box = $('recent'); if (!"
    "items.length) { box.innerHTML = '<div class=\"empty\">暂无记录。保持本页打开，然后让 iPhone 收到一条通知，再点击刷新或等待自动刷新。</div>'; return; } box.innerHTML = items.map(item => { const appId = item.appId || item.appIdentifier || ''; const name = item.displayName || item.title || appId || '未知 App'; const category = item.category || item.categoryId || 'any'; const eventType = item.eventType || item.event || 'added'; const summary = item.summary || item.message || item.subtitle || ''; return ` <div class=\"recent-item\"><div><p>${escapeHtml(name)}</p><span>${escapeHtml(appId)} · ${escapeHtml(category)} · ${escapeHtml(eventType)}${summary ? ' · ' + escapeHtml(summary) : ''}</span></div><button class=\"secondary\" onclick=\"createFromRecent('${escapeAttr(appId)}','${escapeAttr(name)}','${escapeAttr(category)}','${escapeAttr(eventType)}')\">生成规则</button></div> `; }).join(''); } function scrollToEditor() { const editor ="
    " $('editorTitle'); editor.scrollIntoView({ behavior: 'smooth', block: 'start' }); } function numberValue(id, fallback) { const n = Number($(id).value); return Number.isFinite(n) ? n : fallback; } function makeId() { return 'rule-' + Date.now().toString(36) + '-' + Math.random().toString(36).slice(2, 6); } function capitalize(str) { return str.charAt(0).toUpperCase() + str.slice(1); } function escapeHtml(str) { return String(str).replace(/[&<>'\"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;',\"'\":'&#39;','\"':'&quot;'}[c])); } function escapeAttr(str) { return escapeHtml(str).replace(/`/g, '&#96;'); } window.addEventListener('beforeunload', () => { if (recentEnabled) navigator.sendBeacon(API.recentStop); }); loadAll(); </script></body></html>";

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
    return notification_rules_parse_mode(mode_str, mode);
}

static bool parse_hex_color(const char *text, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    return notification_rules_parse_color(text, red, green, blue);
}

static esp_err_t build_status_json(char *buf, size_t buf_size)
{
    system_status_snapshot_t snapshot = {0};
    size_t rule_count = 0;

    system_status_get_snapshot(&snapshot);
    notification_rules_get(NULL, 0, &rule_count);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        snprintf(buf, buf_size, "{}");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddBoolToObject(root, "bleConnected", snapshot.ble_connected);
    cJSON_AddBoolToObject(root, "ancsAuthorized", snapshot.ancs_connected);
    cJSON_AddBoolToObject(root, "configMode", !snapshot.wifi_connected);
    cJSON_AddBoolToObject(root, "testOverrideActive", snapshot.test_override_active);
    cJSON_AddStringToObject(root, "ledSource", snapshot.led_source_string);
    cJSON_AddNumberToObject(root, "ruleCount", (int)rule_count);

    if (!cJSON_PrintPreallocated(root, buf, (int)buf_size, false)) {
        snprintf(buf, buf_size, "{}");
    }
    cJSON_Delete(root);
    return ESP_OK;
}

static char *build_config_json(void)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    device_config_t cfg;
    device_config_get(&cfg);

    cJSON_AddNumberToObject(root, "version", 3);
    cJSON_AddStringToObject(root, "deviceName", cfg.device_name);
    cJSON_AddNumberToObject(root, "defaultBrightness", cfg.default_brightness);

    const char *privacy_str = "app_only";
    if (cfg.privacy_mode == 1) {
        privacy_str = "title_only";
    } else if (cfg.privacy_mode == 2) {
        privacy_str = "full";
    }
    cJSON_AddStringToObject(root, "privacyMode", privacy_str);

    const char *clear_str = "restore";
    if (cfg.clear_behavior == 1) {
        clear_str = "keep";
    } else if (cfg.clear_behavior == 2) {
        clear_str = "off";
    }
    cJSON_AddStringToObject(root, "clearBehavior", clear_str);

    cJSON *phaseEffects = cJSON_AddObjectToObject(root, "phaseEffects");
    if (phaseEffects != NULL) {
        phase_effect_add_json(phaseEffects, "boot", &cfg.phase_boot);
        phase_effect_add_json(phaseEffects, "disconnected", &cfg.phase_disconnected);
        phase_effect_add_json(phaseEffects, "standby", &cfg.phase_standby);
        phase_effect_add_json(phaseEffects, "unmatchedCall", &cfg.phase_unmatched_call);
        phase_effect_add_json(phaseEffects, "unmatchedMessage", &cfg.phase_unmatched_message);
    }

    notification_rules_add_json(root);
    recent_apps_get_json(root);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
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

static esp_err_t parse_config_body(const char *body, device_config_t *cfg,
                                   notification_rule_t *rules, size_t *rule_count)
{
    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *deviceName = cJSON_GetObjectItemCaseSensitive(root, "deviceName");
    const cJSON *defaultBrightness = cJSON_GetObjectItemCaseSensitive(root, "defaultBrightness");
    const cJSON *privacyMode = cJSON_GetObjectItemCaseSensitive(root, "privacyMode");
    const cJSON *clearBehavior = cJSON_GetObjectItemCaseSensitive(root, "clearBehavior");

    if (cJSON_IsString(deviceName)) {
        strncpy(cfg->device_name, deviceName->valuestring, sizeof(cfg->device_name) - 1);
        cfg->device_name[sizeof(cfg->device_name) - 1] = '\0';
    }
    if (cJSON_IsNumber(defaultBrightness)) {
        int v = defaultBrightness->valueint;
        cfg->default_brightness = (uint8_t)((v < 1) ? 1 : (v > 100) ? 100 : v);
    }
    if (cJSON_IsString(privacyMode)) {
        if (strcmp(privacyMode->valuestring, "app_only") == 0) {
            cfg->privacy_mode = 0;
        } else if (strcmp(privacyMode->valuestring, "title_only") == 0) {
            cfg->privacy_mode = 1;
        } else if (strcmp(privacyMode->valuestring, "full") == 0) {
            cfg->privacy_mode = 2;
        }
    }
    if (cJSON_IsString(clearBehavior)) {
        if (strcmp(clearBehavior->valuestring, "restore") == 0) {
            cfg->clear_behavior = 0;
        } else if (strcmp(clearBehavior->valuestring, "keep") == 0) {
            cfg->clear_behavior = 1;
        } else if (strcmp(clearBehavior->valuestring, "off") == 0) {
            cfg->clear_behavior = 2;
        }
    }

    const cJSON *phaseEffects = cJSON_GetObjectItemCaseSensitive(root, "phaseEffects");
    if (cJSON_IsObject(phaseEffects)) {
        const cJSON *boot = cJSON_GetObjectItemCaseSensitive(phaseEffects, "boot");
        const cJSON *disconnected = cJSON_GetObjectItemCaseSensitive(phaseEffects, "disconnected");
        const cJSON *standby = cJSON_GetObjectItemCaseSensitive(phaseEffects, "standby");
        const cJSON *unmatchedCall = cJSON_GetObjectItemCaseSensitive(phaseEffects, "unmatchedCall");
        const cJSON *unmatchedMessage = cJSON_GetObjectItemCaseSensitive(phaseEffects, "unmatchedMessage");
        if (cJSON_IsObject(boot)) {
            phase_effect_parse_json(boot, &cfg->phase_boot);
        }
        if (cJSON_IsObject(disconnected)) {
            phase_effect_parse_json(disconnected, &cfg->phase_disconnected);
        }
        if (cJSON_IsObject(standby)) {
            phase_effect_parse_json(standby, &cfg->phase_standby);
        }
        if (cJSON_IsObject(unmatchedCall)) {
            phase_effect_parse_json(unmatchedCall, &cfg->phase_unmatched_call);
        }
        if (cJSON_IsObject(unmatchedMessage)) {
            phase_effect_parse_json(unmatchedMessage, &cfg->phase_unmatched_message);
        }
    }

    const cJSON *rules_array = cJSON_GetObjectItemCaseSensitive(root, "rules");
    if (cJSON_IsArray(rules_array)) {
        int array_size = cJSON_GetArraySize(rules_array);
        if (array_size > (int)NOTIFICATION_RULE_MAX_COUNT) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }

        int idx = 0;
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, rules_array) {
            if (idx >= (int)NOTIFICATION_RULE_MAX_COUNT) {
                break;
            }

            memset(&rules[idx], 0, sizeof(rules[idx]));
            rules[idx].enabled = true;
            rules[idx].category = 255;
            rules[idx].event_type = 0;
            rules[idx].brightness = 100;
            rules[idx].priority = 50;
            rules[idx].period_ms = 2000;
            rules[idx].on_ms = 300;
            rules[idx].off_ms = 300;
            rules[idx].duration_ms = 8000;
            rules[idx].repeat = 1;

            const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
            const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(item, "enabled");
            const cJSON *priority = cJSON_GetObjectItemCaseSensitive(item, "priority");
            const cJSON *match = cJSON_GetObjectItemCaseSensitive(item, "match");
            const cJSON *led = cJSON_GetObjectItemCaseSensitive(item, "led");

            if (cJSON_IsString(name)) {
                strncpy(rules[idx].label, name->valuestring, sizeof(rules[idx].label) - 1);
            }
            if (cJSON_IsBool(enabled)) {
                rules[idx].enabled = cJSON_IsTrue(enabled);
            }
            if (cJSON_IsNumber(priority)) {
                int p = priority->valueint;
                rules[idx].priority = (uint8_t)((p < 0) ? 0 : (p > 100) ? 100 : p);
            }

            if (cJSON_IsObject(match)) {
                const cJSON *appId = cJSON_GetObjectItemCaseSensitive(match, "appId");
                const cJSON *category = cJSON_GetObjectItemCaseSensitive(match, "category");
                const cJSON *eventType = cJSON_GetObjectItemCaseSensitive(match, "eventType");
                const cJSON *keyword = cJSON_GetObjectItemCaseSensitive(match, "keyword");

                if (cJSON_IsString(appId)) {
                    strncpy(rules[idx].app_id, appId->valuestring, sizeof(rules[idx].app_id) - 1);
                }
                if (cJSON_IsString(category)) {
                    notification_rules_parse_category(category->valuestring, &rules[idx].category);
                }
                if (cJSON_IsString(eventType)) {
                    notification_rules_parse_event_type(eventType->valuestring, &rules[idx].event_type);
                }
                if (cJSON_IsString(keyword)) {
                    strncpy(rules[idx].keyword, keyword->valuestring, sizeof(rules[idx].keyword) - 1);
                }
            }

            if (cJSON_IsObject(led)) {
                const cJSON *color = cJSON_GetObjectItemCaseSensitive(led, "color");
                const cJSON *mode = cJSON_GetObjectItemCaseSensitive(led, "mode");
                const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(led, "brightness");
                const cJSON *durationMs = cJSON_GetObjectItemCaseSensitive(led, "durationMs");
                const cJSON *periodMs = cJSON_GetObjectItemCaseSensitive(led, "periodMs");
                const cJSON *repeat = cJSON_GetObjectItemCaseSensitive(led, "repeat");

                if (cJSON_IsString(color)) {
                    parse_hex_color(color->valuestring, &rules[idx].color_r,
                                    &rules[idx].color_g, &rules[idx].color_b);
                }
                if (cJSON_IsString(mode)) {
                    led_mode_t m;
                    if (parse_led_mode(mode->valuestring, &m)) {
                        rules[idx].mode = m;
                    }
                }
                if (cJSON_IsNumber(brightness)) {
                    int b = brightness->valueint;
                    rules[idx].brightness = (uint8_t)((b < 0) ? 0 : (b > 100) ? 100 : b);
                }
                if (cJSON_IsNumber(durationMs) && durationMs->valueint >= 0) {
                    rules[idx].duration_ms = (uint32_t)durationMs->valueint;
                }
                if (cJSON_IsNumber(periodMs) && periodMs->valueint > 0) {
                    rules[idx].period_ms = (uint32_t)periodMs->valueint;
                }
                if (cJSON_IsNumber(repeat)) {
                    int r = repeat->valueint;
                    rules[idx].repeat = (uint8_t)((r < 0) ? 0 : (r > 99) ? 99 : r);
                }
            }

            idx++;
        }
        *rule_count = (size_t)idx;
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char response[256];
    httpd_resp_set_type(req, "application/json");
    if (build_status_json(response, sizeof(response)) != ESP_OK) {
        return send_json_error(req, "500 Internal Server Error", "failed to build status");
    }
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t config_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    char *response = build_config_json();
    if (response == NULL) {
        return send_json_error(req, "500 Internal Server Error", "failed to build config");
    }
    esp_err_t ret = httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    free(response);
    return ret;
}

static esp_err_t config_put_handler(httpd_req_t *req)
{
    char *body = read_request_body(req, 16384);
    if (body == NULL) {
        return send_json_error(req, "400 Bad Request", "invalid request body");
    }

    device_config_t cfg = {0};
    notification_rule_t rules[NOTIFICATION_RULE_MAX_COUNT];
    size_t rule_count = 0;

    if (device_config_get(&cfg) != ESP_OK) {
        free(body);
        return send_json_error(req, "500 Internal Server Error", "failed to load current device config");
    }
    if (notification_rules_get(rules, NOTIFICATION_RULE_MAX_COUNT, &rule_count) != ESP_OK) {
        free(body);
        return send_json_error(req, "500 Internal Server Error", "failed to load current rules");
    }

    esp_err_t ret = parse_config_body(body, &cfg, rules, &rule_count);
    free(body);

    if (ret != ESP_OK) {
        return send_json_error(req, "400 Bad Request", "invalid config format");
    }

    if (device_config_set(&cfg) != ESP_OK) {
        return send_json_error(req, "500 Internal Server Error", "failed to save device config");
    }

    if (notification_rules_set(rules, rule_count) != ESP_OK) {
        return send_json_error(req, "500 Internal Server Error", "failed to save rules");
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t rules_test_post_handler(httpd_req_t *req)
{
    char body[256];
    int remaining = req->content_len;
    int offset = 0;

    if (remaining <= 0 || remaining >= (int)sizeof(body)) {
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
    command.source = CONTROL_SOURCE_HTTP;
    command.brightness = 60;
    command.period_ms = 2000;
    command.on_ms = 300;
    command.off_ms = 300;
    command.duration_ms = 0;
    command.repeat = 0;

    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(root, "mode");
    if (!cJSON_IsString(mode) || !parse_led_mode(mode->valuestring, &command.mode)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid mode");
    }

    if (command.mode != LED_MODE_OFF) {
        const cJSON *color = cJSON_GetObjectItemCaseSensitive(root, "color");
        if (!cJSON_IsString(color) ||
            !parse_hex_color(color->valuestring, &command.color_r, &command.color_g,
                             &command.color_b)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid color format");
        }
    }

    const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(root, "brightness");
    if (brightness != NULL && cJSON_IsNumber(brightness)) {
        int b = brightness->valueint;
        command.brightness = (uint8_t)((b < 0) ? 0 : (b > 100) ? 100 : b);
    }

    const cJSON *periodMs = cJSON_GetObjectItemCaseSensitive(root, "periodMs");
    if (periodMs != NULL && cJSON_IsNumber(periodMs) && periodMs->valueint > 0) {
        command.period_ms = (uint32_t)periodMs->valueint;
    }
    const cJSON *durationMs = cJSON_GetObjectItemCaseSensitive(root, "durationMs");
    if (durationMs != NULL && cJSON_IsNumber(durationMs) && durationMs->valueint >= 0) {
        command.duration_ms = (uint32_t)durationMs->valueint;
    }
    const cJSON *repeat = cJSON_GetObjectItemCaseSensitive(root, "repeat");
    if (repeat != NULL && cJSON_IsNumber(repeat)) {
        int r = repeat->valueint;
        command.repeat = (uint8_t)((r < 0) ? 0 : (r > 99) ? 99 : r);
    }
    const cJSON *onMs = cJSON_GetObjectItemCaseSensitive(root, "onMs");
    if (onMs != NULL && cJSON_IsNumber(onMs) && onMs->valueint > 0) {
        command.on_ms = (uint32_t)onMs->valueint;
    }
    const cJSON *offMs = cJSON_GetObjectItemCaseSensitive(root, "offMs");
    if (offMs != NULL && cJSON_IsNumber(offMs) && offMs->valueint > 0) {
        command.off_ms = (uint32_t)offMs->valueint;
    }

    cJSON_Delete(root);

    if (message_center_submit(&command) != ESP_OK) {
        return send_json_error(req, "500 Internal Server Error", "failed to queue LED command");
    }

    system_status_set_test_override(true);
    system_status_set_led_source("test_override");
    system_status_set_last_result(0, "ok");

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t rules_test_stop_post_handler(httpd_req_t *req)
{
    system_status_set_test_override(false);
    led_command_t command = {
        .color_r = 0,
        .color_g = 0,
        .color_b = 0,
        .brightness = 0,
        .mode = LED_MODE_OFF,
        .period_ms = 0,
        .on_ms = 0,
        .off_ms = 0,
        .source = CONTROL_SOURCE_HTTP,
    };
    message_center_submit(&command);
    system_status_set_led_source("standby");
    system_status_set_last_result(0, "ok");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t recent_start_post_handler(httpd_req_t *req)
{
    recent_apps_start();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t recent_stop_post_handler(httpd_req_t *req)
{
    recent_apps_stop();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t recent_list_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return send_json_error(req, "500 Internal Server Error", "failed to build list");
    }
    recent_apps_get_list_json(root);
    char *response = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (response == NULL) {
        return send_json_error(req, "500 Internal Server Error", "failed to build list");
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    free(response);
    return ret;
}

static esp_err_t recent_clear_post_handler(httpd_req_t *req)
{
    recent_apps_clear();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t led_post_handler(httpd_req_t *req)
{
    char body[256];
    int remaining = req->content_len;
    int offset = 0;

    if (remaining <= 0 || remaining >= (int)sizeof(body)) {
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
    command.source = CONTROL_SOURCE_HTTP;
    command.brightness = 100;
    command.period_ms = 2000;
    command.on_ms = 500;
    command.off_ms = 500;

    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(root, "mode");
    if (!cJSON_IsString(mode) || !parse_led_mode(mode->valuestring, &command.mode)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid mode");
    }

    if (command.mode != LED_MODE_OFF) {
        const cJSON *color = cJSON_GetObjectItemCaseSensitive(root, "color");
        if (!cJSON_IsString(color) ||
            !parse_hex_color(color->valuestring, &command.color_r, &command.color_g,
                             &command.color_b)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid color format");
        }
    }

    const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(root, "brightness");
    if (brightness != NULL && cJSON_IsNumber(brightness)) {
        int b = brightness->valueint;
        command.brightness = (uint8_t)((b < 0) ? 0 : (b > 100) ? 100 : b);
    }

    const cJSON *period_ms = cJSON_GetObjectItemCaseSensitive(root, "period_ms");
    if (period_ms != NULL && cJSON_IsNumber(period_ms) && period_ms->valueint > 0) {
        command.period_ms = (uint32_t)period_ms->valueint;
    }
    const cJSON *on_ms = cJSON_GetObjectItemCaseSensitive(root, "on_ms");
    if (on_ms != NULL && cJSON_IsNumber(on_ms) && on_ms->valueint > 0) {
        command.on_ms = (uint32_t)on_ms->valueint;
    }
    const cJSON *off_ms = cJSON_GetObjectItemCaseSensitive(root, "off_ms");
    if (off_ms != NULL && cJSON_IsNumber(off_ms) && off_ms->valueint > 0) {
        command.off_ms = (uint32_t)off_ms->valueint;
    }

    cJSON_Delete(root);

    if (message_center_submit(&command) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue LED command");
        system_status_set_last_result(3001, "led queue failed");
        return send_json_error(req, "500 Internal Server Error", "failed to queue LED command");
    }

    system_status_set_last_result(0, "ok");

    char response[128];
    httpd_resp_set_type(req, "application/json");
    snprintf(response, sizeof(response),
             "{\"ok\":true,\"color\":\"#%02X%02X%02X\",\"mode\":\"%s\",\"brightness\":%u}",
             command.color_r, command.color_g, command.color_b,
             notification_rules_mode_to_string(command.mode), command.brightness);
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t http_server_app_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 14;
    config.stack_size = 16384;

    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "start server failed");

    const httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t config_get_uri = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = config_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t config_put_uri = {
        .uri = "/api/config",
        .method = HTTP_PUT,
        .handler = config_put_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t rules_test_uri = {
        .uri = "/api/rules/test",
        .method = HTTP_POST,
        .handler = rules_test_post_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t rules_test_stop_uri = {
        .uri = "/api/rules/test/stop",
        .method = HTTP_POST,
        .handler = rules_test_stop_post_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t recent_start_uri = {
        .uri = "/api/recent/start",
        .method = HTTP_POST,
        .handler = recent_start_post_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t recent_stop_uri = {
        .uri = "/api/recent/stop",
        .method = HTTP_POST,
        .handler = recent_stop_post_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t recent_list_uri = {
        .uri = "/api/recent/list",
        .method = HTTP_GET,
        .handler = recent_list_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t recent_clear_uri = {
        .uri = "/api/recent/clear",
        .method = HTTP_POST,
        .handler = recent_clear_post_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t led_uri = {
        .uri = "/api/led",
        .method = HTTP_POST,
        .handler = led_post_handler,
        .user_ctx = NULL,
    };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &index_uri), TAG, "register / failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &status_uri), TAG,
                        "register /api/status failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &config_get_uri), TAG,
                        "register GET /api/config failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &config_put_uri), TAG,
                        "register PUT /api/config failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &rules_test_uri), TAG,
                        "register /api/rules/test failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &rules_test_stop_uri), TAG,
                        "register /api/rules/test/stop failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &recent_start_uri), TAG,
                        "register /api/recent/start failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &recent_stop_uri), TAG,
                        "register /api/recent/stop failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &recent_list_uri), TAG,
                        "register /api/recent/list failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &recent_clear_uri), TAG,
                        "register /api/recent/clear failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &led_uri), TAG,
                        "register /api/led failed");

    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}
