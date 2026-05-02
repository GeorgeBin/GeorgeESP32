# 阶段 3：Android BLE LED 控制 App 架构文档

> 适用目标：基于已有开源项目快速改造出 Android BLE 控灯 App。  
> 推荐基于 Nordic nRF Blinky 或 Nordic Android BLE Library 实现，而不是从零直接操作 Android BluetoothGatt。

---

## 1. 阶段目标

本阶段目标是实现一个 Android 原型 App：

```text
扫描 ESP32 LED 设备
    ↓
连接 BLE GATT Server
    ↓
订阅 State Notify
    ↓
选择颜色 / 模式 / 亮度
    ↓
写入 Command Characteristic
    ↓
ESP32 LED 状态变化
    ↓
App 显示当前状态
```

这个阶段只做 BLE 控灯，不做 BLE 配网。BLE 配网由阶段 2 的官方 provisioning 方案负责。

---

## 2. 推荐改造基础

### 2.1 快速改造路线

推荐从以下项目二选一：

```text
1. Nordic nRF Blinky Android
2. 自建 Kotlin 工程 + Nordic Android BLE Library
```

如果目标是“最快能跑”，优先使用：

```text
Nordic nRF Blinky Android
```

如果目标是“作为正式 App 的基础”，优先使用：

```text
Kotlin + Compose + Nordic Android BLE Library
```

---

## 3. 阶段边界

### 3.1 本阶段实现

```text
1. BLE 权限申请
2. 扫描 ESP32 LED Service
3. 连接 ESP32
4. 发现 GATT Services
5. 查找 LED Command Characteristic
6. 查找 LED State Characteristic
7. 开启 Notify
8. 写入 LED 控制 JSON
9. 解析 ESP32 Notify 状态
10. UI 显示连接状态和 LED 状态
```

### 3.2 本阶段不实现

```text
1. BLE Wi-Fi 配网
2. iOS
3. 云端控制
4. 账号体系
5. OTA
6. 设备绑定
7. 复杂多设备管理
```

---

## 4. Android App 模块建议

如果是新建正式 App，建议结构：

```text
app/
├── build.gradle.kts
└── src/main/java/com/george/led/app/
    ├── MainActivity.kt
    ├── App.kt
    ├── ui/
    │   ├── scan/
    │   │   ├── ScanScreen.kt
    │   │   └── ScanViewModel.kt
    │   ├── control/
    │   │   ├── LedControlScreen.kt
    │   │   └── LedControlViewModel.kt
    │   └── common/
    │       ├── PermissionScreen.kt
    │       └── AppNavigation.kt
    ├── ble/
    │   ├── EspLedBleManager.kt
    │   ├── EspLedBleScanner.kt
    │   ├── BleConnectionState.kt
    │   └── BleConstants.kt
    ├── protocol/
    │   ├── LedCommand.kt
    │   ├── LedState.kt
    │   ├── LedMode.kt
    │   ├── LedProtocolCodec.kt
    │   └── LedProtocolError.kt
    └── device/
        ├── EspLedDevice.kt
        └── DeviceRepository.kt
```

如果是直接 fork nRF Blinky，可以先不拆这么细，但最终应向该结构收敛。

---

## 5. BLE UUID 定义

建议集中放在：

```text
ble/BleConstants.kt
```

示例：

```kotlin
object BleConstants {
    val LED_SERVICE_UUID: UUID =
        UUID.fromString("0000A001-0000-1000-8000-00805F9B34FB")

    val LED_COMMAND_UUID: UUID =
        UUID.fromString("0000A002-0000-1000-8000-00805F9B34FB")

    val LED_STATE_UUID: UUID =
        UUID.fromString("0000A003-0000-1000-8000-00805F9B34FB")

    val DEVICE_INFO_UUID: UUID =
        UUID.fromString("0000A004-0000-1000-8000-00805F9B34FB")
}
```

扫描时优先按 `LED_SERVICE_UUID` 过滤。

---

## 6. 权限设计

### 6.1 Android 12 及以上

需要关注：

```text
BLUETOOTH_SCAN
BLUETOOTH_CONNECT
```

如果扫描结果涉及定位推断，可能仍需处理定位权限或相关系统开关。

### 6.2 Android 11 及以下

通常需要：

```text
ACCESS_FINE_LOCATION
BLUETOOTH
BLUETOOTH_ADMIN
```

### 6.3 权限处理原则

```text
1. App 启动先检查权限
2. 缺少权限时显示解释页
3. 用户授权后进入扫描页
4. 权限被拒绝时给出明确提示
```

---

## 7. 页面设计

### 7.1 扫描页

显示内容：

```text
标题：选择 LED 设备

状态：
  正在扫描 / 扫描已停止 / 蓝牙未开启 / 权限不足

设备列表：
  设备名
  MAC 地址
  RSSI
  是否匹配 LED Service
```

操作：

```text
1. 开始扫描
2. 停止扫描
3. 点击设备连接
```

### 7.2 控制页

显示内容：

```text
设备名：George_LED_1234
连接状态：Connected / Connecting / Disconnected
当前颜色：#FF0000
当前模式：Solid / Breath / Blink / Off
当前亮度：80%
最后结果：OK / 错误码
```

控制项：

```text
1. 颜色选择
2. 模式选择
   - Off
   - Solid
   - Breath
   - Blink
3. 亮度 Slider
4. 呼吸周期
5. 闪烁 on/off 时间
6. 发送按钮
7. 断开连接按钮
```

---

## 8. 协议模型

### 8.1 LedMode

```kotlin
enum class LedMode(val wireName: String) {
    OFF("off"),
    SOLID("solid"),
    BREATH("breath"),
    BLINK("blink")
}
```

### 8.2 LedCommand

```kotlin
data class LedCommand(
    val seq: Int,
    val cmd: String = "set_led",
    val color: String? = null,
    val mode: LedMode,
    val brightness: Int? = null,
    val periodMs: Int? = null,
    val onMs: Int? = null,
    val offMs: Int? = null
)
```

### 8.3 LedState

```kotlin
data class LedState(
    val color: String?,
    val mode: LedMode,
    val brightness: Int?,
    val periodMs: Int?,
    val onMs: Int?,
    val offMs: Int?,
    val source: String?
)
```

### 8.4 LedResponse

```kotlin
data class LedResponse(
    val seq: Int?,
    val code: Int,
    val msg: String,
    val state: LedState?
)
```

---

## 9. 协议编码

建议由 `LedProtocolCodec` 统一处理。

### 9.1 发送 set_led

```kotlin
fun encodeSetLed(command: LedCommand): ByteArray {
    // 第一版使用 JSON UTF-8
}
```

输出示例：

```json
{
  "seq": 1,
  "cmd": "set_led",
  "color": "#FF0000",
  "mode": "solid",
  "brightness": 100
}
```

### 9.2 发送 get_state

```kotlin
fun encodeGetState(seq: Int): ByteArray {
    // {"seq":1,"cmd":"get_state"}
}
```

### 9.3 解析 Notify

```kotlin
fun decodeResponse(bytes: ByteArray): LedResponse {
    // JSON UTF-8 → LedResponse
}
```

---

## 10. BLE 管理器设计

建议将 BLE 操作集中到 `EspLedBleManager`。

### 10.1 对外能力

```kotlin
interface EspLedBleClient {
    val connectionState: StateFlow<BleConnectionState>
    val ledState: StateFlow<LedState?>
    val lastError: StateFlow<LedProtocolError?>

    fun connect(device: BluetoothDevice)
    fun disconnect()

    suspend fun setLed(command: LedCommand)
    suspend fun getState()
}
```

### 10.2 状态定义

```kotlin
sealed class BleConnectionState {
    data object Idle : BleConnectionState()
    data object Scanning : BleConnectionState()
    data object Connecting : BleConnectionState()
    data object Connected : BleConnectionState()
    data object Disconnecting : BleConnectionState()
    data class Disconnected(val reason: String?) : BleConnectionState()
    data class Error(val message: String) : BleConnectionState()
}
```

### 10.3 内部职责

```text
1. 连接设备
2. 发现 Service
3. 获取 Characteristic
4. 开启 Notify
5. 写入命令
6. 处理 Notify
7. 处理断开重连
8. 统一错误回调
```

---

## 11. 扫描器设计

`EspLedBleScanner` 负责扫描设备。

对外接口：

```kotlin
interface EspLedBleScanner {
    val devices: StateFlow<List<EspLedDevice>>
    val scanning: StateFlow<Boolean>

    fun startScan()
    fun stopScan()
}
```

设备模型：

```kotlin
data class EspLedDevice(
    val name: String?,
    val address: String,
    val rssi: Int,
    val bluetoothDevice: BluetoothDevice
)
```

扫描策略：

```text
1. 优先使用 Service UUID 过滤
2. 如果部分手机无法通过 UUID 过滤，再降级为名称过滤
3. 扫描结果按 RSSI 排序
4. 避免重复设备
5. 进入连接页后停止扫描
```

---

## 12. ViewModel 设计

### 12.1 ScanViewModel

职责：

```text
1. 检查权限
2. 启动扫描
3. 停止扫描
4. 管理设备列表
5. 处理设备点击
```

状态：

```kotlin
data class ScanUiState(
    val hasPermission: Boolean = false,
    val bluetoothEnabled: Boolean = false,
    val scanning: Boolean = false,
    val devices: List<EspLedDevice> = emptyList(),
    val message: String? = null
)
```

### 12.2 LedControlViewModel

职责：

```text
1. 连接设备
2. 管理颜色、模式、亮度输入
3. 发送 LED 命令
4. 接收状态 Notify
5. 处理断开和错误
```

状态：

```kotlin
data class LedControlUiState(
    val connectionState: BleConnectionState = BleConnectionState.Idle,
    val selectedColor: String = "#FF0000",
    val selectedMode: LedMode = LedMode.SOLID,
    val brightness: Int = 100,
    val periodMs: Int = 2000,
    val onMs: Int = 500,
    val offMs: Int = 500,
    val currentState: LedState? = null,
    val lastMessage: String? = null
)
```

---

## 13. nRF Blinky 改造点

如果 fork nRF Blinky，主要修改这些位置：

```text
1. 替换原有 Nordic Service UUID
2. 替换 LED Characteristic UUID
3. 删除或弱化 Button Notify 逻辑
4. 新增 State Characteristic Notify 逻辑
5. 将原来的开关按钮改为颜色 / 模式 / 亮度控制
6. 写入内容由单字节改为 JSON UTF-8
7. 解析 ESP32 返回的 JSON 状态
8. 调整 UI 文案和包名
```

---

## 14. 错误处理

### 14.1 BLE 错误

```text
1. 蓝牙未开启
2. 权限未授予
3. 扫描失败
4. 连接失败
5. Service 未找到
6. Characteristic 未找到
7. Notify 开启失败
8. 写入失败
9. 设备主动断开
```

### 14.2 协议错误

```text
1001 JSON 格式错误
1002 缺少 cmd 字段
1003 不支持的 cmd
1004 不支持的 mode
1005 color 格式错误
1006 brightness 超出范围
1007 参数组合非法
2001 未认证
3001 LED 控制失败
9001 内部错误
```

### 14.3 UI 提示原则

```text
1. 用户可理解
2. 不直接展示底层异常堆栈
3. 调试模式可以显示详细错误
4. 错误后允许重试
```

---

## 15. 连接策略

第一版建议：

```text
1. 用户手动选择设备
2. 连接失败不自动无限重试
3. 断开后显示重连按钮
4. App 退出控制页时断开连接
```

后续可扩展：

```text
1. 记住上次设备
2. 自动重连
3. 多设备列表
4. 后台连接
```

---

## 16. AI 实现任务清单

### 16.1 第一步：准备工程

```text
1. fork nRF Blinky 或新建 Kotlin 工程
2. 修改包名
3. 修改 App 名称
4. 配置 minSdk
5. 配置 BLE 权限
```

### 16.2 第二步：实现扫描

```text
1. 增加 LED Service UUID
2. 按 Service UUID 扫描
3. 显示设备列表
4. 点击设备进入连接流程
```

### 16.3 第三步：实现连接

```text
1. 连接 BluetoothDevice
2. Discover Services
3. 找到 Command Characteristic
4. 找到 State Characteristic
5. 开启 Notify
```

### 16.4 第四步：实现协议

```text
1. LedMode
2. LedCommand
3. LedState
4. LedResponse
5. LedProtocolCodec
```

### 16.5 第五步：实现控制页

```text
1. 颜色选择
2. 模式选择
3. 亮度调节
4. 发送按钮
5. 当前状态显示
```

### 16.6 第六步：联调

```text
1. 连接 ESP32
2. 发送 solid 命令
3. 发送 breath 命令
4. 发送 blink 命令
5. 发送 off 命令
6. 验证 Notify 状态
```

---

## 17. 验收标准

### 17.1 扫描

```text
1. App 能扫描到 ESP32
2. 设备列表显示 George_LED_Device
3. RSSI 正常显示
```

### 17.2 连接

```text
1. 点击设备后可连接成功
2. UI 显示 Connected
3. 找不到 Service 时给出明确错误
```

### 17.3 控制

```text
1. 选择红色 + 常亮，ESP32 LED 变红
2. 选择蓝色 + 呼吸，ESP32 LED 蓝色呼吸
3. 选择黄色 + 闪烁，ESP32 LED 黄色闪烁
4. 选择 Off，ESP32 LED 关闭
```

### 17.4 状态

```text
1. ESP32 Notify 后 App 更新当前状态
2. ESP32 返回错误码时 App 显示错误
3. 断开连接时 UI 状态更新
```

---

## 18. 后续演进

阶段 3 完成后，进入阶段 4：

```text
将 BLE 配网和 BLE 控灯整合到自己的正式 Android App 中。
```

本阶段代码可以作为正式 App 的 `feature-led-control` 原型。

