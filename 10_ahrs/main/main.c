/**
 * 示例10：Madgwick AHRS 姿态解算
 *
 * 硬件：QMI8658 IMU（I2C，SDA=GPIO10，SCL=GPIO11）
 * 输出：四元数 + 欧拉角（Roll/Pitch/Yaw），每 5ms 更新，每 100ms 打印
 *
 * Madgwick 算法：
 *   - 6-DOF 模式（仅加速度计 + 陀螺仪）
 *   - β=0.1（收敛速度与噪声抑制的平衡点）
 *   - 使用 Quake III 快速反平方根优化
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "qmi8658.h"
#include "madgwick.h"

#define I2C_SDA     10
#define I2C_SCL     11
#define IMU_ADDR    0x6B

static const char *TAG = "ahrs";

static madgwick_t g_ahrs;
static i2c_master_dev_handle_t g_imu_dev;

static void task_imu(void *arg)
{
    qmi8658_data_t d;
    int print_cnt = 0;

    while (1) {
        if (qmi8658_read(&d)) {
            madgwick_update_imu(&g_ahrs, d.gx, d.gy, d.gz, d.ax, d.ay, d.az);

            if (++print_cnt >= 20) {  /* print every 100ms */
                print_cnt = 0;
                float roll, pitch, yaw;
                madgwick_get_euler(&g_ahrs, &roll, &pitch, &yaw);
                printf("q: %.4f %.4f %.4f %.4f  euler: R=%.1f P=%.1f Y=%.1f\n",
                       g_ahrs.q0, g_ahrs.q1, g_ahrs.q2, g_ahrs.q3,
                       roll, pitch, yaw);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void app_main(void)
{
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

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = IMU_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &g_imu_dev));

    qmi8658_init(g_imu_dev);
    madgwick_init(&g_ahrs, 0.1f);

    ESP_LOGI(TAG, "AHRS running (6-DOF Madgwick, beta=0.1)");
    xTaskCreatePinnedToCore(task_imu, "imu", 4096, NULL, 5, NULL, 1);
}
