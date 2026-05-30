#include "qmi8658.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#define QMI_REG_WHO_AM_I    0x00
#define QMI_REG_CTRL1       0x02
#define QMI_REG_CTRL2       0x03
#define QMI_REG_CTRL3       0x04
#define QMI_REG_CTRL7       0x08
#define QMI_REG_STATUS1     0x2E
#define QMI_REG_TEMP_L      0x33

#define QMI_WHO_AM_I_VAL    0x05

#define HISTORY_LEN             10
#define IMU_AVG_SAMPLES         5
#define ACCEL_SCALE             8192.0f   // LSB/g  for ±4g  (32768/4)
#define GYRO_SCALE              32.0f     // LSB/dps for ±1024dps (32768/1024)

#define GYRO_BIAS_SAMPLES       100
#define GYRO_BIAS_BATCH_SAMPLES 1000
#define STATIONARY_COUNT_THRESHOLD 10

// Static detection thresholds
#define STATIC_ACC_VAR_MAX      0.001f    // g²: accel Z variance
#define STATIC_AMAG_TOL         0.2f      // g: |amag - 1g|
#define STATIC_GYR_NORM_MAX     1.5f      // dps: gyro Z mean / full norm

// Bias sanity: reject per-update step > 5 dps (glitch guard)
#define BIAS_STEP_MAX           0.5f

static i2c_master_dev_handle_t s_dev = NULL;

static float s_acc_hist[HISTORY_LEN][3];
static float s_gyr_hist[HISTORY_LEN][3];
static int   s_hist_idx   = 0;
static int   s_hist_count = 0;

static float s_bias[3]         = {0};
static float s_accel_offset[3] = {0};
static bool  s_bias_ready      = false;
static bool  s_is_static       = false;
static int   s_static_count    = 0;

static float s_batch_sum[3] = {0};
static int   s_batch_count  = 0;

static float s_avg_acc[3] = {0};
static float s_avg_gyr[3] = {0};
static float s_avg_temp   = 0.0f;
static int   s_avg_n      = 0;

// ---------- low-level I2C helpers ----------

static void write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    i2c_master_transmit(s_dev, buf, 2, 50);
}

static uint8_t read_reg(uint8_t reg)
{
    uint8_t val = 0;
    i2c_master_transmit_receive(s_dev, &reg, 1, &val, 1, 50);
    return val;
}

static bool read_burst(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, 50) == ESP_OK;
}

// ---------- public API ----------

void qmi8658_init(i2c_master_dev_handle_t dev)
{
    s_dev = dev;
    memset(s_acc_hist, 0, sizeof(s_acc_hist));
    memset(s_gyr_hist, 0, sizeof(s_gyr_hist));

    uint8_t id = read_reg(QMI_REG_WHO_AM_I);
    if (id != QMI_WHO_AM_I_VAL) {
        printf("WARN: QMI8658 WHO_AM_I=0x%02X expected 0x%02X\n", id, QMI_WHO_AM_I_VAL);
    }

    write_reg(QMI_REG_CTRL1, 0x40);  // address auto-increment, little-endian
    write_reg(QMI_REG_CTRL2, 0x13);  // accel ±4g, 1000Hz ODR
    write_reg(QMI_REG_CTRL3, 0x63);  // gyro ±1024dps, 1000Hz ODR
    write_reg(QMI_REG_CTRL7, 0x03);  // enable accel + gyro

    vTaskDelay(pdMS_TO_TICKS(50));
    printf("INFO: QMI8658 init OK\n");
}

void qmi8658_calibrate_bias(void)
{
    float sum[3] = {0};
    int count = 0;
    int attempts = 0;
    while (count < GYRO_BIAS_SAMPLES && attempts < GYRO_BIAS_SAMPLES * 4) {
        attempts++;
        uint8_t status = read_reg(QMI_REG_STATUS1);
        if (!(status & 0x03)) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }
        uint8_t raw[14];
        if (!read_burst(QMI_REG_TEMP_L, raw, 14)) continue;
        int16_t gx = (int16_t)((raw[9]  << 8) | raw[8]);
        int16_t gy = (int16_t)((raw[11] << 8) | raw[10]);
        int16_t gz = (int16_t)((raw[13] << 8) | raw[12]);
        sum[0] += gx / GYRO_SCALE;
        sum[1] += gy / GYRO_SCALE;
        sum[2] += gz / GYRO_SCALE;
        count++;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (count >= GYRO_BIAS_SAMPLES) {
        float nb[3] = { sum[0]/count, sum[1]/count, sum[2]/count };
        if (fabsf(nb[0]) < BIAS_STEP_MAX * 10.0f
         && fabsf(nb[1]) < BIAS_STEP_MAX * 10.0f
         && fabsf(nb[2]) < BIAS_STEP_MAX * 10.0f) {
            s_bias[0] = nb[0];
            s_bias[1] = nb[1];
            s_bias[2] = nb[2];
            s_bias_ready = true;
            printf("INFO: gyro bias init: %.3f %.3f %.3f dps\n",
                   s_bias[0], s_bias[1], s_bias[2]);
        } else {
            printf("WARN: gyro bias out of range (%.3f %.3f %.3f), skipped\n",
                   nb[0], nb[1], nb[2]);
        }
    }
}

// ---------- static detection ----------

static bool detect_static(void)
{
    if (s_hist_count < HISTORY_LEN) return false;

    float am[3] = {0};
    float gm[3] = {0};
    for (int i = 0; i < HISTORY_LEN; i++) {
        am[0] += s_acc_hist[i][0];
        am[1] += s_acc_hist[i][1];
        am[2] += s_acc_hist[i][2];
        gm[0] += s_gyr_hist[i][0];
        gm[1] += s_gyr_hist[i][1];
        gm[2] += s_gyr_hist[i][2];
    }
    for (int j = 0; j < 3; j++) { am[j] /= HISTORY_LEN; gm[j] /= HISTORY_LEN; }

    float az_var = 0;
    for (int i = 0; i < HISTORY_LEN; i++) {
        float d = s_acc_hist[i][2] - am[2];
        az_var += d * d;
    }
    az_var /= HISTORY_LEN;

    float gz_mean_abs = fabsf(gm[2]);
    float gnorm = sqrtf(gm[0]*gm[0] + gm[1]*gm[1] + gm[2]*gm[2]);
    float amag  = sqrtf(am[0]*am[0] + am[1]*am[1] + am[2]*am[2]);

    return (fabsf(amag - 1.0f) < STATIC_AMAG_TOL)
        && (az_var < STATIC_ACC_VAR_MAX)
        && (gz_mean_abs < STATIC_GYR_NORM_MAX)
        && (gnorm < STATIC_GYR_NORM_MAX);
}

static void update_bias(float gx, float gy, float gz)
{
    bool currently_static = detect_static();

    if (currently_static) {
        s_static_count++;
        if (s_static_count > STATIONARY_COUNT_THRESHOLD) {
            if (!s_is_static) {
                s_is_static  = true;
                s_bias_ready = true;
            }

            s_batch_sum[0] += gx;
            s_batch_sum[1] += gy;
            s_batch_sum[2] += gz;
            s_batch_count++;

            if (s_batch_count >= GYRO_BIAS_BATCH_SAMPLES) {
                float nb[3] = {
                    s_batch_sum[0] / s_batch_count,
                    s_batch_sum[1] / s_batch_count,
                    s_batch_sum[2] / s_batch_count,
                };
                if (fabsf(nb[0] - s_bias[0]) < BIAS_STEP_MAX * 10.0f
                 && fabsf(nb[1] - s_bias[1]) < BIAS_STEP_MAX * 10.0f
                 && fabsf(nb[2] - s_bias[2]) < BIAS_STEP_MAX * 10.0f) {
                    s_bias[0] = nb[0];
                    s_bias[1] = nb[1];
                    s_bias[2] = nb[2];
                    printf("INFO: gyro bias updated: %.3f %.3f %.3f dps\n",
                           s_bias[0], s_bias[1], s_bias[2]);
                }
                s_batch_sum[0] = s_batch_sum[1] = s_batch_sum[2] = 0.0f;
                s_batch_count  = 0;
            }
        }
    } else {
        s_static_count = 0;
        s_is_static    = false;
        s_batch_sum[0] = s_batch_sum[1] = s_batch_sum[2] = 0.0f;
        s_batch_count  = 0;
    }
}

bool qmi8658_read(qmi8658_data_t *out)
{
    uint8_t status = read_reg(QMI_REG_STATUS1);
    if (!(status & 0x03)) return false;

    uint8_t raw[14];
    if (!read_burst(QMI_REG_TEMP_L, raw, 14)) return false;

    int16_t temp_raw = (int16_t)((raw[1] << 8) | raw[0]);
    s_avg_temp += temp_raw / 256.0f;

    int16_t ax = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t ay = (int16_t)((raw[5] << 8) | raw[4]);
    int16_t az = (int16_t)((raw[7] << 8) | raw[6]);
    s_avg_acc[0] += ax / ACCEL_SCALE;
    s_avg_acc[1] += ay / ACCEL_SCALE;
    s_avg_acc[2] += az / ACCEL_SCALE;

    int16_t gx = (int16_t)((raw[9]  << 8) | raw[8]);
    int16_t gy = (int16_t)((raw[11] << 8) | raw[10]);
    int16_t gz = (int16_t)((raw[13] << 8) | raw[12]);
    s_avg_gyr[0] += gx / GYRO_SCALE;
    s_avg_gyr[1] += gy / GYRO_SCALE;
    s_avg_gyr[2] += gz / GYRO_SCALE;

    if (++s_avg_n < IMU_AVG_SAMPLES) return false;

    out->temp   = s_avg_temp   / IMU_AVG_SAMPLES;
    out->accelX = s_avg_acc[0] / IMU_AVG_SAMPLES - s_accel_offset[0];
    out->accelY = s_avg_acc[1] / IMU_AVG_SAMPLES - s_accel_offset[1];
    out->accelZ = s_avg_acc[2] / IMU_AVG_SAMPLES - s_accel_offset[2];

    float gxf = s_avg_gyr[0] / IMU_AVG_SAMPLES;
    float gyf = s_avg_gyr[1] / IMU_AVG_SAMPLES;
    float gzf = s_avg_gyr[2] / IMU_AVG_SAMPLES;

    s_avg_acc[0] = s_avg_acc[1] = s_avg_acc[2] = 0.0f;
    s_avg_gyr[0] = s_avg_gyr[1] = s_avg_gyr[2] = 0.0f;
    s_avg_temp = 0.0f;
    s_avg_n    = 0;

    s_acc_hist[s_hist_idx][0] = out->accelX;
    s_acc_hist[s_hist_idx][1] = out->accelY;
    s_acc_hist[s_hist_idx][2] = out->accelZ;
    s_gyr_hist[s_hist_idx][0] = gxf;
    s_gyr_hist[s_hist_idx][1] = gyf;
    s_gyr_hist[s_hist_idx][2] = gzf;
    s_hist_idx = (s_hist_idx + 1) % HISTORY_LEN;
    if (s_hist_count < HISTORY_LEN) s_hist_count++;

    update_bias(gxf, gyf, gzf);

    if (s_bias_ready) {
        gxf -= s_bias[0];
        gyf -= s_bias[1];
        gzf -= s_bias[2];
    }

    out->gyroX = gxf;
    out->gyroY = gyf;
    out->gyroZ = gzf;

    return true;
}

bool qmi8658_is_static(void) { return s_is_static; }

void qmi8658_set_level_offset(float ox, float oy, float oz)
{
    s_accel_offset[0] = ox;
    s_accel_offset[1] = oy;
    s_accel_offset[2] = oz;
}
