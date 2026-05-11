#include "qmi8658.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define REG_WHO_AM_I  0x00
#define REG_CTRL1     0x02
#define REG_CTRL2     0x03
#define REG_CTRL3     0x04
#define REG_CTRL7     0x08
#define REG_STATUS1   0x2E
#define REG_TEMP_L    0x33

#define WHO_AM_I_VAL  0x05

static i2c_master_dev_handle_t s_dev;

static void write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    i2c_master_transmit(s_dev, buf, 2, 50);
}

static uint8_t read_reg(uint8_t reg)
{
    uint8_t val = 0;
    i2c_master_transmit_receive(s_dev, &reg, 1, &val, 1, 50);
    return val;
}

static bool read_burst(uint8_t reg, uint8_t *buf, int len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, 50) == ESP_OK;
}

void qmi8658_init(i2c_master_dev_handle_t dev)
{
    s_dev = dev;

    uint8_t id = read_reg(REG_WHO_AM_I);
    if (id != WHO_AM_I_VAL)
        printf("WARN: QMI8658 WHO_AM_I=0x%02X (expected 0x%02X)\n", id, WHO_AM_I_VAL);

    write_reg(REG_CTRL1, 0x40);  /* address auto-increment, little-endian */
    write_reg(REG_CTRL2, 0x23);  /* accel ±8g, 1000Hz ODR */
    write_reg(REG_CTRL3, 0x73);  /* gyro ±2048dps, 1000Hz ODR */
    write_reg(REG_CTRL7, 0x03);  /* enable accel + gyro */

    vTaskDelay(pdMS_TO_TICKS(50));
    printf("INFO: QMI8658 init OK\n");
}

bool qmi8658_read(qmi8658_data_t *out)
{
    uint8_t status = read_reg(REG_STATUS1);
    if (!(status & 0x03))
        return false;

    uint8_t raw[14];
    if (!read_burst(REG_TEMP_L, raw, 14))
        return false;

    int16_t temp_raw = (int16_t)((raw[1] << 8) | raw[0]);
    out->temp = temp_raw / 256.0f;

    int16_t ax = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t ay = (int16_t)((raw[5] << 8) | raw[4]);
    int16_t az = (int16_t)((raw[7] << 8) | raw[6]);
    out->ax = ax / 4096.0f;  /* ±8g: 32768/8 = 4096 LSB/g */
    out->ay = ay / 4096.0f;
    out->az = az / 4096.0f;

    int16_t gx = (int16_t)((raw[9]  << 8) | raw[8]);
    int16_t gy = (int16_t)((raw[11] << 8) | raw[10]);
    int16_t gz = (int16_t)((raw[13] << 8) | raw[12]);
    out->gx = gx / 16.0f;  /* ±2048dps: 32768/2048 = 16 LSB/dps */
    out->gy = gy / 16.0f;
    out->gz = gz / 16.0f;

    return true;
}
