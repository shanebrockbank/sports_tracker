#pragma once

/*
 * ILI9341 private interface — used only within the display component.
 * Do not include this header outside of ili9341.c / display.c.
 */

#include "display.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include <stdint.h>

/* ── ILI9341 register addresses ──────────────────────────────────────── */

#define ILI9341_SWRESET   0x01  /* Software reset            */
#define ILI9341_RDDID     0x04  /* Read display ID           */
#define ILI9341_SLPIN     0x10  /* Sleep in                  */
#define ILI9341_SLPOUT    0x11  /* Sleep out                 */
#define ILI9341_INVOFF    0x20  /* Display inversion off     */
#define ILI9341_INVON     0x21  /* Display inversion on      */
#define ILI9341_GAMMASET  0x26  /* Gamma set                 */
#define ILI9341_DISPOFF   0x28  /* Display off               */
#define ILI9341_DISPON    0x29  /* Display on                */
#define ILI9341_CASET     0x2A  /* Column address set        */
#define ILI9341_PASET     0x2B  /* Page (row) address set    */
#define ILI9341_RAMWR     0x2C  /* Memory write              */
#define ILI9341_MADCTL    0x36  /* Memory access control     */
#define ILI9341_PIXFMT    0x3A  /* Pixel format              */
#define ILI9341_FRMCTR1   0xB1  /* Frame rate control        */
#define ILI9341_DFUNCTR   0xB6  /* Display function control  */
#define ILI9341_PWCTR1    0xC0  /* Power control 1           */
#define ILI9341_PWCTR2    0xC1  /* Power control 2           */
#define ILI9341_VMCTR1    0xC5  /* VCOM control 1            */
#define ILI9341_VMCTR2    0xC7  /* VCOM control 2            */
#define ILI9341_GMCTRP1   0xE0  /* Positive gamma correction */
#define ILI9341_GMCTRN1   0xE1  /* Negative gamma correction */

/* ── MADCTL bits ──────────────────────────────────────────────────────── */

#define MADCTL_MY   0x80  /* Row address order: bottom-to-top */
#define MADCTL_MX   0x40  /* Column address order: right-to-left */
#define MADCTL_MV   0x20  /* Row/column exchange (transpose) */
#define MADCTL_ML   0x10  /* Vertical refresh order */
#define MADCTL_BGR  0x08  /* BGR colour order (vs RGB) */
#define MADCTL_MH   0x04  /* Horizontal refresh order */

/*
 * Landscape orientation: 320 wide × 240 tall, origin top-left.
 * MV=1 swaps row/col; MX=1 mirrors X so scan goes left→right.
 * BGR=1 because the ILI9341 on common 2.8" modules uses BGR order.
 */
#define ILI9341_MADCTL_LANDSCAPE  (MADCTL_MX | MADCTL_MV | MADCTL_BGR)

/* ── DMA pixel buffer ─────────────────────────────────────────────────── */

/* Number of RGB565 pixels sent per DMA chunk.  Must be DMA-word-aligned. */
#define ILI9341_DMA_PIXELS  1024   /* 2 kB per transfer */

/* ── Internal API (called from display.c) ─────────────────────────────── */

esp_err_t ili9341_init(const display_cfg_t *cfg);
void      ili9341_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void      ili9341_write_pixels(const uint16_t *data, uint32_t count);
void      ili9341_fill_color(uint16_t color, uint32_t count);
