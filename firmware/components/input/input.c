#include "input.h"
#include "system.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "input";

/* ── Config ───────────────────────────────────────────────────────────── */

#define INPUT_QUEUE_DEPTH       16
#define BTN_DEBOUNCE_MS         30
#define BTN_LONG_PRESS_MS       800
#define PWR_LONG_PRESS_MS       2000
#define ENC_DEBOUNCE_US         2000    /* ignore encoder pulses faster than this */

/* ── Module state ─────────────────────────────────────────────────────── */

QueueHandle_t g_input_queue;

static int s_pin_enc_a;
static int s_pin_enc_b;
static int s_pin_enc_sw;
static int s_pin_pwr;

/*
 * Raw ISR event — tells the task which pin fired, and when.
 * The task does all debounce and direction logic in normal context.
 */
typedef enum {
    RAW_ENC_PULSE,
    RAW_ENC_SW_CHANGE,
    RAW_PWR_CHANGE,
} raw_evt_type_e;

typedef struct {
    raw_evt_type_e type;
    int            level;   /* GPIO level at interrupt time */
    int64_t        time_us; /* esp_timer_get_time() at interrupt time */
} raw_event_t;

static QueueHandle_t s_raw_queue;

/* ── ISR handlers ─────────────────────────────────────────────────────── */

static void IRAM_ATTR enc_a_isr(void *arg)
{
    raw_event_t e = {
        .type    = RAW_ENC_PULSE,
        .level   = gpio_get_level(s_pin_enc_b),   /* read DT at CLK edge */
        .time_us = esp_timer_get_time(),
    };
    xQueueSendFromISR(s_raw_queue, &e, NULL);
}

static void IRAM_ATTR enc_sw_isr(void *arg)
{
    raw_event_t e = {
        .type    = RAW_ENC_SW_CHANGE,
        .level   = gpio_get_level(s_pin_enc_sw),
        .time_us = esp_timer_get_time(),
    };
    xQueueSendFromISR(s_raw_queue, &e, NULL);
}

static void IRAM_ATTR pwr_isr(void *arg)
{
    raw_event_t e = {
        .type    = RAW_PWR_CHANGE,
        .level   = gpio_get_level(s_pin_pwr),
        .time_us = esp_timer_get_time(),
    };
    xQueueSendFromISR(s_raw_queue, &e, NULL);
}

/* ── GPIO init ────────────────────────────────────────────────────────── */

static esp_err_t gpio_init_input_pullup(int pin)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io);
}

/* GPIO 34–39 on ESP32 are input-only — no internal pull-up/down */
static esp_err_t gpio_init_input_only(int pin)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io);
}

/* ── Public: init ─────────────────────────────────────────────────────── */

esp_err_t input_init(const input_cfg_t *cfg)
{
    esp_err_t err;

    s_pin_enc_a  = cfg->pin_enc_a;
    s_pin_enc_b  = cfg->pin_enc_b;
    s_pin_enc_sw = cfg->pin_enc_sw;
    s_pin_pwr    = cfg->pin_pwr_btn;

    /* Configure input pins — enc_sw (GPIO35) is input-only, needs external pull-up */
    err  = gpio_init_input_pullup(cfg->pin_enc_a);
    err |= gpio_init_input_pullup(cfg->pin_enc_b);
    err |= gpio_init_input_only(cfg->pin_enc_sw);   /* GPIO35: input-only, no internal PU */
    err |= gpio_init_input_pullup(cfg->pin_pwr_btn);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed (pins: enc_a=%d enc_b=%d enc_sw=%d pwr=%d)",
                 cfg->pin_enc_a, cfg->pin_enc_b, cfg->pin_enc_sw, cfg->pin_pwr_btn);
        return err;
    }

    /* Encoder A fires on falling edge (CLK goes low at each detent) */
    gpio_set_intr_type(cfg->pin_enc_a, GPIO_INTR_NEGEDGE);

    /* Button and power button fire on any edge for debounce tracking */
    gpio_set_intr_type(cfg->pin_enc_sw,  GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(cfg->pin_pwr_btn, GPIO_INTR_ANYEDGE);

    s_raw_queue   = xQueueCreate(32, sizeof(raw_event_t));
    g_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_raw_queue || !g_input_queue) {
        ESP_LOGE(TAG, "queue creation failed");
        return ESP_ERR_NO_MEM;
    }

    /* Install GPIO ISR service and hook our handlers */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        /* ESP_ERR_INVALID_STATE means already installed — that's fine */
        ESP_LOGE(TAG, "gpio_install_isr_service: %s", esp_err_to_name(err));
        return err;
    }

    gpio_isr_handler_add(cfg->pin_enc_a,  enc_a_isr,  NULL);
    gpio_isr_handler_add(cfg->pin_enc_sw, enc_sw_isr, NULL);
    gpio_isr_handler_add(cfg->pin_pwr_btn, pwr_isr,   NULL);

    ESP_LOGI(TAG, "init complete");
    return ESP_OK;
}

/* ── Public: task ─────────────────────────────────────────────────────── */

void input_task(void *pvParameters)
{
    raw_event_t raw;

    int64_t last_enc_us   = 0;
    int64_t enc_sw_press  = 0;   /* time of last SW falling edge */
    int64_t pwr_press     = 0;   /* time of last PWR falling edge */

    ESP_LOGI(TAG, "task started");
    ESP_LOGI(TAG, "stack hwm: %d words", uxTaskGetStackHighWaterMark(NULL));

    for (;;) {
        if (xQueueReceive(s_raw_queue, &raw, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (raw.type) {

        case RAW_ENC_PULSE: {
            /* Debounce: ignore pulses arriving within ENC_DEBOUNCE_US */
            if ((raw.time_us - last_enc_us) < ENC_DEBOUNCE_US) break;
            last_enc_us = raw.time_us;

            /*
             * EC11 direction: on CLK falling edge, read DT.
             *   DT = 1 (high) → clockwise
             *   DT = 0 (low)  → counter-clockwise
             */
            input_event_t evt = {
                .type = (raw.level == 1) ? INPUT_EVT_ROT_CW : INPUT_EVT_ROT_CCW,
            };
            xQueueSend(g_input_queue, &evt, 0);
            break;
        }

        case RAW_ENC_SW_CHANGE: {
            if (raw.level == 0) {
                /* Falling edge — button pressed, record time */
                enc_sw_press = raw.time_us;
            } else {
                /* Rising edge — button released, classify */
                if (enc_sw_press == 0) break;   /* no press recorded */

                int64_t held_ms = (raw.time_us - enc_sw_press) / 1000;
                enc_sw_press = 0;

                if (held_ms < BTN_DEBOUNCE_MS) break;  /* noise */

                input_event_t evt = {
                    .type = (held_ms >= BTN_LONG_PRESS_MS)
                                ? INPUT_EVT_LONG_PRESS
                                : INPUT_EVT_CLICK,
                };
                xQueueSend(g_input_queue, &evt, 0);
            }
            break;
        }

        case RAW_PWR_CHANGE: {
            if (raw.level == 0) {
                /* Falling edge — power button pressed */
                pwr_press = raw.time_us;
            } else {
                /* Rising edge — classify */
                if (pwr_press == 0) break;

                int64_t held_ms = (raw.time_us - pwr_press) / 1000;
                pwr_press = 0;

                if (held_ms < BTN_DEBOUNCE_MS) break;

                if (held_ms >= PWR_LONG_PRESS_MS) {
                    /* Long press → request deep sleep via event group */
                    input_event_t evt = { .type = INPUT_EVT_PWR_LONG };
                    xQueueSend(g_input_queue, &evt, 0);
                    xEventGroupSetBits(g_sys_events, SYS_EVT_REQ_DEEP_SLEEP);
                    xEventGroupSetBits(g_sys_events, SYS_EVT_SLEEP_READY_INPUT);
                    vTaskDelete(NULL);
                } else {
                    input_event_t evt = { .type = INPUT_EVT_PWR_PRESS };
                    xQueueSend(g_input_queue, &evt, 0);
                }
            }
            break;
        }
        }
    }
}
