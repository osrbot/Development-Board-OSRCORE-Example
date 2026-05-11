/**
 * QMI8658 6-DOF IMU 驱动（I2C，ESP-IDF 新 API）
 *
 * 配置：±8g 加速度计，±2048dps 陀螺仪，1000Hz ODR
 * 寄存器操作直接通过 i2c_master_transmit / transmit_receive 完成。
 */

#pragma once
#include <stdbool.h>
#include "driver/i2c_master.h"

typedef struct {
    float ax, ay, az;  /* g */
    float gx, gy, gz;  /* deg/s */
    float temp;        /* °C */
} qmi8658_data_t;

void qmi8658_init(i2c_master_dev_handle_t dev);
bool qmi8658_read(qmi8658_data_t *out);
