# Sports Tracker — Architecture Decision Log

Decisions recorded here to prevent relitigating settled choices in future sessions.
Format: date, decision, rationale, alternatives rejected.

---

## Framework

**Decision:** ESP-IDF + FreeRTOS, no Arduino framework
**Rationale:** Portfolio target (Garmin) uses professional embedded toolchains. FreeRTOS task model maps directly to real device architecture. Arduino abstracts away too much.
**Rejected:** Arduino (too high level for portfolio goals)

---

## Display Library

**Decision:** Direct draw on ILI9341 in v1. LVGL-ready abstraction boundary maintained.
**Rationale:** LVGL has real memory and integration overhead. Direct draw is sufficient for early phases and keeps complexity low. Clean boundary means LVGL can be dropped in later.
**Rejected:** LVGL in v1 (premature complexity)

---

## Sensor → Display Data Flow

**Decision:** Shared `system_state_t` struct + mutex. display_task reads snapshot on its own cadence.
**Rationale:** Display refresh (5Hz) does not need to match sensor rate (100Hz IMU). Queue-based display would require tuned depths and risks blocking fast sensor tasks on a slow display. Shared state is simpler and always shows latest value.
**Rejected:** Full queue chain sensor → fusion → activity → display (over-engineered for this use case)

---

## SD Card Write Strategy

**Decision:** Batch writes via RAM ring buffer. Flush on threshold (10–20 records) or timer (5s), forced flush on pause/stop.
**Rationale:** Per-record fwrite/fflush can take 10–50ms per call. At 10Hz GPS this blocks sensor tasks. Batching amortises SD latency.
**Tradeoff accepted:** Up to one flush interval of data lost on hard power loss.

---

## RTC

**Decision:** Use ESP32 internal RTC domain (kept alive in deep sleep). No external DS3231.
**Rationale:** GPS syncs time on session start. ESP32 RTC drift (~100ppm) is acceptable for daily sport use with GPS resync. Removes a component, a coin cell, and I2C complexity.
**Rejected:** DS3231 (unnecessary for this use case)

---

## Distance Formula

**Decision:** Haversine with latitude-dependent Earth radius.
**Rationale:** Pure spherical Haversine introduces ~30m error over 20km at Ottawa latitude. Latitude-dependent radius reduces this to ~3–5m, within GPS noise floor. Vincenty is more accurate but overkill given ±15–20m GPS noise.
**Rejected:** Naive Cartesian (up to 300m error), Vincenty (sub-mm accuracy buried in GPS noise)

---

## External Sensor Protocol

**Decision:** BLE for v1. ANT+ reserved as future hardware addition.
**Rationale:** ESP32 has BLE built in. Most modern HR straps and power meters support dual ANT+/BLE. ANT+ requires a separate module (e.g. nRF51422) not on current BOM.
**Rejected:** ANT+ in v1 (requires additional hardware)

---

## Activity Structure

**Decision:** Profile-based activity engine. One generic engine, behaviour driven by `activity_profile_t` config struct loaded at session start.
**Rationale:** Clean extensibility — adding a new sport means adding a profile, not new engine code. Rowing and cycling share most infrastructure (GPS, IMU, display) with only metrics and sensor selection differing.

---

## LoRa Scope

**Decision:** RFM95W hardware on BOM. v1 defines packet struct only, no transmission.
**Rationale:** Fleet tracking is a future feature. Hardware present enables future development without a board respin. Packet struct definition keeps architecture LoRa-aware without implementing it prematurely.
