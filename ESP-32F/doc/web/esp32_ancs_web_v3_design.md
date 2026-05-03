# ESP-32F ANCS Web 配置页 v3 设计说明

本文档描述 ESP-32F 开发板中用于配置 iPhone ANCS 通知灯效的 WebServer 页面设计。页面定位为：设备进入 Wi-Fi AP 配置模式后，用户通过手机浏览器打开配置页，查看当前状态、维护灯效规则、按需采集最近 ANCS 通知，并将配置保存到 ESP32 本地 NVS/Flash。

## 1. 设计目标

v3 版本的目标是把配置页拆成清晰的三个工作区，避免所有功能堆在一个页面中：

1. **概览与已配置**：只看状态和已有配置，不做复杂编辑。
2. **规则配置**：编辑全局规则、设备阶段灯效、ANCS 应用规则。
3. **最近通知采集**：仅在需要时记录最近收到的 ANCS 通知，用于辅助生成规则。

页面必须满足以下约束：

- 单文件 HTML，不依赖 Vue/React/Bootstrap 等前端框架。
- 适合 ESP-IDF `esp_http_server` 直接托管。
- 所有配置通过 JSON 与固件交互。
- 最近通知采集默认关闭，进入采集页才开启，最多记录 10 条。
- 测试灯效必须是临时覆盖层，不能污染正常 LED 状态机。

## 2. 页面结构

### 2.1 Tab 结构

```text
George Light ANCS 配置 v3
├── 概览与已配置
├── 规则配置
└── 最近通知采集
```

### 2.2 概览与已配置

此页用于查看设备当前状态和已配置规则。

包含：

```text
设备状态
├── BLE 连接状态
├── ANCS 授权状态
├── Wi-Fi/AP 配置模式状态
├── 测试灯效覆盖状态
├── 当前灯效来源
└── 规则数量

全局设置摘要
├── 设备名称
├── 默认亮度
├── 隐私模式
└── 通知移除后的行为

已配置：设备全局规则
├── 开机阶段
├── 未连接时
├── 待机
├── 未匹配呼叫
└── 未匹配消息

已配置：ANCS 应用规则
├── App ID 匹配规则
├── Category 匹配规则
├── 关键字匹配规则
└── LED 灯效动作
```

### 2.3 规则配置

此页用于编辑所有配置。

包含：

```text
全局设置
├── 设备名称
├── 默认亮度
├── 隐私模式
└── 通知移除后行为

设备阶段灯效配置
├── 开机阶段 boot
├── 未连接 disconnected
├── 待机 standby
├── 未匹配呼叫 unmatchedCall
└── 未匹配消息 unmatchedMessage

微信高级规则模板
├── 微信普通消息
├── 微信语音/视频通话
├── 微信 @ 我
├── 微信红包
├── 微信转账
└── 微信兜底通知

ANCS 应用规则编辑器
├── 规则名称
├── 启用状态
├── App Identifier
├── ANCS Category
├── EventType
├── 关键字
├── LED 颜色
├── LED 模式
├── 亮度
├── 持续时间
├── 优先级
└── 重复次数
```

### 2.4 最近通知采集

此页用于辅助获取真实 App ID 和通知分类。

设计原则：

- 用户切换到“最近通知采集”Tab 时，页面调用 `/api/recent/start`。
- 用户离开该 Tab 时，页面调用 `/api/recent/stop`。
- 页面每 3 秒调用 `/api/recent/list` 刷新一次。
- 固件端最多保留 10 条记录。
- 清空按钮调用 `/api/recent/clear`。
- 每条记录可以点击“生成规则”，将 App ID、Category、EventType 填入规则编辑器。

## 3. 推荐后端接口

### 3.1 获取设备状态

```http
GET /api/status
```

推荐响应：

```json
{
  "bleConnected": true,
  "ancsAuthorized": true,
  "configMode": true,
  "testOverrideActive": false,
  "ledSource": "standby"
}
```

字段说明：

| 字段 | 类型 | 说明 |
|---|---:|---|
| `bleConnected` | bool | 是否已连接 iPhone BLE |
| `ancsAuthorized` | bool | 是否已完成 ANCS 授权/订阅 |
| `configMode` | bool | 当前是否处于 Wi-Fi AP 配置模式 |
| `testOverrideActive` | bool | 是否存在测试灯效覆盖层 |
| `ledSource` | string | 当前灯效来源，例如 `standby`、`ancs_matched`、`test_override` |

### 3.2 获取配置

```http
GET /api/config
```

返回完整配置 JSON，格式见第 4 节。

### 3.3 保存配置

```http
PUT /api/config
Content-Type: application/json
```

请求体为完整配置 JSON。

固件端保存建议：

1. 校验 `version`。
2. 校验规则数量上限。
3. 校验字符串长度。
4. 校验颜色、模式、亮度、时间等字段。
5. 保存到 NVS/Flash。
6. 重新加载运行时规则。

### 3.4 测试灯效

```http
POST /api/rules/test
Content-Type: application/json
```

请求：

```json
{
  "color": "#ff3b30",
  "mode": "blink",
  "brightness": 90,
  "durationMs": 3000,
  "repeat": 3,
  "temporary": true
}
```

测试灯效必须进入临时覆盖层：

```text
LED_SOURCE_TEST_OVERRIDE
```

测试灯效不应该修改当前设备状态机，也不应该写入配置。

### 3.5 停止测试灯效

```http
POST /api/rules/test/stop
```

行为：

```text
退出 LED_SOURCE_TEST_OVERRIDE
重新 evaluate_led_state()
恢复当前真实状态应该显示的灯效
```

注意：停止测试不等于关灯。设备可能需要恢复到：

- 未连接灯效
- 待机灯效
- 当前未清除的通知灯效
- 未匹配呼叫/消息灯效

### 3.6 最近通知采集

```http
POST /api/recent/start
POST /api/recent/stop
GET  /api/recent/list
POST /api/recent/clear
```

`GET /api/recent/list` 推荐响应：

```json
{
  "items": [
    {
      "appId": "com.tencent.xin",
      "displayName": "微信",
      "category": "social",
      "eventType": "added",
      "summary": "短摘要",
      "timeMs": 12345678
    }
  ]
}
```

最近通知记录建议只保留轻量字段，最多 10 条。

## 4. 配置 JSON v3

推荐完整结构：

```json
{
  "version": 3,
  "deviceName": "George Light",
  "defaultBrightness": 60,
  "privacyMode": "app_only",
  "clearBehavior": "restore",
  "phaseEffects": {
    "boot": {
      "color": "#1fb7a6",
      "mode": "pulse",
      "brightness": 50,
      "durationMs": 3000,
      "repeat": 1
    },
    "disconnected": {
      "color": "#f0a92e",
      "mode": "breath",
      "brightness": 35,
      "durationMs": 0,
      "repeat": 0
    },
    "standby": {
      "color": "#16343d",
      "mode": "breath",
      "brightness": 8,
      "durationMs": 0,
      "repeat": 0
    },
    "unmatchedCall": {
      "color": "#ff3b30",
      "mode": "blink",
      "brightness": 85,
      "durationMs": 15000,
      "repeat": 8
    },
    "unmatchedMessage": {
      "color": "#1fb7a6",
      "mode": "breath",
      "brightness": 55,
      "durationMs": 8000,
      "repeat": 1
    }
  },
  "rules": [
    {
      "id": "wechat-normal",
      "name": "微信普通消息",
      "enabled": true,
      "priority": 70,
      "match": {
        "appId": "com.tencent.xin",
        "category": "social",
        "eventType": "added",
        "keyword": ""
      },
      "led": {
        "color": "#21c45d",
        "mode": "breath",
        "brightness": 70,
        "durationMs": 8000,
        "repeat": 1
      }
    }
  ]
}
```

## 5. 规则分类

v3 将规则明确分成两类。

### 5.1 设备全局规则

设备全局规则与 ANCS App 无关，由设备状态决定。

```text
boot
启动阶段，通常显示 2~3 秒。

disconnected
未连接 iPhone 或 ANCS 未授权时显示。

standby
已连接并空闲时显示。

unmatchedCall
收到呼叫类通知但没有命中任何 App 规则时显示。

unmatchedMessage
收到普通消息类通知但没有命中任何 App 规则时显示。
```

### 5.2 ANCS 应用规则

ANCS 应用规则用于处理通知。

匹配字段：

```text
appId       App Identifier，例如 com.tencent.xin
category    ANCS Category，例如 social、incoming_call
eventType   added、modified、removed、any
keyword     标题/副标题/正文关键字，可为空
```

执行字段：

```text
color       LED 颜色
mode        solid、breath、blink、pulse、rainbow、off
brightness  1~100
durationMs  持续时间，单位 ms
repeat      重复次数
```

## 6. LED 状态机建议

推荐固件内部将 LED 输出拆成两层：

```text
正常状态机
├── boot
├── disconnected
├── standby
├── ancs_matched
├── ancs_unmatched_call
└── ancs_unmatched_message

测试覆盖层
└── test_override
```

优先级建议：

```text
test_override
> ancs_matched
> ancs_unmatched_call / ancs_unmatched_message
> disconnected
> standby
> off
```

停止测试时，不能简单 `led_off()`，而应该：

```c
clear_test_override();
evaluate_led_state();
apply_current_effect();
```

## 7. ANCS 匹配流程建议

收到 ANCS 通知后：

```text
1. 读取 EventID
   added / modified / removed

2. 读取 CategoryID
   incoming_call / missed_call / social / email / etc.

3. 读取 AppIdentifier
   例如 com.tencent.xin

4. 按优先级排序 rules

5. 对每条 enabled 规则执行匹配
   appId 为空则不限制 App
   category = any 则不限制分类
   eventType = any 则不限制事件类型
   keyword 为空则不限制关键字

6. 命中第一条规则后执行其 LED effect

7. 如果未命中
   呼叫类 Category → unmatchedCall
   其他通知 → unmatchedMessage
```

## 8. 微信高级规则说明

微信的 `com.tencent.xin` 作为 App Identifier 通常可以稳定识别。

但以下类型并不是 ANCS 直接提供的标准字段：

```text
微信语音通话
微信视频通话
@ 自己
红包
转账
```

这些只能通过通知标题/副标题/正文关键字做尽力匹配。因此 v3 页面中将这些模板标记为“实验”。

建议默认提供以下关键字：

```text
微信语音/视频通话：语音通话|视频通话|来电|邀请你通话
微信 @ 我：@我|有人@我|提到了你
微信红包：红包|微信红包|给你发了一个红包
微信转账：转账|收款|向你转账
```

产品策略：

```text
微信普通通知：稳定能力
微信高级细分：增强能力
高级细分失败：走微信兜底规则
微信兜底也未配置：走 unmatchedMessage
```

## 9. 内存与隐私建议

### 9.1 最近通知缓存

最近通知采集只在用户进入对应 Tab 时开启。

固件端建议：

```c
#define RECENT_NOTIF_MAX 10
```

仅记录：

```text
appId
category
eventType
summary 短摘要
timeMs
```

不建议长期保存完整通知正文。

### 9.2 规则数量限制

第一版建议：

```text
ANCS 应用规则最多 32 条
最近通知最多 10 条
单个字符串最长 64 或 128 字节
keyword 最长 128 字节
```

### 9.3 隐私模式

```text
app_only
只使用 App ID 和 Category，不读取正文。

title_only
允许临时读取标题，用于关键字匹配。

full
允许临时读取标题和内容，用于更精细匹配。
```

即便是 `full`，也建议只做内存态匹配，不长期落盘保存正文。

## 10. 推荐文件放置

建议工程内放置：

```text
main/web/index.html
main/web/esp32_ancs_web_v3_design.md
```

或如果项目已有 docs 目录：

```text
docs/1-design/ancs-web-config-v3.md
main/web/index.html
```

## 11. Codex 实现提示

可将本文档和 HTML 一起交给 Codex，要求它完成：

```text
1. 在 ESP-IDF 中托管 index.html
2. 实现 /api/status
3. 实现 /api/config GET/PUT
4. 实现 /api/rules/test
5. 实现 /api/rules/test/stop
6. 实现 /api/recent/start
7. 实现 /api/recent/stop
8. 实现 /api/recent/list
9. 实现 /api/recent/clear
10. 将配置保存到 NVS
11. 将规则匹配接入 ANCS Notification Source / Data Source 处理流程
12. 将测试灯效作为 LED 状态机最高优先级临时覆盖层
```
