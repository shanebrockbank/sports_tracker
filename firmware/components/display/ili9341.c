#include "ili9341.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "ili9341";

/* ── Module state ─────────────────────────────────────────────────────── */

static spi_device_handle_t s_spi;
static int                 s_pin_dc;
static int                 s_pin_bl;

/* DMA-capable pixel buffer for large fills */
static uint16_t *s_dma_buf;

/* ── SPI helpers ──────────────────────────────────────────────────────── */

/*
 * Pre-transfer callback: drives the DC pin before each SPI transaction.
 * The transaction's `user` field encodes DC state (0 = command, 1 = data).
 */
static void IRAM_ATTR ili9341_pre_transfer_cb(spi_transaction_t *t)
{
    gpio_set_level(s_pin_dc, (int)t->user);
}

static void ili9341_write_cmd(uint8_t cmd)
{
    spi_transaction_t t = {
        .length    = 8,
        .tx_buffer = &cmd,
        .user      = (void *)0,   /* DC = 0 → command */
    };
    spi_device_polling_transmit(s_spi, &t);
}

static void ili9341_write_data(const uint8_t *data, size_t len)
{
    if (len == 0) return;
    spi_transaction_t t = {
        .length    = len * 8,
        .tx_buffer = data,
        .user      = (void *)1,   /* DC = 1 → data */
    };
    spi_device_polling_transmit(s_spi, &t);
}

static void ili9341_write_byte(uint8_t b)
{
    ili9341_write_data(&b, 1);
}

/* ── Init sequence ────────────────────────────────────────────────────── */

static void ili9341_reset(int pin_rst)
{
    gpio_set_level(pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void ili9341_send_init_sequence(void)
{
    /* Software reset */
    ili9341_write_cmd(ILI9341_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* Power control */
    ili9341_write_cmd(ILI9341_PWCTR1);
    ili9341_write_byte(0x23);           /* VRH[5:0] = 4.60V */

    ili9341_write_cmd(ILI9341_PWCTR2);
    ili9341_write_byte(0x10);           /* SAP, BT[3:0] */

    /* VCOM */
    ili9341_write_cmd(ILI9341_VMCTR1);
    ili9341_write_byte(0x3E);
    ili9341_write_byte(0x28);

    ili9341_write_cmd(ILI9341_VMCTR2);
    ili9341_write_byte(0x86);

    /* Memory access control — landscape 320×240, BGR */
    ili9341_write_cmd(ILI9341_MADCTL);
    ili9341_write_byte(ILI9341_MADCTL_LANDSCAPE);

    /* Pixel format: 16-bit RGB565 */
    ili9341_write_cmd(ILI9341_PIXFMT);
    ili9341_write_byte(0x55);

    /* Frame rate: 79 Hz */
    ili9341_write_cmd(ILI9341_FRMCTR1);
    ili9341_write_byte(0x00);
    ili9341_write_byte(0x18);

    /* Display function control */
    ili9341_write_cmd(ILI9341_DFUNCTR);
    ili9341_write_byte(0x08);
    ili9341_write_byte(0x82);
    ili9341_write_byte(0x27);

    /* Gamma curve 1 */
    ili9341_write_cmd(ILI9341_GAMMASET);
    ili9341_write_byte(0x01);

    /* Positive gamma correction */
    ili9341_write_cmd(ILI9341_GMCTRP1);
    {
        static const uint8_t d[] = {
            0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
            0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03,
            0x0E, 0x09, 0x00
        };
        ili9341_write_data(d, sizeof(d));
    }

    /* Negative gamma correction */
    ili9341_write_cmd(ILI9341_GMCTRN1);
    {
        static const uint8_t d[] = {
            0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
            0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C,
            0x31, 0x36, 0x0F
        };
        ili9341_write_data(d, sizeof(d));
    }

    /* Sleep out — must wait 120 ms before sending next command */
    ili9341_write_cmd(ILI9341_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* Display on */
    ili9341_write_cmd(ILI9341_DISPON);
    vTaskDelay(pdMS_TO_TICKS(10));
}

/* ── Backlight ────────────────────────────────────────────────────────── */

static esp_err_t backlight_init(int pin_bl)
{
    ledc_timer_config_t timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz         = 5000,
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) return err;

    ledc_channel_config_t ch = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 255,          /* full brightness at startup */
        .gpio_num   = pin_bl,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0,
        .hpoint     = 0,
    };
    return ledc_channel_config(&ch);
}

/* ── Public: init ─────────────────────────────────────────────────────── */

esp_err_t ili9341_init(const display_cfg_t *cfg)
{
    esp_err_t err;

    s_pin_dc = cfg->pin_dc;
    s_pin_bl = cfg->pin_bl;

    /* DC and RST as outputs */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << cfg->pin_dc) | (1ULL << cfg->pin_rst),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&io);
    if (err != ESP_OK) return err;

    /* SPI bus */
    spi_bus_config_t bus = {
        .mosi_io_num     = cfg->pin_mosi,
        .miso_io_num     = cfg->pin_miso,
        .sclk_io_num     = cfg->pin_clk,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2,
    };
    err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return err;
    }

    /* SPI device — 40 MHz, mode 0 */
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode           = 0,
        .spics_io_num   = cfg->pin_cs,
        .queue_size     = 7,
        .pre_cb         = ili9341_pre_transfer_cb,
    };
    err = spi_bus_add_device(SPI2_HOST, &dev, &s_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
        return err;
    }

    /* DMA pixel buffer — must be in DMA-capable RAM */
    s_dma_buf = heap_caps_malloc(ILI9341_DMA_PIXELS * sizeof(uint16_t),
                                 MALLOC_CAP_DMA);
    if (!s_dma_buf) {
        ESP_LOGE(TAG, "failed to allocate DMA pixel buffer");
        return ESP_ERR_NO_MEM;
    }

    /* Hardware reset then init sequence */
    ili9341_reset(cfg->pin_rst);
    ili9341_send_init_sequence();

    /* Backlight */
    if (cfg->pin_bl >= 0) {
        err = backlight_init(cfg->pin_bl);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "backlight init failed: %s", esp_err_to_name(err));
        }
    }

    ESP_LOGI(TAG, "init complete — %dx%d landscape", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    return ESP_OK;
}

/* ── Public: drawing primitives ───────────────────────────────────────── */

void ili9341_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t d[4];

    /* Column address set */
    ili9341_write_cmd(ILI9341_CASET);
    d[0] = x1 >> 8; d[1] = x1 & 0xFF;
    d[2] = x2 >> 8; d[3] = x2 & 0xFF;
    ili9341_write_data(d, 4);

    /* Row (page) address set */
    ili9341_write_cmd(ILI9341_PASET);
    d[0] = y1 >> 8; d[1] = y1 & 0xFF;
    d[2] = y2 >> 8; d[3] = y2 & 0xFF;
    ili9341_write_data(d, 4);

    /* Enter write mode */
    ili9341_write_cmd(ILI9341_RAMWR);
}

void ili9341_write_pixels(const uint16_t *data, uint32_t count)
{
    /* data[] must already be in big-endian (byte-swapped) RGB565 */
    spi_transaction_t t = {
        .length    = count * 16,
        .tx_buffer = data,
        .user      = (void *)1,   /* DC = 1 → data */
    };
    spi_device_polling_transmit(s_spi, &t);
}

void ili9341_fill_color(uint16_t color, uint32_t count)
{
    /*
     * ILI9341 expects big-endian RGB565.  Swap bytes before filling
     * the DMA buffer so we can memcpy efficiently.
     */
    uint16_t be = (color >> 8) | (color << 8);

    /* Pre-fill the DMA buffer with the swapped colour */
    for (uint32_t i = 0; i < ILI9341_DMA_PIXELS; i++) {
        s_dma_buf[i] = be;
    }

    /* Send in ILI9341_DMA_PIXELS-sized chunks */
    while (count > 0) {
        uint32_t chunk = (count > ILI9341_DMA_PIXELS) ? ILI9341_DMA_PIXELS : count;
        spi_transaction_t t = {
            .length    = chunk * 16,
            .tx_buffer = s_dma_buf,
            .user      = (void *)1,
        };
        spi_device_polling_transmit(s_spi, &t);
        count -= chunk;
    }
}

void display_set_backlight(uint8_t pct)
{
    if (s_pin_bl < 0) return;
    if (pct > 100) pct = 100;
    uint32_t duty = (pct * 255) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
