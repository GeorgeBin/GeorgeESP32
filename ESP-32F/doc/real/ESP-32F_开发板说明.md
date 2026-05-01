# 开发板说明

## 1. 开发环境

当前开发环境：

- 操作系统：macOS
- IDE：CLion
- SDK：ESP-IDF

## 2. 开发板概述

当前使用的开发板，建议统一描述为：

**ESP-32F 开发板（核心模组为 ESP-32F / ESP32）**

这块板并不是只带最小系统的基础板，而是一块带有多种板载外设和扩展接口的综合型开发板，适合用于：

- HTTP Server / Web 控制
- LED 灯控制
- OLED / TFT 显示
- TF 卡读写
- 触摸交互
- 声光反馈类实验
- Wi-Fi / BLE 联网应用

---

## 3. 核心模组信息

核心模组为 **ESP-32F**，属于基于 **ESP32** 的 Wi-Fi + 蓝牙 + BLE MCU 模组。

### 3.1 模组能力概览

- CPU：Xtensa 32-bit LX6，单核/双核
- 主频：80 MHz ～ 240 MHz
- ROM：448 KB
- SRAM：520 KB
- RTC SRAM：16 KB
- 供电电压：2.2V ～ 3.6V
- 无线能力：
  - 2.4 GHz Wi-Fi
  - 802.11 b/g/n
  - Bluetooth 4.2 BR/EDR
  - BLE

### 3.2 常见适用场景

- 本地 Web Server
- 物联网控制器
- 小型显示与交互终端
- 传感器采集节点
- 声音 / 灯光 / 触摸联合控制
- 低功耗联网设备

---

## 4. 开发板硬件组成

从原理图看，该开发板主要由以下部分组成：

### 4.1 供电与下载部分

- Micro USB 接口
- CP2102 USB 转串口
- AMS1117-3.3 稳压芯片
- 自动下载电路（DTR / RTS 控制 EN / GPIO0）
- EN 复位按键

### 4.2 板载外设

- 单色 LED
- RGB LED
- 两个触摸按键
- NS4148 功放电路
- 喇叭接口
- OLED 接口
- 1.44 TFT 接口
- TF Card 接口
- 两组排针扩展接口

---

## 5. 板载资源与 GPIO 对应关系

以下是根据原理图整理出的板载资源映射关系。

### 5.1 RGB LED

- RGB_R = GPIO27
- RGB_B = GPIO32
- RGB_G = GPIO33

### 5.2 触摸输入

- TouchA = GPIO4
- TouchB = GPIO2

### 5.3 OLED 接口

- OLED_SCL = GPIO15
- OLED_SDA = GPIO16

### 5.4 TFT 接口

- TFT_CS = GPIO26
- TFT_RST = GPIO17
- TFT_RS = GPIO5
- TFT_LED = GPIO14

### 5.5 SPI 总线

TFT 和 TF 卡复用 SPI 总线：

- MOSI = GPIO23
- MISO = GPIO19
- CLK  = GPIO18

### 5.6 TF 卡

- TF_CS = GPIO22

### 5.7 串口下载

- RXD0 / TXD0 已连接到 CP2102
- 使用 UART0 进行下载和日志输出

---

## 6. 需要特别注意的引脚

ESP32 有一部分 **启动相关引脚（Strapping Pins）**，这些引脚在上电复位时会被采样，其电平状态可能影响芯片启动模式。

本板开发时应特别注意以下引脚：

- GPIO0
- GPIO2
- GPIO5
- GPIO12
- GPIO15

### 6.1 说明

这些引脚在程序运行后可以作为普通 GPIO 使用，但在 **上电、复位、下载、启动阶段**，不要随意外接会强拉高或强拉低的电路，否则可能出现：

- 无法正常启动
- 无法下载程序
- 启动模式错误
- 串口日志异常

### 6.2 本板上与这些引脚相关的功能

- GPIO2：TouchB
- GPIO5：TFT_RS
- GPIO15：OLED_SCL
- GPIO12：ESP32 经典敏感启动脚之一
- GPIO0：下载相关脚

---

## 7. 这块板的特点总结

相对于普通 ESP32 最小系统板，这块板的特点是：

1. **板载资源较多**  
   除了基础下载和供电外，还集成了 RGB 灯、触摸、音频功放、显示接口和 TF 卡接口。

2. **适合快速做功能验证**
   不需要额外焊接太多外围器件，就可以直接做：
   - LED 控制
   - 网页控制
   - 触摸输入
   - 屏幕显示
   - TF 卡读写

3. **适合做综合 Demo**
   例如：
   - HTTP Server 控制 LED
   - 网页配置面板
   - 触摸切换灯光模式
   - OLED / TFT 显示状态
   - 从 TF 卡读取配置或资源

---

## 8. 建议的标准描述方式

以后给 AI 描述这块板时，建议直接使用下面这段。

### 8.1 简洁版

我当前使用 macOS + CLion + ESP-IDF 开发，板子是基于 ESP-32F 模组的 ESP32 开发板。开发板通过 Micro USB 供电和下载，USB 转串口芯片为 CP2102，电源稳压为 AMS1117-3.3。板上带有单色 LED、RGB LED、两个触摸按键、喇叭功放电路、OLED 接口、1.44 TFT 接口、TF 卡接口，并将大部分 GPIO 通过排针引出。核心模组支持 2.4GHz Wi-Fi、Bluetooth 4.2/BLE，适合做 HTTP Server、网页控制、显示和外设控制类项目。

### 8.2 详细版

我当前使用 macOS + CLion + ESP-IDF，开发板是 ESP-32F 开发板，核心模组为 ESP-32F（ESP32）。

硬件特点：
- Micro USB 下载和供电
- CP2102 USB 转串口
- AMS1117-3.3 稳压
- 自动下载电路（DTR/RTS 控制 EN/GPIO0）
- 板载单色 LED
- 板载 RGB LED
- 两个触摸按键
- NS4148 功放和喇叭接口
- OLED 接口
- 1.44 TFT 接口
- TF 卡接口
- 两组排针引出大部分 GPIO

已知板载功能对应关系：
- RGB_R = GPIO27
- RGB_B = GPIO32
- RGB_G = GPIO33
- TouchA = GPIO4
- TouchB = GPIO2
- OLED_SCL = GPIO15
- OLED_SDA = GPIO16
- TFT_CS = GPIO26
- TFT_RST = GPIO17
- TFT_RS = GPIO5
- TFT_LED = GPIO14
- SPI: MOSI = GPIO23, MISO = GPIO19, CLK = GPIO18
- TF_CS = GPIO22

注意事项：
- GPIO0、GPIO2、GPIO5、GPIO12、GPIO15 属于启动相关引脚，写程序和接外设时要注意上电状态，避免影响启动。

### 8.3 面向 AI 写代码的版

我在用 ESP-IDF 为一块 ESP-32F 开发板写程序。请按 ESP32 经典芯片处理，不是 ESP32-S3/C3。开发环境是 macOS + CLion + ESP-IDF。板子支持 Wi-Fi 和 BLE，带 Micro USB、CP2102、AMS1117-3.3、自动下载电路。板载资源包括单色 LED、RGB LED、两个触摸按键、OLED、1.44 TFT、TF 卡、喇叭功放。

请优先基于以下引脚分配生成代码：
- RGB_R GPIO27
- RGB_B GPIO32
- RGB_G GPIO33
- TouchA GPIO4
- TouchB GPIO2
- OLED_SCL GPIO15
- OLED_SDA GPIO16
- TFT_CS GPIO26
- TFT_RST GPIO17
- TFT_RS GPIO5
- TFT_LED GPIO14
- SPI: MOSI GPIO23, MISO GPIO19, CLK GPIO18
- TF_CS GPIO22

并注意 GPIO0 / GPIO2 / GPIO5 / GPIO12 / GPIO15 为启动相关引脚。

---

## 9. 当前最适合的入门项目

结合这块板的资源，当前最适合的 ESP-IDF 入门项目有：

1. HTTP Server 控制板载 LED / RGB LED
2. 触摸按键切换灯光模式
3. OLED 显示设备状态
4. TFT 显示简单控制页面
5. TF 卡读取配置文件
6. Wi-Fi 配网页面
7. 声光联动演示 Demo

---

## 10. 备注

以后如果需要更准确地让 AI 生成代码，建议在描述时明确以下几点：

- 芯片类型：ESP32（ESP-32F 模组）
- 开发框架：ESP-IDF
- IDE：CLion
- 操作系统：macOS
- 是否使用板载外设
- 具体 GPIO 分配
- 是否需要避开启动相关引脚
- 目标功能：HTTP Server / LED / OLED / TFT / TF / Touch / Wi-Fi / BLE

这样 AI 生成的代码会更贴近实际硬件。
