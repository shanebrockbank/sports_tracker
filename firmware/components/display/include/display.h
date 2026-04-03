#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Screen geometry ──────────────────────────────────────────────────── */

#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240

/* ── RGB565 colour helpers ────────────────────────────────────────────── */

#define RGB565(r, g, b) \
    ((uint16_t)(((uint16_t)((r) & 0xF8) << 8) | \
                ((uint16_t)((g) & 0xFC) << 3) | \
                ((uint16_t)((b) & 0xFF) >> 3)))

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     RGB565(255,   0,   0)
#define COLOR_GREEN   RGB565(  0, 255,   0)
#define COLOR_BLUE    RGB565(  0,   0, 255)
#define COLOR_YELLOW  RGB565(255, 255,   0)
#define COLOR_CYAN    RGB565(  0, 255, 255)
#define COLOR_ORANGE  RGB565(255, 165,   0)
#define COLOR_GRAY    RGB565(128, 128, 128)
#define COLOR_DKGRAY  RGB565( 64,  64,  64)

/* ── Font metrics (5×7 bitmap, 1 px column gap) ───────────────────────── */

#define FONT_CHAR_W   5   /* active pixel columns */
#define FONT_CHAR_H   7   /* active pixel rows     */
#define FONT_CELL_W   6   /* character cell width including gap */
#define FONT_CELL_H   8   /* character cell height including gap */

/* ── Init config ──────────────────────────────────────────────────────── */

typedef struct {
    int pin_mosi;
    int pin_miso;
    int pin_clk;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    int pin_bl;     /* backlight PWM; -1 to skip LEDC init */
} display_cfg_t;

/* ── API ──────────────────────────────────────────────────────────────── */

esp_err_t display_init(const display_cfg_t *cfg);

/* Backlight level 0–100 (percent).  Requires pin_bl != -1 in cfg. */
void display_set_backlight(uint8_t pct);

/* Primitive drawing — all coordinates are screen-space pixels. */
void display_clear(uint16_t color);
void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void display_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
void display_draw_vline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
void display_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/*
 * Text — scale=1 → 6×8 px cell, scale=2 → 12×16, etc.
 * No wrapping: text is clipped at screen edge.
 */
void display_draw_char(uint16_t x, uint16_t y, char c,
                       uint16_t fg, uint16_t bg, uint8_t scale);
void display_draw_string(uint16_t x, uint16_t y, const char *str,
                         uint16_t fg, uint16_t bg, uint8_t scale);
