# Sports Tracker — Hardware Configuration

**When to fill this out:** Complete this document before starting Phase 3 (sensor integration). Do not write drivers until the register decisions here are locked in. This prevents revisiting the same decisions mid-implementation.

---

## Pin Assignment

| Signal | ESP32 GPIO | Notes |
|---|---|---|
| SPI MOSI | GPIO_x | Shared SPI bus |
| SPI MISO | GPIO_x | Shared SPI bus |
| SPI CLK | GPIO_x | Shared SPI bus |
| TFT CS | GPIO_x | |
| TFT DC | GPIO_x | Data/command select |
| TFT RST | GPIO_x | |
| SD CS | GPIO_x | |
| LoRa CS | GPIO_x | |
| LoRa IRQ | GPIO_x | DIO0 |
| I2C SDA | GPIO_x | Shared I2C bus |
| I2C SCL | GPIO_x | Shared I2C bus |
| GPS TX→ESP RX | GPIO_x | UART |
| GPS RX→ESP TX | GPIO_x | UART |
| GPS PPS | GPIO_x | Optional 1Hz pulse |
| GPS EN | GPIO_x | Power gate |
| Encoder CLK | GPIO_x | |
| Encoder DT | GPIO_x | |
| Encoder SW | GPIO_x | Push button |
| Power button | GPIO_x | Wake from deep sleep |
| Buzzer | GPIO_x | PWM capable |
| Backlight | GPIO_x | PWM capable |
| Power gate | GPIO_x | MOSFET gate |

_Fill in GPIO numbers once prototype wiring is confirmed._

---

## I2C Bus

| Device | Address | Notes |
|---|---|---|
| ICM-20948 | 0x68 | AD0 low (default) |
| BMP390 | 0x76 | SDO low (default) |
| INA226 | 0x40 | A0+A1 low (default) |

I2C clock: **400kHz** (fast mode). All three devices support it.

---

## SPI Bus

| Device | Max Clock | Mode | Notes |
|---|---|---|---|
| ILI9341 | 40MHz | Mode 0 | Write only in practice |
| MicroSD | 25MHz | Mode 0 | Must init at 400kHz then negotiate |
| RFM95W | 10MHz | Mode 0 | |

Use a single SPI host (SPI2_HOST). Never share a transaction across CS boundaries.

---

## ICM-20948 Configuration

_To be filled before Phase 3._

| Parameter | Value | Rationale |
|---|---|---|
| Accel full-scale range | ±4g or ±8g | TBD — depends on stroke detection needs |
| Accel ODR | TBD Hz | |
| Accel DLPF | TBD Hz | |
| Gyro full-scale range | ±500 or ±1000 dps | TBD |
| Gyro ODR | TBD Hz | |
| Gyro DLPF | TBD Hz | |
| Mag ODR | 10Hz or 50Hz | TBD — heading update rate |
| Low power mode | TBD | Cycle mode vs continuous |

---

## BMP390 Configuration

_To be filled before Phase 3._

| Parameter | Value | Rationale |
|---|---|---|
| Pressure oversampling | TBD | Higher = less noise, more power |
| Temp oversampling | TBD | |
| IIR filter coefficient | TBD | Smooths pressure spikes |
| Output data rate | TBD Hz | |
| Power mode | Normal / Forced | TBD |

---

## NEO-M8N GPS Configuration

_To be filled before Phase 3._

UBX messages to send on init:

| Message | Purpose | Value |
|---|---|---|
| UBX-CFG-PRT | Set baud rate | 115200 |
| UBX-CFG-RATE | Set measurement rate | TBD (1Hz / 5Hz / 10Hz) |
| UBX-CFG-MSG | Enable NMEA GGA | On |
| UBX-CFG-MSG | Enable NMEA RMC | On |
| UBX-CFG-MSG | Disable unused NMEA | Off (GSV, GSA, etc.) |
| UBX-CFG-PMS | Power save mode | TBD |

Minimum fix quality for valid data: **fix type >= 2, HDOP < 3.0**

---

## INA226 Configuration

| Parameter | Value | Notes |
|---|---|---|
| Shunt resistor | TBD mΩ | Determines current resolution |
| Current LSB | TBD mA | Set via calibration register |
| Averaging | TBD samples | More = smoother, more latency |
| Conversion time | TBD µs | |
| Alert threshold | TBD | Low voltage alert GPIO |
