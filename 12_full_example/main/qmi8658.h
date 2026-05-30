/**
 * QMI8658 6-DOF IMU driver (I2C, ESP-IDF new API)
 *
 * Config: ±4g accel, ±1024dps gyro, 1000Hz ODR
 * Features: 5-sample averaging, gyro bias calibration, static detection,
 *           level offset correction.
 */

#pragma once
#include <stdbool.h>
#include "driver/i2c_master.h"

typedef struct {
    float accelX, accelY, accelZ;  // g, level-corrected
    float gyroX,  gyroY,  gyroZ;   // deg/s, bias-corrected
    float temp;                     // °C
} qmi8658_data_t;

// Initialize the QMI8658 and reset internal state.
void qmi8658_init(i2c_master_dev_handle_t dev);

// Blocking startup gyro bias calibration — call once after temperature is stable.
void qmi8658_calibrate_bias(void);

// Returns true when a new averaged sample is ready.
// Applies bias correction and level offset internally.
bool qmi8658_read(qmi8658_data_t *out);

// Returns true when the platform has been stationary long enough for bias tracking.
bool qmi8658_is_static(void);

// Level calibration: offsets in g, applied as accel_corrected = accel_raw - offset.
// oz should be (az_mean - 1.0) so accelZ reads 1.0g on flat ground.
void qmi8658_set_level_offset(float ox, float oy, float oz);
