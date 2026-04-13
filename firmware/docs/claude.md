# Sports Tracker — Claude Code Guide

## On Every Session Start
1. Read `docs/session_start.md` and follow the protocol there exactly
2. Do not write any code until the protocol is complete
3. Confirm the active phase and plan to the user before starting

## Session Flow
The session follows this user-driven sequence:

```
"session start"  → run docs/session_start.md protocol
 ... work ...
"code review"    → run docs/code_review.md checklist, report results to user
"git commit"     → stage files, propose commit message, wait for user approval
"session end"    → auto-update session_end.md + related docs
```

Each stage is triggered explicitly by the user. Do not run code review, commit, or session end automatically — wait to be called.

## On "session end"
1. Append a new dated entry to `docs/session_end.md`
2. Update `docs/known_issues.md` if any issues were found or resolved
3. Append any architecture decisions made to `docs/decisions.md`
4. If current draw measurements were taken, update `docs/power_budget.md`
5. If hardware register settings were decided, update `docs/hardware_config.md`

## Doc Trimming
As the project matures, docs should be trimmed to avoid token bloat:
- Once a component is implemented, replace its struct/API example in `architecture.md` with a pointer to the actual header file
- When `session_end.md` exceeds ~30 entries, summarise the oldest 20 into a `## Archived summary` block and notify the user before doing so
- When resolved issues in `known_issues.md` exceed 15, archive them to `docs/known_issues_archive.md`
- **Never trim without notifying the user first** — state what will be trimmed and ask for confirmation

## Model Selection (Claude Code CLI)
Choose the model based on task complexity:

```bash
claude --model claude-sonnet-4-5    # default — architecture, logic, new features
claude --model claude-opus-4-5      # hard problems — complex FreeRTOS, sensor fusion design
claude --model claude-haiku-4-5     # fast tasks — updating docs, boilerplate, session end
```

---

## When to Load Supporting Docs
Load only when directly relevant — not speculatively:
- Task structure, shared state, component APIs, structs → `docs/architecture.md`
- Starting Phase 3 → `docs/hardware_config.md` before any driver code
- FIT encoder work → `docs/fit_format_notes.md`
- Power decisions → `docs/power_budget.md`
- Debugging → `docs/known_issues.md` + relevant section of `docs/testing_log.md`
- Architecture question → `docs/decisions.md` before proposing a new approach
- BOM or pin details → `docs/bom.md`
- State machine question → `docs/state_machine.md`

---

## Project
ESP32-based sport tracker. Rowing (primary), cycling (secondary).
ESP-IDF + FreeRTOS. Breakout hardware now, custom PCB later.
Full BOM and pin map: `docs/bom.md`

---

## Core Constraints
- **Battery-first.** Default to off. Gate peripherals, scale CPU, use deep sleep. Every new feature needs a power consideration before implementation.
- **Simple until proven necessary.** No premature abstraction, libraries, or complexity.
- **Phase discipline.** Do not implement ahead of the active phase. Placeholder hooks are acceptable, speculative implementation is not.

---

## Architecture Rules

**Hardware access:** Component API only. No register access outside the component's own `.c` file. `sensor_task` calls `imu_read()` — never touches I2C directly.

**vTaskDelay:** Acceptable for hardware settle after reset only. Never as a substitute for synchronisation — use queues, semaphores, or event groups.

**Sensor → Display:** Shared `system_state_t` with per-subsystem accessor functions hiding the mutex. Display reads a local snapshot. Mutex held for microseconds only — never across slow operations.

**Sensor → Storage:** Queue-based. Every sample must reach the file. Batch flush on 10–20 records or 5s timer, forced flush on activity pause or stop.

**State transitions:** Only via `system_set_state()`. No task changes state directly.

**Units:** SI internally (m, m/s, Pa, °C, rad). Conversion at display layer only.

**Code:** C only. No C++. Magic numbers in `#define` or menuconfig, never inline.

---

## Sensor Fusion Summary
- **Heading:** GPS COG above 2 km/h, magnetometer below, complementary blend at transition
- **Altitude:** BMP390 primary, GPS initialises reference only, gain filtered ±2m threshold
- **Speed:** GPS primary (fix ≥ 2, HDOP < 3.0), IMU dead reckoning for short dropouts
- **Distance:** Haversine with latitude-dependent Earth radius

Full rationale: `docs/decisions.md`

---

## Power States
| State | CPU | Display | GPS | IMU | BLE |
|---|---|---|---|---|---|
| ACTIVE_FULL | 240MHz | On | On | On | On |
| ACTIVE_STANDARD | 160MHz | On | On | On | Off |
| ACTIVE_ENDURANCE | 80MHz | Dim | On | On | Off |
| ACTIVE_ULTRA | 40MHz | Off | On | On | Off |
| IDLE | 40MHz | Menu | Off | Off | Off |
| DEEP_SLEEP | ULP | Off | Off | Off | Off |

Measured values and targets: `docs/power_budget.md`

---

## Development Phases
| Phase | Focus | Gate |
|---|---|---|
| 1 | Hardware bring-up, display driver, input, INA226, state machine skeleton | — |
| 2 | Basic display UX — menus, placeholder screens, encoder nav | Phase 1 done |
| 3 | Sensor drivers, fusion, SD card, CSV logging | Fill `hardware_config.md` first |
| 4 | Activity engine, profiles, FIT encoder, auto-pause/lap | Phase 3 done |
| 5 | Display polish — live data screens, settings overlay, status bar | Phase 4 done |
| 6 | Connectivity — BLE sensors, WiFi export, OTA | Phase 5 done |
| 7 | Power profiling, LoRa packet struct, enclosure | Phase 6 done |

---

## Out of Scope for v1
ANT+, LoRa transmission, LVGL, cloud backend, navigation/mapping, swim/run profiles.
