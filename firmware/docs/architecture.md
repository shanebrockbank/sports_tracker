# Sports Tracker — Architecture Reference

_Load this file when working on task structure, shared state, component APIs, display boundaries, or connectivity. Do not load speculatively._

---

## FreeRTOS Task Table

| Task | Priority | Core | Responsibility |
|---|---|---|---|
| `sensor_task` | High | 1 | ICM-20948 + BMP390 polling, raw data to g_state |
| `gps_task` | High | 1 | NMEA parsing, fix quality, position to g_state |
| `fusion_task` | High | 1 | Consumes raw g_state, writes g_state.fused |
| `display_task` | Medium | 0 | Only task that calls draw functions, snapshots g_state |
| `activity_task` | Medium | 0 | Metric computation, state machine, pushes to storage queue |
| `storage_task` | Low | 0 | Drains storage queue, batch writes FIT + CSV to SD |
| `ble_task` | Low | 0 | BLE client (HR/power meter) + broadcast server |
| `power_task` | High | 1 | INA226 sampling, power state management, peripheral gating |
| `input_task` | Medium | 0 | EC11 encoder + button decode, pushes to event queue |

---

## Shared State — system_state_t

All sensor and activity data lives in one struct. Access only via accessor functions defined in `components/state/state.h`. No task reads or writes fields directly.

```c
typedef struct {
    imu_data_t       imu;       // WRITER: sensor_task
    gps_data_t       gps;       // WRITER: gps_task
    baro_data_t      baro;      // WRITER: sensor_task
    fused_data_t     fused;     // WRITER: fusion_task only
    activity_data_t  activity;  // WRITER: activity_task only
    power_data_t     power;     // WRITER: power_task only
} system_state_t;
```

Each subsystem has its own mutex hidden behind accessors:

```c
// state.h — public interface only, mutex is never exposed
void state_set_imu(const imu_data_t *data);
void state_get_imu(imu_data_t *out);

void state_set_fused(const fused_data_t *data);
void state_get_fused(fused_data_t *out);

void state_set_activity(const activity_data_t *data);
void state_get_activity(activity_data_t *out);
// ... same pattern for gps, baro, power
```

Mutex is held for microseconds only — copy in, copy out, release immediately. Never hold across slow operations (SD write, BLE tx, printf).

---

## Activity Profile Struct

```c
typedef struct {
    char     name[16];
    bool     use_stroke_rate;          // rowing only
    bool     use_cadence;              // cycling only
    bool     use_split_pace;           // rowing: /500m pace
    bool     use_power;                // external BLE power meter
    bool     use_heart_rate;           // external BLE HR strap
    bool     gps_required;
    uint32_t autolap_distance_m;
    uint32_t autopause_speed_mps_threshold;
    sensor_config_t  sensor_cfg;       // which sensors to enable
    display_page_t   pages[4];         // data field layout per page
    power_profile_t  power_profile;    // default power profile
} activity_profile_t;
```

### Rowing Profile Defaults
- split pace /500m as primary metric
- stroke rate via IMU accelerometer peak detection
- auto-pause at speed < 0.5 m/s
- GPS + IMU + BMP390 all active

### Cycling Profile Defaults
- speed km/h as primary metric
- elevation gain/loss via BMP390
- auto-pause at speed < 1.0 m/s
- BLE HR + power meter optional, populate when connected

---

## Storage Queue + Batch Write

```
activity_task ──► [storage_queue] ──► storage_task ──► SD card
```

- Every GPS record and fused sample is pushed onto storage_queue
- storage_task accumulates in a RAM ring buffer
- Flush to SD when: buffer hits 10–20 records OR 5s timer fires
- Forced flush on: activity pause, activity stop, low battery warning
- Tradeoff: up to one flush interval lost on hard power loss — acceptable

---

## Display Boundary Rule

```
Any task (reads g_state snapshot)
         │
         ▼
   display_task          ← ONLY task that calls any draw function
         │
         ▼
   ili9341_driver         (SPI transactions, DMA where possible)
```

No task other than `display_task` calls any draw, SPI, or display function.
display_task takes a full g_state snapshot under mutex, releases mutex, then renders from the local copy. Mutex is never held during rendering.

---

## Screen Types

| Screen | Description |
|---|---|
| Activity screen | 1–4 configurable data field pages, scrolled via encoder rotate |
| Menu screen | Hierarchical list navigation, encoder scroll + click |
| Settings overlay | Slides over activity screen — activity continues behind it |
| Status bar | Always visible — battery %, GPS fix, time, BLE icon |

---

## UI Input Model

| Input | Action |
|---|---|
| Encoder rotate | Scroll list / increment value |
| Encoder click | Select / confirm |
| Encoder long press | Back / cancel |
| Tactile button short | Wake from sleep / backlight on |
| Tactile button long | Power off / enter deep sleep |

---

## Component API Pattern

Hardware access is always through a component's public API. No register access outside the component's own `.c` file.

Example — ICM-20948:
```c
// imu.h — what other tasks see (no registers, no I2C)
esp_err_t imu_init(void);
esp_err_t imu_read(imu_data_t *out);
esp_err_t imu_set_accel_range(uint8_t range);
esp_err_t imu_sleep(void);
esp_err_t imu_wake(void);

// sensor_task.c — correct usage
imu_data_t data;
imu_read(&data);               // only call ever needed
state_set_imu(&data);

// sensor_task.c — NEVER do this
i2c_write_byte(0x68, 0x6B, 0x00);  // direct register — forbidden outside imu.c
```

Same pattern applies to every component: gps, baro, display, storage, ble, power.

---

## Connectivity Detail

### BLE
- **HR strap:** BLE HRS profile (Heart Rate Service 0x180D)
- **Power meter:** BLE CSCP profile (Cycling Speed and Cadence) or FTMP
- External sensors are optional — activity runs without them, fields populate when connected
- **Live broadcast:** custom GATT service, current activity metrics, active during ACTIVITY_ACTIVE only
- ANT+ future hardware addition — separate module required, not on current BOM

### WiFi
- Active only during UPLOAD system state — never during activity recording
- Credentials stored in NVS
- Exports FIT + CSV to configured endpoint (local server or cloud)
- OTA via `esp_https_ota`
- Settings import via JSON from server or SD card

### LoRa
- RFM95W present on BOM, no transmission in v1
- Define packet struct only:
```c
typedef struct {
    uint32_t device_id;
    uint32_t timestamp;
    float    lat;
    float    lon;
    float    speed_mps;
    uint8_t  stroke_rate;
    uint8_t  battery_pct;
} lora_packet_t;
```
- Architecture must leave a clean enable path — do not assume LoRa is absent

---

## Settings — NVS Parameters

All settings stored in NVS. Exported/imported as JSON.

| Parameter | Type | Notes |
|---|---|---|
| display_brightness | uint8 | 0–100% |
| auto_dim_timeout_s | uint16 | seconds before backlight dims |
| gps_update_rate_hz | uint8 | 1, 5, or 10 |
| autolap_rowing_m | uint32 | per profile |
| autolap_cycling_m | uint32 | per profile |
| autopause_rowing_mps | float | speed threshold |
| autopause_cycling_mps | float | speed threshold |
| heading_crossover_kmh | float | GPS/mag fusion threshold |
| altitude_gain_filter_m | float | min gain to count |
| data_field_layout | array | display page config |
| wifi_ssid | string | stored in NVS |
| wifi_password | string | stored in NVS |
| ble_paired_devices | array | HR + power meter MACs |
| unit_system | enum | METRIC / IMPERIAL |

---

## File Structure

```
sports_tracker/
├── main/
│   ├── main.c                    # boot sequence, task creation
│   └── CMakeLists.txt
├── components/
│   ├── state/                    # system_state_t, accessors, mutexes
│   ├── display/                  # ILI9341 driver + draw primitives
│   ├── input/                    # EC11 encoder + button
│   ├── gps/                      # NEO-M8N NMEA/UBX driver
│   ├── imu/                      # ICM-20948 driver
│   ├── baro/                     # BMP390 driver
│   ├── power/                    # INA226 + power state manager
│   ├── storage/                  # SD card, FIT encoder, CSV writer
│   ├── ble/                      # BLE client + broadcast server
│   ├── wifi/                     # WiFi, OTA, export
│   ├── activity/                 # profile structs, state machine, metrics
│   ├── fusion/                   # sensor fusion algorithms
│   ├── settings/                 # NVS read/write, JSON import/export
│   └── lora/                     # RFM95W driver + packet struct (stub)
├── docs/
│   ├── architecture.md           # this file — task table, structs, APIs
│   ├── bom.md
│   ├── decisions.md
│   ├── fit_format_notes.md
│   ├── hardware_config.md
│   ├── known_issues.md
│   ├── power_budget.md
│   ├── state_machine.md
│   ├── testing_log.md
│   ├── session_start.md
│   └── session_end.md
├── CMakeLists.txt
└── sdkconfig.defaults
```
