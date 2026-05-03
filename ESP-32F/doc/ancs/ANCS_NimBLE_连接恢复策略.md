# ANCS NimBLE 连接恢复策略

> 适用场景：ESP32 使用 NimBLE 实现 iPhone ANCS 客户端。  
> 本文档只描述 ANCS / BLE 连接恢复逻辑，不包含 LED、灯效、业务路由等上层业务逻辑。  
> 目标：指导 Codex 在工程中实现稳定的 ANCS 首次连接、距离断连恢复、iPhone 蓝牙关闭再打开后的自动恢复。

---

## 1. 核心结论

ANCS 连接策略必须遵守以下原则：

```text
只要 BLE 连接断开，就认为当前 ANCS session 已结束。

断开后：
    清理当前 ANCS session 的所有临时状态
    保留 bonding / pairing 信息
    重新进入可连接广播

重连后：
    重新完成 BLE 安全认证 / 加密
    重新发现 ANCS Service
    重新发现 ANCS Characteristics
    重新写 CCCD，订阅 Data Source 和 Notification Source
    重新进入 ANCS_READY
```

不要尝试做“ANCS 断点续连”。

ANCS 不是完整同步服务，不保证跨 session 保存通知状态。断线前的 `NotificationUID`、正在等待的 Control Point 请求、Data Source 分片缓存，都只能视为当前连接内有效。

---

## 2. BLE / ANCS 角色说明

在 ESP32 实现 iPhone ANCS 时，角色比较特殊：

```text
ESP32：
    BLE Peripheral
    对外广播
    等待 iPhone 主动连接

ESP32：
    GATT Client
    连接后访问 iPhone 上的 ANCS Service

 iPhone：
    BLE Central
    主动连接 ESP32

 iPhone：
    GATT Server
    提供 ANCS Service
```

因此，大多数实现中，ESP32 断开后不是主动连接 iPhone，而是：

```text
ESP32 重新进入 connectable advertising
等待 iPhone 回连
```

如果工程采用的是 ESP32 主动扫描并连接 iPhone 的特殊方案，则可以将本文中的“重新广播”替换为“重新扫描并连接”。但 ANCS session 清理、重连后重新 discovery、重新 subscribe 的原则不变。

---

## 3. ANCS Session 生命周期

ANCS session 的生命周期可以理解为：

```text
开始：
    ESP32 订阅 Notification Source

结束：
    ESP32 取消订阅 Notification Source
    或 BLE 连接断开
```

一旦 BLE 连接断开，当前 ANCS session 立即失效。

断开后不能继续使用：

```text
- NotificationUID
- Control Point 未完成请求
- Data Source 分片缓存
- 上一次发现到的 ANCS handle
- 上一次 CCCD 订阅状态
- 当前 ANCS_READY 状态
```

可以继续保留：

```text
- bonding / pairing key
- iPhone 绑定记录
- NimBLE NVS 中保存的安全信息
- 本地配置参数
```

---

## 4. 距离过远导致断开时的处理逻辑

当 ESP32 和 iPhone 距离过远时，BLE 链路通常会因为 supervision timeout 或连接异常断开。

NimBLE 会收到：

```c
BLE_GAP_EVENT_DISCONNECT
```

此时应该执行：

```text
BLE disconnect
  ↓
标记 conn_handle 无效
  ↓
标记 ANCS 不可用
  ↓
停止当前 GATT / ANCS 操作
  ↓
清理 ANCS session 临时状态
  ↓
保留 bonding 信息
  ↓
重新进入 connectable advertising
```

### 4.1 必须清理的状态

断开时必须清理：

```text
- conn_handle
- ancs_ready
- ancs_state
- 当前正在等待的 Control Point 请求
- 当前 Data Source 重组缓存
- 当前 NotificationUID 缓存
- 当前 GATT 操作状态
- 当前 ANCS 订阅状态
```

### 4.2 建议清理的状态

第一版实现中，建议也清理：

```text
- ANCS Service handle
- Notification Source characteristic handle
- Control Point characteristic handle
- Data Source characteristic handle
- 各 characteristic 对应的 CCCD handle
```

虽然部分情况下 handle 可能不变，但不应依赖这一点。重连后重新 discovery 最稳。

### 4.3 不应该清理的状态

断开时不要清理：

```text
- bonding key
- pairing 信息
- NimBLE NVS 中的 peer 信息
```

除非明确判断配对信息损坏，否则不要调用删除 bond 的逻辑。

错误示例：

```c
// 普通断线时不要这样做
ble_store_util_delete_peer(&peer_addr);
```

正确策略：

```text
普通距离断开：保留 bond
手机蓝牙关闭：保留 bond
ESP32 重启：依靠 NVS 恢复 bond
配对信息损坏：才执行清 bond + 重新配对
```

---

## 5. iPhone 蓝牙关闭再打开时的处理逻辑

用户在 iPhone 上手动关闭蓝牙开关时，ESP32 侧最终会收到断开事件，或者连接超时事件。

处理流程与距离断开一致：

```text
iPhone 蓝牙关闭
  ↓
ESP32 收到 BLE disconnect / supervision timeout
  ↓
清理 ANCS session
  ↓
保留 bonding 信息
  ↓
ESP32 重新进入 connectable advertising
  ↓
iPhone 蓝牙重新打开
  ↓
iOS 根据系统策略尝试回连
  ↓
BLE connected
  ↓
重新 security / encryption
  ↓
重新 discovery ANCS
  ↓
重新 subscribe Data Source
  ↓
重新 subscribe Notification Source
  ↓
ANCS_READY
```

注意：iPhone 蓝牙重新打开后，ESP32 不能强迫 iPhone 立即连接。ESP32 侧需要确保自己始终处于可被连接的状态。

ESP32 需要保证：

```text
- 断开后确实重新开始 advertising
- advertising 是 connectable advertising
- 设备 identity 没有变化
- bonding 信息没有丢失
- NimBLE NVS 持久化已开启
- 重连后重新执行 ANCS 初始化流程
```

---

## 6. 重连后的 ANCS 初始化流程

每一次 BLE 重连后，都应该完整执行以下流程：

```text
BLE connected
  ↓
启动或等待 BLE security / encryption
  ↓
可选：Exchange MTU
  ↓
Discover ANCS Service
  ↓
Discover Notification Source characteristic
  ↓
Discover Control Point characteristic
  ↓
Discover Data Source characteristic
  ↓
Discover CCCD descriptors
  ↓
Subscribe Data Source
  ↓
Subscribe Notification Source
  ↓
ANCS_READY
```

推荐订阅顺序：

```text
1. 先订阅 Data Source
2. 再订阅 Notification Source
```

原因：`Notification Source` 一旦订阅成功，iPhone 可能马上推送通知事件。此时如果 `Data Source` 还没有订阅好，后续请求通知详情时可能无法正常接收响应。

---

## 7. 推荐状态机

建议将 BLE 状态和 ANCS 状态分开管理。

### 7.1 BLE 状态

```c
typedef enum {
    BLE_STATE_IDLE = 0,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
    BLE_STATE_ENCRYPTED,
    BLE_STATE_DISCONNECTED,
} ble_state_t;
```

### 7.2 ANCS 状态

```c
typedef enum {
    ANCS_STATE_IDLE = 0,
    ANCS_STATE_WAIT_SECURITY,
    ANCS_STATE_DISCOVER_SERVICE,
    ANCS_STATE_DISCOVER_CHARS,
    ANCS_STATE_DISCOVER_DESCRIPTORS,
    ANCS_STATE_SUBSCRIBE_DATA_SOURCE,
    ANCS_STATE_SUBSCRIBE_NOTIFICATION_SOURCE,
    ANCS_STATE_READY,
    ANCS_STATE_CLOSED,
    ANCS_STATE_ERROR,
} ancs_state_t;
```

### 7.3 状态转换规则

首次启动：

```text
BLE_STATE_IDLE
  ↓
BLE_STATE_ADVERTISING
  ↓
BLE_STATE_CONNECTED
  ↓
BLE_STATE_ENCRYPTED
  ↓
ANCS_STATE_DISCOVER_SERVICE
  ↓
ANCS_STATE_DISCOVER_CHARS
  ↓
ANCS_STATE_DISCOVER_DESCRIPTORS
  ↓
ANCS_STATE_SUBSCRIBE_DATA_SOURCE
  ↓
ANCS_STATE_SUBSCRIBE_NOTIFICATION_SOURCE
  ↓
ANCS_STATE_READY
```

断开连接：

```text
任意 BLE / ANCS 状态
  ↓
BLE_GAP_EVENT_DISCONNECT
  ↓
BLE_STATE_DISCONNECTED
  ↓
ANCS_STATE_CLOSED
  ↓
清理 ANCS session
  ↓
BLE_STATE_ADVERTISING
```

重连：

```text
BLE_STATE_ADVERTISING
  ↓
BLE_STATE_CONNECTED
  ↓
ANCS_STATE_WAIT_SECURITY
  ↓
后续重新执行完整 ANCS 初始化流程
```

---

## 8. NimBLE GAP 事件处理建议

### 8.1 CONNECT 事件

```c
static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            conn_handle = event->connect.conn_handle;

            ble_state = BLE_STATE_CONNECTED;
            ancs_state = ANCS_STATE_WAIT_SECURITY;
            ancs_ready = false;

            ancs_clear_session_state();

            // ANCS characteristic 通常需要加密 / 授权访问
            ble_gap_security_initiate(conn_handle);
        } else {
            // 连接失败，继续广播
            conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ble_state = BLE_STATE_ADVERTISING;
            start_advertising();
        }
        return 0;

    default:
        return 0;
    }
}
```

### 8.2 DISCONNECT 事件

```c
static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected, reason=%d", event->disconnect.reason);

        conn_handle = BLE_HS_CONN_HANDLE_NONE;

        ble_state = BLE_STATE_DISCONNECTED;
        ancs_state = ANCS_STATE_CLOSED;
        ancs_ready = false;

        ancs_clear_pending_request();
        ancs_clear_data_source_buffer();
        ancs_clear_uid_cache();
        ancs_clear_discovered_handles();
        ancs_clear_subscribe_state();

        // 注意：普通断线不要删除 bonding 信息
        // 不要在这里调用 ble_store_util_delete_peer()

        start_advertising();
        return 0;

    default:
        return 0;
    }
}
```

### 8.3 ENCRYPTION / SECURITY 完成事件

具体事件名称需要按当前 NimBLE 版本和工程封装确认，通常可以在安全状态变化后继续 ANCS discovery。

伪代码：

```c
static void on_ble_security_complete(uint16_t conn_handle, bool success)
{
    if (!success) {
        ancs_state = ANCS_STATE_ERROR;
        return;
    }

    ble_state = BLE_STATE_ENCRYPTED;
    ancs_state = ANCS_STATE_DISCOVER_SERVICE;

    ancs_start_service_discovery(conn_handle);
}
```

---

## 9. ANCS Discovery 与 Subscribe 伪代码

### 9.1 开始发现 ANCS Service

```c
static void ancs_start_service_discovery(uint16_t conn_handle)
{
    ancs_state = ANCS_STATE_DISCOVER_SERVICE;

    ble_gattc_disc_svc_by_uuid(
        conn_handle,
        &ancs_service_uuid.u,
        ancs_on_service_discovered,
        NULL
    );
}
```

### 9.2 Service 发现完成后发现 Characteristics

```c
static int ancs_on_service_discovered(
    uint16_t conn_handle,
    const struct ble_gatt_error *error,
    const struct ble_gatt_svc *service,
    void *arg
) {
    if (error->status != 0 || service == NULL) {
        ancs_state = ANCS_STATE_ERROR;
        return 0;
    }

    ancs_svc_start_handle = service->start_handle;
    ancs_svc_end_handle = service->end_handle;

    ancs_state = ANCS_STATE_DISCOVER_CHARS;

    ble_gattc_disc_all_chrs(
        conn_handle,
        ancs_svc_start_handle,
        ancs_svc_end_handle,
        ancs_on_chr_discovered,
        NULL
    );

    return 0;
}
```

### 9.3 发现 Characteristics 后订阅

```c
static void ancs_on_discovery_complete(void)
{
    if (!ancs_has_required_handles()) {
        ancs_state = ANCS_STATE_ERROR;
        return;
    }

    ancs_state = ANCS_STATE_SUBSCRIBE_DATA_SOURCE;
    ancs_subscribe_data_source();
}
```

### 9.4 先订阅 Data Source

```c
static void ancs_on_data_source_subscribed(bool success)
{
    if (!success) {
        ancs_state = ANCS_STATE_ERROR;
        return;
    }

    ancs_state = ANCS_STATE_SUBSCRIBE_NOTIFICATION_SOURCE;
    ancs_subscribe_notification_source();
}
```

### 9.5 再订阅 Notification Source

```c
static void ancs_on_notification_source_subscribed(bool success)
{
    if (!success) {
        ancs_state = ANCS_STATE_ERROR;
        return;
    }

    ancs_state = ANCS_STATE_READY;
    ancs_ready = true;
}
```

---

## 10. ANCS Session 清理函数建议

工程中建议提供统一清理函数，避免断线处理散落在多个地方。

```c
void ancs_clear_session_state(void)
{
    ancs_ready = false;

    ancs_clear_pending_request();
    ancs_clear_data_source_buffer();
    ancs_clear_uid_cache();
    ancs_clear_subscribe_state();

    // 是否清 handle 取决于工程策略。
    // 第一版建议清理，重连后重新 discovery。
    ancs_clear_discovered_handles();
}
```

### 10.1 清理 pending request

```c
void ancs_clear_pending_request(void)
{
    pending_request.active = false;
    pending_request.command_id = 0;
    pending_request.notification_uid = 0;
    pending_request.timeout_ms = 0;
}
```

### 10.2 清理 Data Source 分片缓存

```c
void ancs_clear_data_source_buffer(void)
{
    data_source_buffer.len = 0;
    data_source_buffer.expected_len = 0;
    data_source_buffer.active = false;
}
```

### 10.3 清理 NotificationUID 缓存

```c
void ancs_clear_uid_cache(void)
{
    uid_cache_clear();
}
```

### 10.4 清理 discovered handles

```c
void ancs_clear_discovered_handles(void)
{
    ancs_svc_start_handle = 0;
    ancs_svc_end_handle = 0;

    notification_source_handle = 0;
    notification_source_cccd_handle = 0;

    control_point_handle = 0;

    data_source_handle = 0;
    data_source_cccd_handle = 0;
}
```

---

## 11. Advertising 策略

断开后必须重新开始 advertising。

推荐最小策略：

```text
断开后立即启动 connectable advertising
一直保持可连接
直到 iPhone 回连
```

如果是接电设备，可以使用相对积极的广播策略：

```text
断开后 0 ~ 30 秒：快速广播
30 秒后：中速广播
数分钟后：慢速广播
用户触发按键或重新上电：再次进入快速广播
```

第一版工程可以先简单实现：

```text
断开后一直使用固定 advertising 参数
保证稳定回连优先
后续再优化功耗
```

伪代码：

```c
static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params = {0};

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    int rc = ble_gap_adv_start(
        own_addr_type,
        NULL,
        BLE_HS_FOREVER,
        &adv_params,
        gap_event,
        NULL
    );

    if (rc == 0) {
        ble_state = BLE_STATE_ADVERTISING;
    } else {
        ESP_LOGE(TAG, "start advertising failed, rc=%d", rc);
    }
}
```

---

## 12. Bonding / Pairing 策略

ANCS 通常需要加密和授权访问，因此 ESP32 需要支持 pairing / bonding。

`sdkconfig` 建议启用：

```ini
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_SECURITY_ENABLE=y
CONFIG_BT_NIMBLE_NVS_PERSIST=y
```

其中关键是：

```ini
CONFIG_BT_NIMBLE_NVS_PERSIST=y
```

它用于将 BLE bonding key 持久化到 NVS，避免 ESP32 重启后丢失配对信息。

### 12.1 什么时候不需要重新配对？

以下场景不需要重新配对：

```text
- 距离过远导致断开
- iPhone 蓝牙关闭后再打开
- 普通 BLE supervision timeout
- iPhone 暂时离开后又回来
```

这些场景只需要：

```text
保留 bond
重新广播
重连后重新 ANCS 初始化
```

### 12.2 什么时候才需要重新配对？

只有以下情况才考虑重新配对：

```text
- ESP32 擦除了 NVS
- ESP32 没有保存 bonding key
- iPhone 用户手动忽略了该设备
- ESP32 更换了 identity address
- iPhone 和 ESP32 的 bond 信息不一致
- 重连后反复出现权限不足、加密失败、配对失败
```

重新配对流程：

```text
1. iPhone 设置 → 蓝牙 → 忽略该设备
2. ESP32 删除对应 peer bond
3. ESP32 重新广播
4. iPhone 重新连接并配对
5. 重连后重新初始化 ANCS
```

---

## 13. 异常场景处理

### 13.1 iPhone 蓝牙打开后没有自动回连

ESP32 侧检查：

```text
- 是否正在 advertising
- advertising 是否为 connectable
- 设备名称 / service data 是否正常
- own_addr_type 是否稳定
- bonding 信息是否存在
- 是否误删了 bond
- 是否重启后 NVS 没保存 bond
```

不要在业务层假设 iPhone 一定会立即回连。iOS 的回连时机由系统决定。

### 13.2 BLE 已连接，但 ANCS 访问失败

可能原因：

```text
- 没有完成 security / encryption
- ANCS characteristic 需要授权访问
- pairing / bonding 信息异常
- 没有重新 discovery
- 使用了旧 handle
- 没有重新订阅 CCCD
```

处理策略：

```text
1. 检查 security 是否成功
2. 重新 discovery ANCS
3. 重新 subscribe CCCD
4. 若仍失败，再考虑清 bond + 重新配对
```

### 13.3 Data Source 收到半包后断线

处理策略：

```text
断线时直接丢弃半包
不要跨连接继续拼接
```

即：

```c
ancs_clear_data_source_buffer();
```

### 13.4 正在请求 Notification Attributes 时断线

处理策略：

```text
断线时取消当前请求
不要等待响应
不要重连后自动重发旧请求
```

即：

```c
ancs_clear_pending_request();
```

原因：旧 `NotificationUID` 只属于旧 session，重连后不应继续使用。

---

## 14. Codex 实现约束

Codex 在实现时必须遵守以下约束。

### 14.1 必须实现

```text
- BLE_GAP_EVENT_CONNECT 处理
- BLE_GAP_EVENT_DISCONNECT 处理
- 断开后重新 start_advertising()
- 断开后清理 ANCS session
- 重连后重新 security
- 重连后重新 discover ANCS service
- 重连后重新 discover characteristics
- 重连后重新 subscribe Data Source
- 重连后重新 subscribe Notification Source
- CONFIG_BT_NIMBLE_NVS_PERSIST=y
```

### 14.2 禁止实现

```text
- 普通断线时删除 bonding 信息
- 重连后继续使用旧 NotificationUID
- 重连后继续等待旧 Control Point 响应
- 重连后继续拼接旧 Data Source 分片
- 重连后默认 ANCS 仍然 ready
- 连接成功后跳过 security 直接访问 ANCS
- 跳过 discovery 直接使用旧 handle，除非后续明确实现了可靠的 handle cache 校验机制
```

### 14.3 推荐日志

建议至少记录：

```text
- BLE connect status
- conn_handle
- BLE disconnect reason
- security complete result
- ANCS service discovery result
- characteristic discovery result
- CCCD subscribe result
- ANCS_READY 状态进入时间
```

示例：

```c
ESP_LOGI(TAG, "BLE connected, conn_handle=%d", conn_handle);
ESP_LOGI(TAG, "BLE disconnected, reason=%d", reason);
ESP_LOGI(TAG, "BLE security complete, success=%d", success);
ESP_LOGI(TAG, "ANCS service discovered");
ESP_LOGI(TAG, "ANCS data source subscribed");
ESP_LOGI(TAG, "ANCS notification source subscribed");
ESP_LOGI(TAG, "ANCS ready");
```

---

## 15. 最终规则摘要

```text
规则 1：
只要 BLE disconnect，就认为 ANCS session 结束。

规则 2：
断开时清理所有 ANCS 临时状态，但不删除 bonding 信息。

规则 3：
断开后立即重新进入 connectable advertising。

规则 4：
重连后必须重新执行 security、discovery、subscribe。

规则 5：
先订阅 Data Source，再订阅 Notification Source。

规则 6：
不要跨 session 使用 NotificationUID、Control Point 请求和 Data Source 分片缓存。

规则 7：
iPhone 蓝牙关闭再打开，与距离过远断开使用同一套恢复策略。

规则 8：
只有 pairing / bonding 信息确实损坏或不一致时，才清 bond 并重新配对。
```

---

## 16. 参考资料

- Apple ANCS Specification  
  https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Specification/Specification.html

- ESP-IDF NimBLE NVS Persist 配置  
  https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/api-reference/kconfig.html

- ESP-IDF BLE Connection / Advertising 说明  
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/ble/get-started/ble-connection.html

