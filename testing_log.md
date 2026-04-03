# Sports Tracker — Testing Log

_Record bench measurements, INA226 current readings, GPS accuracy observations, and algorithm validation results here._

---

## Power Profile Measurements

_Fill in with INA226 measurements once hardware is running._

| Profile | CPU | GPS | IMU | Display | Measured mA | Notes |
|---|---|---|---|---|---|---|
| ACTIVE_FULL | 240MHz | 10Hz | On | Full | — | |
| ACTIVE_STANDARD | 160MHz | 5Hz | On | Full | — | |
| ACTIVE_ENDURANCE | 80MHz | 1Hz | On | Dim | — | |
| ACTIVE_ULTRA | 40MHz | 1Hz | On | Off | — | |
| IDLE | 40MHz | Off | Off | Menu | — | |
| DEEP_SLEEP | ULP | Off | Off | Off | — | |

---

## GPS Accuracy Observations

_Log GPS fix quality, HDOP values, and distance accuracy observations during field testing._

| Date | Location | Fix Type | HDOP | Observations |
|---|---|---|---|---|
| | | | | |

---

## Sensor Fusion Validation

_Log heading crossover behaviour, altitude gain accuracy, speed smoothing results._

| Date | Test | Expected | Actual | Notes |
|---|---|---|---|---|
| | Heading crossover at 2km/h | Smooth mag→GPS blend | | |
| | Elevation gain on known climb | Known value | | |
| | Speed vs phone GPS | Match within 0.2km/h | | |

---

## CPU Usage (vTaskGetRunTimeStats)

_Paste runtime stats output here after profiling sessions._

```
Task           Abs Time    % Time
-----          ---------   ------
(paste output here)
```

---

## Stack High Water Marks

_Record per task to catch overflow risk early._

| Task | Stack Allocated | HWM Remaining | Status |
|---|---|---|---|
| sensor_task | | | |
| gps_task | | | |
| display_task | | | |
| activity_task | | | |
| storage_task | | | |
| power_task | | | |
| input_task | | | |
