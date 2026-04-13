# Sports Tracker — Bill of Materials

## Breakout / Prototype BOM

| # | Function | Component | Interface | Notes |
|---|---|---|---|---|
| 1 | MCU | ESP32 DevKit V1 | — | 38-pin, onboard USB |
| 2 | Display | 2.8" SPI TFT ILI9341 240x320 | SPI | Touchscreen not used |
| 3 | GPS | NEO-M8N breakout | UART | 3.3V, active antenna header |
| 4 | IMU | ICM-20948 (SparkFun/GY-912) | I2C | Accel + gyro + mag |
| 5 | Baro/Temp | BMP390 breakout (Adafruit) | I2C | Pressure + temperature |
| 6 | SD Card | SPI MicroSD breakout | SPI | FAT32, FATFS via ESP-IDF |
| 7 | Charging | TP4056 module | — | Micro-USB input |
| 8 | Batt. Protection | Included on most TP4056 modules | — | Overdischarge + short |
| 9 | Boost Converter | MT3608 breakout | — | 3.7V→5V or adjustable |
| 10 | Power Monitor | CJMCU-226 (INA226) | I2C | Voltage + current |
| 11 | Input | EC11 rotary encoder module | GPIO | CLK, DT, SW |
| 12 | Buzzer | Passive buzzer module | GPIO (PWM) | Drive via transistor |
| 13 | LoRa | RFM95W breakout | SPI | 915MHz (Canada) |
| 14 | Power Switch | MOSFET breakout or manual | GPIO | P-channel latch circuit |

## PCB Target BOM

| # | Function | Component | Package | Notes |
|---|---|---|---|---|
| 1 | MCU | ESP32-WROOM-32E | Module (castellation) | Handles RF internally |
| 2 | Display | ILI9341 | Bare IC + FPC | Or slim module on castellations |
| 3 | GPS | u-blox NEO-M8N | SMD + patch antenna | |
| 4 | IMU | ICM-20948 | LGA-24 | Reflow/stencil recommended |
| 5 | Baro/Temp | BMP390 | LGA-8 | |
| 6 | SD Card | Molex 503182 (or equiv) | SMD push-push | |
| 7 | Charging | TP4056 | SOP-8 + passives | |
| 8 | Batt. Protection | DW01A + FS8205A | SOT-23-6 x2 | Standard single-cell combo |
| 9 | Boost Converter | TPS61023 | SOT-23-5 + inductor | Cleaner than MT3608 for PCB |
| 10 | 3.3V Reg | AMS1117-3.3 | SOT-223 | Post-boost LDO |
| 11 | Power Monitor | INA226 | VSSOP-10 | |
| 12 | Input | EC11 + 6mm tactile | Through-hole + SMD | |
| 13 | Buzzer | MLT-8530 + 2N3904 | SMD + SOT-23 | 100Ω + flyback diode |
| 14 | LoRa | RFM95W | Castellated module | No bare SX1276 — RF matching too complex |
| 15 | Power Switch | SI2307 P-ch MOSFET | SOT-23 + passives | Latch circuit, GPIO cutoff |

## Bus Allocation

### SPI Bus
| Device | CS Pin | Notes |
|---|---|---|
| ILI9341 TFT | GPIO_x | DC pin also required |
| MicroSD | GPIO_x | |
| RFM95W | GPIO_x | + IRQ pin |

### I2C Bus
| Device | Address | Notes |
|---|---|---|
| ICM-20948 | 0x68 or 0x69 | AD0 pin selectable |
| BMP390 | 0x76 or 0x77 | SDO pin selectable |
| INA226 | 0x40 (default) | A0/A1 configurable |

## Power Path
```
LiPo cell
  └─> TP4056 (charging via USB)
        └─> DW01A + FS8205A (protection)
              └─> SI2307 (power switch/latch)
                    └─> INA226 (current monitoring)
                          └─> MT3608 / TPS61023 (boost)
                                └─> AMS1117-3.3 (LDO)
                                      └─> ESP32 + peripherals
```

## Deferred / Future Hardware
- ANT+ module — external sensor protocol (after BLE validated)
- HR sensor (MAX30102) — external BLE HR strap used instead for v1
- Additional LiPo capacity — battery life testing will determine sizing
