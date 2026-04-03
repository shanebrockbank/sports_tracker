#include "display.h"
#include "ili9341.h"
#include "font.h"
#include "esp_log.h"

static const char *TAG = "display";

/* ── Init ─────────────────────────────────────────────────────────────── */

esp_err_t display_init(const display_cfg_t *cfg)
{
    esp_err_t err = ili9341_init(cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ILI9341 init failed: %s", esp_err_to_name(err));
        return err;
    }
    display_clear(COLOR_BLACK);
    return ESP_OK;
}

/* ── Primitives ───────────────────────────────────────────────────────── */

void display_clear(uint16_t color)
{
    ili9341_set_window(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
    ili9341_fill_color(color, (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT);
}

void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH  - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;

    ili9341_set_window(x, y, x + w - 1, y + h - 1);
    ili9341_fill_color(color, (uint32_t)w * h);
}

void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
    ili9341_set_window(x, y, x, y);
    ili9341_fill_color(color, 1);
}

void display_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    display_fill_rect(x, y, len, 1, color);
}

void display_draw_vline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    display_fill_rect(x, y, 1, len, color);
}

void display_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    display_draw_hline(x,         y,         w, color);
    display_draw_hline(x,         y + h - 1, w, color);
    display_draw_vline(x,         y,         h, color);
    display_draw_vline(x + w - 1, y,         h, color);
}

/* ── Text ─────────────────────────────────────────────────────────────── */

void display_draw_char(uint16_t x, uint16_t y, char c,
                       uint16_t fg, uint16_t bg, uint8_t scale)
{
    if ((uint8_t)c < FONT_FIRST_CHAR || (uint8_t)c > FONT_LAST_CHAR) {
        c = '?';
    }

    const uint8_t *glyph = g_font5x7[(uint8_t)c - FONT_FIRST_CHAR];

    for (uint8_t col = 0; col < FONT_CHAR_W; col++) {
        uint8_t col_bits = glyph[col];
        for (uint8_t row = 0; row < FONT_CHAR_H; row++) {
            uint16_t color = (col_bits & (1 << row)) ? fg : bg;
            if (scale == 1) {
                display_draw_pixel(x + col, y + row, color);
            } else {
                display_fill_rect(x + col * scale, y + row * scale,
                                  scale, scale, color);
            }
        }
    }

    /* One-pixel-wide gap column to the right, filled with bg */
    for (uint8_t row = 0; row < FONT_CHAR_H; row++) {
        if (scale == 1) {
            display_draw_pixel(x + FONT_CHAR_W, y + row, bg);
        } else {
            display_fill_rect(x + FONT_CHAR_W * scale, y + row * scale,
                              scale, scale, bg);
        }
    }
}

void display_draw_string(uint16_t x, uint16_t y, const char *str,
                         uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (!str || scale == 0) return;

    uint16_t cx = x;
    uint16_t cell_w = FONT_CELL_W * scale;   /* 6 px * scale */

    while (*str) {
        if (cx + cell_w > DISPLAY_WIDTH) break;   /* clip at right edge */
        display_draw_char(cx, y, *str, fg, bg, scale);
        cx += cell_w;
        str++;
    }
}
