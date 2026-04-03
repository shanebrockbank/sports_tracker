# Sports Tracker — FIT File Format Notes

## Overview

The FIT (Flexible and Interoperable Data Transfer) protocol is Garmin's open standard for sport activity files. It is the native format for Garmin Connect and is supported by Strava, TrainingPeaks, and most sport platforms.

Spec: https://developer.garmin.com/fit/protocol/

## Why FIT Over GPX/CSV

- Native import on Garmin Connect (no conversion)
- Compact binary format — smaller SD card footprint than XML/GPX
- Supports rowing and cycling specific data fields
- Strong portfolio signal — shows domain knowledge of sport industry standards

## Key FIT Concepts

### Messages
FIT files are a sequence of messages. Each message has a type (definition or data).

Key message types for this project:

| Message | Purpose |
|---|---|
| `file_id` | File type, device info, timestamp |
| `session` | One per activity — summary stats |
| `lap` | Auto-lap or manual lap splits |
| `record` | Per-sample data (position, speed, HR, etc.) |
| `event` | Start, stop, pause markers |
| `device_info` | Hardware info |

### Record Message Fields (target)

| Field | Type | Unit | Source |
|---|---|---|---|
| `timestamp` | uint32 | seconds | ESP32 RTC / GPS |
| `position_lat` | sint32 | semicircles | GPS |
| `position_long` | sint32 | semicircles | GPS |
| `altitude` | uint16 | m (scaled) | BMP390 |
| `speed` | uint16 | m/s (scaled) | GPS fused |
| `distance` | uint32 | m (scaled) | Haversine accumulated |
| `heart_rate` | uint8 | bpm | BLE HR strap |
| `cadence` | uint8 | rpm | IMU / BLE |
| `power` | uint16 | watts | BLE power meter |
| `temperature` | sint8 | °C | BMP390 |
| `stroke_type` | enum | — | Rowing profile |

### Coordinate Encoding
FIT uses **semicircles** for lat/lon:
```c
// Convert decimal degrees to FIT semicircles
int32_t to_semicircles(float degrees) {
    return (int32_t)(degrees * (pow(2, 31) / 180.0));
}
```

### Timestamps
FIT timestamps are seconds since **31 Dec 1989 00:00:00 UTC** (not Unix epoch).
```c
#define FIT_EPOCH_OFFSET 631065600  // seconds between 1970 and 1989 epochs
uint32_t fit_timestamp = unix_timestamp - FIT_EPOCH_OFFSET;
```

## Implementation Plan

### Phase 1 — Minimal viable FIT writer
- file_id message
- session message (summary only)
- record messages (position + speed + altitude)
- event messages (start/stop)

### Phase 2 — Full sport data
- lap messages (auto-lap triggers)
- HR, power, cadence fields
- stroke_type for rowing
- device_info message

### Phase 3 — Validation
- Test import into Garmin Connect
- Test import into Strava
- Validate with Garmin's FitCSVTool

## Reference Implementation

Garmin provides an open source C SDK:
https://github.com/garmin/fit-sdk-c

For an embedded implementation without the full SDK, a minimal FIT encoder can be written from scratch following the protocol spec. The binary format is straightforward for the message types needed here.

## File Naming Convention
```
/sdcard/activities/YYYYMMDD_HHMMSS_rowing.fit
/sdcard/activities/YYYYMMDD_HHMMSS_cycling.fit
```
