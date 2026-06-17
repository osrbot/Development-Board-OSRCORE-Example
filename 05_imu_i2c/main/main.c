/**
 * 示例05：QMI8658 IMU 读取
 *
 * 硬件：I2C，SDA=GPIO10，SCL=GPIO11，地址 0x6B
 * 配置：±4g 加速度计，±1024dps 陀螺仪，1000Hz ODR，5样本均值输出
 * 输出：加速度（g，水平校正）、角速度（deg/s，零偏校正）、温度（°C），每 50ms 打印一次
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "osrcore_fw_update.h"
#include "qmi8658.h"

#define I2C_SDA      10
#define I2C_SCL      11
#define I2C_FREQ_HZ  400000
#define IMU_ADDR     0x6B

static const char *TAG = "imu";

void app_main(void)
{
    osrcore_fw_update_start();

    /* Initialize I2C bus */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port      = I2C_NUM_0,
        .sda_io_num    = I2C_SDA,
        .scl_io_num    = I2C_SCL,
        .clk_source    = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    /* Add QMI8658 device */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = IMU_ADDR,
        .scl_speed_hz    = I2C_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev));

    qmi8658_init(dev);

    /* Startup gyro bias calibration — keep the board still */
    ESP_LOGI(TAG, "Calibrating gyro bias, keep board still...");
    qmi8658_calibrate_bias();
    ESP_LOGI(TAG, "Calibration done. Reading IMU...");

    qmi8658_data_t d;
    while (1) {
        if (qmi8658_read(&d)) {
            printf("ax=%.3f ay=%.3f az=%.3f  gx=%.2f gy=%.2f gz=%.2f  T=%.1f  static=%d\n",
                   d.accelX, d.accelY, d.accelZ,
                   d.gyroX,  d.gyroY,  d.gyroZ,
                   d.temp,
                   (int)qmi8658_is_static());
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
