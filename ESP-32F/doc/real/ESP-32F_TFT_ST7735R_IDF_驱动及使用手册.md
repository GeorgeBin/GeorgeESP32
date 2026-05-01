# ESP-32F 开发板 TFT 屏幕驱动及使用手册（ESP-IDF 版）

> 适用对象：Codex / ESP-IDF 开发者  
> 目标：基于原 Arduino 例程 `lcd.h / lcd.cpp`，在 ESP-IDF 工程中实现 ESP-32F 开发板板载 1.44 寸 TFT LCD 的底层驱动，并提供基础绘图 API。

---

## 1. 硬件结论

该 ESP-32F 开发板板载一块 **1.44 寸 SPI TFT 彩色 LCD**。

根据原 Arduino 例程中的初始化序列和注释，可判断该屏幕控制器为：

```text
ST7735R / ST7735 兼容控制器
```

屏幕关键参数：

| 项目 | 值 |
|---|---|
| 屏幕类型 | TFT 彩色 LCD |
| 屏幕尺寸 | 1.44 寸 |
| 分辨率 | 128 × 128 |
| 通信接口 | SPI，写入型，基本不使用 MISO |
| 色彩格式 | RGB565，16-bit/pixel |
| 控制器 | ST7735R / ST7735 兼容 |
| 显存写入命令 | `0x2C` |
| 设置列地址命令 | `0x2A` |
| 设置行地址命令 | `0x2B` |
| 像素数据字节序 | 高字节在前，低字节在后 |

注意：原 `lcd.h` 中存在如下旧模板残留：

```c
#if USE_HORIZONTAL==1
#define LCD_W 320
#define LCD_H 240
#else
#define LCD_W 240
#define LCD_H 320
#endif
```

这部分 **不要照搬**。实际有效尺寸应以 `LCD_SetParam()` 中的值为准：

```c
lcddev.width = 128;
lcddev.height = 128;
```

---

## 2. ESP32 与 TFT LCD 引脚连接

原 Arduino 例程中的引脚定义如下：

```c
#define LCD_RS   17  // RS / DC
#define LCD_CS   26  // CS
#define LCD_RST  16  // Reset
#define LCD_SCL  18  // SPI CLK / SCLK
#define LCD_SDA  23  // SPI SDI / MOSI
#define LCD_LED  13  // Backlight
```

ESP-IDF 工程中建议改名如下：

```c
#define TFT_PIN_SCLK  18
#define TFT_PIN_MOSI  23
#define TFT_PIN_MISO  -1
#define TFT_PIN_CS    26
#define TFT_PIN_DC    17
#define TFT_PIN_RST   16
#define TFT_PIN_BL    13
```

信号含义：

| LCD 信号 | Arduino 命名 | IDF 建议命名 | ESP32 GPIO | 说明 |
|---|---:|---:|---:|---|
| SCL / CLK | `LCD_SCL` | `TFT_PIN_SCLK` | GPIO18 | SPI 时钟 |
| SDA / SDI / DIN | `LCD_SDA` | `TFT_PIN_MOSI` | GPIO23 | SPI MOSI |
| MISO | 原例程填 GPIO22 | `TFT_PIN_MISO = -1` | 不使用 | LCD 不需要读数据 |
| CS | `LCD_CS` | `TFT_PIN_CS` | GPIO26 | SPI 片选 |
| RS / DC / A0 | `LCD_RS` | `TFT_PIN_DC` | GPIO17 | 命令/数据选择 |
| RST | `LCD_RST` | `TFT_PIN_RST` | GPIO16 | LCD 硬复位 |
| LED / BL | `LCD_LED` | `TFT_PIN_BL` | GPIO13 | 背光控制 |

---

## 3. SPI 通信模型

该屏幕使用 SPI 写入命令和数据。

### 3.1 命令写入

原 Arduino：

```cpp
LCD_CS_CLR;
LCD_RS_CLR;
SPI.write(data);
LCD_CS_SET;
```

ESP-IDF 对应逻辑：

```c
gpio_set_level(TFT_PIN_DC, 0);
spi_device_polling_transmit(spi, &transaction);
```

`DC = 0` 表示当前 SPI 发送的是 LCD 命令。

### 3.2 数据写入

原 Arduino：

```cpp
LCD_CS_CLR;
LCD_RS_SET;
SPI.write(data);
LCD_CS_SET;
```

ESP-IDF 对应逻辑：

```c
gpio_set_level(TFT_PIN_DC, 1);
spi_device_polling_transmit(spi, &transaction);
```

`DC = 1` 表示当前 SPI 发送的是 LCD 数据。

### 3.3 16-bit RGB565 像素写入

原 Arduino：

```cpp
SPI.write(data >> 8);
SPI.write(data);
```

因此 IDF 中也必须按如下顺序发送：

```text
RGB565 高字节 -> RGB565 低字节
```

例如红色：

```c
uint16_t red = 0xF800;
发送顺序：0xF8, 0x00
```

---

## 4. SPI 参数建议

建议初版使用：

```c
#define TFT_SPI_HOST SPI2_HOST
```

SPI 设备配置建议：

```c
spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10 * 1000 * 1000,
    .mode = 0,
    .spics_io_num = TFT_PIN_CS,
    .queue_size = 1,
};
```

说明：

1. 原 Arduino 例程调用 `SPI.begin(LCD_SCL, 22, LCD_SDA, LCD_CS)`，但没有启用 `SPI.beginTransaction()`。
2. 原代码中有一行被注释的配置：

   ```cpp
   // SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE2));
   ```

3. 因此 IDF 初版优先使用 SPI Mode 0。
4. 如果屏幕花屏、错位、无显示，可以尝试：

   ```c
   .mode = 2
   ```

5. 初始调试频率建议 5 MHz 或 10 MHz。稳定后可尝试 20 MHz。

推荐调试顺序：

```text
5 MHz + SPI Mode 0
10 MHz + SPI Mode 0
5 MHz + SPI Mode 2
10 MHz + SPI Mode 2
```

---

## 5. 方向、偏移与显示窗口

### 5.1 竖屏配置

原 Arduino 竖屏配置：

```cpp
lcddev.dir = 0;
lcddev.width = 128;
lcddev.height = 128;
lcddev.setxcmd = 0x2A;
lcddev.setycmd = 0x2B;
LCD_WriteReg(0x36, 0xC8);
```

竖屏窗口偏移：

```cpp
x + 2
y + 3
```

ESP-IDF 中建议定义：

```c
#define TFT_WIDTH       128
#define TFT_HEIGHT      128
#define TFT_X_OFFSET    2
#define TFT_Y_OFFSET    3
#define TFT_MADCTL      0xC8
```

### 5.2 横屏配置

原 Arduino 横屏配置：

```cpp
lcddev.dir = 1;
lcddev.width = 128;
lcddev.height = 128;
lcddev.setxcmd = 0x2A;
lcddev.setycmd = 0x2B;
LCD_WriteReg(0x36, 0xA8);
```

横屏窗口偏移：

```cpp
x + 3
y + 2
```

ESP-IDF 中建议定义：

```c
#define TFT_WIDTH       128
#define TFT_HEIGHT      128
#define TFT_X_OFFSET    3
#define TFT_Y_OFFSET    2
#define TFT_MADCTL      0xA8
```

### 5.3 设置显示窗口

ST7735R 设置窗口的命令：

```text
0x2A: Column Address Set
0x2B: Row Address Set
0x2C: Memory Write
```

推荐 IDF 实现：

```c
static esp_err_t tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    x0 += TFT_X_OFFSET;
    x1 += TFT_X_OFFSET;
    y0 += TFT_Y_OFFSET;
    y1 += TFT_Y_OFFSET;

    uint8_t data[4];

    ESP_RETURN_ON_ERROR(tft_write_cmd(0x2A), TAG, "write 0x2A failed");
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    ESP_RETURN_ON_ERROR(tft_write_data(data, sizeof(data)), TAG, "write column failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0x2B), TAG, "write 0x2B failed");
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    ESP_RETURN_ON_ERROR(tft_write_data(data, sizeof(data)), TAG, "write row failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0x2C), TAG, "write 0x2C failed");

    return ESP_OK;
}
```

---

## 6. ST7735R 初始化序列

必须迁移原 Arduino `LCD_Init()` 中的初始化序列。

### 6.1 复位时序

原 Arduino：

```cpp
LCD_RST_CLR;
delay(100);
LCD_RST_SET;
delay(50);
```

IDF：

```c
gpio_set_level(TFT_PIN_RST, 0);
vTaskDelay(pdMS_TO_TICKS(100));
gpio_set_level(TFT_PIN_RST, 1);
vTaskDelay(pdMS_TO_TICKS(50));
```

### 6.2 初始化命令表

建议 Codex 用命令表方式实现初始化，便于维护。

初始化顺序如下：

```text
硬复位
0x11, delay 120ms        Sleep Out
0x13                     Normal Display Mode On
0xB1 01 2C 2D             Frame Rate Control 1
0xB2 01 2C 2D             Frame Rate Control 2
0xB3 01 2C 2D 01 2C 2D    Frame Rate Control 3
0xB4 07                   Column Inversion
0xC0 A2 02 84             Power Control 1
0xC1 C5                   Power Control 2
0xC2 0A 00                Power Control 3
0xC3 8A 2A                Power Control 4
0xC4 8A EE                Power Control 5
0xC5 0E                   VCOM Control
0x36 C8                   MADCTL，默认竖屏
0xE0 ...                  Positive Gamma
0xE1 ...                  Negative Gamma
0x2A 00 00 00 7F          Column Address Init
0x2B 00 00 00 9F          Row Address Init
0xF0 01                   Enable Test Command
0xF6 00                   Disable RAM Power Save Mode
0x3A 05                   Interface Pixel Format: RGB565
0x29                      Display On
设置 LCD 参数
打开背光
```

### 6.3 建议 IDF 初始化实现

```c
static esp_err_t tft_init_panel(void)
{
    tft_reset();

    ESP_RETURN_ON_ERROR(tft_write_cmd(0x11), TAG, "sleep out failed");
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_RETURN_ON_ERROR(tft_write_cmd(0x13), TAG, "normal display failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xB1), TAG, "B1 failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x01), TAG, "B1 data failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x2C), TAG, "B1 data failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x2D), TAG, "B1 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xB2), TAG, "B2 failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x01), TAG, "B2 data failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x2C), TAG, "B2 data failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x2D), TAG, "B2 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xB3), TAG, "B3 failed");
    const uint8_t b3[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
    ESP_RETURN_ON_ERROR(tft_write_data(b3, sizeof(b3)), TAG, "B3 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xB4), TAG, "B4 failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x07), TAG, "B4 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xC0), TAG, "C0 failed");
    const uint8_t c0[] = {0xA2, 0x02, 0x84};
    ESP_RETURN_ON_ERROR(tft_write_data(c0, sizeof(c0)), TAG, "C0 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xC1), TAG, "C1 failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0xC5), TAG, "C1 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xC2), TAG, "C2 failed");
    const uint8_t c2[] = {0x0A, 0x00};
    ESP_RETURN_ON_ERROR(tft_write_data(c2, sizeof(c2)), TAG, "C2 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xC3), TAG, "C3 failed");
    const uint8_t c3[] = {0x8A, 0x2A};
    ESP_RETURN_ON_ERROR(tft_write_data(c3, sizeof(c3)), TAG, "C3 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xC4), TAG, "C4 failed");
    const uint8_t c4[] = {0x8A, 0xEE};
    ESP_RETURN_ON_ERROR(tft_write_data(c4, sizeof(c4)), TAG, "C4 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xC5), TAG, "C5 failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x0E), TAG, "C5 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0x36), TAG, "MADCTL failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(TFT_MADCTL), TAG, "MADCTL data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xE0), TAG, "E0 failed");
    const uint8_t gamma_pos[] = {
        0x0F, 0x1A, 0x0F, 0x18,
        0x2F, 0x28, 0x20, 0x22,
        0x1F, 0x1B, 0x23, 0x37,
        0x00, 0x07, 0x02, 0x10
    };
    ESP_RETURN_ON_ERROR(tft_write_data(gamma_pos, sizeof(gamma_pos)), TAG, "E0 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xE1), TAG, "E1 failed");
    const uint8_t gamma_neg[] = {
        0x0F, 0x1B, 0x0F, 0x17,
        0x33, 0x2C, 0x29, 0x2E,
        0x30, 0x30, 0x39, 0x3F,
        0x00, 0x07, 0x03, 0x10
    };
    ESP_RETURN_ON_ERROR(tft_write_data(gamma_neg, sizeof(gamma_neg)), TAG, "E1 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0x2A), TAG, "2A failed");
    const uint8_t col[] = {0x00, 0x00, 0x00, 0x7F};
    ESP_RETURN_ON_ERROR(tft_write_data(col, sizeof(col)), TAG, "2A data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0x2B), TAG, "2B failed");
    const uint8_t row[] = {0x00, 0x00, 0x00, 0x9F};
    ESP_RETURN_ON_ERROR(tft_write_data(row, sizeof(row)), TAG, "2B data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xF0), TAG, "F0 failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x01), TAG, "F0 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0xF6), TAG, "F6 failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x00), TAG, "F6 data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0x3A), TAG, "3A failed");
    ESP_RETURN_ON_ERROR(tft_write_data_byte(0x05), TAG, "3A data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(0x29), TAG, "display on failed");
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(TFT_PIN_BL, 1);

    return ESP_OK;
}
```

---

## 7. 建议的 ESP-IDF 工程结构

建议 Codex 生成如下结构：

```text
main/
├── CMakeLists.txt
├── main.c
├── tft_st7735r.c
└── tft_st7735r.h
```

如果后续需要字库和图片：

```text
main/
├── assets/
│   ├── font_8x16.c
│   ├── font_8x16.h
│   ├── font_cn_16.c
│   └── font_cn_16.h
├── tft_st7735r.c
├── tft_st7735r.h
├── tft_gfx.c
└── tft_gfx.h
```

第一期建议只做：

```text
1. 初始化屏幕
2. 清屏
3. 画点
4. 填充矩形
5. 画线
6. 显示简单测试色块
```

第二期再做：

```text
1. 英文字体
2. 中文点阵字体
3. 图片显示
4. LVGL 接入
```

---

## 8. 建议对外 API

`tft_st7735r.h` 建议定义如下：

```c
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TFT_COLOR_BLACK    0x0000
#define TFT_COLOR_WHITE    0xFFFF
#define TFT_COLOR_RED      0xF800
#define TFT_COLOR_GREEN    0x07E0
#define TFT_COLOR_BLUE     0x001F
#define TFT_COLOR_YELLOW   0xFFE0
#define TFT_COLOR_CYAN     0x07FF
#define TFT_COLOR_MAGENTA  0xF81F
#define TFT_COLOR_GRAY     0x8430

typedef enum {
    TFT_ROTATION_0 = 0,
    TFT_ROTATION_90,
    TFT_ROTATION_180,
    TFT_ROTATION_270,
} tft_rotation_t;

esp_err_t tft_init(void);
esp_err_t tft_set_backlight(bool on);
esp_err_t tft_set_rotation(tft_rotation_t rotation);
esp_err_t tft_fill_screen(uint16_t color);
esp_err_t tft_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
esp_err_t tft_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
esp_err_t tft_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
esp_err_t tft_draw_bitmap_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels);
uint16_t tft_width(void);
uint16_t tft_height(void);

#ifdef __cplusplus
}
#endif
```

说明：

1. `tft_init()` 完成 GPIO、SPI、LCD 控制器初始化，并打开背光。
2. `tft_fill_screen()` 应使用整行缓存批量发送，不要逐点 `spi_device_transmit()`。
3. `tft_draw_pixel()` 可用于调试，但大量绘图不要逐点调用。
4. `tft_fill_rect()` 是后续 UI 绘制的基础接口。
5. `tft_draw_bitmap_rgb565()` 中的 `pixels` 按主机端 `uint16_t` RGB565 输入，驱动内部负责转成高字节先发。

---

## 9. 关键实现要求

### 9.1 不要逐像素启动 SPI 事务

错误方式：

```c
for each pixel:
    tft_write_data(two_bytes, 2);
```

这样会极慢。

推荐方式：

```c
准备一行或一块像素缓冲区
一次发送 128 * 2 字节，或更多
```

对于 128×128 屏，单帧数据大小：

```text
128 * 128 * 2 = 32768 bytes
```

可以选择：

```text
一行缓存：128 * 2 = 256 bytes
多行缓存：128 * 16 * 2 = 4096 bytes
全屏缓存：128 * 128 * 2 = 32768 bytes
```

第一期推荐使用一行缓存，简单且稳定。

### 9.2 RGB565 转 SPI 字节序

输入颜色为：

```c
uint16_t color = 0xF800;
```

发送时：

```c
buf[0] = color >> 8;
buf[1] = color & 0xFF;
```

不要直接把 `uint16_t *` 强转为 `uint8_t *` 发送，否则在小端 CPU 上字节顺序会反。

### 9.3 坐标边界检查

所有对外绘图 API 必须做边界检查。

例如：

```c
if (x >= TFT_WIDTH || y >= TFT_HEIGHT) {
    return ESP_ERR_INVALID_ARG;
}
```

矩形填充需要裁剪：

```c
if (x + w > TFT_WIDTH) {
    w = TFT_WIDTH - x;
}
if (y + h > TFT_HEIGHT) {
    h = TFT_HEIGHT - y;
}
```

### 9.4 显示窗口坐标使用闭区间

ST7735R 的地址窗口使用：

```text
x0, y0, x1, y1
```

其中 `x1`、`y1` 是包含在内的结束坐标。

因此填充一个宽 `w`、高 `h` 的矩形时应调用：

```c
tft_set_window(x, y, x + w - 1, y + h - 1);
```

### 9.5 关于原 Arduino `LCD_Fill()` 的注意事项

原代码：

```cpp
u16 width = ex - sx + 1;
u16 height = ey - sy + 1;
LCD_SetWindows(sx, sy, ex - 1, ey - 1);
```

这里存在接口语义混杂的问题：

1. `width/height` 按 `ex/ey` 为闭区间计算。
2. `LCD_SetWindows()` 又传入 `ex - 1`、`ey - 1`。

IDF 版本不要照搬这个细节。建议明确 API：

```c
tft_fill_rect(x, y, w, h, color)
```

内部统一：

```c
tft_set_window(x, y, x + w - 1, y + h - 1)
```

---

## 10. `main.c` 测试程序建议

生成驱动后，先用如下逻辑验证：

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "tft_st7735r.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(tft_init());

    while (1) {
        ESP_LOGI(TAG, "fill red");
        ESP_ERROR_CHECK(tft_fill_screen(TFT_COLOR_RED));
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "fill green");
        ESP_ERROR_CHECK(tft_fill_screen(TFT_COLOR_GREEN));
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "fill blue");
        ESP_ERROR_CHECK(tft_fill_screen(TFT_COLOR_BLUE));
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "draw primitives");
        ESP_ERROR_CHECK(tft_fill_screen(TFT_COLOR_BLACK));
        ESP_ERROR_CHECK(tft_draw_line(0, 0, 127, 127, TFT_COLOR_WHITE));
        ESP_ERROR_CHECK(tft_draw_line(0, 127, 127, 0, TFT_COLOR_YELLOW));
        ESP_ERROR_CHECK(tft_fill_rect(10, 10, 30, 30, TFT_COLOR_RED));
        ESP_ERROR_CHECK(tft_fill_rect(50, 10, 30, 30, TFT_COLOR_GREEN));
        ESP_ERROR_CHECK(tft_fill_rect(90, 10, 30, 30, TFT_COLOR_BLUE));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

预期现象：

```text
1. 屏幕背光点亮
2. 屏幕依次显示红、绿、蓝
3. 黑底上出现两条对角线和三个色块
```

---

## 11. CMakeLists.txt 示例

`main/CMakeLists.txt`：

```cmake
idf_component_register(
    SRCS
        "main.c"
        "tft_st7735r.c"
    INCLUDE_DIRS
        "."
)
```

如果后续拆出 `tft_gfx.c`：

```cmake
idf_component_register(
    SRCS
        "main.c"
        "tft_st7735r.c"
        "tft_gfx.c"
    INCLUDE_DIRS
        "."
)
```

---

## 12. 常见问题排查

### 12.1 背光不亮

检查：

```c
gpio_set_level(TFT_PIN_BL, 1);
```

如果仍不亮：

1. 确认 GPIO13 是否被其他外设占用。
2. 确认开发板上背光是否为高电平点亮。原 Arduino 代码使用 `LCD_LED_SET` 点亮背光，因此应为高电平点亮。

### 12.2 背光亮，但屏幕无图像

依次检查：

1. `TFT_PIN_DC` 是否为 GPIO17。
2. `TFT_PIN_RST` 是否为 GPIO16。
3. `TFT_PIN_CS` 是否为 GPIO26。
4. SPI 是否使用 GPIO18 / GPIO23。
5. `tft_reset()` 是否执行。
6. 初始化序列是否完整迁移。
7. 尝试 SPI Mode 2。
8. SPI 频率降到 1 MHz 或 5 MHz。

### 12.3 图像整体偏移

检查偏移：

竖屏：

```c
#define TFT_X_OFFSET 2
#define TFT_Y_OFFSET 3
```

横屏：

```c
#define TFT_X_OFFSET 3
#define TFT_Y_OFFSET 2
```

### 12.4 红蓝颜色反了

尝试修改 `MADCTL` 的 RGB/BGR 位。

默认竖屏：

```c
0xC8
```

可尝试：

```c
0xC0
0x08
0x00
```

但优先保持原 Arduino 的 `0xC8`。

### 12.5 画面花屏

处理顺序：

```text
1. SPI 频率降到 1 MHz
2. 尝试 SPI mode 0 / 2
3. 检查 RGB565 字节序
4. 检查 CS 是否由 SPI 外设控制
5. 检查 DC 是否在发送前正确切换
6. 检查初始化延时，尤其是 0x11 后的 120ms
```

### 12.6 显示区域不是 128×128

注意原初始化里有：

```text
0x2A: 00 00 00 7F     列：0 到 127
0x2B: 00 00 00 9F     行：0 到 159
```

这说明控制器内部显存可能是 128×160，但实际面板显示区域是 128×128，需要通过偏移定位。

不要把实际屏幕高度改成 160。对外 API 仍然按 128×128。

---

## 13. 不建议第一期实现的内容

第一期不要做：

```text
1. 不要直接接 LVGL
2. 不要先做中文字体
3. 不要先做图片解码
4. 不要先做触摸屏，因为这块资料中没有 TFT 触摸信息
5. 不要依赖 Arduino 库
6. 不要照搬 lcd.h 中错误的 LCD_W/LCD_H
```

第一期只验证：

```text
SPI + GPIO + ST7735R 初始化 + RGB565 刷屏
```

---

## 14. Codex 开发任务说明

请按照以下要求生成 ESP-IDF 代码：

```text
你正在为 ESP-32F 开发板实现板载 1.44 寸 TFT LCD 驱动。
开发环境为 ESP-IDF，不使用 Arduino。
屏幕控制器按 ST7735R / ST7735 兼容处理。
屏幕分辨率为 128x128，RGB565，SPI 写入。

硬件引脚：
- SCLK: GPIO18
- MOSI: GPIO23
- MISO: 不使用，配置为 -1
- CS: GPIO26
- DC/RS: GPIO17
- RST: GPIO16
- BL/LED: GPIO13

SPI：
- 使用 SPI2_HOST
- 初始频率 10MHz
- SPI mode 0
- 如果需要调试，允许改为 5MHz 或 mode 2

实现文件：
- main/tft_st7735r.h
- main/tft_st7735r.c
- main/main.c

必须实现 API：
- esp_err_t tft_init(void)
- esp_err_t tft_set_backlight(bool on)
- esp_err_t tft_fill_screen(uint16_t color)
- esp_err_t tft_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
- esp_err_t tft_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
- esp_err_t tft_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
- uint16_t tft_width(void)
- uint16_t tft_height(void)

初始化序列必须使用本文档第 6 节的 ST7735R 初始化命令。
像素数据必须按 RGB565 高字节在前、低字节在后发送。
默认方向使用竖屏：MADCTL = 0xC8，X_OFFSET = 2，Y_OFFSET = 3。
设置窗口时使用 0x2A / 0x2B / 0x2C。
对外坐标范围为 0..127。

main.c 中需要提供测试：
- 红、绿、蓝、白、黑全屏切换
- 黑底画两条对角线
- 显示三个色块

代码要求：
- 使用 ESP_ERROR_CHECK 或返回 esp_err_t
- 所有对外 API 做参数检查
- 不要逐像素创建 SPI 事务进行大面积刷屏
- 不依赖 Arduino.h、SPI.h、delay、pinMode、digitalWrite
- 使用 gpio_config、spi_bus_initialize、spi_bus_add_device、spi_device_polling_transmit
```

---

## 15. 原 Arduino 到 IDF 的映射表

| Arduino API / 宏 | IDF 对应实现 |
|---|---|
| `pinMode(pin, OUTPUT)` | `gpio_config()` |
| `digitalWrite(pin, value)` | `gpio_set_level(pin, value)` |
| `delay(ms)` | `vTaskDelay(pdMS_TO_TICKS(ms))` |
| `SPI.begin(sclk, miso, mosi, cs)` | `spi_bus_initialize()` + `spi_bus_add_device()` |
| `SPI.write(data)` | `spi_device_polling_transmit()` |
| `LCD_RS_CLR` | `gpio_set_level(TFT_PIN_DC, 0)` |
| `LCD_RS_SET` | `gpio_set_level(TFT_PIN_DC, 1)` |
| `LCD_LED_SET` | `gpio_set_level(TFT_PIN_BL, 1)` |
| `LCD_RST_CLR / SET` | `gpio_set_level(TFT_PIN_RST, 0/1)` |
| `LCD_WR_REG()` | `tft_write_cmd()` |
| `LCD_WR_DATA()` | `tft_write_data_byte()` / `tft_write_data()` |
| `LCD_WR_DATA_16Bit()` | RGB565 高字节、低字节发送 |
| `LCD_SetWindows()` | `tft_set_window()` |
| `LCD_Clear()` | `tft_fill_screen()` |
| `LCD_DrawPoint()` | `tft_draw_pixel()` |
| `LCD_Fill()` | `tft_fill_rect()` |

---

## 16. 推荐实现顺序

Codex 应按以下顺序开发，避免一次性生成过多复杂功能：

```text
1. 新建 tft_st7735r.h，定义 API、颜色常量、屏幕尺寸。
2. 新建 tft_st7735r.c，实现 GPIO 初始化。
3. 实现 SPI 初始化。
4. 实现 tft_write_cmd / tft_write_data / tft_write_data_byte。
5. 实现 reset。
6. 迁移 ST7735R 初始化序列。
7. 实现 set_window。
8. 实现 fill_screen。
9. 实现 draw_pixel。
10. 实现 fill_rect。
11. 实现 draw_line。
12. 编写 main.c 刷色测试。
13. 编译修复。
14. 烧录验证。
15. 如有显示异常，再调 SPI mode、频率、MADCTL、offset。
```

---

## 17. 最小验收标准

驱动完成后，应满足：

```text
1. 上电后屏幕背光点亮。
2. tft_init() 返回 ESP_OK。
3. tft_fill_screen(RED/GREEN/BLUE/WHITE/BLACK) 能正确全屏显示颜色。
4. tft_draw_pixel(0, 0, WHITE) 能在左上角附近显示白点。
5. tft_draw_line(0, 0, 127, 127, WHITE) 能显示对角线。
6. tft_fill_rect(10, 10, 30, 30, RED) 能显示 30x30 红色矩形。
7. 画面没有明显整体偏移；如偏移，微调 X/Y offset。
```

