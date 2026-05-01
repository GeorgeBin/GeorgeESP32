# ESP-32F TFT ST7735R Hello World 简要说明

本文记录当前在 `ESP-32F/esp32k` 示例中已经验证成功的 TFT 屏幕显示方案。

## 1. 当前效果

- 工程目录：`ESP-32F/esp32k`
- 驱动文件：`main/display_service.c`
- 显示内容：黑底白字 `hello world`
- 显示方式：分两行显示 `hello` 和 `world`

## 2. 屏幕参数

| 项目 | 值 |
|---|---|
| 控制器 | ST7735R / ST7735 兼容 |
| 分辨率 | 128 x 128 |
| 接口 | SPI |
| SPI Host | `SPI2_HOST` |
| SPI Mode | `0` |
| SPI 频率 | `10 MHz` |
| 像素格式 | RGB565 |
| 字节序 | 高字节先发，低字节后发 |

## 3. 已验证引脚

当前成功显示使用的是 Arduino 示例和 TFT 文档中的引脚配置：

| LCD 信号 | ESP32 GPIO | 说明 |
|---|---:|---|
| SCLK / CLK | GPIO18 | SPI 时钟 |
| SDA / MOSI | GPIO23 | SPI MOSI |
| MISO | 不使用 | 配置为 `-1` |
| CS | GPIO26 | SPI 片选 |
| RS / DC | GPIO17 | 命令/数据选择 |
| RST | GPIO16 | LCD 复位 |
| LED / BL | GPIO13 | 背光控制 |

注意：之前 `esp32k` 中使用过 `DC=GPIO5`、`RST=GPIO17`、`BL=GPIO14`，该配置在当前屏幕上没有显示效果。本工程应以上表为准。

## 4. 关键初始化参数

```c
#define TFT_WIDTH    128
#define TFT_HEIGHT   128
#define TFT_X_OFFSET 2
#define TFT_Y_OFFSET 3
#define TFT_MADCTL   0xC8
```

复位时序：

```text
RST = 0，延时 100 ms
RST = 1，延时 50 ms
```

背光控制：

```text
BL = 1 点亮背光
BL = 0 关闭背光
```

窗口设置命令：

| 命令 | 含义 |
|---|---|
| `0x2A` | 设置列地址 |
| `0x2B` | 设置行地址 |
| `0x2C` | 写显存 |

## 5. 参考来源

- Arduino 示例：`ESP-32F/doc/Arduino/lcd.h`
- Arduino 示例：`ESP-32F/doc/Arduino/lcd.cpp`
- ESP-IDF 版手册：`ESP-32F/doc/real/ESP-32F_TFT_ST7735R_IDF_驱动及使用手册.md`

后续如果扩展绘图、字体或 UI，建议继续基于当前已验证的引脚、复位时序、偏移和初始化序列开发。
