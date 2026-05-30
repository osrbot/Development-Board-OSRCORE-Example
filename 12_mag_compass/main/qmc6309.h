#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"

#define QMC6309_ADDR  0x7C

/* Register map */
#define QMC_REG_CHIP_ID     0x00
#define QMC_REG_X_LSB       0x01
#define QMC_REG_STATUS      0x09
#define QMC_REG_CTRL1       0x0A
#define QMC_REG_CTRL2       0x0B

#define STATUS_DRDY         (1 << 0)

/* Sensitivity for 8G range */
#define SENSITIVITY_8G      4000.0f

typedef struct {
    float x, y, z;   /* Gauss, calibrated */
    float heading;   /* degrees 0-360 */
} qmc6309_data_t;

/**
 * @brief Initialise the QMC6309.  Must be called before any read.
 *
 * @param dev  I2C master device handle obtained from i2c_master_bus_add_device().
 */
void qmc6309_init(i2c_master_dev_handle_t dev);

/**
 * @brief Read calibrated magnetometer data.
 *
 * @param out  Pointer to result struct; populated on success.
 * @return     true if new data was available and read successfully.
 */
bool qmc6309_read(qmc6309_data_t *out);

/**
 * @brief Set hard-iron and soft-iron calibration coefficients.
 *
 * @param hard  3-element hard-iron offset vector (Gauss).
 * @param soft  9-element soft-iron matrix, row-major 3×3.
 */
void qmc6309_set_calibration(const float hard[3], const float soft[9]);

/**
 * @brief Blocking calibration: rotate the board 360° during duration_ms.
 *        Computes min/max hard-iron offsets and diagonal XY soft-iron scale.
 *        Z soft-iron is kept at 1.0 (ground-vehicle assumption).
 *
 * @param duration_ms  Collection window in milliseconds (e.g. 30000).
 */
void qmc6309_calibrate(uint32_t duration_ms);

/**
 * @brief Retrieve the current calibration coefficients.
 *
 * @param hard  Output: 3-element hard-iron offset (Gauss).
 * @param soft  Output: 9-element soft-iron matrix, row-major 3×3.
 */
void qmc6309_get_calibration(float hard[3], float soft[9]);

/**
 * @brief Compute tilt-compensated heading from calibrated mag readings and
 *        roll/pitch angles (e.g. from an AHRS).
 *
 * @param mx         Calibrated X field (Gauss).
 * @param my         Calibrated Y field (Gauss).
 * @param mz         Calibrated Z field (Gauss).
 * @param roll_rad   Roll angle in radians.
 * @param pitch_rad  Pitch angle in radians.
 * @return           Heading in degrees [0, 360).
 */
float qmc6309_tilt_compensated_heading(float mx, float my, float mz,
                                        float roll_rad, float pitch_rad);
