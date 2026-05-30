#include "qmc6309.h"
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

/* ------------------------------------------------------------------ */
/* Internal constants                                                   */
/* ------------------------------------------------------------------ */
#define QMC_READ_I2C_TIMEOUT_MS         2
#define QMC_CALIBRATION_READ_TIMEOUT_MS 10

#define CTRL1_OSR2(n)   ((n) << 5)
#define CTRL1_OSR1(n)   ((n) << 3)
#define CTRL1_MODE(m)   ((m) & 0x03)

#define CTRL2_SOFT_RST  (1 << 7)
#define CTRL2_ODR(n)    ((n) << 4)
#define CTRL2_RNG(n)    ((n) << 2)
#define CTRL2_SR_MODE(n) ((n) << 0)

static const float k_identity[9] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
};

/* ------------------------------------------------------------------ */
/* Module state                                                         */
/* ------------------------------------------------------------------ */
static i2c_master_dev_handle_t s_dev = NULL;
static float s_sensitivity = SENSITIVITY_8G;
static float s_hard_iron[3];
static float s_soft_iron[9];
static float s_min_val[3];
static float s_max_val[3];

/* ------------------------------------------------------------------ */
/* Low-level I2C helpers                                                */
/* ------------------------------------------------------------------ */
static void write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    i2c_master_transmit(s_dev, buf, 2, 100);
}

static bool read_reg(uint8_t reg, uint8_t *val, uint32_t timeout_ms)
{
    esp_err_t ret = i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, (int)timeout_ms);
    return ret == ESP_OK;
}

static bool read_burst(uint8_t start_reg, uint8_t *buf, uint8_t len, uint32_t timeout_ms)
{
    esp_err_t ret = i2c_master_transmit_receive(s_dev, &start_reg, 1, buf, len, (int)timeout_ms);
    return ret == ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void qmc6309_init(i2c_master_dev_handle_t dev)
{
    s_dev = dev;
    s_sensitivity = SENSITIVITY_8G;
    memset(s_hard_iron, 0, sizeof(s_hard_iron));
    memcpy(s_soft_iron, k_identity, sizeof(s_soft_iron));
    for (int i = 0; i < 3; i++) {
        s_min_val[i] =  1e6f;
        s_max_val[i] = -1e6f;
    }

    /* Soft reset */
    write_reg(QMC_REG_CTRL2, CTRL2_SOFT_RST);
    vTaskDelay(pdMS_TO_TICKS(20));
    write_reg(QMC_REG_CTRL2, 0x00);
    vTaskDelay(pdMS_TO_TICKS(20));

    /* Verify chip ID */
    uint8_t chip_id = 0;
    read_reg(QMC_REG_CHIP_ID, &chip_id, 100);
    /* chip_id should be 0x90; caller may check via ESP_LOGI if desired */
    (void)chip_id;

    /* rng=2 (8G), odr=4, osr1=3, osr2=4 — same defaults as osrcore */
    uint8_t rng  = 2;
    uint8_t odr  = 4;
    uint8_t osr1 = 3;
    uint8_t osr2 = 4;

    uint8_t ctrl1 = CTRL1_OSR2(osr2) | CTRL1_OSR1(osr1) | CTRL1_MODE(0);
    write_reg(QMC_REG_CTRL1, ctrl1);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t ctrl2 = CTRL2_ODR(odr) | CTRL2_RNG(rng) | CTRL2_SR_MODE(0);
    write_reg(QMC_REG_CTRL2, ctrl2);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Switch to continuous mode (mode=3) */
    ctrl1 = (ctrl1 & ~0x03u) | CTRL1_MODE(3);
    write_reg(QMC_REG_CTRL1, ctrl1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

bool qmc6309_read(qmc6309_data_t *out)
{
    if (!out) return false;

    /* Check DRDY — non-blocking (timeout=0 means single poll) */
    uint8_t status = 0;
    if (!read_reg(QMC_REG_STATUS, &status, QMC_READ_I2C_TIMEOUT_MS)) return false;
    if (!(status & STATUS_DRDY)) return false;

    /* Burst-read 6 bytes: X_LSB, X_MSB, Y_LSB, Y_MSB, Z_LSB, Z_MSB */
    uint8_t data[6];
    if (!read_burst(QMC_REG_X_LSB, data, 6, QMC_READ_I2C_TIMEOUT_MS)) return false;

    int16_t rx = (int16_t)((data[1] << 8) | data[0]);
    int16_t ry = (int16_t)((data[3] << 8) | data[2]);
    int16_t rz = (int16_t)((data[5] << 8) | data[4]);

    /* Convert to Gauss */
    float raw_x = rx / s_sensitivity;
    float raw_y = ry / s_sensitivity;
    float raw_z = rz / s_sensitivity;

    /* Apply hard-iron then soft-iron: cal = soft * (raw - hard) */
    float dx = raw_x - s_hard_iron[0];
    float dy = raw_y - s_hard_iron[1];
    float dz = raw_z - s_hard_iron[2];

    out->x = s_soft_iron[0]*dx + s_soft_iron[1]*dy + s_soft_iron[2]*dz;
    out->y = s_soft_iron[3]*dx + s_soft_iron[4]*dy + s_soft_iron[5]*dz;
    out->z = s_soft_iron[6]*dx + s_soft_iron[7]*dy + s_soft_iron[8]*dz;

    /* Heading from calibrated X/Y */
    float heading = atan2f(out->y, out->x) * 180.0f / 3.14159265358979f;
    if (heading < 0.0f) heading += 360.0f;
    out->heading = heading;

    return true;
}

void qmc6309_set_calibration(const float hard[3], const float soft[9])
{
    memcpy(s_hard_iron, hard, 3 * sizeof(float));
    memcpy(s_soft_iron, soft, 9 * sizeof(float));
}

void qmc6309_get_calibration(float hard[3], float soft[9])
{
    memcpy(hard, s_hard_iron, 3 * sizeof(float));
    memcpy(soft, s_soft_iron, 9 * sizeof(float));
}

void qmc6309_calibrate(uint32_t duration_ms)
{
    for (int i = 0; i < 3; i++) {
        s_min_val[i] =  1e6f;
        s_max_val[i] = -1e6f;
    }

    int64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < (int64_t)duration_ms * 1000) {
        uint8_t status = 0;
        if (!read_reg(QMC_REG_STATUS, &status, QMC_CALIBRATION_READ_TIMEOUT_MS)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (!(status & STATUS_DRDY)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        uint8_t data[6];
        if (!read_burst(QMC_REG_X_LSB, data, 6, QMC_CALIBRATION_READ_TIMEOUT_MS)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int16_t rx = (int16_t)((data[1] << 8) | data[0]);
        int16_t ry = (int16_t)((data[3] << 8) | data[2]);
        int16_t rz = (int16_t)((data[5] << 8) | data[4]);

        float gx = rx / s_sensitivity;
        float gy = ry / s_sensitivity;
        float gz = rz / s_sensitivity;

        if (gx < s_min_val[0]) s_min_val[0] = gx;
        if (gx > s_max_val[0]) s_max_val[0] = gx;
        if (gy < s_min_val[1]) s_min_val[1] = gy;
        if (gy > s_max_val[1]) s_max_val[1] = gy;
        if (gz < s_min_val[2]) s_min_val[2] = gz;
        if (gz > s_max_val[2]) s_max_val[2] = gz;

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Hard-iron: midpoint of min/max */
    for (int i = 0; i < 3; i++) {
        s_hard_iron[i] = (s_max_val[i] + s_min_val[i]) / 2.0f;
    }

    /* Soft-iron: diagonal XY scale; Z kept at 1.0 */
    float range_x = s_max_val[0] - s_min_val[0];
    float range_y = s_max_val[1] - s_min_val[1];
    if (range_x < 1e-4f) range_x = 1e-4f;
    if (range_y < 1e-4f) range_y = 1e-4f;
    float avg_range_xy = (range_x + range_y) / 2.0f;

    memcpy(s_soft_iron, k_identity, sizeof(s_soft_iron));
    s_soft_iron[0] = avg_range_xy / range_x;
    s_soft_iron[4] = avg_range_xy / range_y;
    /* s_soft_iron[8] = 1.0f already set by identity copy */
}

float qmc6309_tilt_compensated_heading(float mx, float my, float mz,
                                        float roll_rad, float pitch_rad)
{
    float cos_r = cosf(roll_rad);
    float sin_r = sinf(roll_rad);
    float cos_p = cosf(pitch_rad);
    float sin_p = sinf(pitch_rad);

    /* Project mag vector onto horizontal plane using roll and pitch */
    float xh = mx * cos_p + mz * sin_p;
    float yh = mx * sin_r * sin_p + my * cos_r - mz * sin_r * cos_p;

    float heading = atan2f(yh, xh) * 180.0f / 3.14159265358979f;
    if (heading < 0.0f) heading += 360.0f;
    return heading;
}
