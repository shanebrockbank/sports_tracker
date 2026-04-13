#include "power.h"
#include "ina226.h"
#include "system.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_pm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "power";

#define I2C_CLK_SPEED_HZ       400000
#define POWER_POLL_IDLE_MS      10000
#define POWER_POLL_ACTIVE_MS     1000
#define CHARGING_THRESHOLD_MA  -10.0f

/* ── Module state ─────────────────────────────────────────────────────── */

static int s_pin_pwr_gate;
static int s_pin_gps_en;
static power_profile_e s_current_profile = POWER_PROFILE_MAX;

/* ── LiPo discharge curve — percentage from bus voltage ──────────────── */
/*
 * Piecewise lookup: voltage → approximate SoC%.
 * Based on a standard 1S LiPo at 0.5C discharge.
 * Accuracy ±5%; replace with measured curve from actual cell.
 *
 * INA226 measures pack voltage AFTER the load.  Ensure the shunt is on
 * the correct side of the circuit.
 */
typedef struct { float v; uint8_t pct; } batt_curve_t;

static const batt_curve_t k_batt_curve[] = {
    { 4.20f, 100 },
    { 4.10f,  95 },
    { 4.00f,  88 },
    { 3.90f,  78 },
    { 3.80f,  66 },
    { 3.70f,  52 },
    { 3.60f,  37 },
    { 3.50f,  22 },
    { 3.40f,  11 },
    { 3.30f,   5 },
    { 3.00f,   0 },
};
#define BATT_CURVE_LEN (sizeof(k_batt_curve) / sizeof(k_batt_curve[0]))

static uint8_t voltage_to_pct(float v)
{
    if (v >= k_batt_curve[0].v) return 100;
    if (v <= k_batt_curve[BATT_CURVE_LEN - 1].v) return 0;

    for (int i = 0; i < (int)BATT_CURVE_LEN - 1; i++) {
        if (v <= k_batt_curve[i].v && v > k_batt_curve[i + 1].v) {
            /* Linear interpolation between the two table entries */
            float range_v   = k_batt_curve[i].v     - k_batt_curve[i + 1].v;
            float range_pct = k_batt_curve[i].pct   - k_batt_curve[i + 1].pct;
            float offset    = (v - k_batt_curve[i + 1].v) / range_v;
            return (uint8_t)(k_batt_curve[i + 1].pct + (uint8_t)(offset * range_pct));
        }
    }
    return 0;
}

/* ── GPIO power gate ──────────────────────────────────────────────────── */

void power_gate_on(void)
{
    gpio_set_level(s_pin_pwr_gate, 1);
    ESP_LOGD(TAG, "peripheral power gate: ON");
}

void power_gate_off(void)
{
    gpio_set_level(s_pin_pwr_gate, 0);
    ESP_LOGD(TAG, "peripheral power gate: OFF");
}

void power_gps_enable(bool on)
{
    gpio_set_level(s_pin_gps_en, on ? 1 : 0);
    ESP_LOGD(TAG, "GPS: %s", on ? "ON" : "OFF");
}

/* ── Power profiles ───────────────────────────────────────────────────── */

void power_set_profile(power_profile_e profile)
{
    if (profile == s_current_profile) return;

    /*
     * CPU frequency scaling via esp_pm_configure().
     * Requires CONFIG_PM_ENABLE=y in sdkconfig.
     * If power management is not enabled these calls do nothing but log.
     */
    esp_pm_config_t pm = {0};

    switch (profile) {
    case POWER_PROFILE_MAX:
        pm.max_freq_mhz = 240;
        pm.min_freq_mhz = 240;
        break;
    case POWER_PROFILE_STANDARD:
        pm.max_freq_mhz = 160;
        pm.min_freq_mhz = 80;
        break;
    case POWER_PROFILE_ENDURANCE:
        pm.max_freq_mhz = 80;
        pm.min_freq_mhz = 40;
        break;
    case POWER_PROFILE_ULTRA:
        pm.max_freq_mhz = 40;
        pm.min_freq_mhz = 40;
        break;
    }

    pm.light_sleep_enable = false;  /* phase 1: no light sleep */

    esp_err_t err = esp_pm_configure(&pm);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_pm_configure: %s (CONFIG_PM_ENABLE enabled?)",
                 esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "power profile: %d → max %d MHz", (int)profile, pm.max_freq_mhz);
    s_current_profile = profile;
}

/* ── Public: init ─────────────────────────────────────────────────────── */

esp_err_t power_init(const power_cfg_t *cfg)
{
    esp_err_t err;

    s_pin_pwr_gate = cfg->pin_pwr_gate;
    s_pin_gps_en   = cfg->pin_gps_en;

    /* Configure power gate and GPS enable as outputs */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << cfg->pin_pwr_gate) | (1ULL << cfg->pin_gps_en),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    /* GPS off until activity starts */
    power_gps_enable(false);

    /* Gate peripherals on — display and INA226 need power */
    power_gate_on();
    vTaskDelay(pdMS_TO_TICKS(10));  /* settle time after power-on */

    /* I2C bus init */
    i2c_config_t i2c = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = cfg->pin_sda,
        .scl_io_num       = cfg->pin_scl,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_CLK_SPEED_HZ,
    };
    err = i2c_param_config(cfg->i2c_port, &i2c);
    if (err != ESP_OK) return err;

    err = i2c_driver_install(cfg->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install: %s", esp_err_to_name(err));
        return err;
    }

    /* INA226 */
    err = ina226_init(cfg->i2c_port);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "INA226 init failed — power data unavailable");
        /* Non-fatal: the device can operate without battery monitoring */
    }

    ESP_LOGI(TAG, "init complete");
    return ESP_OK;
}

/* ── Public: task ─────────────────────────────────────────────────────── */

void power_task(void *pvParameters)
{
    ESP_LOGI(TAG, "task started");
    ESP_LOGI(TAG, "stack hwm: %d words", uxTaskGetStackHighWaterMark(NULL));

    bool low_battery_warned = false;

    for (;;) {
        /*
         * Poll rate: 1 Hz during activity, 0.1 Hz (10s) in idle.
         * Phase 1: fixed at 1 Hz until system state is wired up.
         */
        uint32_t poll_ms = (system_get_state() == SYS_STATE_IDLE) ? POWER_POLL_IDLE_MS : POWER_POLL_ACTIVE_MS;
        vTaskDelay(pdMS_TO_TICKS(poll_ms));

        if (system_get_state() == SYS_STATE_DEEP_SLEEP) {
            xEventGroupSetBits(g_sys_events, SYS_EVT_SLEEP_READY_POWER);
            vTaskDelete(NULL);
        }

        /* Read INA226 — non-fatal on error, keep last values */
        float v = 0.0f, i_ma = 0.0f, p_mw = 0.0f;
        bool read_ok = true;

        if (ina226_read_bus_voltage(&v)   != ESP_OK) read_ok = false;
        if (ina226_read_current(&i_ma)    != ESP_OK) read_ok = false;
        if (ina226_read_power(&p_mw)      != ESP_OK) read_ok = false;

        if (!read_ok) {
            ESP_LOGW(TAG, "INA226 read error");
            continue;
        }

        uint8_t pct = voltage_to_pct(v);

        ESP_LOGD(TAG, "%.3fV  %.1fmA  %.1fmW  %d%%", v, i_ma, p_mw, pct);

        /* Update shared state */
        power_data_t pwr = {
            .bus_voltage_v = v,
            .current_ma    = i_ma,
            .power_mw      = p_mw,
            .battery_pct   = pct,
            .charging      = (i_ma < CHARGING_THRESHOLD_MA),
        };
        state_set_power(&pwr);

        /* Low battery warning */
        if (pct <= BATT_WARN_PCT && !low_battery_warned) {
            ESP_LOGW(TAG, "low battery: %d%%", pct);
            xEventGroupSetBits(g_sys_events, SYS_EVT_LOW_BATTERY);
            low_battery_warned = true;
        }

        /* Critical: auto-save and sleep */
        if (pct <= BATT_CRITICAL_PCT) {
            ESP_LOGE(TAG, "critical battery %d%% — requesting deep sleep", pct);
            xEventGroupSetBits(g_sys_events, SYS_EVT_REQ_DEEP_SLEEP);
            xEventGroupSetBits(g_sys_events, SYS_EVT_SLEEP_READY_POWER);
            vTaskDelete(NULL);
        }

        /* Reset warning flag when battery recovers (e.g. charger connected) */
        if (pct > BATT_WARN_PCT + 5) {
            low_battery_warned = false;
        }
    }
}
