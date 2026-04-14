#include "pin_config.h"
#include "system.h"
#include "display.h"
#include "input.h"
#include "power.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char *TAG = "main";

/* ── Task stack sizes and priorities ──────────────────────────────────── */
/*
 * Phase 1 tasks: power_task, input_task, display_task (minimal).
 * Sensor, GPS, activity, storage, BLE tasks are added in later phases.
 */

#define STACK_POWER_TASK    3072
#define STACK_INPUT_TASK    2048
#define STACK_DISPLAY_TASK  4096

#define PRI_POWER_TASK      (configMAX_PRIORITIES - 2)   /* High  */
#define PRI_INPUT_TASK      (configMAX_PRIORITIES - 3)   /* Medium */
#define PRI_DISPLAY_TASK    (configMAX_PRIORITIES - 3)   /* Medium */

/* ── Phase 1 display task ─────────────────────────────────────────────── */
/*
 * Minimal display_task for Phase 1: reflects the current system state on
 * screen using the draw primitives.  Full UI with menus and data fields
 * is implemented in Phase 2.
 */
static void display_task(void *pvParameters)
{
    static const char *state_names[] = {
        "BOOT", "INIT", "IDLE", "ACT_SETUP",
        "ACTIVE", "PAUSED", "SAVING", "SETTINGS",
        "UPLOAD", "DEEP_SLEEP",
    };

    system_state_e last_state = (system_state_e)-1;

    ESP_LOGI("display_task", "started");
    ESP_LOGI("display_task", "stack hwm: %d words", uxTaskGetStackHighWaterMark(NULL));

    for (;;) {
        /* Wait for a state change, then redraw */
        xEventGroupWaitBits(g_sys_events, SYS_EVT_STATE_CHANGED,
                            pdTRUE,   /* clear on exit */
                            pdFALSE,
                            pdMS_TO_TICKS(200));

        system_state_e state = system_get_state();
        if (state == SYS_STATE_DEEP_SLEEP) {
            xEventGroupSetBits(g_sys_events, SYS_EVT_SLEEP_READY_DISPLAY);
            vTaskDelete(NULL);
        }
        if (state == last_state) continue;
        last_state = state;

        display_clear(COLOR_BLACK);

        /* Title bar — top */
        display_fill_rect(0, 0, DISPLAY_WIDTH, 20, COLOR_DKGRAY);
        display_draw_string(4, 6, "Sports Tracker", COLOR_WHITE, COLOR_DKGRAY, 1);

        /* State label — middle */
        const char *label = (state < (int)(sizeof(state_names) / sizeof(state_names[0])))
                                ? state_names[state] : "UNKNOWN";
        display_draw_string(60, 100, "State:", COLOR_GRAY, COLOR_BLACK, 2);
        display_draw_string(60, 120, label,   COLOR_GREEN, COLOR_BLACK, 2);

        /* Battery info — bottom */
        power_data_t pwr = state_get_power();
        char batt_str[16];
        snprintf(batt_str, sizeof(batt_str), "Batt: %d%%", pwr.battery_pct);
        display_draw_string(4, DISPLAY_HEIGHT - 20, batt_str,
                            COLOR_YELLOW, COLOR_BLACK, 1);
    }
}

/* ── Hardware init ────────────────────────────────────────────────────── */

static esp_err_t hw_init(void)
{
    esp_err_t err;

    /* --- Power component first: gates peripherals on and inits I2C --- */
    power_cfg_t pwr_cfg = {
        .pin_pwr_gate = PIN_PWR_GATE,
        .pin_gps_en   = PIN_GPS_EN,
        .i2c_port     = I2C_NUM_0,
        .pin_sda      = PIN_I2C_SDA,
        .pin_scl      = PIN_I2C_SCL,
    };
    err = power_init(&pwr_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "power_init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* --- Display --- */
    display_cfg_t disp_cfg = {
        .pin_mosi = PIN_SPI_MOSI,
        .pin_miso = PIN_SPI_MISO,
        .pin_clk  = PIN_SPI_CLK,
        .pin_cs   = PIN_TFT_CS,
        .pin_dc   = PIN_TFT_DC,
        .pin_rst  = PIN_TFT_RST,
        .pin_bl   = PIN_TFT_BL,
    };
    err = display_init(&disp_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "display_init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* --- Input --- */
    input_cfg_t inp_cfg = {
        .pin_enc_a   = PIN_ENC_A,
        .pin_enc_b   = PIN_ENC_B,
        .pin_enc_sw  = PIN_ENC_SW,
        .pin_pwr_btn = PIN_PWR_BTN,
    };
    err = input_init(&inp_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "input_init failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/* ── app_main ─────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "Sports Tracker — Phase 1 Hardware Bring-up");

    /* Shared state and event group must exist before any task starts */
    system_init();
    system_set_state(SYS_STATE_INIT);

    /* Initialise hardware — abort on fatal error */
    esp_err_t err = hw_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "hw_init failed — halting");
        /* In production replace with a fault state and error screen */
        for (;;) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    /* Transition to IDLE */
    system_set_state(SYS_STATE_IDLE);

    /* Start tasks */
    xTaskCreatePinnedToCore(power_task,   "power",   STACK_POWER_TASK,
                            NULL, PRI_POWER_TASK,  NULL, 1);

    xTaskCreatePinnedToCore(input_task,   "input",   STACK_INPUT_TASK,
                            NULL, PRI_INPUT_TASK,  NULL, 0);

    xTaskCreatePinnedToCore(display_task, "display", STACK_DISPLAY_TASK,
                            NULL, PRI_DISPLAY_TASK, NULL, 0);

    ESP_LOGI(TAG, "all tasks started");

    /*
     * app_main acts as the Phase 1 state manager.
     * Tasks post SYS_EVT_REQ_DEEP_SLEEP; app_main is the only caller of
     * system_set_state(SYS_STATE_DEEP_SLEEP).
     */
    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(
            g_sys_events,
            SYS_EVT_REQ_DEEP_SLEEP,
            pdTRUE,          /* clear on exit */
            pdFALSE,
            portMAX_DELAY);

        if (bits & SYS_EVT_REQ_DEEP_SLEEP) {
            system_set_state(SYS_STATE_DEEP_SLEEP);
            /* system_set_state → system_enter_deep_sleep → does not return */
        }
    }
}
