# ANCS → 微信识别 → LED 灯效 MVP 实现文档

> 目标：在 ESP-32F 开发板上，基于 ESP-IDF 实现 iPhone ANCS 通知接收，识别微信通知，并调用已有 LED 控制逻辑展示灯效。
>
> 本文档用于指导 Codex 生成或修改工程代码。

---

## 1. 实现目标

本阶段只打通最小链路：

```text
iPhone 微信通知
→ iOS Notification Center
→ ANCS over BLE
→ ESP32 接收通知
→ 解析 AppIdentifier / Title / Message
→ 判断是否为微信通知
→ 调用已有 LED 灯效接口
```

本阶段不实现：

```text
不实现 iOS App
不实现 Android App
不实现配网
不实现 OTA
不实现复杂配置页面
不实现通知历史存储
不实现多用户绑定
```

---

## 2. 技术路线

使用 ESP-IDF 官方 ANCS 示例作为基础：

```text
esp-idf/examples/bluetooth/bluedroid/ble/ble_ancs
```

开发板：

```text
ESP-32F
Target: esp32
Framework: ESP-IDF
Bluetooth Stack: Bluedroid
BLE Role: GATT Client
ANCS Role: Notification Consumer
```

iPhone 侧不需要安装 App。

ESP32 通过 BLE 与 iPhone 配对后，由 iOS 系统提供 ANCS 服务。

---

## 3. 目录结构建议

建议在 `main/` 下新增或整理以下模块：

```text
main/
├── app_main.c
├── ble_ancs_manager.c
├── ble_ancs_manager.h
├── ancs_notification_parser.c
├── ancs_notification_parser.h
├── notification_filter.c
├── notification_filter.h
├── led_notify_adapter.c
└── led_notify_adapter.h
```

### 3.1 模块职责

| 模块 | 职责 |
|---|---|
| `app_main.c` | 初始化 NVS、BLE、ANCS Manager、LED 模块 |
| `ble_ancs_manager.*` | 管理 BLE 初始化、广播、连接、配对、服务发现、ANCS 订阅、ANCS 数据读取 |
| `ancs_notification_parser.*` | 解析 ANCS 返回的通知属性数据 |
| `notification_filter.*` | 判断通知来源，识别微信、电话、短信、邮件等类型 |
| `led_notify_adapter.*` | 将通知事件转换为已有 LED 控制接口调用 |

---

## 4. ANCS 核心 UUID

ANCS Service UUID：

```text
7905F431-B5CE-4E99-A40F-4B1E122D00D0
```

Notification Source Characteristic：

```text
9FBF120D-6301-42D9-8C58-25E699A21DBD
```

Control Point Characteristic：

```text
69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9
```

Data Source Characteristic：

```text
22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB
```

用途：

```text
Notification Source:
  iOS 通知事件入口，只告诉 ESP32 有通知新增、修改、删除。

Control Point:
  ESP32 向 iPhone 请求某条通知的详细属性。

Data Source:
  iPhone 返回通知详细属性，例如 AppIdentifier、Title、Subtitle、Message。
```

---

## 5. 最小状态机

ANCS 模块应维护如下状态：

```c
typedef enum {
    ANCS_STATE_IDLE = 0,
    ANCS_STATE_BLE_INIT,
    ANCS_STATE_ADVERTISING,
    ANCS_STATE_CONNECTED,
    ANCS_STATE_BONDED,
    ANCS_STATE_SERVICE_DISCOVERED,
    ANCS_STATE_NOTIFICATION_SOURCE_SUBSCRIBED,
    ANCS_STATE_DATA_SOURCE_SUBSCRIBED,
    ANCS_STATE_READY,
    ANCS_STATE_DISCONNECTED,
    ANCS_STATE_ERROR,
} ancs_state_t;
```

状态流：

```text
IDLE
→ BLE_INIT
→ ADVERTISING
→ CONNECTED
→ BONDED
→ SERVICE_DISCOVERED
→ NOTIFICATION_SOURCE_SUBSCRIBED
→ DATA_SOURCE_SUBSCRIBED
→ READY
```

要求：

```text
1. 每次状态变化都打印日志。
2. 只有进入 READY 后才认为 ANCS 可用。
3. 断开连接后应回到 ADVERTISING 或可重新连接状态。
4. 失败时打印明确错误原因。
```

日志格式建议：

```text
[ANCS] state: CONNECTED
[ANCS] state: BONDED
[ANCS] service found
[ANCS] notification source subscribed
[ANCS] data source subscribed
[ANCS] ready
```

---

## 6. 通知事件结构

定义统一通知事件结构，供后续过滤和 LED 层使用。

```c
#define ANCS_APP_ID_MAX_LEN     96
#define ANCS_TITLE_MAX_LEN      128
#define ANCS_SUBTITLE_MAX_LEN   128
#define ANCS_MESSAGE_MAX_LEN    256

typedef enum {
    ANCS_EVENT_ADDED = 0,
    ANCS_EVENT_MODIFIED,
    ANCS_EVENT_REMOVED,
    ANCS_EVENT_UNKNOWN,
} ancs_event_action_t;

typedef struct {
    uint32_t notification_uid;
    ancs_event_action_t action;

    char app_id[ANCS_APP_ID_MAX_LEN];
    char title[ANCS_TITLE_MAX_LEN];
    char subtitle[ANCS_SUBTITLE_MAX_LEN];
    char message[ANCS_MESSAGE_MAX_LEN];

    uint32_t received_time_ms;
} ancs_notification_event_t;
```

说明：

```text
notification_uid:
  ANCS 通知唯一 ID，用于后续请求详情或处理通知移除。

app_id:
  App Bundle ID，例如微信通常为 com.tencent.xin。

message:
  通知正文可能为空，也可能受 iOS 隐私设置影响。
```

---

## 7. ANCS 数据处理流程

### 7.1 收到 Notification Source

当收到 Notification Source 通知后，解析：

```text
EventID
EventFlags
CategoryID
CategoryCount
NotificationUID
```

只对 `EventID = Added` 的通知请求详情。

```text
Added:
  请求 AppIdentifier、Title、Subtitle、Message。

Modified:
  MVP 阶段可以只打印日志，不处理。

Removed:
  MVP 阶段可以只打印日志；后续可用于关闭电话闪烁灯效。
```

### 7.2 写 Control Point 请求详情

请求属性建议：

```text
AppIdentifier
Title，最大 128 字节
Subtitle，最大 128 字节
Message，最大 256 字节
```

伪代码：

```c
ancs_request_notification_attributes(
    notification_uid,
    true,  // app identifier
    128,   // title max length
    128,   // subtitle max length
    256    // message max length
);
```

### 7.3 从 Data Source 解析详情

Data Source 返回后，解析为：

```c
ancs_notification_event_t event;
```

然后交给过滤层：

```c
notification_filter_handle(&event);
```

---

## 8. 微信识别规则

微信 iOS Bundle ID 通常为：

```text
com.tencent.xin
```

判断函数：

```c
bool notification_filter_is_wechat(const ancs_notification_event_t *event);
```

推荐实现：

```c
bool notification_filter_is_wechat(const ancs_notification_event_t *event) {
    if (event == NULL) {
        return false;
    }

    return strcmp(event->app_id, "com.tencent.xin") == 0;
}
```

为了兼容调试阶段，也可以增加宽松判断：

```c
bool notification_filter_is_wechat_loose(const ancs_notification_event_t *event) {
    if (event == NULL) {
        return false;
    }

    if (strcmp(event->app_id, "com.tencent.xin") == 0) {
        return true;
    }

    if (strstr(event->app_id, "tencent") != NULL) {
        return true;
    }

    if (strstr(event->title, "微信") != NULL) {
        return true;
    }

    return false;
}
```

MVP 阶段建议默认使用严格判断：

```text
app_id == com.tencent.xin
```

调试时可以临时启用宽松判断。

---

## 9. 通知类型定义

```c
typedef enum {
    LED_NOTIFY_TYPE_WECHAT = 0,
    LED_NOTIFY_TYPE_PHONE,
    LED_NOTIFY_TYPE_SMS,
    LED_NOTIFY_TYPE_MAIL,
    LED_NOTIFY_TYPE_OTHER,
} led_notify_type_t;
```

过滤映射函数：

```c
led_notify_type_t notification_filter_map_to_led_type(
    const ancs_notification_event_t *event
) {
    if (event == NULL) {
        return LED_NOTIFY_TYPE_OTHER;
    }

    if (strcmp(event->app_id, "com.tencent.xin") == 0) {
        return LED_NOTIFY_TYPE_WECHAT;
    }

    if (strstr(event->app_id, "mobilephone") != NULL ||
        strstr(event->app_id, "MobilePhone") != NULL) {
        return LED_NOTIFY_TYPE_PHONE;
    }

    if (strstr(event->app_id, "MobileSMS") != NULL ||
        strstr(event->app_id, "sms") != NULL) {
        return LED_NOTIFY_TYPE_SMS;
    }

    if (strstr(event->app_id, "mobilemail") != NULL ||
        strstr(event->app_id, "MobileMail") != NULL) {
        return LED_NOTIFY_TYPE_MAIL;
    }

    return LED_NOTIFY_TYPE_OTHER;
}
```

---

## 10. LED 适配层

由于工程中已经有 LED 控制实现，本阶段不要重写底层 LED 驱动。

只新增一个适配层：

```c
void led_notify_adapter_on_notification(
    const ancs_notification_event_t *event,
    led_notify_type_t type
);
```

示例行为：

```c
void led_notify_adapter_on_notification(
    const ancs_notification_event_t *event,
    led_notify_type_t type) {
    switch (type) {
        case LED_NOTIFY_TYPE_WECHAT:
            // 调用已有 LED 控制：绿色常亮 5 秒
            led_show_green_for_ms(5000);
            break;

        case LED_NOTIFY_TYPE_PHONE:
            // 调用已有 LED 控制：红色快闪
            led_blink_red_fast_for_ms(10000);
            break;

        case LED_NOTIFY_TYPE_SMS:
            // 调用已有 LED 控制：蓝色闪烁
            led_blink_blue_times(3);
            break;

        case LED_NOTIFY_TYPE_MAIL:
            // 调用已有 LED 控制：黄色呼吸
            led_breathe_yellow_for_ms(5000);
            break;

        case LED_NOTIFY_TYPE_OTHER:
        default:
            // MVP 阶段：其他通知可以忽略，也可以白色短闪
            // 建议默认忽略，避免通知太多影响体验。
            break;
    }
}
```

如果当前工程已有 LED API 名称不同，Codex 应只修改 `led_notify_adapter.c`，不要侵入 ANCS 层。

---

## 11. 主链路伪代码

```c
void on_ancs_notification_source(const uint8_t *data, size_t len) {
    ancs_source_event_t source_event;

    if (!ancs_parse_notification_source(data, len, &source_event)) {
        ESP_LOGW(TAG, "parse notification source failed");
        return;
    }

    ESP_LOGI(TAG, "notification uid=%lu event=%d category=%d",
             source_event.notification_uid,
             source_event.event_id,
             source_event.category_id);

    if (source_event.event_id == ANCS_EVENT_ADDED) {
        ancs_request_notification_attributes(source_event.notification_uid);
    }
}

void on_ancs_data_source(const uint8_t *data, size_t len) {
    ancs_notification_event_t event;

    if (!ancs_parse_notification_attributes(data, len, &event)) {
        ESP_LOGW(TAG, "parse notification attributes failed");
        return;
    }

    ESP_LOGI(TAG, "app_id=%s title=%s subtitle=%s message=%s",
             event.app_id,
             event.title,
             event.subtitle,
             event.message);

    led_notify_type_t type = notification_filter_map_to_led_type(&event);

    if (type == LED_NOTIFY_TYPE_WECHAT) {
        led_notify_adapter_on_notification(&event, type);
    }
}
```

MVP 阶段建议只响应微信：

```text
只有 LED_NOTIFY_TYPE_WECHAT 才触发 LED。
其他通知只打印日志。
```

---

## 12. 日志要求

必须打印以下关键日志：

```text
[BLE] init ok
[BLE] advertising started
[BLE] connected
[BLE] bonded
[ANCS] service discovered
[ANCS] notification source subscribed
[ANCS] data source subscribed
[ANCS] ready
[ANCS] source event: uid=xxx event=added category=xxx
[ANCS] detail: app_id=xxx title=xxx subtitle=xxx message=xxx
[FILTER] type=wechat
[LED] show wechat effect
```

调试失败时至少能判断卡在哪一步：

```text
是否连上 iPhone
是否完成配对
是否发现 ANCS 服务
是否订阅成功
是否收到 Notification Source
是否收到 Data Source
是否解析出 app_id
是否识别为微信
是否调用 LED
```

---

## 13. iPhone 测试步骤

### 13.1 首次配对

```text
1. 烧录固件到 ESP-32F。
2. 打开串口监视器。
3. iPhone 打开 设置 → 蓝牙。
4. 找到 ESP32 设备名。
5. 点击连接 / 配对。
6. 如果 iOS 弹出通知共享授权，选择允许。
7. 串口应显示 ANCS ready。
```

### 13.2 微信通知测试

```text
1. 确认 iPhone 微信通知权限已开启。
2. 确认 iPhone 系统通知预览没有完全隐藏。
3. 使用另一台设备给该微信发送消息。
4. 观察 ESP32 串口日志。
5. 观察 LED 是否触发微信灯效。
```

### 13.3 失败排查

如果没有通知：

```text
1. 删除 iPhone 蓝牙里该 ESP32 设备。
2. 清除 ESP32 NVS 中的 bonding 信息。
3. 重新烧录或重启 ESP32。
4. 重新配对。
5. 确认 iOS 允许通知共享。
```

---

## 14. 关键注意事项

### 14.1 不能承诺读取完整微信内容

ANCS 只能读取系统通知内容。实际内容受以下因素影响：

```text
iOS 通知预览设置
微信通知设置
锁屏隐私设置
Focus / 勿扰模式
iOS 版本差异
```

所以产品语义应该是：

```text
根据 iPhone 系统通知触发提醒灯效。
```

不要写成：

```text
读取微信聊天消息。
```

### 14.2 MVP 阶段只做微信

虽然代码可以保留电话、短信、邮件类型，但本阶段验收只看：

```text
微信通知 → ESP32 识别 → LED 灯效
```

### 14.3 ANCS 层不要依赖 LED 底层

ANCS 层只产出通知事件。

LED 相关调用必须集中在 `led_notify_adapter.c`。

避免出现：

```text
ble_ancs_manager.c 直接调用 led_strip_set_pixel
```

应保持：

```text
ble_ancs_manager
→ ancs_notification_parser
→ notification_filter
→ led_notify_adapter
→ existing led driver
```

---

## 15. MVP 验收标准

### 15.1 必须满足

```text
1. ESP-32F 能被 iPhone 发现并连接。
2. ESP-32F 能完成 BLE 配对。
3. ESP-32F 能发现 ANCS Service。
4. ESP-32F 能订阅 Notification Source。
5. ESP-32F 能订阅 Data Source。
6. 微信通知到达时，串口能打印 app_id。
7. app_id 为 com.tencent.xin 时，能识别为微信。
8. 微信通知触发已有 LED 灯效。
```

### 15.2 建议满足

```text
1. 断开后可重新连接。
2. 非微信通知只打印日志，不触发 LED。
3. 微信通知正文为空时，仍然可以根据 app_id 触发 LED。
4. 日志能清楚显示每个阶段状态。
```

---

## 16. 推荐实现顺序

Codex 应按以下顺序实现，避免一次性改太多：

```text
Step 1: 跑通 ESP-IDF ble_ancs 示例。
Step 2: 整理 ble_ancs_manager 模块。
Step 3: 确认能打印 Notification Source 原始事件。
Step 4: 实现 Control Point 请求通知详情。
Step 5: 实现 Data Source 属性解析。
Step 6: 打印 app_id / title / subtitle / message。
Step 7: 实现 notification_filter_is_wechat。
Step 8: 实现 led_notify_adapter。
Step 9: 微信通知触发 LED。
Step 10: 清理日志和模块边界。
```

---

## 17. 最小完成效果

最终串口期望看到类似日志：

```text
[BLE] connected
[BLE] bonded
[ANCS] service discovered
[ANCS] notification source subscribed
[ANCS] data source subscribed
[ANCS] ready
[ANCS] source event: uid=123456 event=added category=social
[ANCS] detail: app_id=com.tencent.xin title=微信 message=张三: 你好
[FILTER] type=wechat
[LED] show wechat effect
```

LED 期望效果：

```text
微信消息到达后，LED 绿色亮起 5 秒。
```

如果当前 LED 层已有其他灯效命名，则以当前工程实际 API 为准，只需保证微信通知能触发一个明确、稳定、可观察的灯效。
