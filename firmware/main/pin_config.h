#pragma once

/*
 * Pin assignments for ESP32 DevKit V1 breadboard prototype.
 *
 * Update this file when wiring changes.  Once the PCB is finalised,
 * migrate to Kconfig (menuconfig) so assignments can be changed without
 * editing source.
 *
 * SPI bus uses VSPI (SPI2_HOST) on standard ESP32 IO-matrix pins.
 * I2C uses the default master port (I2C_NUM_0).
 * GPS uses UART2.
 *
 * NOTE: GPIO16/17 are used for PSRAM on some WROVER modules.
 * The DevKit V1 (WROOM-32, no PSRAM) leaves them free for UART2.
 *
 * Verify against your actual wiring before first flash.
 */

/* ── SPI bus (shared: display, SD card, LoRa) ────────────────────────── */
#define PIN_SPI_MOSI    23
#define PIN_SPI_MISO    19
#define PIN_SPI_CLK     18

/* ── ILI9341 TFT display ──────────────────────────────────────────────── */
#define PIN_TFT_CS       5
#define PIN_TFT_DC      27
#define PIN_TFT_RST      4
#define PIN_TFT_BL      15   /* PWM backlight via LEDC channel 0 */

/* ── SD card ──────────────────────────────────────────────────────────── */
#define PIN_SD_CS       13

/* ── LoRa RFM95W ──────────────────────────────────────────────────────── */
#define PIN_LORA_CS     12
#define PIN_LORA_IRQ    34   /* DIO0 — input only */

/* ── I2C bus (ICM-20948, BMP390, INA226) ─────────────────────────────── */
#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22

/* ── GPS NEO-M8N (UART2) ──────────────────────────────────────────────── */
#define PIN_GPS_RX      16   /* ESP32 RX ← GPS TX */
#define PIN_GPS_TX      17   /* ESP32 TX → GPS RX */
#define PIN_GPS_PPS     39   /* 1 Hz pulse — input only */
#define PIN_GPS_EN      26   /* Power enable: HIGH = on */

/* ── EC11 rotary encoder ──────────────────────────────────────────────── */
#define PIN_ENC_A       33   /* CLK */
#define PIN_ENC_B       32   /* DT  */
#define PIN_ENC_SW      35   /* SW  — input only */

/* ── Power button (also RTC wake source) ─────────────────────────────── */
#define PIN_PWR_BTN      0   /* Boot button on DevKit; active low */

/* ── Buzzer ───────────────────────────────────────────────────────────── */
#define PIN_BUZZER       2   /* PWM capable */

/* ── Peripheral power gate (P-channel MOSFET gate) ───────────────────── */
/*
 * HIGH = peripherals powered ON  (MOSFET driver circuit inverts if needed)
 * LOW  = peripherals cut off
 */
#define PIN_PWR_GATE    25
