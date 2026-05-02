# 008 BLE ANCS Demo

本目录仅保存 ANCS 演示代码，方便从 `esp32k/main` 中单独查看和复用。它不是完整 ESP-IDF 工程，不包含 `sdkconfig`、根 `CMakeLists.txt` 或 build 产物。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| `ble_ancs_manager.c/.h` | NimBLE 初始化、广播、配对、ANCS 服务发现、通知订阅、重连 |
| `ancs_notification_parser.c/.h` | 解析 ANCS Notification Source 和 Data Source |
| `notification_filter.c/.h` | 基础通知类型识别示例 |
| `notification_rules.c/.h` | Bundle ID 与 LED 规则匹配、通知移除熄灯 |
| `message_center.h` | LED 命令队列接口定义 |
| `system_status.h` | 系统状态接口定义 |

复用到 ESP-IDF 工程时，需要在组件中启用 NimBLE、NVS、`esp_timer`、`json`，并提供 `message_center`、`system_status`、LED 输出等实现。

## iPhone 权限申请注意事项

ANCS 不需要 iPhone 安装 App。ESP32 作为 BLE 外设广播，iPhone 连接并配对后，iOS 才会开放 ANCS 服务。

首次连接时通常应出现两个系统交互：

1. 蓝牙配对提示。
2. 通知访问授权提示，用于允许设备接收 iPhone 通知。

如果只出现配对提示，没有出现通知授权提示，优先检查：

- ESP32 是否进入 `READY` 状态。日志应包含：
  - `[ANCS] service discovered`
  - `[ANCS] notification source subscribed`
  - `[ANCS] data source subscribed`
  - `[ANCS] state: READY`
- 配对后是否成功发现 ANCS Service UUID `7905F431-B5CE-4E99-A40F-4B1E122D00D0`。
- 是否成功写入 Notification Source 和 Data Source 的 CCCD。
- iPhone 是否保存了旧的异常配对记录。必要时在 iPhone 蓝牙设置中忽略 `George_ANCS`，再重新配对。
- ESP32 NVS 中是否保存了旧 bond。遇到 repeat pairing 时，代码会删除旧 peer 并重新配对。

iOS 可能受锁屏、勿扰、通知预览隐私等设置影响，通知标题或正文可能为空。

## 重连注意事项

当前实现保存 NimBLE bond，并在启动或断开后优先尝试已绑定设备：

1. 如果有 bonded peer，先执行 directed advertising。
2. 如果 iPhone 未在短时间内回连，回退到普通 `George_ANCS` 广播。
3. 如果没有 bonded peer，直接普通广播等待 iPhone 连接。

注意：

- iOS 是否立即回连由系统策略决定，ESP32 只能主动提供可回连广播。
- 重启后如果长时间不能进入 `READY`，先看是否回退到了普通广播。
- 如果手机端和 ESP32 端绑定状态不一致，iPhone 可能连接失败或反复配对。处理方式是手机忽略设备，必要时擦除 ESP32 NVS 后重新配对。
- ANCS 通知只有连接、加密、订阅完成后才会发送；仅 BLE connected 不代表可读通知。

## ANCS 消息类型处理

ANCS Notification Source 会先给出轻量事件：

| Event | 处理方式 |
| --- | --- |
| `Added` | 记录 UID，并通过 Control Point 请求 AppIdentifier、Title、Subtitle、Message |
| `Modified` | MVP 中只记录日志，不改变 LED |
| `Removed` | 不请求详情；如果 UID 等于最后一个控制 LED 的通知 UID，则发送 `LED_MODE_OFF` |

当前规则分类：

| 类型 | 判断依据 |
| --- | --- |
| 普通消息 | `CategoryID` 不是 Incoming Call |
| 来电/呼叫 | `CategoryID == 1` |
| 微信消息 | `app_id == com.tencent.xin` 且不是 Incoming Call |
| 微信呼叫 | `app_id == com.tencent.xin` 且 `CategoryID == 1` |
| 系统电话 | 可用空 Bundle ID + Incoming Call 作为兜底规则 |
| 短信 | `app_id == com.apple.MobileSMS` |

`Removed` 事件通常只有 UID，没有 AppIdentifier 或正文，所以移除逻辑必须基于之前记录的 UID。

## 常见 App Bundle ID

| 应用 | Bundle ID |
| --- | --- |
| 微信 | `com.tencent.xin` |
| 企业微信 | `com.tencent.WeWork` |
| QQ | `com.tencent.mqq` |
| 钉钉 | `com.laiwang.DingTalk` |
| 支付宝 | `com.alipay.iphoneclient` |
| 淘宝 | `com.taobao.taobao4iphone` |
| 抖音 | `com.ss.iphone.ugc.Aweme` |
| 小红书 | `com.xingin.discover` |
| 哔哩哔哩 | `tv.danmaku.bilianime` |
| 电话 | `com.apple.mobilephone` |
| 短信 | `com.apple.MobileSMS` |

部分 App 的 Bundle ID 可能随版本或地区变化。实际调试时以 ANCS 返回的 `app_id` 日志为准。

## 默认 LED 规则示例

`notification_rules.c` 内置了常见默认规则：

- 微信呼叫：绿色闪烁。
- 微信消息：绿色常亮。
- 系统呼叫兜底：红色闪烁。
- 短信：蓝色闪烁。
- 企业微信、QQ、钉钉、支付宝、淘宝、抖音、小红书、哔哩哔哩：分别映射到不同颜色或模式。

规则可以扩展为 Web 配置并保存到 NVS。每条规则至少包含：

- 是否启用。
- 名称。
- Bundle ID。
- 类型：`any`、`message`、`incoming_call`。
- 颜色。
- 模式：`off`、`solid`、`breath`、`blink`。
- 亮度和闪烁/呼吸参数。

## 调试日志

建议关注以下日志：

```text
[BLE] host synced; starting advertising
[BLE] advertising started as George_ANCS
[BLE] connected
[ANCS] state: SECURED
[ANCS] service discovered
[ANCS] notification source subscribed
[ANCS] data source subscribed
[ANCS] state: READY
[ANCS] source event: uid=... event=ADDED category=...
[ANCS] detail: app_id=... title=... message=...
active ANCS LED notification uid=...
removed active ANCS notification uid=...; turning LED off
```

