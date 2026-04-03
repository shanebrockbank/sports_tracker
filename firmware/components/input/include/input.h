#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include <stdint.h>

/* ── Input event types ────────────────────────────────────────────────── */

typedef enum {
    INPUT_EVT_ROT_CW,        /* Encoder rotated clockwise          */
    INPUT_EVT_ROT_CCW,       /* Encoder rotated counter-clockwise  */
    INPUT_EVT_CLICK,         /* Encoder button short press         */
    INPUT_EVT_LONG_PRESS,    /* Encoder button held > threshold    */
    INPUT_EVT_PWR_PRESS,     /* Power button short press           */
    INPUT_EVT_PWR_LONG,      /* Power button held > threshold → sleep */
} input_event_e;

typedef struct {
    input_event_e type;
} input_event_t;

/* ── Init config ──────────────────────────────────────────────────────── */

typedef struct {
    int pin_enc_a;      /* EC11 CLK pin  */
    int pin_enc_b;      /* EC11 DT  pin  */
    int pin_enc_sw;     /* EC11 SW  pin  */
    int pin_pwr_btn;    /* Power button  */
} input_cfg_t;

/* ── API ──────────────────────────────────────────────────────────────── */

/*
 * Initialise GPIO, install ISRs, create the input event queue.
 * Must be called before input_task_start().
 */
esp_err_t input_init(const input_cfg_t *cfg);

/*
 * FreeRTOS task entry point.
 * Create with xTaskCreate and pass NULL as pvParameters.
 * Priority: medium; stack 2048 bytes is sufficient.
 */
void input_task(void *pvParameters);

/*
 * Queue handle — other tasks may pend on this to receive input events.
 * Valid only after input_init() returns ESP_OK.
 */
extern QueueHandle_t g_input_queue;
