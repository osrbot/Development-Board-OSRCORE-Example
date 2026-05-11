/**
 * 示例05：QMI8658 IMU 读取
 *
 * 硬件：I2C，SDA=GPIO10，SCL=GPIO11，地址 0x6B
 * 输出：加速度（g）、角速度（deg/s）、温度（°C），每 10ms 打印一次
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "qmi8658.h"

#define I2C_SDA      10
#define I2C_SCL      11
#define I2C_FREQ_HZ  400000
#define IMU_ADDR     0x6B

static const char *TAG = "imu";

void app_main(void)
{
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
    ESP_LOGI(TAG, "Reading IMU at 100Hz...");

    qmi8658_data_t d;
    while (1) {
        if (qmi8658_read(&d)) {
            printf("ax=%.3f ay=%.3f az=%.3f  gx=%.2f gy=%.2f gz=%.2f  T=%.1f\n",
                   d.ax, d.ay, d.az, d.gx, d.gy, d.gz, d.temp);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
