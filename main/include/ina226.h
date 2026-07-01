#pragma once

#include <stdbool.h>
#include "esp_err.h"

// INA226 I2C address: A1=GND, A0=GND → 0x40
#define INA226_ADDR 0x40

// Shunt resistor value in ohms (0.1Ω)
#define INA226_SHUNT_OHMS 0.1f

// Full-scale current limit for calibration (300 mA)
#define INA226_MAX_CURRENT_A 0.300f

/**
 * @brief Initialise the INA226: probe the bus, verify the manufacturer/die ID,
 *        configure averaging and conversion time, and write the calibration register.
 *
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if the device is absent,
 *         or another esp_err_t on I2C failure.
 */
esp_err_t ina226_init(void);

/**
 * @brief Read the bus voltage (V), shunt voltage (mV), and calculated current (mA).
 *        Any pointer may be NULL if that value is not needed.
 */
esp_err_t ina226_read(float *bus_voltage_v, float *shunt_mv, float *current_ma);
