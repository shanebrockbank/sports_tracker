# Sports Tracker — System State Machine

## States

| State | Description |
|---|---|
| `STATE_BOOT` | Hardware init, driver setup, self-check |
| `STATE_INIT` | Load settings from NVS, mount SD, check battery |
| `STATE_IDLE` | Menu browsing, low power, GPS off |
| `STATE_ACTIVITY_SETUP` | Sport selected, GPS acquiring, sensors warming up |
| `STATE_ACTIVITY_ACTIVE` | Recording in progress |
| `STATE_ACTIVITY_PAUSED` | User paused, sensors still running, GPS still on |
| `STATE_ACTIVITY_SAVING` | Writing FIT + CSV, queuing for upload |
| `STATE_SETTINGS` | Settings menu — accessible as overlay during activity |
| `STATE_UPLOAD` | WiFi connect, export files, OTA check |
| `STATE_DEEP_SLEEP` | ULP + RTC only, wake on button GPIO |

## Transition Table

| From | To | Trigger |
|---|---|---|
| BOOT | INIT | Boot complete |
| INIT | IDLE | Init success |
| INIT | DEEP_SLEEP | Battery < 5% on boot |
| IDLE | ACTIVITY_SETUP | User selects activity from menu |
| IDLE | SETTINGS | User enters settings menu |
| IDLE | UPLOAD | User triggers upload |
| IDLE | DEEP_SLEEP | Long press power button / timeout |
| ACTIVITY_SETUP | ACTIVITY_ACTIVE | GPS fix acquired + user confirms start |
| ACTIVITY_SETUP | IDLE | User cancels |
| ACTIVITY_ACTIVE | ACTIVITY_PAUSED | Encoder click on pause / auto-pause threshold |
| ACTIVITY_ACTIVE | ACTIVITY_SAVING | User ends activity |
| ACTIVITY_ACTIVE | SETTINGS | User opens settings overlay |
| ACTIVITY_PAUSED | ACTIVITY_ACTIVE | Encoder click resume |
| ACTIVITY_PAUSED | ACTIVITY_SAVING | User discards or saves |
| ACTIVITY_SAVING | IDLE | Save complete |
| SETTINGS | IDLE | User exits settings (if was IDLE) |
| SETTINGS | ACTIVITY_ACTIVE | User exits settings (if was in activity) |
| UPLOAD | IDLE | Upload complete or cancelled |
| DEEP_SLEEP | INIT | Button wake |

## Rules

- `system_set_state()` is the only function that changes system state
- All tasks observe state via event group — they do not hold state themselves
- SETTINGS is an overlay state — it does not suspend the activity
- Auto-pause is a sub-condition of ACTIVITY_ACTIVE, not a separate state
- ACTIVITY_SAVING writes FIT first, then CSV, then signals IDLE transition
- On any unrecoverable error → log to SD if mounted → DEEP_SLEEP

## Power Actions Per State Transition

| Transition | Power Action |
|---|---|
| IDLE → ACTIVITY_SETUP | Power on GPS |
| IDLE → DEEP_SLEEP | Gate all peripherals, set CPU to ULP |
| ACTIVITY_ACTIVE → ACTIVITY_SAVING | Disable BLE broadcast |
| ACTIVITY_SAVING → IDLE | Power off GPS if not needed |
| IDLE → UPLOAD | Enable WiFi |
| UPLOAD → IDLE | Disable WiFi |
| Any → DEEP_SLEEP | All peripherals off, RTC running |
