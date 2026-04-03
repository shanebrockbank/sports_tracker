# Sports Tracker — Power Budget

## Target Battery Life

| Use Case | Target |
|---|---|
| Active rowing session | > 8 hours |
| Active cycling session | > 10 hours |
| Idle / standby | > 7 days |
| Deep sleep | > 30 days |

## Estimated Current Draw Per Component

| Component | Active | Idle / Low Power | Off |
|---|---|---|---|
| ESP32 (240MHz) | ~100mA | — | — |
| ESP32 (80MHz) | ~50mA | — | — |
| ESP32 (40MHz) | ~30mA | — | — |
| ESP32 deep sleep (RTC) | — | — | ~0.01mA |
| ILI9341 display (full bright) | ~20mA | ~5mA (dim) | ~0.1mA |
| NEO-M8N GPS | ~30mA | ~15mA (power save) | ~0.001mA |
| ICM-20948 IMU | ~3mA | ~0.5mA (low power) | — |
| BMP390 baro | ~0.7mA | ~0.002mA (standby) | — |
| RFM95W LoRa (TX) | ~120mA | ~1mA (standby) | ~0.1mA |
| RFM95W LoRa (RX) | ~12mA | ~1mA (standby) | ~0.1mA |
| INA226 | ~1mA | ~1mA | — |
| SD card (write) | ~50mA | ~0.2mA (idle) | — |
| BLE (active) | ~10mA | ~5mA | — |

## Estimated System State Totals

| State | Est. Total | Notes |
|---|---|---|
| ACTIVE_FULL (240MHz, all on) | ~180mA | GPS + IMU + display + BLE |
| ACTIVE_LOW (80MHz, display dim) | ~90mA | BLE off, display dimmed |
| IDLE (40MHz, menu only) | ~30mA | GPS off, IMU off, display on |
| DEEP_SLEEP | ~0.15mA | RTC + wake GPIO only |

## Projected Runtime (1800mAh LiPo)

| State | Est. Runtime |
|---|---|
| ACTIVE_FULL constant | ~10 hours |
| ACTIVE_LOW constant | ~20 hours |
| IDLE constant | ~60 hours |
| DEEP_SLEEP | ~500 days |

_Note: These are estimates based on datasheet typical values. Actual measurements via INA226 should replace these figures after bench testing._

## Power Budget Tasks

- [ ] Measure each component current draw on bench with INA226
- [ ] Measure full system current in each STATE
- [ ] Validate GPS power save mode current vs acquisition time tradeoff
- [ ] Validate display dim current savings
- [ ] Measure BLE advertising vs connected current
- [ ] Document final measured values in this file
