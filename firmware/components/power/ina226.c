#include "ina226.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ina226";

static i2c_port_t s_port;

/* ── I2C helpers ──────────────────────────────────────────────────────── */

static esp_err_t i2c_write_reg(uint8_t reg, uint16_t value)
{
    uint8_t buf[3] = {
        reg,
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFF),
    };

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA226_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, sizeof(buf), true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(s_port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return err;
}

static esp_err_t i2c_read_reg(uint8_t reg, uint16_t *out)
{
    uint8_t buf[2];

    /* Write register pointer */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA226_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(s_port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) return err;

    /* Read 2 bytes */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA226_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &buf[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &buf[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(s_port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) return err;

    *out = ((uint16_t)buf[0] << 8) | buf[1];
    return ESP_OK;
}

/* ── Public API ───────────────────────────────────────────────────────── */

esp_err_t ina226_init(i2c_port_t port)
{
    esp_err_t err;
    s_port = port;

    /* Verify presence by reading manufacturer ID (should be 0x5449) */
    uint16_t mfg_id = 0;
    err = i2c_read_reg(INA226_REG_MFG_ID, &mfg_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed — check wiring and address: %s",
                 esp_err_to_name(err));
        return err;
    }
    if (mfg_id != 0x5449) {
        ESP_LOGW(TAG, "unexpected manufacturer ID: 0x%04X (expected 0x5449)",
                 mfg_id);
    }

    /* Write configuration register */
    err = i2c_write_reg(INA226_REG_CONFIG, INA226_CONFIG_DEFAULT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config write failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Write calibration register */
    err = i2c_write_reg(INA226_REG_CALIB, INA226_CAL_VALUE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "calibration write failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "init OK (mfg_id=0x%04X, CAL=%d)", mfg_id, INA226_CAL_VALUE);
    return ESP_OK;
}

esp_err_t ina226_read_bus_voltage(float *out_v)
{
    uint16_t raw;
    esp_err_t err = i2c_read_reg(INA226_REG_BUS_V, &raw);
    if (err != ESP_OK) return err;

    /* Bus voltage LSB = 1.25 mV */
    *out_v = (float)raw * 1.25f / 1000.0f;
    return ESP_OK;
}

esp_err_t ina226_read_current(float *out_ma)
{
    uint16_t raw;
    esp_err_t err = i2c_read_reg(INA226_REG_CURRENT, &raw);
    if (err != ESP_OK) return err;

    /* Current register is signed (two's complement) */
    int16_t signed_raw = (int16_t)raw;
    *out_ma = (float)signed_raw * INA226_CURRENT_LSB_MA;
    return ESP_OK;
}

esp_err_t ina226_read_power(float *out_mw)
{
    uint16_t raw;
    esp_err_t err = i2c_read_reg(INA226_REG_POWER, &raw);
    if (err != ESP_OK) return err;

    /* Power register LSB = 25 × Current_LSB */
    *out_mw = (float)raw * INA226_POWER_LSB_MW;
    return ESP_OK;
}
