#pragma once

/*
 * INA226 private interface — used only within the power component.
 */

#include "driver/i2c.h"
#include "esp_err.h"
#include <stdint.h>

/* ── I2C address ──────────────────────────────────────────────────────── */

#define INA226_I2C_ADDR     0x40    /* A0=GND, A1=GND */

/* ── Register addresses ───────────────────────────────────────────────── */

#define INA226_REG_CONFIG   0x00
#define INA226_REG_SHUNT_V  0x01
#define INA226_REG_BUS_V    0x02
#define INA226_REG_POWER    0x03
#define INA226_REG_CURRENT  0x04
#define INA226_REG_CALIB    0x05
#define INA226_REG_MASK_EN  0x06
#define INA226_REG_ALERT    0x07
#define INA226_REG_MFG_ID   0xFE
#define INA226_REG_DIE_ID   0xFF

/* ── Configuration register bits ─────────────────────────────────────── */

/* Averaging mode: number of samples to average */
#define INA226_AVG_1        (0 << 9)
#define INA226_AVG_4        (1 << 9)
#define INA226_AVG_16       (2 << 9)
#define INA226_AVG_64       (3 << 9)
#define INA226_AVG_128      (4 << 9)
#define INA226_AVG_256      (5 << 9)
#define INA226_AVG_512      (6 << 9)
#define INA226_AVG_1024     (7 << 9)

/* Bus voltage conversion time */
#define INA226_VCT_140US    (0 << 6)
#define INA226_VCT_204US    (1 << 6)
#define INA226_VCT_332US    (2 << 6)
#define INA226_VCT_588US    (3 << 6)
#define INA226_VCT_1100US   (4 << 6)
#define INA226_VCT_2116US   (5 << 6)
#define INA226_VCT_4156US   (6 << 6)
#define INA226_VCT_8244US   (7 << 6)

/* Shunt voltage conversion time (same encoding, bits [5:3]) */
#define INA226_SCT_140US    (0 << 3)
#define INA226_SCT_204US    (1 << 3)
#define INA226_SCT_332US    (2 << 3)
#define INA226_SCT_588US    (3 << 3)
#define INA226_SCT_1100US   (4 << 3)
#define INA226_SCT_2116US   (5 << 3)
#define INA226_SCT_4156US   (6 << 3)
#define INA226_SCT_8244US   (7 << 3)

/* Operating mode */
#define INA226_MODE_SHUTDOWN    0
#define INA226_MODE_SHUNT_TRIG  1
#define INA226_MODE_BUS_TRIG    2
#define INA226_MODE_BOTH_TRIG   3
#define INA226_MODE_SHUNT_CONT  5
#define INA226_MODE_BUS_CONT    6
#define INA226_MODE_BOTH_CONT   7   /* continuous shunt + bus */

/*
 * Default config: 16 averages, 1.1ms conversion times, continuous mode.
 * Gives ~18ms per reading with good noise rejection.
 * Adjust once actual shunt resistor and application noise are known.
 */
#define INA226_CONFIG_DEFAULT \
    (INA226_AVG_16 | INA226_VCT_1100US | INA226_SCT_1100US | INA226_MODE_BOTH_CONT)

/*
 * Calibration register value.
 *
 * Formula: CAL = 0.00512 / (Current_LSB × R_shunt)
 *
 * Placeholder values (update in hardware_config.md when shunt is known):
 *   R_shunt    = 0.1 Ω  (100 mΩ — common value, measure actual)
 *   Current_LSB = 1 mA  (0.001 A)
 *   CAL = 0.00512 / (0.001 × 0.1) = 51.2 → round to 51
 *
 * With these values:
 *   Current resolution = 1 mA / LSB
 *   Power LSB = 25 × Current_LSB = 25 mW
 */
#define INA226_CURRENT_LSB_MA   1.0f    /* mA per LSB — update with real shunt */
#define INA226_POWER_LSB_MW     25.0f   /* mW per LSB = 25 × Current_LSB */
#define INA226_CAL_VALUE        51      /* See formula above */

/* ── Internal API ─────────────────────────────────────────────────────── */

esp_err_t ina226_init(i2c_port_t port);
esp_err_t ina226_read_bus_voltage(float *out_v);
esp_err_t ina226_read_current(float *out_ma);
esp_err_t ina226_read_power(float *out_mw);
