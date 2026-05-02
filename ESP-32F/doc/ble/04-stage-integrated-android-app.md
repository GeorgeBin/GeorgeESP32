# 阶段 4：Android App 集成版架构文档

> 适用目标：将 BLE 配网能力和 BLE LED 控制能力整合成一个自己的 Android App。  
> 本阶段从“验证工具”升级为“产品原型 App”。

---

## 1. 阶段目标

本阶段目标是实现一个完整 Android App 原型：

```text
首次使用：
  扫描 ESP32
  ↓
  BLE 配网 Wi-Fi
  ↓
  ESP32 连上 Wi-Fi
  ↓
  进入 LED 控制页

日常使用：
  扫描 / 连接已配网 ESP32
  ↓
  通过 BLE 控制 LED
  ↓
  查看设备状态

调试使用：
  显示 ESP32 IP
  可通过浏览器访问 HTTP 页面
```

---

## 2. 产品定位

本 App 不是通用 BLE 调试工具，而是面向你的 ESP32 LED 产品的专用控制 App。

核心能力：

```text
1. 设备发现
2. BLE 配网
3. BLE 控灯
4. 设备状态显示
5. 重新配网
6. 基础设备管理
```

---

## 3. 技术选型建议

推荐：

```text
语言：
  Kotlin

UI：
  Jetpack Compose

架构：
  MVVM + Repository

异步：
  Coroutine + Flow / StateFlow

BLE 控制：
  Nordic Android BLE Library 或基于阶段 3 的封装

Wi-Fi 配网：
  Espressif Provisioning Android Library

本地存储：
  DataStore

最低版本：
  minSdk 可先设为 26，方便直接使用 Espressif Provisioning Android Library
```

如果你希望覆盖更低 Android 版本，需要单独评估配网库兼容性和 BLE 权限处理成本。

---

## 4. 工程模块建议

### 4.1 单模块快速版

适合第一版产品原型：

```text
app/
└── src/main/java/com/george/led/
    ├── MainActivity.kt
    ├── App.kt
    ├── navigation/
    ├── ui/
    ├── provisioning/
    ├── ble/
    ├── protocol/
    ├── device/
    ├── settings/
    └── storage/
```

### 4.2 多模块正式版

适合后续长期维护：

```text
george-led-android/
├── app/
├── core/
│   ├── core-common/
│   ├── core-ui/
│   ├── core-storage/
│   └── core-permission/
├── feature/
│   ├── feature-scan/
│   ├── feature-provisioning/
│   ├── feature-led-control/
│   └── feature-device-settings/
├── domain/
│   └── device-domain/
├── data/
│   └── device-repository/
└── ble/
    ├── ble-led-client/
    └── ble-provisioning-client/
```

第一版建议采用单模块快速版，等功能稳定后再拆多模块。

---

## 5. 推荐包结构

```text
com.george.led
├── MainActivity.kt
├── LedApp.kt
├── navigation/
│   ├── AppNavGraph.kt
│   └── AppDestination.kt
├── ui/
│   ├── scan/
│   ├── provisioning/
│   ├── control/
│   ├── settings/
│   └── common/
├── permission/
│   ├── BluetoothPermissionManager.kt
│   └── PermissionUiState.kt
├── provisioning/
│   ├── EspProvisioningManager.kt
│   ├── ProvisioningRepository.kt
│   ├── ProvisioningState.kt
│   └── WifiNetwork.kt
├── ble/
│   ├── EspLedBleManager.kt
│   ├── EspLedBleScanner.kt
│   ├── BleConnectionState.kt
│   └── BleConstants.kt
├── protocol/
│   ├── LedCommand.kt
│   ├── LedState.kt
│   ├── LedMode.kt
│   ├── LedResponse.kt
│   └── LedProtocolCodec.kt
├── device/
│   ├── EspLedDevice.kt
│   ├── DeviceRepository.kt
│   ├── DeviceConnectionMode.kt
│   └── DeviceRuntimeState.kt
└── storage/
    ├── AppDataStore.kt
    └── SavedDevice.kt
```

---

## 6. App 页面流程

### 6.1 首次使用流程

```text
Splash / 权限检查
  ↓
设备扫描页
  ↓
选择未配网设备
  ↓
配网页
  ↓
输入 PoP / 扫码
  ↓
扫描 Wi-Fi
  ↓
选择 Wi-Fi
  ↓
输入 Wi-Fi 密码
  ↓
开始配网
  ↓
配网成功
  ↓
进入 LED 控制页
```

### 6.2 日常控制流程

```text
Splash / 权限检查
  ↓
读取上次设备
  ↓
尝试扫描并连接
  ↓
进入 LED 控制页
```

如果自动连接失败：

```text
显示设备扫描页
```

### 6.3 重新配网流程

```text
设置页
  ↓
重新配网
  ↓
提示用户长按设备 BOOT 键
  ↓
扫描 Provisioning 设备
  ↓
重新执行 BLE 配网
```

---

## 7. 导航设计

建议页面：

```text
PermissionScreen
ScanScreen
ProvisioningIntroScreen
ProvisioningPopScreen
WifiScanScreen
WifiPasswordScreen
ProvisioningProgressScreen
LedControlScreen
DeviceSettingsScreen
AboutScreen
```

导航图：

```text
PermissionScreen
  └── ScanScreen
        ├── ProvisioningIntroScreen
        │     └── ProvisioningPopScreen
        │           └── WifiScanScreen
        │                 └── WifiPasswordScreen
        │                       └── ProvisioningProgressScreen
        │                             └── LedControlScreen
        └── LedControlScreen
              └── DeviceSettingsScreen
```

---

## 8. 设备模型

### 8.1 EspLedDevice

```kotlin
data class EspLedDevice(
    val id: String,
    val name: String?,
    val address: String?,
    val rssi: Int?,
    val provisioned: Boolean?,
    val bluetoothDevice: BluetoothDevice?
)
```

### 8.2 SavedDevice

```kotlin
data class SavedDevice(
    val id: String,
    val name: String,
    val address: String?,
    val lastIp: String?,
    val lastConnectedAt: Long
)
```

### 8.3 DeviceRuntimeState

```kotlin
data class DeviceRuntimeState(
    val connectionState: BleConnectionState,
    val ledState: LedState?,
    val wifiIp: String?,
    val firmwareVersion: String?,
    val hardwareVersion: String?,
    val lastError: String?
)
```

---

## 9. Repository 设计

### 9.1 DeviceRepository

统一协调设备扫描、连接、状态保存。

```kotlin
interface DeviceRepository {
    val savedDevices: Flow<List<SavedDevice>>
    val currentDevice: StateFlow<EspLedDevice?>
    val runtimeState: StateFlow<DeviceRuntimeState>

    fun startScan()
    fun stopScan()

    suspend fun connect(device: EspLedDevice)
    suspend fun disconnect()

    suspend fun saveDevice(device: SavedDevice)
    suspend fun forgetDevice(deviceId: String)
}
```

### 9.2 ProvisioningRepository

封装 Espressif Provisioning Library。

```kotlin
interface ProvisioningRepository {
    val provisioningState: StateFlow<ProvisioningState>

    fun startScanProvisioningDevices()
    fun stopScanProvisioningDevices()

    suspend fun connectForProvisioning(device: EspLedDevice, proofOfPossession: String)
    suspend fun scanWifiNetworks(): List<WifiNetwork>
    suspend fun provision(ssid: String, password: String)
    suspend fun cancel()
}
```

### 9.3 LedControlRepository

封装 BLE LED 控制。

```kotlin
interface LedControlRepository {
    val connectionState: StateFlow<BleConnectionState>
    val ledState: StateFlow<LedState?>

    suspend fun setLed(command: LedCommand)
    suspend fun getState()
    suspend fun turnOff()
}
```

第一版中 `DeviceRepository` 可以直接聚合 `ProvisioningRepository` 和 `LedControlRepository`。

---

## 10. 配网状态设计

```kotlin
sealed class ProvisioningState {
    data object Idle : ProvisioningState()
    data object ScanningDevice : ProvisioningState()
    data object ConnectingDevice : ProvisioningState()
    data object NeedProofOfPossession : ProvisioningState()
    data object ScanningWifi : ProvisioningState()
    data class WifiListLoaded(val list: List<WifiNetwork>) : ProvisioningState()
    data object SendingCredential : ProvisioningState()
    data object ApplyingConfig : ProvisioningState()
    data object WaitingWifiConnection : ProvisioningState()
    data class Success(val ip: String?) : ProvisioningState()
    data class Failed(val reason: String) : ProvisioningState()
}
```

---

## 11. LED 控制状态设计

```kotlin
data class LedControlUiState(
    val deviceName: String = "",
    val connectionState: BleConnectionState = BleConnectionState.Idle,
    val currentColor: String = "#FF0000",
    val currentMode: LedMode = LedMode.SOLID,
    val brightness: Int = 100,
    val periodMs: Int = 2000,
    val onMs: Int = 500,
    val offMs: Int = 500,
    val remoteState: LedState? = null,
    val sending: Boolean = false,
    val message: String? = null
)
```

---

## 12. BLE 与配网的关系

App 内部需要区分两种 BLE 用途：

```text
1. Provisioning BLE
2. LED Control BLE
```

建议实现上分开：

```text
EspProvisioningManager：
  只处理 ESP-IDF BLE Provisioning

EspLedBleManager：
  只处理自定义 LED GATT Service
```

不要让一个类同时处理两套协议。

---

## 13. 权限与蓝牙状态

App 启动时检查：

```text
1. 蓝牙是否开启
2. BLE 扫描权限
3. BLE 连接权限
4. 位置权限，按 Android 版本判断
5. 定位服务是否开启，按 Android 版本和扫描策略判断
```

权限页应说明：

```text
本 App 需要蓝牙权限，用于扫描和连接 ESP32 LED 设备。
```

---

## 14. 本地存储

使用 DataStore 保存：

```text
1. 上次连接设备 ID
2. 上次设备名称
3. 上次设备 MAC 地址
4. 上次设备 IP
5. 默认颜色
6. 默认模式
7. 默认亮度
8. 是否启用自动连接
```

示例：

```kotlin
data class AppSettings(
    val lastDeviceId: String?,
    val lastDeviceName: String?,
    val lastDeviceAddress: String?,
    val lastDeviceIp: String?,
    val defaultColor: String,
    val defaultMode: LedMode,
    val defaultBrightness: Int,
    val autoConnect: Boolean
)
```

---

## 15. 设置页设计

设置页显示：

```text
设备名称
固件版本
硬件版本
BLE 地址
Wi-Fi IP
当前协议版本
重新配网
忘记设备
恢复默认 LED 参数
关于 App
```

操作：

```text
1. 断开设备
2. 重新配网
3. 忘记设备
4. 打开 HTTP 调试页面
```

---

## 16. HTTP 调试入口

如果 ESP32 已连接 Wi-Fi，并且 App 能读取到 IP，可提供按钮：

```text
打开网页控制台
```

目标：

```text
http://{deviceIp}/
```

这不是核心控制链路，只是调试辅助。

---

## 17. 错误处理策略

### 17.1 配网错误

```text
1. 未找到设备
2. PoP 错误
3. Wi-Fi 密码错误
4. 连接 Wi-Fi 超时
5. 设备断开
6. 手机蓝牙异常
```

### 17.2 控灯错误

```text
1. 未连接设备
2. 写入失败
3. Notify 未开启
4. 协议错误
5. ESP32 返回错误码
```

### 17.3 UI 提示

```text
1. 用户操作错误：用普通语言提示
2. 设备异常：给出重试按钮
3. 开发调试模式：显示错误码和原始消息
```

---

## 18. 开发模式开关

建议加入开发模式：

```text
1. 显示 BLE 原始日志
2. 显示发送 JSON
3. 显示接收 JSON
4. 显示 Service / Characteristic UUID
5. 显示 ESP32 返回错误码
```

开发模式可以通过设置页连续点击版本号开启。

---

## 19. AI 实现任务清单

### 19.1 第一步：创建 App 骨架

```text
1. Kotlin + Compose 工程
2. 配置 minSdk
3. 配置蓝牙权限
4. 创建导航结构
5. 创建 PermissionScreen
6. 创建 ScanScreen
7. 创建 LedControlScreen
8. 创建 SettingsScreen
```

### 19.2 第二步：集成 BLE LED 控制

```text
1. 迁移阶段 3 的 EspLedBleManager
2. 迁移协议模型
3. 实现扫描设备
4. 实现连接设备
5. 实现控制 LED
6. 实现 Notify 状态显示
```

### 19.3 第三步：集成 BLE 配网

```text
1. 引入 Espressif Provisioning Android Library
2. 封装 EspProvisioningManager
3. 实现 Provisioning 页面流
4. 实现 Wi-Fi 扫描
5. 实现发送 Wi-Fi 凭据
6. 实现配网状态显示
```

### 19.4 第四步：打通流程

```text
1. 未配网设备 → 配网页
2. 配网成功 → 控制页
3. 已配网设备 → 控制页
4. 设置页 → 重新配网
5. 设置页 → 打开 HTTP 页面
```

### 19.5 第五步：本地存储

```text
1. 保存上次设备
2. 保存默认 LED 参数
3. 保存自动连接开关
4. App 启动时恢复状态
```

### 19.6 第六步：联调

```text
1. 清除 ESP32 配置
2. App 扫描未配网设备
3. App 通过 BLE 配网
4. ESP32 显示 Wi-Fi 已连接
5. App 进入控制页
6. App 控制 LED
7. App 断开后重新连接
8. 长按 ESP32 按键重新配网
```

---

## 20. 验收标准

### 20.1 首次配网

```text
1. 新设备上电后 App 能发现
2. App 能输入 PoP
3. App 能扫描 Wi-Fi 列表
4. App 能发送 Wi-Fi 密码
5. ESP32 成功联网
6. App 显示配网成功
7. App 自动进入控制页
```

### 20.2 日常控灯

```text
1. App 能连接已配网设备
2. App 能控制颜色
3. App 能控制模式
4. App 能控制亮度
5. App 能显示 ESP32 Notify 状态
```

### 20.3 重新配网

```text
1. 设置页点击重新配网
2. App 提示用户长按设备按键
3. ESP32 进入配网模式
4. App 能重新执行配网
```

### 20.4 异常处理

```text
1. 蓝牙关闭时有提示
2. 权限不足时有提示
3. 设备断开时 UI 更新
4. Wi-Fi 密码错误时能重新输入
5. ESP32 返回协议错误时 App 显示错误
```

---

## 21. 后续产品化方向

完成阶段 4 后，可以继续扩展：

```text
1. iOS App
2. 多设备管理
3. 设备绑定
4. 云端远程控制
5. OTA 固件升级
6. BLE 二进制协议
7. 低功耗模式
8. 设备二维码
9. 生产测试工具
10. 自研 PCB 适配
```

---

## 22. 最终建议

第一版集成 App 不要追求复杂。

建议第一版只实现：

```text
1. 权限页
2. 扫描页
3. 配网页
4. LED 控制页
5. 设置页
```

先保证：

```text
BLE 配网成功
BLE 控灯稳定
ESP32 状态显示准确
```

然后再逐步增加多设备、账号、云端等产品能力。

