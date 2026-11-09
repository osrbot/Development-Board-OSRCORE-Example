/**
 * QMI8658 six-axis IMU driver using the ESP-IDF I2C API.
 * Configuration: +/-4 g, +/-1024 dps, 1000 Hz ODR.
 * Features: five-sample averaging, ten-sample stationary detection,
 * gyroscope bias calibration and level-offset compensation.
 */

#pragma once
#include <stdbool.h>
#include "driver/i2c_master.h"

typedef struct {
    float accelX, accelY, accelZ;  /* g, level-corrected */
    float gyroX,  gyroY,  gyroZ;   /* deg/s, bias-corrected */
    float temp;                     /* °C */
} qmi8658_data_t;

void qmi8658_init(i2c_master_dev_handle_t dev);

/* Blocking startup bias calibration — call once after IMU temperature is stable. */
void qmi8658_calibrate_bias(void);

/* Returns true when a new averaged sample is ready. Applies bias and level correction. */
bool qmi8658_read(qmi8658_data_t *out);

bool qmi8658_is_static(void);
bool qmi8658_bias_ready(void);
void qmi8658_get_bias(float *bx, float *by, float *bz);  /* deg/s */

/* Level calibration: offsets in g, applied as accel_corrected = accel_raw - offset. */
void qmi8658_set_level_offset(float ox, float oy, float oz);
void qmi8658_get_level_offset(float *ox, float *oy, float *oz);
