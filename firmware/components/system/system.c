#include "system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

static const char *TAG = "system";

/* ── Shared state ─────────────────────────────────────────────────────── */

static system_state_t g_state;
SemaphoreHandle_t  g_state_mutex;
EventGroupHandle_t g_sys_events;

static system_state_e s_state = SYS_STATE_BOOT;

/* ── Public API ───────────────────────────────────────────────────────── */

void system_init(void)
{
    g_state_mutex = xSemaphoreCreateMutex();
    configASSERT(g_state_mutex != NULL);

    g_sys_events = xEventGroupCreate();
    configASSERT(g_sys_events != NULL);

    memset(&g_state, 0, sizeof(g_state));

    ESP_LOGI(TAG, "init complete");
}

void system_set_state(system_state_e new_state)
{
    if (new_state == s_state) {
        return;
    }

    ESP_LOGI(TAG, "state %d -> %d", (int)s_state, (int)new_state);
    s_state = new_state;

    xEventGroupSetBits(g_sys_events, SYS_EVT_STATE_CHANGED);

    if (new_state == SYS_STATE_DEEP_SLEEP) {
        system_enter_deep_sleep();
    }
}

system_state_e system_get_state(void)
{
    return s_state;
}

power_data_t state_get_power(void)
{
    power_data_t snap = {0};
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        snap = g_state.power;
        xSemaphoreGive(g_state_mutex);
    } else {
        ESP_LOGW(TAG, "state_get_power: mutex timeout");
    }
    return snap;
}

void state_set_power(const power_data_t *data)
{
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_state.power = *data;
        xSemaphoreGive(g_state_mutex);
    } else {
        ESP_LOGW(TAG, "state_set_power: mutex timeout — update dropped");
    }
}

void system_enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "entering deep sleep");

    /*
     * Wake source: power button on GPIO0 (active low).
     * GPIO0 is RTC_GPIO11 on ESP32 and supports EXT0 wakeup.
     */
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);

    /* Wait for all tasks to signal sleep-ready. 500ms safety timeout
     * in case a task is stuck; prefer all three bits being set promptly. */
    xEventGroupWaitBits(g_sys_events,
                        SYS_EVT_ALL_SLEEP_READY,
                        pdFALSE,            /* don't clear */
                        pdTRUE,             /* wait for ALL bits */
                        pdMS_TO_TICKS(500));

    esp_deep_sleep_start();
    /* Does not return. */
}
