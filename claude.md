# Sports Tracker — Project Guide for Claude Code

## Project Overview

An ESP32-based sport tracking device targeting rowing (primary) and cycling (secondary). Built with ESP-IDF + FreeRTOS. Designed as a portfolio project demonstrating embedded firmware architecture, sensor fusion, power management, and consumer-grade UX.

The device resembles a sports watch in behaviour: it has distinct system states, activity profiles, configurable data screens, and persistent logging. Hardware is currently breadboard/breakout-based with a clean path to a custom PCB.

---

## Core Constraints — Read First

**Battery-first design.** Every feature decision must consider power impact. Default to turning things off. Peripheral gating, CPU frequency scaling, and deep sleep are not optimisations to add later — they are first-class architectural requirements.

**Simple until proven necessary.** Do not introduce abstraction, libraries, or complexity unless a simpler approach has been tried and found insufficient. This applies to display rendering, sensor drivers, file I/O, and task design.

**Iterative development.** The codebase is built in phases. Do not implement phase 2 features during phase 1 work. Placeholder hooks are acceptable; speculative implementation is not.

---

## Hardware BOM

| # | Function | Breakout (current) | PCB Target |
|---|---|---|---|
| 1 | MCU | ESP32 DevKit V1 | ESP32-WROOM-32E |
| 2 | Display | 2.8" SPI TFT ILI9341 | ILI9341 + FPC connector |
| 3 | GPS | NEO-M8N breakout | u-blox NEO-M8N SMD |
| 4 | IMU | ICM-20948 breakout | ICM-20948 LGA-24 |
| 5 | Baro/Temp | BMP390 breakout | BMP390 LGA-8 |
| 6 | SD Card | SPI MicroSD breakout | Molex 503182 socket |
| 7 | Charging | TP4056 module | TP4056 SOP-8 + passives |
| 8 | Batt. Protection | On TP4056 module | DW01A + FS8205A |
| 9 | Boost Converter | MT3608 breakout | TPS61023 SOT-23-5 |
| 10 | 3.3V Reg | On ESP32 DevKit | AMS1117-3.3 SOT-223 |
| 11 | Power Monitor | CJMCU-226 (INA226) | INA226 VSSOP-10 |
| 12 | Input | EC11 rotary encoder module | EC11 + 6mm tactile SMD |
| 13 | Buzzer | Passive buzzer module | MLT-8530 + 2N3904 |
| 14 | LoRa | RFM95W breakout | RFM95W castellated |
| 15 | Power Switch | MOSFET breakout | SI2307 P-ch MOSFET |

**SPI bus occupants:** TFT display, SD card, RFM95W. Manage CS pins explicitly. Never assume bus is idle.

**I2C bus occupants:** ICM-20948, BMP390, INA226. Confirm addresses, check for conflicts before adding new devices.

---

## Firmware Architecture

### Framework
- **ESP-IDF** with **FreeRTOS**
- No Arduino framework
- Component-based source layout under `/components`
- `menuconfig` for compile-time hardware configuration

### Task Structure

Each major subsystem runs as a FreeRTOS task. Tasks communicate via queues and event groups. Mutexes protect shared state. No global variables accessed from multiple tasks without synchronisation.

| Task | Priority | Core | Responsibility |
|---|---|---|---|
| `sensor_task` | High | 1 | IMU, baro polling, sensor fusion |
| `gps_task` | High | 1 | NMEA parsing, position updates |
| `display_task` | Medium | 0 | Screen rendering, UI state |
| `activity_task` | Medium | 0 | Metric computation, state machine |
| `storage_task` | Low | 0 | FIT/CSV write, SD card management |
| `ble_task` | Low | 0 | BLE broadcasts, peripheral connections |
| `power_task` | High | 1 | INA226 monitoring, power state management |
| `input_task` | Medium | 0 | Encoder/button decode, event dispatch |

### Inter-task Communication

**Sensor → Display (shared state + mutex)**
All sensor and activity data is written into a single `system_state_t` struct protected by a mutex. The display_task reads a snapshot of this struct on its own refresh cadence. This avoids queue depth mismatches between fast sensors (100Hz IMU) and a slow display (5Hz). No queue is used for display data.

```c
typedef struct {
    imu_data_t       imu;
    gps_data_t       gps;
    baro_data_t      baro;
    fused_data_t     fused;
    activity_data_t  activity;
    power_data_t     power;
} system_state_t;

extern system_state_t g_state;
extern SemaphoreHandle_t g_state_mutex;
```

**Sensor → Storage (queue)**
Every GPS record and fused sample destined for SD card is pushed onto a storage queue. storage_task drains this queue in batches. This ensures no samples are dropped even if SD writes are slow.

**SD Card Batch Writes**
storage_task accumulates records in a RAM ring buffer and flushes to SD when the buffer reaches a threshold (e.g. 10–20 records) or a flush timer expires (e.g. every 5 seconds), whichever comes first. A forced flush occurs on activity pause or stop. This reduces SD wear and prevents write latency from blocking sensor tasks. Tradeoff: up to one flush interval of data may be lost on hard power loss — acceptable for sport tracking.

**System state changes → event groups**
All tasks observe system state via a shared event group. No task calls `system_set_state()` except through the designated state manager.

**Shared config → mutex**
NVS-loaded settings are held in a config struct protected by a mutex. Tasks read config at init and on explicit reload events only.

---

## System State Machine

The device operates as a state machine. All state transitions must go through a single `system_set_state()` function. No task changes state directly.

```
BOOT
  └─> INIT          (hardware init, self-check)
        └─> IDLE         (low power, menu browsing)
              ├─> ACTIVITY_SETUP   (sport select, sensor warmup, GPS acquire)
              │     └─> ACTIVITY_ACTIVE   (recording)
              │           ├─> ACTIVITY_PAUSED
              │           └─> ACTIVITY_SAVING  (write FIT + CSV, upload queue)
              ├─> SETTINGS         (accessible from IDLE or ACTIVITY_ACTIVE overlay)
              ├─> UPLOAD           (WiFi connect, export session files)
              └─> DEEP_SLEEP       (RTC running, wake on button)
```

**Key rule:** The device can enter SETTINGS or browse menus while ACTIVITY_ACTIVE is running. The activity continues uninterrupted in the background. The display_task switches render context; the activity_task does not pause.

---

## Activity Profile System

Activities are driven by a profile struct loaded at session start. The activity engine is generic — the profile determines behaviour.

```c
typedef struct {
    char     name[16];
    bool     use_stroke_rate;
    bool     use_cadence;
    bool     use_split_pace;       // rowing: /500m, cycling: km/h
    bool     use_power;            // external power meter via BLE
    bool     use_heart_rate;       // external HR strap via BLE
    bool     gps_required;
    uint32_t autolap_distance_m;
    uint32_t autopause_speed_mps_threshold;
    sensor_config_t  sensor_cfg;
    display_page_t   pages[4];
} activity_profile_t;
```

### Defined Profiles

**Rowing**
- Primary metric: split pace (/500m)
- IMU: stroke rate detection via accelerometer peak detection
- GPS: speed + distance + heading (COG when moving, mag when stationary)
- BMP390: altitude + temperature context
- Auto-pause: triggered when speed < 0.5 m/s

**Cycling**
- Primary metric: speed (km/h)
- GPS: speed + distance + heading
- BMP390: elevation gain/loss
- Optional BLE: HR strap, power meter
- Cadence: derivable from IMU or external BLE sensor
- Auto-pause: triggered when speed < 1.0 m/s

---

## Sensor Fusion

Fusion decisions are made at the `sensor_task` level and exposed as fused values to all other tasks. Raw sensor data is never used directly outside of `sensor_task`.

### Heading
```
speed > 2.0 km/h  →  GPS course over ground (COG)
speed <= 2.0 km/h →  ICM-20948 magnetometer (tilt-compensated)
transition        →  complementary filter blend
```

### Altitude
```
Primary:  BMP390 barometric altitude (filtered, low noise)
Fusion:   GPS altitude used only to initialise baro reference on session start
Gain:     Threshold filtered (ignore changes < 2m to prevent overcount)
```

### Speed
```
Primary:  GPS speed (when fix quality >= 2, HDOP < 3.0)
Fallback: IMU dead reckoning (accelerometer integration, short gaps only)
Display:  Rolling weighted average, window configurable per profile
```

### Distance
```
Formula:  Haversine with latitude-dependent Earth radius
Accumulation: Per GPS fix, only when fix quality valid
```

---

## Power Management

Power management is a first-class subsystem, not an afterthought.

### Power States

| State | CPU Freq | Display | GPS | IMU | BLE | Expected Current |
|---|---|---|---|---|---|---|
| ACTIVE_FULL | 240MHz | On | On | On | On | ~180mA |
| ACTIVE_LOW | 80MHz | Dim | On | On | Off | ~90mA |
| IDLE | 40MHz | On (menu) | Off | Off | Off | ~30mA |
| DEEP_SLEEP | ULP only | Off | Off | Off | Off | ~0.15mA |

### Rules
- GPS is powered off in IDLE and DEEP_SLEEP
- Display backlight dims after 30s of no input (configurable)
- BLE disabled unless activity is active or user explicitly enables
- WiFi only active during UPLOAD state, never during activity
- INA226 sampled at 1Hz during activity, 0.1Hz in IDLE
- CPU frequency set via `esp_pm_configure()` per state transition
- LoRa module powered off unless transmit is triggered (future)

### CPU Usage Monitoring

Enable FreeRTOS runtime stats in menuconfig (`Component Config → FreeRTOS → Enable runtime stats`). Use during testing to validate power profile CPU frequency choices before locking them in.

```c
// Call on button press or periodic timer during development
char stats_buf[2048];
vTaskGetRunTimeStats(stats_buf);
ESP_LOGI("CPU", "\n%s", stats_buf);

// Also check stack headroom per task to catch overflow risk
ESP_LOGI("TASK", "sensor_task HWM: %d", 
         uxTaskGetStackHighWaterMark(sensor_task_handle));
```

### Power Profiles

Defined per activity profile. Selected by user or automatically based on GPS signal quality and battery level.

```c
typedef enum {
    POWER_PROFILE_MAX,        // 240MHz, GPS 10Hz, all sensors on
    POWER_PROFILE_STANDARD,   // 160MHz, GPS 5Hz
    POWER_PROFILE_ENDURANCE,  // 80MHz,  GPS 1Hz, display dim
    POWER_PROFILE_ULTRA,      // 40MHz,  GPS 1Hz, display off
} power_profile_t;
```

Measure actual current draw for each profile with INA226 and record in `power_budget.md`.

### Battery Monitoring
- INA226 provides voltage + current
- Battery percentage via LiPo discharge curve lookup (not linear)
- Low battery warning at 15%, auto-save and sleep at 5%
- Power budget documented per state for portfolio

---

## Display Architecture

### Approach
Direct draw using ILI9341 SPI driver. No GUI library in v1. Clean abstraction boundary maintained so LVGL can be introduced later without rewriting business logic.

### Display Layer Boundary
```
activity_task / menu_task
       │  (state snapshot, no display calls)
       ▼
display_task   ← only task that calls draw functions
       │
       ▼
ili9341_driver  (SPI transactions, DMA where possible)
```

No task other than `display_task` calls any draw or SPI function.

### Screen Types
- **Activity screen:** 1–4 configurable data field pages, scrolled via encoder
- **Menu screen:** Hierarchical list navigation, encoder scroll + click
- **Settings overlay:** Slides over activity screen, activity continues behind
- **Status bar:** Always visible — battery %, GPS status, time, BLE icon

### UI Input Model
- Encoder rotate → scroll / increment
- Encoder click → select / confirm
- Encoder long press → back / cancel
- Tactile button → power on/wake, long press = power off/sleep

---

## Data Logging

### FIT File (Activity Data)
- Written to SD card at session end (and periodically during activity as safety backup)
- Contains: position, speed, altitude, HR (if connected), power (if connected), temperature
- Compatible with Garmin Connect and Strava upload
- Encoded per Garmin FIT SDK open specification

### CSV File (Raw/Debug Data)
- Written continuously during activity at full sensor resolution
- One row per sensor fusion cycle
- Columns: timestamp, lat, lon, speed_mps, heading_deg, altitude_m, pitch, roll, stroke_rate, hr_bpm, power_w, batt_v, batt_ma, temp_c
- Used for post-session analysis, algorithm tuning, and debugging

### File Naming
```
/sdcard/activities/YYYYMMDD_HHMMSS_rowing.fit
/sdcard/raw/YYYYMMDD_HHMMSS_rowing.csv
```

---

## Connectivity

### BLE
- **External sensors:** HR strap (BLE HRS profile), cycling power meter (BLE CSCP/FTMP profile)
- **Live broadcast:** Custom GATT service exposing current activity metrics during session
- External sensors treated as optional — activity runs without them, metrics populated when connected
- ANT+ reserved as future hardware addition (separate module required)

### WiFi
- Active only in UPLOAD state
- Connects to saved network (credentials stored in NVS)
- Exports FIT + CSV files to configured endpoint (local server or cloud)
- OTA firmware update via `esp_https_ota`
- Settings import via JSON file from server or SD card

### LoRa (Future)
- RFM95W hardware present on BOM
- v1: define data packet struct only, no transmission implemented
- Packet should contain: device ID, timestamp, position, speed, stroke rate, battery
- Architecture must not assume LoRa is absent — leave clean enable path

---

## Settings System

Settings stored in **NVS** (non-volatile storage). Exported/imported as JSON via WiFi or SD card.

### Configurable Parameters
- Display brightness, auto-dim timeout
- GPS update rate
- Auto-pause threshold per profile
- Auto-lap distance per profile
- Sensor fusion thresholds (heading crossover speed, altitude gain filter)
- Data field layout per activity page
- WiFi credentials
- BLE device pairings
- Unit system (metric/imperial)

---

## Development Phases

### Phase 1 — Hardware Bring-up
- ESP-IDF project scaffold, component structure
- ILI9341 display driver, basic direct draw primitives
- EC11 encoder + button input driver
- INA226 power monitoring
- Basic system state machine (BOOT → IDLE → DEEP_SLEEP)
- Power gating skeleton

### Phase 2 — Basic Display UX
- Direct draw UI primitives (text, rectangles, status bar)
- Menu system skeleton (hierarchical list, encoder navigation)
- Placeholder data screens (static values, no real sensors yet)
- This phase exists so all future phases have a real interface to test against

### Phase 3 — Sensor Integration
**Fill out `hardware_config.md` before writing any drivers in this phase.**
Document ODR, DLPF, full-scale range, UBX config messages, and I2C/SPI timing for each sensor before implementation begins.
- ICM-20948 driver (accel, gyro, mag)
- BMP390 driver
- NEO-M8N GPS (NMEA parsing, fix quality, UBX config)
- Sensor fusion: heading, altitude, speed
- SD card mount, CSV raw logging
- CPU usage profiling across power profiles

### Phase 4 — Activity Engine
- Activity profile structs (rowing, cycling)
- Activity state machine
- Metric computation (pace, split, distance, elevation gain)
- FIT file encoder
- Auto-pause, auto-lap
- SD card batch write implementation

### Phase 5 — Display Polish
- Activity data screens populated with real sensor data
- Page navigation, configurable data fields
- Settings overlay during activity
- Status bar with live battery, GPS, BLE indicators

### Phase 6 — Connectivity
- BLE HR + power meter client
- BLE live broadcast server
- WiFi export
- OTA update

### Phase 7 — Power Profiling + Polish
- Measure and document all power profiles with INA226
- Validate battery life estimates
- LVGL evaluation (optional upgrade)
- LoRa packet structure definition
- Enclosure design

---

## Code Style

- C only (no C++)
- ESP-IDF coding style conventions
- All hardware access through component APIs — no direct register access outside driver files
- Every FreeRTOS task has a clearly defined inbox (queue) and does not reach into other task state
- `vTaskDelay` is acceptable for hardware settle times (e.g. after reset or power-on). It is not acceptable as a substitute for synchronisation — use queues, semaphores, or event groups when waiting for another task's output
- All hardware access through component APIs — no direct register access outside the component's own `.c` file. Example: `sensor_task` calls `imu_read()`, never writes I2C registers directly
- Sensor data structs use SI units internally (m, m/s, Pa, °C, rad)
- Unit conversion happens at display layer only

---

## File Structure

```
sports_tracker/
├── main/
│   ├── main.c
│   └── CMakeLists.txt
├── components/
│   ├── display/          # ILI9341 driver + draw primitives
│   ├── input/            # EC11 encoder + button
│   ├── gps/              # NEO-M8N NMEA/UBX driver
│   ├── imu/              # ICM-20948 driver
│   ├── baro/             # BMP390 driver
│   ├── power/            # INA226 + power state manager
│   ├── storage/          # SD card, FIT encoder, CSV writer
│   ├── ble/              # BLE client (sensors) + server (broadcast)
│   ├── wifi/             # WiFi connect, OTA, export
│   ├── activity/         # Profile structs, state machine, metrics
│   ├── fusion/           # Sensor fusion algorithms
│   ├── settings/         # NVS read/write, JSON import/export
│   └── lora/             # RFM95W driver + packet struct (stub)
├── docs/
│   ├── bom.md
│   ├── power_budget.md
│   ├── state_machine.md
│   ├── fit_format_notes.md
│   ├── hardware_config.md     # Register settings, pin map, timing — fill before Phase 3
│   ├── decisions.md           # Architecture decision log
│   ├── testing_log.md         # Bench measurements, INA226 results, GPS observations
│   ├── known_issues.md        # Bugs, hardware quirks, active workarounds
│   ├── session_start.md       # Current project state, active phase, what to load
│   └── session_end.md         # What was completed, what is broken, next steps
├── CMakeLists.txt
└── sdkconfig
```

---

## Out of Scope for v1

- ANT+ external sensors (hardware not present)
- LoRa transmission (struct definition only)
- LVGL (direct draw first)
- Cloud backend / mobile app
- Navigation / mapping / breadcrumb trail
- Swim / run profiles (add after rowing/cycling validated)
