/**
 * QMI8658 6-DOF IMU 驱动（I2C，ESP-IDF 新 API）
 *
 * 配置：±4g 加速度计，±1024dps 陀螺仪，1000Hz ODR
 * 特性：5 样本均值、10 样本静止检测、陀螺仪零偏校准、水平偏移补偿
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
