#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── System state machine ─────────────────────────────────────────────── */

typedef enum {
    SYS_STATE_BOOT,
    SYS_STATE_INIT,
    SYS_STATE_IDLE,
    SYS_STATE_ACTIVITY_SETUP,
    SYS_STATE_ACTIVITY_ACTIVE,
    SYS_STATE_ACTIVITY_PAUSED,
    SYS_STATE_ACTIVITY_SAVING,
    SYS_STATE_SETTINGS,
    SYS_STATE_UPLOAD,
    SYS_STATE_DEEP_SLEEP,
} system_state_e;

/* ── Event group bits ─────────────────────────────────────────────────── */

#define SYS_EVT_STATE_CHANGED   (1 << 0)
#define SYS_EVT_ACTIVITY_START  (1 << 1)
#define SYS_EVT_ACTIVITY_STOP   (1 << 2)
#define SYS_EVT_LOW_BATTERY     (1 << 3)
#define SYS_EVT_GPS_FIX         (1 << 4)

/* ── Sensor data stubs — populated in later phases ────────────────────── */
/* These are forward-declared here so system_state_t compiles in Phase 1. */
/* Each component will replace its stub with a real struct in its phase.  */

typedef struct { uint8_t _reserved; } imu_data_t;
typedef struct { uint8_t _reserved; } gps_data_t;
typedef struct { uint8_t _reserved; } baro_data_t;
typedef struct { uint8_t _reserved; } fused_data_t;
typedef struct { uint8_t _reserved; } activity_data_t;

typedef struct {
    float   bus_voltage_v;
    float   current_ma;
    float   power_mw;
    uint8_t battery_pct;    /* 0–100 */
    bool    charging;
} power_data_t;

/* ── Shared system state ──────────────────────────────────────────────── */
/* Written by sensor/activity tasks under g_state_mutex.                 */
/* display_task takes a snapshot on its own refresh cadence.             */

typedef struct {
    imu_data_t      imu;
    gps_data_t      gps;
    baro_data_t     baro;
    fused_data_t    fused;
    activity_data_t activity;
    power_data_t    power;
} system_state_t;

/* Shared state — always write under g_state_mutex */
extern system_state_t     g_state;
extern SemaphoreHandle_t  g_state_mutex;
extern EventGroupHandle_t g_sys_events;

/* ── API ──────────────────────────────────────────────────────────────── */

void           system_init(void);
void           system_set_state(system_state_e new_state);
system_state_e system_get_state(void);
void           system_enter_deep_sleep(void);
