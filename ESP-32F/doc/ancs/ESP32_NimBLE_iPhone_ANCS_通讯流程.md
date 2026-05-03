# ESP32 NimBLE iPhone ANCS 通讯流程

> 适用工程：`ESP-32F/esp32k`  
> 适用场景：ESP32 使用 ESP-IDF NimBLE 与 iPhone 进行 ANCS 通讯，并根据通知触发本地业务逻辑。

本文档描述当前工程中 ESP32 与 iPhone 建立 ANCS 通讯的完整流程，包括 BLE 角色、广播、配对加密、ANCS 初始化、通知读取和断线恢复策略。

---

## 1. 角色关系

ANCS 场景下，ESP32 和 iPhone 的角色不是单一的。

```text
ESP32：
    BLE Peripheral
    对外广播
    等待 iPhone 主动连接

ESP32：
    GATT Client
    连接后访问 iPhone 提供的 ANCS Service

iPhone：
    BLE Central
    主动连接 ESP32

iPhone：
    GATT Server
    提供 ANCS Service、Characteristics 和通知数据
```

因此，ESP32 断开后不主动扫描连接 iPhone，而是重新进入可连接广播，等待 iOS 根据系统策略回连。

---

## 2. 启动流程

ESP32 启动 ANCS BLE 模块时执行以下初始化：

```text
初始化 NVS
  ↓
初始化 NimBLE host
  ↓
注册 GAP reset / sync 回调
  ↓
配置 BLE bonding / security
  ↓
设置设备名 George_ANCS
  ↓
初始化 NimBLE bond store
  ↓
启动 NimBLE host task
  ↓
host sync 后进入 connectable advertising
```

当前安全配置原则：

```text
启用 bonding
启用 NimBLE NVS 持久化保存 bond
配对 key distribution 使用 ENC | ID
不启用 passkey / OOB
```

`ENC | ID` 很关键：

```text
ENC：用于后续恢复加密连接
ID ：用于 iPhone / ESP32 在隐私地址变化后识别对方身份
```

如果只分发 `ENC`，iPhone 蓝牙开关或 ESP32 重启后可能出现已配对设备无法稳定恢复连接的问题。

---

## 3. 广播流程

ESP32 广播时使用普通可连接广播：

```text
conn_mode = UND
disc_mode = GEN
duration = BLE_HS_FOREVER
```

广播字段包含：

```text
General Discoverable
BR/EDR Not Supported
HID service UUID
Generic HID appearance
```

Scan Response 包含：

```text
设备名 George_ANCS
ANCS Service Solicitation UUID
```

这里的 ANCS Service Solicitation 用于提示 iOS：该设备希望访问 iPhone 的 ANCS 服务。

---

## 4. 首次连接与配对

iPhone 在系统蓝牙设置中连接 ESP32 后，ESP32 收到：

```c
BLE_GAP_EVENT_CONNECT
```

连接成功后，ESP32 执行：

```text
清理上一轮 ANCS session 临时状态
  ↓
保存 conn_handle
  ↓
标记 BLE connected
  ↓
显示 BLE connected 灯效
  ↓
调用 ble_gap_security_initiate()
```

随后 NimBLE 与 iOS 进行 pairing / bonding / encryption。加密完成后，ESP32 收到：

```c
BLE_GAP_EVENT_ENC_CHANGE
```

如果 `status == 0`：

```text
标记 SECURED
  ↓
标记 BONDED
  ↓
开始 ANCS Service discovery
```

如果 `status != 0`：

```text
记录 encryption failed
  ↓
标记 ANCS ERROR
  ↓
保留当前 peer bond
  ↓
主动断开当前连接
  ↓
断开事件中重新进入 connectable advertising
```

注意：单次加密失败不能证明 bond 损坏，尤其是在距离断联恢复时。当前工程不会因为 `ENC_CHANGE` 失败删除 bond。

---

## 5. ANCS 初始化流程

BLE 加密成功后，ESP32 才开始访问 iPhone 的 ANCS GATT 服务。

完整流程：

```text
Discover ANCS Service
  ↓
Discover ANCS Characteristics
  ↓
查找 Notification Source
  ↓
查找 Data Source
  ↓
查找 Control Point
  ↓
发现 Data Source CCCD
  ↓
订阅 Data Source
  ↓
发现 Notification Source CCCD
  ↓
订阅 Notification Source
  ↓
进入 ANCS_READY
```

当前工程先订阅 Data Source，再订阅 Notification Source。

原因：

```text
Notification Source 一旦订阅成功，iPhone 可能立即推送通知事件。
如果 Data Source 尚未订阅，后续请求 Notification Attributes 后可能无法及时接收属性数据。
```

进入 `ANCS_READY` 后，才认为 ANCS 通讯可用。

---

## 6. ANCS Characteristics

当前工程使用三个 ANCS characteristic：

```text
Notification Source：
    iPhone 推送通知事件。
    例如 Added、Modified、Removed。

Control Point：
    ESP32 向 iPhone 写入命令。
    用于请求通知属性。

Data Source：
    iPhone 返回 Control Point 请求的结果。
    例如 AppIdentifier、Title、Subtitle、Message。
```

ESP32 会重新 discovery 每个连接 session 的 handle，不依赖上一次连接缓存的 handle。

---

## 7. 通知处理流程

当 iPhone 有通知变化时，ESP32 从 Notification Source 收到事件：

```text
Notification Source notify
  ↓
解析 EventID / CategoryID / NotificationUID
```

如果事件是：

```text
Added
Modified
```

ESP32 执行：

```text
缓存 NotificationUID 和 category 信息
  ↓
向 Control Point 写 Get Notification Attributes
  ↓
请求 AppIdentifier / Title / Subtitle / Message
  ↓
等待 Data Source 返回属性
```

Data Source 返回后：

```text
解析 Notification Attributes
  ↓
合并之前缓存的 category 信息
  ↓
根据 AppIdentifier 做通知分类
  ↓
交给 notification_rules 执行业务规则
```

当前微信识别条件：

```text
AppIdentifier == com.tencent.xin
```

满足后触发微信对应 LED 灯效。

如果事件是：

```text
Removed
```

ESP32 执行：

```text
根据 NotificationUID 移除当前活动通知
必要时关闭对应灯效
```

---

## 8. 断线恢复流程

只要 BLE 连接断开，就认为当前 ANCS session 结束。

断开事件：

```c
BLE_GAP_EVENT_DISCONNECT
```

处理流程：

```text
记录 disconnect reason
  ↓
清理 ANCS session 临时状态
  ↓
标记 BLE disconnected
  ↓
标记 ANCS DISCONNECTED
  ↓
显示断开灯效
  ↓
重新进入 connectable advertising
```

必须清理：

```text
conn_handle
ANCS service handle
Notification Source handle
Data Source handle
Control Point handle
CCCD handle
MTU 临时值
Data Source 分片缓存
等待中的 NotificationUID
当前订阅状态
ANCS_READY 状态
```

必须保留：

```text
bonding key
pairing 信息
NimBLE NVS 中的 peer 信息
本地通知规则配置
```

重连成功后，ESP32 不做 ANCS 断点续连，而是重新执行：

```text
security / encryption
  ↓
ANCS Service discovery
  ↓
Characteristics discovery
  ↓
CCCD discovery
  ↓
Data Source subscribe
  ↓
Notification Source subscribe
  ↓
ANCS_READY
```

---

## 9. iPhone 蓝牙开关与 ESP32 重启

### 9.1 iPhone 关闭再打开蓝牙

预期流程：

```text
iPhone 关闭蓝牙
  ↓
ESP32 收到 disconnect 或 supervision timeout
  ↓
ESP32 清理 ANCS session
  ↓
ESP32 保留 bond
  ↓
ESP32 持续 connectable advertising
  ↓
iPhone 打开蓝牙
  ↓
iOS 自动或用户手动点击设备进行连接
  ↓
恢复加密
  ↓
重新初始化 ANCS
```

### 9.2 ESP32 重启

预期流程：

```text
ESP32 重启
  ↓
NimBLE 从 NVS 加载 bond
  ↓
ESP32 重新广播
  ↓
iPhone 使用已有配对信息连接
  ↓
恢复加密
  ↓
重新初始化 ANCS
```

这两个场景依赖完整的 `ENC | ID` key distribution。

---

## 10. Bond 处理策略

当前工程的 bond 策略：

```text
普通距离断开：不删除 bond
iPhone 蓝牙开关：不删除 bond
ESP32 重启：不删除 bond
ENC_CHANGE 失败：不删除 bond，只断开重试
REPEAT_PAIRING：删除当前 peer bond，并 retry pairing
```

`REPEAT_PAIRING` 是明确的重新配对信号。此时 ESP32 删除当前 peer 的旧 bond，然后返回：

```c
BLE_GAP_REPEAT_PAIRING_RETRY
```

不要在普通断线、ANCS discovery 失败、subscribe 失败、单次 encryption failure 时删除 bond。否则可能造成：

```text
iPhone 仍保存旧 bond
ESP32 已删除本地 bond
  ↓
双方配对状态不一致
  ↓
用户只能在 iPhone 上忽略设备后重新连接
```

如果设备已经处于这种失配状态，需要在 iPhone 上执行“忽略此设备”，再重新配对。

---

## 11. LED 状态约定

当前 ANCS BLE 状态会触发以下灯效：

```text
断开连接：
    弱白色慢闪 / 熄灭
    表示正在等待 iPhone 重新连接

BLE 已连接：
    短暂蓝色呼吸一次
    表示 BLE 链路已建立，但 ANCS 可能尚未 READY

ANCS_READY：
    恢复正常待机灯效

微信通知：
    根据 notification_rules 触发微信灯效
```

LED 只是状态反馈，不参与 BLE / ANCS 协议决策。

---

## 12. 关键日志

调试时优先观察以下日志：

```text
[BLE] advertising started
[BLE] connected
[BLE] bonded
[ANCS] service discovered
[ANCS] data source subscribed
[ANCS] notification source subscribed
[ANCS] state: READY
[BLE] disconnected: reason=...
encryption failed: status=...; keeping peer bond and retrying after disconnect
[BLE] repeat pairing; deleting old peer bond and retrying
[ANCS] source event: uid=... event=...
[ANCS] detail: app_id=...
```

判断标准：

```text
能到 READY：
    BLE security 和 ANCS 初始化成功。

连接后很快断开：
    先看 encryption failed 还是 ANCS discovery / subscribe failed。

只能忽略设备后恢复：
    优先怀疑 iPhone 和 ESP32 bond 状态已经失配。
```

---

## 13. 测试清单

首次配对：

```text
1. iPhone 忽略旧设备。
2. ESP32 启动并进入广播。
3. iPhone 蓝牙设置中连接 George_ANCS。
4. 确认出现配对提示。
5. 确认出现通知访问授权提示。
6. 串口进入 ANCS_READY。
```

iPhone 蓝牙开关：

```text
1. 已连接并进入 ANCS_READY。
2. 关闭 iPhone 蓝牙。
3. 确认 ESP32 进入断开状态并重新广播。
4. 打开 iPhone 蓝牙。
5. 确认可以自动或手动重连。
6. 确认重新进入 ANCS_READY。
```

ESP32 重启：

```text
1. 不在 iPhone 上忽略设备。
2. 重启 ESP32。
3. iPhone 手动点击 George_ANCS。
4. 确认可以重连并进入 ANCS_READY。
```

距离断联恢复：

```text
1. 已连接并进入 ANCS_READY。
2. 将 iPhone 带离通讯范围，等待断开。
3. 回到通讯范围。
4. 确认 iPhone 可以自动或手动重连。
5. 确认 ESP32 不删除 bond。
6. 确认重新进入 ANCS_READY。
```

微信通知：

```text
1. 确认 iPhone 已允许通知共享。
2. 确认微信通知权限开启。
3. 发送一条微信消息。
4. 串口应看到 app_id=com.tencent.xin。
5. LED 应触发微信对应灯效。
```

---

## 14. 核心原则

一句话总结：

```text
NimBLE 负责恢复 BLE 连接；
bond 负责恢复安全关系；
ANCS 不做断点续连；
每次 BLE 重连都必须当成全新的 ANCS session。
```

实现上要坚持：

```text
断线清 ANCS session
普通断线不删 bond
重连后重新 security
security 成功后重新 discovery
重新 subscribe 后才进入 ANCS_READY
Notification Source 事件到达后再按 UID 请求 Attributes
```
