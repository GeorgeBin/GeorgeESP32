# 阶段 1：ESP32 BLE LED Service 架构文档

> 适用目标：在现有 `HTTP Server + LED 控制 + TFT 显示` 基础上，新增 BLE 控制能力。  
> 第一阶段不开发 Android App，优先使用 nRF Connect 等 BLE 调试工具验证 ESP32 端 GATT Service 是否可用。

---

## 1. 阶段目标

本阶段目标是完成一个最小可用闭环：

```text
nRF Connect / 其他 BLE 调试工具
        ↓
连接 ESP32 BLE GATT Server
        ↓
写入 LED 控制命令
        ↓
ESP32 解析命令
        ↓
复用现有 LED 控制器
        ↓
TFT 显示 BLE 状态与 LED 状态
```

本阶段完成后，产品能力从：

```text
通过 HTTP 控制 LED
```

扩展为：

```text
通过 BLE 近距离控制 LED
```

---

## 2. 不在本阶段实现的内容

本阶段不实现以下内容：

```text
1. Android App 正式页面
2. iOS App
3. BLE 配网
4. 用户账号体系
5. 云端远程控制
6. OTA
7. 完整安全认证
8. 低功耗深度优化
```

本阶段只关注：

```text
ESP32 BLE GATT Server 是否能稳定接收控制命令
```

---

## 3. 角色模型

BLE 角色定义如下：

```text
ESP32：
  BLE Peripheral
  GATT Server

手机 / 调试工具：
  BLE Central
  GATT Client
```

控制方向：

```text
手机 / 调试工具 → ESP32：写入 LED 命令
ESP32 → 手机 / 调试工具：Notify 当前 LED 状态
```

---

## 4. 推荐技术选型

ESP32 端建议使用：

```text
ESP-IDF
NimBLE
GATT Server
cJSON 或自定义轻量 JSON 解析
FreeRTOS Task / Event Queue
```

当前阶段协议建议使用 JSON，后续产品化时再考虑二进制协议。

原因：

```text
1. nRF Connect 可以直接手动写入字符串
2. 调试日志可读性高
3. TFT 可直接显示原始命令或解析后状态
4. Android / iOS 后续构造成本低
```

---

## 5. 工程目录建议

建议在现有工程基础上增加以下目录：

```text
main/
├── app_main.c
├── app_state/
│   ├── app_state.c
│   └── app_state.h
├── command/
│   ├── led_command.c
│   ├── led_command.h
│   ├── command_dispatcher.c
│   └── command_dispatcher.h
├── led/
│   ├── led_controller.c
│   └── led_controller.h
├── http/
│   ├── http_server.c
│   └── http_server.h
├── ble/
│   ├── ble_manager.c
│   ├── ble_manager.h
│   ├── ble_led_service.c
│   └── ble_led_service.h
├── display/
│   ├── tft_display.c
│   └── tft_display.h
└── storage/
    ├── nvs_storage.c
    └── nvs_storage.h
```

如果当前工程还比较简单，也可以先合并目录，但模块边界应保持清晰。

---

## 6. 核心架构

### 6.1 总体链路

```text
HTTP Server
    ↓
HTTP 参数解析
    ↓
LedCommand
    ↓
Command Dispatcher
    ↓
LED Controller
    ↓
AppState
    ↓
TFT Display


BLE GATT Server
    ↓
BLE 写入回调
    ↓
JSON 解析
    ↓
LedCommand
    ↓
Command Dispatcher
    ↓
LED Controller
    ↓
AppState
    ↓
TFT Display
```

关键原则：

```text
HTTP 和 BLE 都不能直接控制 LED。
二者都必须转换为统一的 LedCommand，再交给 Command Dispatcher。
```

---

## 7. 数据模型设计

### 7.1 LED 模式

```c
typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_SOLID,
    LED_MODE_BREATH,
    LED_MODE_BLINK
} led_mode_t;
```

### 7.2 控制来源

```c
typedef enum {
    CONTROL_SOURCE_NONE = 0,
    CONTROL_SOURCE_HTTP,
    CONTROL_SOURCE_BLE
} control_source_t;
```

### 7.3 LED 控制命令

```c
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t brightness;      // 0 - 100
    led_mode_t mode;

    uint32_t period_ms;      // breath 模式使用
    uint32_t on_ms;          // blink 模式使用
    uint32_t off_ms;         // blink 模式使用
} led_command_t;
```

### 7.4 全局应用状态

```c
typedef struct {
    bool ble_enabled;
    bool ble_connected;
    bool wifi_connected;

    led_command_t current_led;
    control_source_t last_source;

    int64_t last_update_time_ms;
} app_state_t;
```

`app_state_t` 用于：

```text
1. TFT 显示
2. BLE Notify
3. HTTP 状态查询
4. 调试日志
```

---

## 8. BLE GATT Service 设计

### 8.1 LED Control Service

```text
Service UUID:
0000A001-0000-1000-8000-00805F9B34FB
```

### 8.2 Command Characteristic

用于接收 App / 调试工具写入的 LED 命令。

```text
UUID:
0000A002-0000-1000-8000-00805F9B34FB

Properties:
Write
Write Without Response
```

写入内容第一版采用 UTF-8 JSON 字符串。

示例：

```json
{
  "seq": 1,
  "cmd": "set_led",
  "color": "#FF0000",
  "mode": "solid",
  "brightness": 100
}
```

### 8.3 State Characteristic

用于读取当前状态，或在状态变化时 Notify 给手机。

```text
UUID:
0000A003-0000-1000-8000-00805F9B34FB

Properties:
Read
Notify
```

返回示例：

```json
{
  "seq": 1,
  "code": 0,
  "msg": "ok",
  "state": {
    "color": "#FF0000",
    "mode": "solid",
    "brightness": 100,
    "source": "ble"
  }
}
```

### 8.4 Device Info Characteristic

用于读取设备信息。

```text
UUID:
0000A004-0000-1000-8000-00805F9B34FB

Properties:
Read
```

返回示例：

```json
{
  "name": "George_LED_Device",
  "fw": "0.1.0",
  "hw": "ESP-32F",
  "protocol": 1
}
```

---

## 9. BLE 广播设计

### 9.1 设备名称

```text
George_LED_Device
```

后续可扩展为：

```text
George_LED_XXXX
```

其中 `XXXX` 可以取 MAC 地址后四位，方便多设备区分。

### 9.2 广播内容

建议广播内容包含：

```text
1. Device Name
2. LED Service UUID
3. 可选 Manufacturer Data
```

### 9.3 扫描过滤依据

Android / iOS 后续优先使用 Service UUID 过滤，而不是只用设备名称过滤。

---

## 10. JSON 协议定义

### 10.1 通用字段

```text
seq:
  命令序号，由客户端生成。
  ESP32 返回时原样带回。

cmd:
  命令类型。

color:
  RGB 颜色，格式 #RRGGBB。

mode:
  LED 模式。

brightness:
  亮度，范围 0 - 100。
```

### 10.2 支持命令：set_led

常亮：

```json
{
  "seq": 1,
  "cmd": "set_led",
  "color": "#00FF00",
  "mode": "solid",
  "brightness": 80
}
```

呼吸：

```json
{
  "seq": 2,
  "cmd": "set_led",
  "color": "#0000FF",
  "mode": "breath",
  "brightness": 80,
  "period_ms": 2000
}
```

闪烁：

```json
{
  "seq": 3,
  "cmd": "set_led",
  "color": "#FFFF00",
  "mode": "blink",
  "brightness": 100,
  "on_ms": 500,
  "off_ms": 500
}
```

关闭：

```json
{
  "seq": 4,
  "cmd": "set_led",
  "mode": "off"
}
```

### 10.3 支持命令：get_state

```json
{
  "seq": 5,
  "cmd": "get_state"
}
```

返回：

```json
{
  "seq": 5,
  "code": 0,
  "msg": "ok",
  "state": {
    "color": "#FFFF00",
    "mode": "blink",
    "brightness": 100,
    "on_ms": 500,
    "off_ms": 500,
    "source": "ble"
  }
}
```

---

## 11. 错误码

```text
0     成功
1001  JSON 格式错误
1002  缺少 cmd 字段
1003  不支持的 cmd
1004  不支持的 mode
1005  color 格式错误
1006  brightness 超出范围
1007  参数组合非法
2001  未认证，预留
3001  LED 控制失败
9001  内部错误
```

返回格式：

```json
{
  "seq": 1,
  "code": 1004,
  "msg": "unsupported mode"
}
```

---

## 12. FreeRTOS 任务与事件流

建议 BLE 写入回调不要直接执行复杂 LED 逻辑，而是投递事件。

### 12.1 事件类型

```c
typedef enum {
    APP_EVENT_BLE_CONNECTED,
    APP_EVENT_BLE_DISCONNECTED,
    APP_EVENT_LED_COMMAND_RECEIVED,
    APP_EVENT_LED_STATE_CHANGED
} app_event_type_t;
```

### 12.2 事件结构

```c
typedef struct {
    app_event_type_t type;
    control_source_t source;
    led_command_t led_command;
} app_event_t;
```

### 12.3 事件处理流

```text
BLE Write Callback
    ↓
解析 JSON
    ↓
生成 led_command_t
    ↓
投递 APP_EVENT_LED_COMMAND_RECEIVED
    ↓
Command Dispatcher Task
    ↓
led_controller_apply()
    ↓
更新 app_state
    ↓
触发 TFT 刷新
    ↓
BLE Notify 最新状态
```

---

## 13. TFT 显示要求

本阶段 TFT 显示建议简洁：

```text
George LED

BLE: Connected / Disconnected
Wi-Fi: Connected / Disconnected
Source: BLE / HTTP
Color: #FF0000
Mode: Solid / Breath / Blink / Off
Brightness: 80%
```

BLE 写入命令后，TFT 应刷新：

```text
Last CMD: set_led
Result: OK / Error
```

---

## 14. 与现有 HTTP 的关系

HTTP Server 保留，不要删除。

HTTP 控制也应转换成 `led_command_t`，进入统一 `command_dispatcher`。

HTTP 状态查询建议返回同一个 `app_state_t`，避免状态不一致。

示例：

```http
GET /api/state
```

返回：

```json
{
  "ble_connected": true,
  "wifi_connected": true,
  "led": {
    "color": "#FF0000",
    "mode": "solid",
    "brightness": 100
  },
  "last_source": "ble"
}
```

---

## 15. AI 实现任务清单

### 15.1 第一步：抽象 LED Command

```text
1. 新增 led_command_t
2. 新增 led_mode_t
3. 将现有 HTTP 控制逻辑改为生成 led_command_t
4. LED Controller 只接收 led_command_t
```

### 15.2 第二步：新增 AppState

```text
1. 新增 app_state 模块
2. 保存当前 LED 状态
3. 保存 BLE 连接状态
4. 保存最后控制来源
5. 提供 app_state_get_snapshot()
```

### 15.3 第三步：新增 BLE Manager

```text
1. 初始化 NimBLE
2. 设置设备名
3. 注册 GATT Service
4. 启动广播
5. 处理连接 / 断开事件
```

### 15.4 第四步：实现 BLE LED Service

```text
1. 注册 LED Service UUID
2. 注册 Command Characteristic
3. 注册 State Characteristic
4. 注册 Device Info Characteristic
5. 在 Command 写入回调中解析 JSON
6. 执行 LED 命令
7. Notify 状态
```

### 15.5 第五步：TFT 状态刷新

```text
1. BLE 连接状态变化时刷新
2. LED 状态变化时刷新
3. 错误命令时显示错误码
```

---

## 16. 验收标准

### 16.1 基础连接

```text
1. ESP32 上电后开始广播
2. nRF Connect 能扫描到 George_LED_Device
3. nRF Connect 能看到 LED Service
4. nRF Connect 能看到 Command / State / Device Info Characteristic
```

### 16.2 控灯验证

向 Command Characteristic 写入：

```json
{
  "seq": 1,
  "cmd": "set_led",
  "color": "#FF0000",
  "mode": "solid",
  "brightness": 100
}
```

期望：

```text
1. LED 变红
2. TFT 显示 Color: #FF0000
3. TFT 显示 Source: BLE
4. State Characteristic Notify 返回 code = 0
```

### 16.3 模式验证

应分别验证：

```text
1. off
2. solid
3. breath
4. blink
```

### 16.4 错误验证

写入：

```json
{
  "seq": 2,
  "cmd": "set_led",
  "color": "red",
  "mode": "solid"
}
```

期望：

```text
1. LED 状态不改变
2. Notify 返回 code = 1005
3. TFT 显示错误
```

---

## 17. 后续演进

本阶段完成后，进入阶段 2：

```text
使用 BLE 对 ESP32 进行 Wi-Fi 配网。
```

本阶段的 BLE LED Service 应保持独立，不要与后续 BLE Provisioning Service 混在一起。

