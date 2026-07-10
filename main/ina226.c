#include "ina226.h"
#include "i2c_handlers.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "INA226";

// Register addresses
#define REG_CONFIG      0x00
#define REG_SHUNT_V     0x01
#define REG_BUS_V       0x02
#define REG_CALIBRATION 0x05
#define REG_MANUF_ID    0xFE
#define REG_DIE_ID      0xFF

#define INA226_MANUF_ID 0x5449  // "TI"
#define INA226_DIE_ID   0x2260

// CONFIG: 16 averages, 1.1ms conversion, shunt+bus continuous
#define CONFIG_VALUE 0x4527

static i2c_master_dev_handle_t s_dev = NULL;
static float s_current_lsb_ma = 0.0f;

static esp_err_t reg_write(uint8_t reg, uint16_t value) {
    uint8_t buf[3] = { reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

static esp_err_t reg_read(uint8_t reg, uint16_t *out) {
    uint8_t rx[2];
    esp_err_t err = i2c_master_transmit_receive(s_dev, &reg, 1, rx, 2, 100);
    if (err == ESP_OK) {
        *out = ((uint16_t)rx[0] << 8) | rx[1];
    }
    return err;
}

esp_err_t ina226_init(void) {
    if (i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialised");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = INA226_ADDR,
        .scl_speed_hz    = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_handle, &cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add INA226 device: %s", esp_err_to_name(err));
        return err;
    }

    // Probe: read manufacturer ID
    uint16_t manuf_id = 0;
    err = reg_read(REG_MANUF_ID, &manuf_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "INA226 not detected at 0x%02X (no ACK): %s", INA226_ADDR, esp_err_to_name(err));
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t die_id = 0;
    reg_read(REG_DIE_ID, &die_id);

    if (manuf_id != INA226_MANUF_ID) {
        ESP_LOGW(TAG, "Unexpected manufacturer ID: 0x%04X (expected 0x%04X)", manuf_id, INA226_MANUF_ID);
    } else {
        ESP_LOGI(TAG, "INA226 detected ✓  manuf=0x%04X die=0x%04X", manuf_id, die_id);
    }

    // Calibration: current_lsb = max_current / 2^15
    s_current_lsb_ma = (INA226_MAX_CURRENT_A / 32768.0f) * 1000.0f; // in mA
    uint16_t cal = (uint16_t)(0.00512f / (INA226_MAX_CURRENT_A / 32768.0f * INA226_SHUNT_OHMS));
    ESP_LOGI(TAG, "CAL register = %u  current_lsb = %.4f mA", cal, s_current_lsb_ma);

    err = reg_write(REG_CONFIG, CONFIG_VALUE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Config write failed: %s", esp_err_to_name(err));
        return err;
    }

    err = reg_write(REG_CALIBRATION, cal);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Calibration write failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t ina226_read(float *bus_voltage_v, float *shunt_mv, float *current_ma) {
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;

    if (bus_voltage_v) {
        uint16_t raw = 0;
        esp_err_t err = reg_read(REG_BUS_V, &raw);
        if (err != ESP_OK) return err;
        // Bus voltage LSB = 1.25 mV
        *bus_voltage_v = (float)raw * 0.00125f;
    }

    if (shunt_mv || current_ma) {
        uint16_t raw = 0;
        esp_err_t err = reg_read(REG_SHUNT_V, &raw);
        if (err != ESP_OK) return err;
        // Shunt voltage LSB = 2.5 µV
        float shunt_uv = (int16_t)raw * 2.5f;
        if (shunt_mv)   *shunt_mv   = shunt_uv / 1000.0f;
        if (current_ma) *current_ma = (shunt_uv / 1e6f) / INA226_SHUNT_OHMS * 1000.0f;
    }

    return ESP_OK;
}
