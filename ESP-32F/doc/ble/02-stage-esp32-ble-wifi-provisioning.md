# 阶段 2：ESP32 BLE Wi-Fi 配网架构文档

> 适用目标：在 ESP32 端引入 BLE 配网能力，使 Android App 可以通过 BLE 给设备配置 Wi-Fi。  
> 本阶段建议使用 ESP-IDF 官方 Wi-Fi Provisioning Manager，不建议自定义明文协议传输 Wi-Fi 密码。

---

## 1. 阶段目标

本阶段目标是实现：

```text
ESP32 未配网
    ↓
启动 BLE Provisioning Service
    ↓
Android App 通过 BLE 发送 Wi-Fi SSID / Password
    ↓
ESP32 连接 Wi-Fi
    ↓
配网成功
    ↓
关闭 Provisioning Service
    ↓
进入正常运行模式
```

正常运行模式包括：

```text
1. HTTP Server 可用
2. BLE LED Service 可用
3. TFT 显示 Wi-Fi / BLE / LED 状态
```

---

## 2. 设计原则

```text
1. 配网通道使用官方 Wi-Fi Provisioning Manager。
2. LED 控制通道使用自定义 BLE LED Service。
3. 不要用 LED Command Characteristic 明文传 Wi-Fi 密码。
4. 配网完成后关闭 Provisioning Service。
5. 如果后续仍需 BLE 控灯，则保留 BLE 栈与 LED Service。
6. 必须提供重新配网入口。
```

---

## 3. 角色模型

```text
ESP32：
  BLE Peripheral
  GATT Server
  Wi-Fi Station

Android App：
  BLE Central
  GATT Client
  Wi-Fi Provisioning Client
```

通信方向：

```text
Android App → ESP32：
  发送 Wi-Fi SSID / Password
  查询配网状态

ESP32 → Android App：
  返回 Wi-Fi 扫描结果
  返回配网执行状态
```

---

## 4. 阶段边界

### 4.1 本阶段实现

```text
1. 检查设备是否已配网
2. 未配网时启动 BLE 配网
3. 通过 BLE 接收 Wi-Fi 凭据
4. 连接 Wi-Fi
5. 保存 Wi-Fi 配置
6. 配网成功后启动 HTTP Server
7. 配网成功后保留或重启 BLE LED Service
8. TFT 显示配网状态
9. 支持清除 Wi-Fi 配置并重新配网
```

### 4.2 本阶段不实现

```text
1. 云端设备绑定
2. 用户账号体系
3. OTA
4. iOS App
5. 自研配网协议
6. 远程控制
```

---

## 5. 工程目录建议

```text
main/
├── app_main.c
├── app_state/
│   ├── app_state.c
│   └── app_state.h
├── wifi/
│   ├── wifi_manager.c
│   └── wifi_manager.h
├── provisioning/
│   ├── ble_provisioning.c
│   └── ble_provisioning.h
├── ble/
│   ├── ble_manager.c
│   ├── ble_manager.h
│   ├── ble_led_service.c
│   └── ble_led_service.h
├── http/
│   ├── http_server.c
│   └── http_server.h
├── led/
│   ├── led_controller.c
│   └── led_controller.h
├── display/
│   ├── tft_display.c
│   └── tft_display.h
└── storage/
    ├── nvs_storage.c
    └── nvs_storage.h
```

---

## 6. 模块职责

### 6.1 wifi_manager

负责 Wi-Fi 生命周期：

```text
1. 初始化 Wi-Fi Station
2. 判断是否已有 Wi-Fi 配置
3. 发起 Wi-Fi 连接
4. 处理连接成功 / 失败事件
5. 获取 IP 地址
6. 清除 Wi-Fi 配置
7. 触发重新配网
```

对外接口建议：

```c
esp_err_t wifi_manager_init(void);
bool wifi_manager_is_provisioned(void);
esp_err_t wifi_manager_start_sta(void);
esp_err_t wifi_manager_reset_config(void);
bool wifi_manager_is_connected(void);
const char* wifi_manager_get_ip_string(void);
```

### 6.2 ble_provisioning

负责 BLE 配网：

```text
1. 初始化 Wi-Fi Provisioning Manager
2. 设置 BLE 设备名
3. 设置安全参数
4. 启动 provisioning
5. 监听 provisioning 事件
6. 配网成功后停止 provisioning
```

对外接口建议：

```c
esp_err_t ble_provisioning_start(void);
esp_err_t ble_provisioning_stop(void);
bool ble_provisioning_is_running(void);
```

### 6.3 ble_led_service

负责 LED 控制 BLE Service：

```text
1. 注册 LED Control Service
2. 接收 LED 控制命令
3. Notify LED 状态
4. 与 provisioning Service 互不耦合
```

### 6.4 app_state

保存全局状态：

```text
1. 当前设备状态
2. Wi-Fi 状态
3. BLE 配网状态
4. BLE 控灯连接状态
5. LED 当前状态
6. 错误码和错误消息
```

### 6.5 display

只负责显示状态，不直接控制 Wi-Fi / BLE / LED。

---

## 7. 设备状态机

### 7.1 状态定义

```c
typedef enum {
    DEVICE_STATE_BOOTING = 0,
    DEVICE_STATE_WAIT_PROVISIONING,
    DEVICE_STATE_PROVISIONING,
    DEVICE_STATE_WIFI_CONNECTING,
    DEVICE_STATE_WIFI_CONNECTED,
    DEVICE_STATE_WIFI_FAILED,
    DEVICE_STATE_NORMAL_RUNNING,
    DEVICE_STATE_RESETTING_WIFI
} device_state_t;
```

### 7.2 状态流转

```text
BOOTING
  ↓
检查 NVS 是否已有 Wi-Fi 配置

未配网：
  WAIT_PROVISIONING
    ↓
  PROVISIONING
    ↓
  WIFI_CONNECTING
    ↓
  WIFI_CONNECTED
    ↓
  NORMAL_RUNNING

已配网：
  WIFI_CONNECTING
    ↓
  WIFI_CONNECTED
    ↓
  NORMAL_RUNNING

连接失败：
  WIFI_FAILED
    ↓
  WAIT_PROVISIONING 或重试连接

用户长按重置：
  RESETTING_WIFI
    ↓
  WAIT_PROVISIONING
```

---

## 8. 启动流程

### 8.1 app_main 初始化顺序

建议顺序：

```text
1. 初始化 NVS
2. 初始化 TCP/IP 网络栈
3. 初始化默认事件循环
4. 初始化 app_state
5. 初始化 display
6. 初始化 led_controller
7. 初始化 wifi_manager
8. 检查是否已配网
9. 根据配网状态进入不同流程
```

### 8.2 未配网流程

```text
1. app_state 设置为 WAIT_PROVISIONING
2. TFT 显示等待配网
3. 启动 BLE Provisioning
4. App 发送 Wi-Fi 信息
5. ESP32 尝试连接 Wi-Fi
6. 成功后保存配置
7. 停止 Provisioning Service
8. 启动 HTTP Server
9. 启动 BLE LED Service
10. app_state 设置为 NORMAL_RUNNING
```

### 8.3 已配网流程

```text
1. app_state 设置为 WIFI_CONNECTING
2. ESP32 使用已保存配置连接 Wi-Fi
3. 成功后启动 HTTP Server
4. 启动 BLE LED Service
5. app_state 设置为 NORMAL_RUNNING
```

---

## 9. BLE Service 共存策略

本项目最终需要两个 BLE 能力：

```text
1. BLE Provisioning Service
2. BLE LED Control Service
```

建议策略：

```text
未配网时：
  只开启 BLE Provisioning Service
  可选开启简化版 Device Info Service

配网完成后：
  停止 BLE Provisioning Service
  开启 BLE LED Control Service

正常运行时：
  BLE LED Control Service 常驻
  HTTP Server 可用于调试
```

注意：

```text
不要让 Provisioning Service 和 LED Control Service 共享同一个 Characteristic。
```

---

## 10. 安全设计

### 10.1 不推荐方式

不要在自定义 Characteristic 中明文写入：

```json
{
  "cmd": "set_wifi",
  "ssid": "Home-WiFi",
  "password": "12345678"
}
```

这只允许用于极早期内部实验，不允许进入正式工程。

### 10.2 推荐方式

使用 ESP-IDF 官方 provisioning security。

阶段建议：

```text
开发阶段：
  Security 1 + 固定 PoP

产品验证阶段：
  Security 1 + 每台设备独立 PoP

正式产品阶段：
  Security 2 或 Security 1 + 设备唯一 PoP
```

### 10.3 PoP 显示

如果设备有 TFT，建议显示：

```text
Device: George_LED_1234
PoP: 739284
```

Android App 输入或扫码后开始配网。

---

## 11. 重新配网设计

必须提供重新配网入口。

### 11.1 物理按键

推荐优先实现：

```text
长按 BOOT 键 5 秒
  ↓
清除 Wi-Fi 配置
  ↓
停止 HTTP Server
  ↓
重启或进入配网模式
```

### 11.2 HTTP 接口

开发调试阶段可提供：

```http
POST /api/wifi/reset
```

返回：

```json
{
  "code": 0,
  "msg": "wifi config reset, rebooting"
}
```

### 11.3 BLE 管理命令

后续可在 LED Service 或 Device Management Service 中增加：

```json
{
  "seq": 100,
  "cmd": "reset_wifi",
  "auth": "xxxxxx"
}
```

产品阶段该命令必须要求认证。

---

## 12. TFT 显示设计

### 12.1 等待配网

```text
George LED

Mode: Provisioning
BLE: ON
Wi-Fi: Not Configured

Device:
George_LED_1234

PoP:
739284
```

### 12.2 正在连接 Wi-Fi

```text
Connecting Wi-Fi

SSID:
Home-WiFi

Please wait...
```

### 12.3 配网成功

```text
Wi-Fi Connected

IP:
192.168.1.23

HTTP: ON
BLE LED: ON
```

### 12.4 配网失败

```text
Wi-Fi Failed

Reason:
Password Error / Timeout

Long press BOOT
to reset Wi-Fi
```

---

## 13. HTTP Server 启动策略

建议：

```text
未配网：
  HTTP Server 不启动

Wi-Fi 连接中：
  HTTP Server 不启动

Wi-Fi 连接成功：
  HTTP Server 启动

Wi-Fi 断开：
  HTTP Server 保持或停止，取决于当前 IP 是否仍可用
```

如果 HTTP Server 是调试功能，产品阶段可以通过配置关闭。

---

## 14. 配网失败处理

### 14.1 失败原因分类

```text
1. Wi-Fi 密码错误
2. 找不到 SSID
3. 路由器拒绝
4. DHCP 获取 IP 失败
5. 超时
6. 未知错误
```

### 14.2 策略

```text
1. 在 TFT 显示失败状态
2. 允许 App 重新发送 Wi-Fi 信息
3. 连续失败 N 次后回到 WAIT_PROVISIONING
4. 不要清除旧配置，除非用户明确重置
```

---

## 15. AI 实现任务清单

### 15.1 第一步：增加 wifi_manager

```text
1. 初始化 Wi-Fi Station
2. 封装是否已配网判断
3. 封装连接 Wi-Fi
4. 监听 Wi-Fi 事件和 IP 事件
5. 更新 app_state
```

### 15.2 第二步：增加 ble_provisioning

```text
1. 引入 ESP-IDF Wi-Fi Provisioning Manager
2. 配置 BLE scheme
3. 设置设备名
4. 设置 PoP
5. 启动 provisioning
6. 监听 provisioning 事件
```

### 15.3 第三步：调整 app_main

```text
1. 上电检查是否已配网
2. 未配网进入 BLE provisioning
3. 已配网连接 Wi-Fi
4. 连接成功后启动 HTTP Server 和 BLE LED Service
```

### 15.4 第四步：增加重新配网

```text
1. BOOT 键长按检测
2. 清除 Wi-Fi 配置
3. 重启或切换到 WAIT_PROVISIONING
```

### 15.5 第五步：完善 TFT 显示

```text
1. 等待配网
2. 正在配网
3. 正在连接 Wi-Fi
4. 配网成功
5. 配网失败
6. 正常运行
```

---

## 16. 验收标准

### 16.1 未配网启动

```text
1. 清除 NVS 后启动 ESP32
2. TFT 显示等待配网
3. 手机能扫描到 BLE Provisioning 设备
```

### 16.2 配网成功

```text
1. Android App 通过 BLE 发送 Wi-Fi 信息
2. ESP32 连接路由器成功
3. TFT 显示 IP 地址
4. HTTP Server 可以访问
5. BLE LED Service 可以继续使用
```

### 16.3 配网失败

```text
1. 输入错误 Wi-Fi 密码
2. ESP32 不崩溃
3. TFT 显示失败
4. App 可以重新发送密码
```

### 16.4 重新配网

```text
1. 长按 BOOT 键 5 秒
2. ESP32 清除 Wi-Fi 配置
3. 重启或回到等待配网
4. 可以重新通过 BLE 配网
```

---

## 17. 与阶段 1 的关系

阶段 1 的 BLE LED Service 不废弃。

阶段 2 完成后，设备最终拥有：

```text
首次使用：
  BLE Provisioning

日常使用：
  BLE LED Control
  HTTP Debug Control
```

阶段 3 将开始实现 Android 端 BLE LED 控制 App。

