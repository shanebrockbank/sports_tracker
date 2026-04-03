#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Power profiles ───────────────────────────────────────────────────── */
/*
 * CPU frequency and peripheral state per profile.
 * Actual current draw should be measured with INA226 and logged in
 * docs/power_budget.md once hardware is available.
 *
 * Profile         CPU      GPS     IMU   BLE   Display   Est. current
 * MAX             240MHz   10Hz    On    On    Full      ~180mA
 * STANDARD        160MHz    5Hz    On    On    Full      ~120mA
 * ENDURANCE        80MHz    1Hz    On    Off   Dim        ~60mA
 * ULTRA            40MHz    1Hz    Off   Off   Off        ~20mA
 */
typedef enum {
    POWER_PROFILE_MAX,
    POWER_PROFILE_STANDARD,
    POWER_PROFILE_ENDURANCE,
    POWER_PROFILE_ULTRA,
} power_profile_e;

/* ── Battery thresholds ───────────────────────────────────────────────── */

#define BATT_WARN_PCT       15  /* Low battery warning */
#define BATT_CRITICAL_PCT    5  /* Auto-save and sleep */

/* ── Init config ──────────────────────────────────────────────────────── */

typedef struct {
    int   pin_pwr_gate;   /* MOSFET gate: HIGH = peripherals on */
    int   pin_gps_en;     /* GPS enable: HIGH = on, LOW = off   */
    int   i2c_port;       /* I2C port number (I2C_NUM_0 / 1)   */
    int   pin_sda;
    int   pin_scl;
} power_cfg_t;

/* ── API ──────────────────────────────────────────────────────────────── */

/*
 * Initialise I2C bus, INA226, and peripheral gate GPIO.
 * Gates peripherals on immediately after init.
 */
esp_err_t power_init(const power_cfg_t *cfg);

/*
 * Apply a power profile: set CPU frequency, configure peripheral enables.
 * Call from power_task or state machine transitions only.
 */
void power_set_profile(power_profile_e profile);

/*
 * Enable or disable the peripheral power gate (MOSFET).
 * Call power_gate_off() before entering deep sleep.
 */
void power_gate_on(void);
void power_gate_off(void);

/*
 * Enable or disable GPS power.
 */
void power_gps_enable(bool on);

/*
 * FreeRTOS task — monitors INA226, updates g_state.power under mutex,
 * fires SYS_EVT_LOW_BATTERY when threshold is crossed.
 * Priority: high; stack 3072 bytes.
 */
void power_task(void *pvParameters);
