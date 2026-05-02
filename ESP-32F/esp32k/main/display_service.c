#include "display_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_status.h"

#define TFT_SPI_HOST SPI2_HOST
#define TFT_PIN_MOSI GPIO_NUM_23
#define TFT_PIN_CLK GPIO_NUM_18
#define TFT_PIN_CS GPIO_NUM_26
#define TFT_PIN_DC GPIO_NUM_17
#define TFT_PIN_RST GPIO_NUM_16
#define TFT_PIN_BL GPIO_NUM_13

#define TFT_WIDTH 128
#define TFT_HEIGHT 128
#define TFT_X_OFFSET 2
#define TFT_Y_OFFSET 3
#define TFT_ROW_BUFFER_PIXELS 128
#define TFT_TASK_STACK_SIZE 4096
#define TFT_TASK_PRIORITY 4
#define TFT_REFRESH_MS 500

#define TFT_MADCTL 0xC8

#define COLOR_WHITE 0xFFFF
#define COLOR_BLACK 0x0000

#define ST7735_SLPOUT 0x11
#define ST7735_NORON 0x13
#define ST7735_DISPON 0x29
#define ST7735_CASET 0x2A
#define ST7735_RASET 0x2B
#define ST7735_RAMWR 0x2C
#define ST7735_MADCTL 0x36
#define ST7735_COLMOD 0x3A

static const char *TAG = "display_service";

typedef struct {
    uint8_t command;
    const uint8_t *data;
    uint8_t data_len;
    uint16_t delay_ms;
} st7735_init_cmd_t;

static spi_device_handle_t s_tft_spi;
static TaskHandle_t s_display_task;

static const uint8_t FONT_DIGITS[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
};

static const uint8_t FONT_UPPER[26][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
};

static const uint8_t FONT_LOWER[26][5] = {
    {0x20, 0x54, 0x54, 0x54, 0x78}, {0x7F, 0x48, 0x44, 0x44, 0x38},
    {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7F},
    {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7E, 0x09, 0x01, 0x02},
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, {0x7F, 0x08, 0x04, 0x04, 0x78},
    {0x00, 0x44, 0x7D, 0x40, 0x00}, {0x20, 0x40, 0x44, 0x3D, 0x00},
    {0x7F, 0x10, 0x28, 0x44, 0x00}, {0x00, 0x41, 0x7F, 0x40, 0x00},
    {0x7C, 0x04, 0x18, 0x04, 0x78}, {0x7C, 0x08, 0x04, 0x04, 0x78},
    {0x38, 0x44, 0x44, 0x44, 0x38}, {0x7C, 0x14, 0x14, 0x14, 0x08},
    {0x08, 0x14, 0x14, 0x18, 0x7C}, {0x7C, 0x08, 0x04, 0x04, 0x08},
    {0x48, 0x54, 0x54, 0x54, 0x20}, {0x04, 0x3F, 0x44, 0x40, 0x20},
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, {0x1C, 0x20, 0x40, 0x20, 0x1C},
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, {0x44, 0x28, 0x10, 0x28, 0x44},
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, {0x44, 0x64, 0x54, 0x4C, 0x44},
};

static const uint8_t FONT_COLON[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
static const uint8_t FONT_DOT[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
static const uint8_t FONT_DASH[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
static const uint8_t FONT_UNDERSCORE[5] = {0x40, 0x40, 0x40, 0x40, 0x40};
static const uint8_t FONT_SLASH[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
static const uint8_t FONT_HASH[5] = {0x14, 0x7F, 0x14, 0x7F, 0x14};
static const uint8_t FONT_QUESTION[5] = {0x02, 0x01, 0x51, 0x09, 0x06};
static const uint8_t FONT_SPACE[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t CMD_FRMCTR1[] = {0x01, 0x2C, 0x2D};
static const uint8_t CMD_FRMCTR2[] = {0x01, 0x2C, 0x2D};
static const uint8_t CMD_FRMCTR3[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
static const uint8_t CMD_INVCTR[] = {0x07};
static const uint8_t CMD_PWCTR1[] = {0xA2, 0x02, 0x84};
static const uint8_t CMD_PWCTR2[] = {0xC5};
static const uint8_t CMD_PWCTR3[] = {0x0A, 0x00};
static const uint8_t CMD_PWCTR4[] = {0x8A, 0x2A};
static const uint8_t CMD_PWCTR5[] = {0x8A, 0xEE};
static const uint8_t CMD_VMCTR1[] = {0x0E};
static const uint8_t CMD_MADCTL[] = {TFT_MADCTL};
static const uint8_t CMD_GMCTRP1[] = {
    0x0F, 0x1A, 0x0F, 0x18, 0x2F, 0x28, 0x20, 0x22,
    0x1F, 0x1B, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10,
};
static const uint8_t CMD_GMCTRN1[] = {
    0x0F, 0x1B, 0x0F, 0x17, 0x33, 0x2C, 0x29, 0x2E,
    0x30, 0x30, 0x39, 0x3F, 0x00, 0x07, 0x03, 0x10,
};
static const uint8_t CMD_CASET_INIT[] = {0x00, 0x00, 0x00, 0x7F};
static const uint8_t CMD_RASET_INIT[] = {0x00, 0x00, 0x00, 0x9F};
static const uint8_t CMD_ENABLE_TEST[] = {0x01};
static const uint8_t CMD_DISABLE_RAM_SAVE[] = {0x00};
static const uint8_t CMD_COLMOD[] = {0x05};

static const st7735_init_cmd_t ST7735_INIT_CMDS[] = {
    {ST7735_SLPOUT, NULL, 0, 120},
    {ST7735_NORON, NULL, 0, 0},
    {0xB1, CMD_FRMCTR1, sizeof(CMD_FRMCTR1), 0},
    {0xB2, CMD_FRMCTR2, sizeof(CMD_FRMCTR2), 0},
    {0xB3, CMD_FRMCTR3, sizeof(CMD_FRMCTR3), 0},
    {0xB4, CMD_INVCTR, sizeof(CMD_INVCTR), 0},
    {0xC0, CMD_PWCTR1, sizeof(CMD_PWCTR1), 0},
    {0xC1, CMD_PWCTR2, sizeof(CMD_PWCTR2), 0},
    {0xC2, CMD_PWCTR3, sizeof(CMD_PWCTR3), 0},
    {0xC3, CMD_PWCTR4, sizeof(CMD_PWCTR4), 0},
    {0xC4, CMD_PWCTR5, sizeof(CMD_PWCTR5), 0},
    {0xC5, CMD_VMCTR1, sizeof(CMD_VMCTR1), 0},
    {ST7735_MADCTL, CMD_MADCTL, sizeof(CMD_MADCTL), 0},
    {0xE0, CMD_GMCTRP1, sizeof(CMD_GMCTRP1), 0},
    {0xE1, CMD_GMCTRN1, sizeof(CMD_GMCTRN1), 0},
    {ST7735_CASET, CMD_CASET_INIT, sizeof(CMD_CASET_INIT), 0},
    {ST7735_RASET, CMD_RASET_INIT, sizeof(CMD_RASET_INIT), 0},
    {0xF0, CMD_ENABLE_TEST, sizeof(CMD_ENABLE_TEST), 0},
    {0xF6, CMD_DISABLE_RAM_SAVE, sizeof(CMD_DISABLE_RAM_SAVE), 0},
    {ST7735_COLMOD, CMD_COLMOD, sizeof(CMD_COLMOD), 0},
    {ST7735_DISPON, NULL, 0, 20},
};

static esp_err_t display_send_bytes(const uint8_t *data, size_t length, bool is_data)
{
    spi_transaction_t transaction = {0};

    if (length == 0) {
        return ESP_OK;
    }

    gpio_set_level(TFT_PIN_DC, is_data ? 1 : 0);
    transaction.length = length * 8;
    transaction.tx_buffer = data;
    return spi_device_polling_transmit(s_tft_spi, &transaction);
}

static esp_err_t display_send_command(uint8_t command)
{
    return display_send_bytes(&command, 1, false);
}

static esp_err_t display_send_data(const uint8_t *data, size_t length)
{
    return display_send_bytes(data, length, true);
}

static esp_err_t display_send_init_sequence(void)
{
    for (size_t i = 0; i < sizeof(ST7735_INIT_CMDS) / sizeof(ST7735_INIT_CMDS[0]); ++i) {
        ESP_RETURN_ON_ERROR(display_send_command(ST7735_INIT_CMDS[i].command), TAG,
                            "command 0x%02x failed", ST7735_INIT_CMDS[i].command);
        ESP_RETURN_ON_ERROR(display_send_data(ST7735_INIT_CMDS[i].data, ST7735_INIT_CMDS[i].data_len),
                            TAG, "data for 0x%02x failed", ST7735_INIT_CMDS[i].command);
        if (ST7735_INIT_CMDS[i].delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(ST7735_INIT_CMDS[i].delay_ms));
        }
    }

    return ESP_OK;
}

static void display_backlight_set(bool on)
{
    gpio_set_level(TFT_PIN_BL, on ? 1 : 0);
}

static void display_reset_panel(void)
{
    gpio_set_level(TFT_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(TFT_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static esp_err_t display_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];
    const uint16_t panel_x0 = x0 + TFT_X_OFFSET;
    const uint16_t panel_x1 = x1 + TFT_X_OFFSET;
    const uint16_t panel_y0 = y0 + TFT_Y_OFFSET;
    const uint16_t panel_y1 = y1 + TFT_Y_OFFSET;

    ESP_RETURN_ON_ERROR(display_send_command(ST7735_CASET), TAG, "set column command failed");
    data[0] = (uint8_t)(panel_x0 >> 8);
    data[1] = (uint8_t)panel_x0;
    data[2] = (uint8_t)(panel_x1 >> 8);
    data[3] = (uint8_t)panel_x1;
    ESP_RETURN_ON_ERROR(display_send_data(data, sizeof(data)), TAG, "set column data failed");

    ESP_RETURN_ON_ERROR(display_send_command(ST7735_RASET), TAG, "set row command failed");
    data[0] = (uint8_t)(panel_y0 >> 8);
    data[1] = (uint8_t)panel_y0;
    data[2] = (uint8_t)(panel_y1 >> 8);
    data[3] = (uint8_t)panel_y1;
    ESP_RETURN_ON_ERROR(display_send_data(data, sizeof(data)), TAG, "set row data failed");

    return display_send_command(ST7735_RAMWR);
}

static esp_err_t display_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    uint8_t row_buffer[TFT_ROW_BUFFER_PIXELS * 2];
    size_t pixels_to_write;

    if (width == 0 || height == 0 || x >= TFT_WIDTH || y >= TFT_HEIGHT) {
        return ESP_OK;
    }
    if ((x + width) > TFT_WIDTH) {
        width = TFT_WIDTH - x;
    }
    if ((y + height) > TFT_HEIGHT) {
        height = TFT_HEIGHT - y;
    }

    for (size_t i = 0; i < sizeof(row_buffer); i += 2) {
        row_buffer[i] = (uint8_t)(color >> 8);
        row_buffer[i + 1] = (uint8_t)color;
    }

    pixels_to_write = (size_t)width * height;
    ESP_RETURN_ON_ERROR(display_set_address_window(x, y, x + width - 1, y + height - 1), TAG,
                        "set address window failed");

    while (pixels_to_write > 0) {
        size_t chunk_pixels = pixels_to_write;
        if (chunk_pixels > TFT_ROW_BUFFER_PIXELS) {
            chunk_pixels = TFT_ROW_BUFFER_PIXELS;
        }
        ESP_RETURN_ON_ERROR(display_send_data(row_buffer, chunk_pixels * 2), TAG,
                            "write pixels failed");
        pixels_to_write -= chunk_pixels;
    }

    return ESP_OK;
}

static const uint8_t *display_get_glyph(char c)
{
    if (c >= '0' && c <= '9') {
        return FONT_DIGITS[c - '0'];
    }
    if (c >= 'A' && c <= 'Z') {
        return FONT_UPPER[c - 'A'];
    }
    if (c >= 'a' && c <= 'z') {
        return FONT_LOWER[c - 'a'];
    }
    switch (c) {
    case ':':
        return FONT_COLON;
    case '.':
        return FONT_DOT;
    case '-':
        return FONT_DASH;
    case '_':
        return FONT_UNDERSCORE;
    case '/':
        return FONT_SLASH;
    case '#':
        return FONT_HASH;
    case '?':
        return FONT_QUESTION;
    case ' ':
        return FONT_SPACE;
    default:
        return FONT_QUESTION;
    }
}

static esp_err_t display_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg,
                                   uint16_t scale)
{
    const uint8_t *glyph = display_get_glyph(c);

    for (uint16_t col = 0; col < 6; ++col) {
        uint8_t bits = (col < 5) ? glyph[col] : 0;
        for (uint16_t row = 0; row < 8; ++row) {
            const uint16_t color = (bits & 0x01) ? fg : bg;
            ESP_RETURN_ON_ERROR(display_fill_rect(x + col * scale, y + row * scale, scale, scale, color),
                                TAG, "draw char failed");
            bits >>= 1;
        }
    }

    return ESP_OK;
}

static esp_err_t display_draw_string(uint16_t x, uint16_t y, const char *text, uint16_t fg,
                                     uint16_t bg, uint16_t scale, size_t max_chars)
{
    const uint16_t char_width = 6 * scale;
    size_t count = 0;

    while (*text != '\0' && count < max_chars && x < TFT_WIDTH) {
        ESP_RETURN_ON_ERROR(display_draw_char(x, y, *text, fg, bg, scale), TAG,
                            "draw string failed");
        x += char_width;
        text++;
        count++;
    }

    return ESP_OK;
}

static void display_sanitize_ascii(char *dest, size_t dest_size, const char *src,
                                   size_t offset, size_t max_chars)
{
    size_t out = 0;
    size_t in = 0;

    if (dest_size == 0) {
        return;
    }

    while (src != NULL && src[in] != '\0' && in < offset) {
        in++;
    }
    while (src != NULL && src[in] != '\0' && out < max_chars && out < (dest_size - 1)) {
        unsigned char c = (unsigned char)src[in++];
        dest[out++] = (c >= 32 && c <= 126) ? (char)c : '?';
    }
    dest[out] = '\0';
}

static esp_err_t display_render_status(const system_status_snapshot_t *snapshot)
{
    char line[32];
    char app_text[17];
    char title_text[15];
    char message_text[17];
    char message_continuation[22];

    ESP_RETURN_ON_ERROR(display_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BLACK), TAG,
                        "clear display failed");

    ESP_RETURN_ON_ERROR(display_draw_string(4, 0, "George ANCS", COLOR_WHITE, COLOR_BLACK, 1, 21), TAG,
                        "draw title failed");

    snprintf(line, sizeof(line), "IP: %s", snapshot->ip_address);
    ESP_RETURN_ON_ERROR(display_draw_string(4, 16, line, COLOR_WHITE, COLOR_BLACK, 1, 21), TAG,
                        "draw ip failed");

    snprintf(line, sizeof(line), "WiFi: %s", snapshot->wifi_connected ? "yes" : "no");
    ESP_RETURN_ON_ERROR(display_draw_string(4, 32, line, COLOR_WHITE, COLOR_BLACK, 1, 21), TAG,
                        "draw wifi failed");

    snprintf(line, sizeof(line), "ANCS: %s", snapshot->ancs_connected ? "ready" : "wait");
    ESP_RETURN_ON_ERROR(display_draw_string(4, 48, line, COLOR_WHITE, COLOR_BLACK, 1, 21), TAG,
                        "draw ancs failed");

    display_sanitize_ascii(app_text, sizeof(app_text), snapshot->ancs_notification.app_id, 0, 16);
    snprintf(line, sizeof(line), "App: %s", app_text);
    ESP_RETURN_ON_ERROR(display_draw_string(4, 64, line, COLOR_WHITE, COLOR_BLACK, 1, 21), TAG,
                        "draw app failed");

    display_sanitize_ascii(title_text, sizeof(title_text), snapshot->ancs_notification.title, 0, 14);
    snprintf(line, sizeof(line), "Title: %s", title_text);
    ESP_RETURN_ON_ERROR(display_draw_string(4, 80, line, COLOR_WHITE, COLOR_BLACK, 1, 21), TAG,
                        "draw title text failed");

    display_sanitize_ascii(message_text, sizeof(message_text), snapshot->ancs_notification.message, 0, 16);
    snprintf(line, sizeof(line), "Msg: %s", message_text);
    ESP_RETURN_ON_ERROR(display_draw_string(4, 96, line, COLOR_WHITE, COLOR_BLACK, 1, 21), TAG,
                        "draw message failed");

    display_sanitize_ascii(message_continuation, sizeof(message_continuation),
                           snapshot->ancs_notification.message, 16, 21);
    ESP_RETURN_ON_ERROR(display_draw_string(4, 112, message_continuation, COLOR_WHITE, COLOR_BLACK, 1, 21), TAG,
                        "draw message continuation failed");

    return ESP_OK;
}

static bool display_snapshot_equals(const system_status_snapshot_t *lhs,
                                    const system_status_snapshot_t *rhs)
{
    return lhs->wifi_connected == rhs->wifi_connected &&
           lhs->device_state == rhs->device_state &&
           lhs->ble_connected == rhs->ble_connected &&
           lhs->ancs_connected == rhs->ancs_connected &&
           lhs->last_source == rhs->last_source &&
           lhs->last_result_code == rhs->last_result_code &&
           lhs->led.color_r == rhs->led.color_r &&
           lhs->led.color_g == rhs->led.color_g &&
           lhs->led.color_b == rhs->led.color_b &&
           lhs->led.brightness == rhs->led.brightness &&
           lhs->led.mode == rhs->led.mode &&
           strcmp(lhs->wifi_ssid, rhs->wifi_ssid) == 0 &&
           strcmp(lhs->ip_address, rhs->ip_address) == 0 &&
           strcmp(lhs->last_result_msg, rhs->last_result_msg) == 0 &&
           strcmp(lhs->ancs_notification.app_id, rhs->ancs_notification.app_id) == 0 &&
           strcmp(lhs->ancs_notification.title, rhs->ancs_notification.title) == 0 &&
           strcmp(lhs->ancs_notification.subtitle, rhs->ancs_notification.subtitle) == 0 &&
           strcmp(lhs->ancs_notification.message, rhs->ancs_notification.message) == 0 &&
           lhs->ancs_notification.notification_uid == rhs->ancs_notification.notification_uid &&
           lhs->ancs_notification.category_id == rhs->ancs_notification.category_id &&
           lhs->last_ancs_rule_matched == rhs->last_ancs_rule_matched &&
           strcmp(lhs->last_ancs_rule_label, rhs->last_ancs_rule_label) == 0;
}

static void display_task(void *arg)
{
    system_status_snapshot_t last_snapshot = {0};
    bool has_snapshot = false;

    (void)arg;

    while (true) {
        system_status_snapshot_t snapshot = {0};
        system_status_get_snapshot(&snapshot);
        if (!has_snapshot || !display_snapshot_equals(&snapshot, &last_snapshot)) {
            ESP_ERROR_CHECK(display_render_status(&snapshot));
            last_snapshot = snapshot;
            has_snapshot = true;
        }
        vTaskDelay(pdMS_TO_TICKS(TFT_REFRESH_MS));
    }
}

esp_err_t display_service_init(void)
{
    const gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << TFT_PIN_RST) | (1ULL << TFT_PIN_DC) | (1ULL << TFT_PIN_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const spi_bus_config_t bus_config = {
        .mosi_io_num = TFT_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = TFT_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * 2,
    };
    const spi_device_interface_config_t dev_config = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = TFT_PIN_CS,
        .queue_size = 1,
    };

    if (s_display_task != NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing ST7735R display");
    ESP_LOGI(TAG, "Panel config: MOSI=%d CLK=%d CS=%d DC=%d RST=%d BL=%d offset=(%d,%d)",
             TFT_PIN_MOSI, TFT_PIN_CLK, TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST, TFT_PIN_BL,
             TFT_X_OFFSET, TFT_Y_OFFSET);

    ESP_RETURN_ON_ERROR(gpio_config(&io_config), TAG, "gpio config failed");
    display_backlight_set(false);
    gpio_set_level(TFT_PIN_DC, 0);
    gpio_set_level(TFT_PIN_RST, 1);

    ESP_RETURN_ON_ERROR(spi_bus_initialize(TFT_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO), TAG,
                        "spi bus init failed");
    ESP_RETURN_ON_ERROR(spi_bus_add_device(TFT_SPI_HOST, &dev_config, &s_tft_spi), TAG,
                        "spi add device failed");

    display_reset_panel();
    ESP_RETURN_ON_ERROR(display_send_init_sequence(), TAG, "init sequence failed");
    ESP_RETURN_ON_ERROR(display_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BLACK), TAG,
                        "clear display failed");
    display_backlight_set(true);

    if (xTaskCreate(display_task, "display_task", TFT_TASK_STACK_SIZE, NULL,
                    TFT_TASK_PRIORITY, &s_display_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Display service ready");
    return ESP_OK;
}
