# 消息终端-ESP32K

桌面消息终端设备，用于接收来自手机蓝牙、USB、本地 HTTP 接口的消息，并通过不同显示介质进行提醒与展示。

开发环境：macOS + CLion + ESP-IDF
开发板：ESP-32K（具体资料参考上层目录内的 doc 内文档）

## 第一阶段

* 配置环境：CLion + ESP-IDF
* HelloWorld
* LED Blink



## 第二阶段

输入层：HTTP Server

核心层：消息的接收、解析、分发

输出层：LED 灯

启动一个 http Server，提供接口+页面，可控制 LED 灯

接收参数：颜色、模式（常亮、呼吸、闪烁）



## 第三阶段

屏幕上显示：当前连接的 Wi-Fi、当前 IP、当前 LED 灯模式



## 第四阶段

通过低功耗蓝牙，和 Android 设备连接，从 Android App 推送颜色变化。Android App 根据不同的第三方 App 消息，来推送不同的颜色，例如：微信消息，则推送绿色。钉钉消息，则推送蓝色，等等。
