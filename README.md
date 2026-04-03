# Sports Tracker — ESP32 Embedded Firmware

An ESP32-based wearable sport computer targeting rowing and cycling, built on ESP-IDF and FreeRTOS without an Arduino abstraction layer. The device operates as a state machine with distinct activity profiles, a direct-draw display pipeline, real-time sensor fusion, and a power architecture designed from first principles — GPS/IMU/barometer on a shared bus, INA226 coulomb counting, and deep sleep wakeup from RTC GPIO. This is a portfolio project demonstrating the kind of embedded firmware architecture found in commercial sport devices.

<!-- photo here -->

---

## Architecture

<!-- architecture diagram here -->

Each hardware subsystem runs as a dedicated FreeRTOS task on an assigned core. Sensor data flows into a single mutex-protected `system_state_t` snapshot read by the display task on its own cadence, avoiding queue-depth mismatches between a 100 Hz IMU and a 5 Hz display. SD card writes are batched through a RAM ring buffer with threshold and timer flush to avoid SD write latency blocking sensor tasks.

```
Core 1  │  sensor_task (High)    gps_task (High)     power_task (High)
        │       │                     │                    │
        │       └─────────────────────┴──── system_state_t (mutex) ──┐
        │                                                             │
Core 0  │  display_task (Med)    activity_task (Med)  input_task (Med)│
        │       │                                                     │
        │       └──── snapshot on refresh cadence ───────────────────┘
        │
        │  storage_task (Low)  ble_task (Low)
        │       │
        │       └──── storage queue (GPS records, fused samples)
```

---

## Key Technical Features

- **ESP-IDF + FreeRTOS task model** — no Arduino. Each subsystem is an isolated task with a defined inbox; no global state touched across tasks without synchronisation.
- **Speed-gated heading fusion** — GPS course-over-ground above 2 km/h, ICM-20948 tilt-compensated magnetometer below, complementary filter blending across the transition boundary.
- **Barometric altitude primary with GPS initialisation** — BMP390 provides low-noise altitude; GPS altitude used only to seed the barometric reference at session start. Elevation gain threshold-filtered at ±2 m to prevent overcount.
- **Profile-driven activity engine** — a single generic engine driven by `activity_profile_t` at session start. Adding a sport means adding a profile struct, not new engine code.
- **INA226 battery monitoring** — bus voltage and current via I2C at 400 kHz. SoC estimated via LiPo discharge curve lookup with linear interpolation. Auto-save and deep sleep at 5% critical threshold.
- **Batch SD writes** — records accumulated in a RAM ring buffer, flushed at 10–20 record threshold or 5-second timer, whichever arrives first. Forced flush on pause/stop. Accepts up to one interval of data loss on hard power loss.
- **ILI9341 direct-draw with LVGL-ready boundary** — no GUI library in v1. The display component is the sole caller of any draw or SPI function; LVGL can be substituted later without touching business logic.
- **Power profile switching** — `esp_pm_configure()` scales CPU frequency from 40–240 MHz across four profiles keyed to activity state and battery level.
- **Haversine distance with latitude-dependent Earth radius** — reduces error from ~30 m to ~3–5 m over 20 km at 45° latitude, within GPS noise floor. Vincenty rejected as sub-millimetre accuracy buried in ±15–20 m GPS noise.
- **FIT file output** — activity data encoded to Garmin FIT open specification for direct import into Garmin Connect and Strava.

---

## Hardware

| # | Component | Function |
|---|-----------|----------|
| 1 | ESP32-WROOM-32 | MCU + Wi-Fi + BLE |
| 2 | ILI9341 2.8" SPI TFT | 320×240 display |
| 3 | u-blox NEO-M8N | GPS — position, speed, heading |
| 4 | ICM-20948 | 9-axis IMU — stroke rate, heading, orientation |
| 5 | BMP390 | Barometric altitude + temperature |
| 6 | MicroSD SPI | FIT and CSV activity logging |
| 7 | TP4056 | Li-Po charging |
| 8 | INA226 | Battery voltage + current monitoring |
| 9 | MT3608 | Boost converter |
| 10 | EC11 rotary encoder | Primary UI input |
| 11 | RFM95W | LoRa (hardware present; packet struct in v1, no TX) |
| 12 | Passive buzzer | Audio feedback |

Current prototype is breadboard/breakout based. PCB targets are planned for a later phase.

**SPI bus:** display, SD card, LoRa — CS pins managed explicitly.  
**I2C bus:** ICM-20948 (0x68), BMP390 (0x76), INA226 (0x40) — 400 kHz fast mode.

---

## System State Machine

```
BOOT
 └─> INIT          hardware init, self-check
       └─> IDLE         low power, menu browsing (GPS off, BLE off)
             ├─> ACTIVITY_SETUP   sport select, GPS acquire, sensor warmup
             │     └─> ACTIVITY_ACTIVE   recording
             │           ├─> ACTIVITY_PAUSED
             │           └─> ACTIVITY_SAVING   write FIT + CSV
             ├─> SETTINGS         accessible during active activity (overlay)
             ├─> UPLOAD           Wi-Fi on, export FIT/CSV, OTA check
             └─> DEEP_SLEEP       RTC alive, wake on power button (EXT0)
```

All state transitions go through a single `system_set_state()` function. No task changes state directly.

---

## Activity Profiles

| Profile | Primary metric | IMU role | Auto-pause |
|---------|---------------|----------|------------|
| Rowing | Split pace (/500m) | Stroke rate via accel peak detection | < 0.5 m/s |
| Cycling | Speed (km/h) | Cadence (optional), orientation | < 1.0 m/s |

---

## Power States

| State | CPU | Display | GPS | IMU | Est. current |
|-------|-----|---------|-----|-----|-------------|
| ACTIVE_FULL | 240 MHz | On | 10 Hz | On | ~180 mA |
| ACTIVE_LOW | 80 MHz | Dim | 1 Hz | On | ~60 mA |
| IDLE | 40 MHz | On (menu) | Off | Off | ~30 mA |
| DEEP_SLEEP | ULP only | Off | Off | Off | ~0.15 mA |

Measured values to be logged using INA226 during Phase 7.

---

## Design Decisions

**Shared state over display queue.** The display task reads a mutex-protected snapshot of `system_state_t` at its own 5 Hz refresh cadence rather than receiving data through a queue. At 100 Hz IMU and 10 Hz GPS, a queue chain to the display would require carefully tuned depths and risks blocking fast sensor tasks if the display stalls. The shared snapshot always shows the latest value and is simpler to reason about.

**Batch SD writes with accepted data loss.** Per-record `fwrite()`/`fflush()` calls take 10–50 ms on FAT32 microSD. At 10 Hz GPS this would block sensor tasks for up to half their period. Records are buffered in a RAM ring buffer and flushed in batches on a threshold or 5-second timer. The accepted tradeoff is up to one flush interval of data on a hard power loss — reasonable for sport tracking where GPS dropouts already create gaps.

**Haversine with latitude-dependent Earth radius.** A spherical Haversine introduces ~30 m cumulative error over a 20 km session at 45° latitude. Scaling Earth's radius by latitude reduces this to ~3–5 m, within GPS measurement noise. Vincenty's formulae offer sub-millimetre accuracy but the improvement is meaningless against a ±15–20 m GPS noise floor.

**ESP32 RTC over external DS3231.** GPS syncs real-time clock at session start. ESP32 internal RTC drift (~100 ppm) introduces at most ~8 seconds of error over a 24-hour period with no GPS — acceptable for a device that acquires GPS fix within seconds of starting an activity. Removing DS3231 eliminates a component, a coin cell, and I2C bus occupancy.

---

## Project Structure

```
sports_tracker/
├── README.md
└── firmware/               ESP-IDF project
    ├── CMakeLists.txt
    ├── main/
    │   ├── main.c          Boot sequence, hw_init, task creation
    │   └── pin_config.h    GPIO assignments (breadboard prototype)
    └── components/
        ├── system/         State machine, g_state, event group
        ├── display/        ILI9341 SPI driver, draw primitives, 5×7 font
        ├── input/          EC11 encoder ISR, button debounce, event queue
        ├── power/          INA226 driver, LiPo curve, power profiles
        ├── gps/            NEO-M8N NMEA/UBX driver          [Phase 3]
        ├── imu/            ICM-20948 accel/gyro/mag driver  [Phase 3]
        ├── baro/           BMP390 driver                    [Phase 3]
        ├── fusion/         Heading, altitude, speed fusion  [Phase 3]
        ├── activity/       Profile structs, state machine   [Phase 4]
        ├── storage/        SD card, FIT encoder, CSV writer [Phase 4]
        ├── ble/            HR + power meter client, broadcast server [Phase 6]
        ├── wifi/           Export, OTA                      [Phase 6]
        ├── settings/       NVS read/write, JSON import      [Phase 6]
        └── lora/           RFM95W stub (packet struct only) [Phase 7]
```

---

## Development Status

| Phase | Scope | Status |
|-------|-------|--------|
| 1 — Hardware Bring-up | ILI9341 driver, EC11 input, INA226, system state machine, power gating | **In progress** |
| 2 — Display UX | Draw primitives, menu system, placeholder data screens | Planned |
| 3 — Sensor Integration | ICM-20948, BMP390, NEO-M8N, sensor fusion, CSV logging | Planned |
| 4 — Activity Engine | Profile structs, metric computation, FIT encoder, auto-pause/lap | Planned |
| 5 — Display Polish | Live data screens, configurable fields, settings overlay | Planned |
| 6 — Connectivity | BLE HR/power client, BLE broadcast, Wi-Fi export, OTA | Planned |
| 7 — Power Profiling | INA226 measurements, battery life validation, LoRa struct, enclosure | Planned |

---

## Building

Requires [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).

```bash
# Clone and install ESP-IDF (first time only)
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && ./install.sh esp32
source ~/esp/esp-idf/export.sh

# Configure
cd firmware
idf.py set-target esp32
idf.py menuconfig
# Required: Component Config → Power Management → Enable power management
```

```bash
# Build and flash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Pin assignments are in `firmware/main/pin_config.h`. Verify against your wiring before first flash. The INA226 calibration register in `firmware/components/power/ina226.h` uses a 100 mΩ placeholder — update `INA226_CAL_VALUE` once the shunt resistor is measured.