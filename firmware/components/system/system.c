#include "system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

static const char *TAG = "system";

/* ── Shared state ─────────────────────────────────────────────────────── */

system_state_t     g_state;
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

void system_enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "entering deep sleep");

    /*
     * Wake source: power button on GPIO0 (active low).
     * GPIO0 is RTC_GPIO11 on ESP32 and supports EXT0 wakeup.
     */
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);

    /*
     * Give tasks a moment to clean up.  In later phases this will be
     * replaced with an explicit shutdown handshake via the event group.
     */
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_deep_sleep_start();
    /* Does not return. */
}
