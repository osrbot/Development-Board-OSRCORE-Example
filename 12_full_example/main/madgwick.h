/**
 * Madgwick AHRS 滤波器（C 移植版）
 * 原作者：SOH Madgwick, http://www.x-io.co.uk/open-source-imu-and-ahrs-algorithms/
 * License: GNU GPL
 */

#pragma once
#include <stdint.h>

typedef struct {
    float q0, q1, q2, q3;  /* quaternion: w, x, y, z */
    float beta;
    int64_t last_us;
} madgwick_t;

void madgwick_init(madgwick_t *m, float beta);

/* 6-DOF update (no magnetometer). gx/gy/gz in deg/s, ax/ay/az in g. */
void madgwick_update_imu(madgwick_t *m,
                         float gx, float gy, float gz,
                         float ax, float ay, float az);

/* 9-DOF update. mx/my/mz in any consistent unit (Gauss). */
void madgwick_update(madgwick_t *m,
                     float gx, float gy, float gz,
                     float ax, float ay, float az,
                     float mx, float my, float mz);

/* Convert quaternion to Euler angles (degrees). */
void madgwick_get_euler(const madgwick_t *m,
                        float *roll, float *pitch, float *yaw);
