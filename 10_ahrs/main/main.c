/** Lesson 10: complementary roll/pitch filter, 5 ms interval. Yaw is not observable from gravity. */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "qmi8658.h"
#include <math.h>

#include "osrcore_fw_update.h"
#include "osrcore_params.h"
#include "osrcore_lesson_config.h"

#define I2C_SDA     10
#define I2C_SCL     11
#define IMU_ADDR    0x6B

static const char *TAG = "ahrs";

static float roll, pitch;
static i2c_master_dev_handle_t g_imu_dev;

static void task_imu(void *arg)
{
    qmi8658_data_t d;
    int64_t previous = esp_timer_get_time(), last_print = previous;

    while (1) {
        if (qmi8658_read(&d)) {
            int64_t now = esp_timer_get_time();
            float dt = fminf((now - previous) / 1000000.0f, 0.1f);
            previous = now;
            float gravity_roll=atan2f(d.accelY,d.accelZ)*57.2957795f;
            float gravity_pitch=atan2f(-d.accelX,sqrtf(d.accelY*d.accelY+d.accelZ*d.accelZ))*57.2957795f;
            float alpha = (float)osrcore_param_get("alpha", OSR_AHRS_ALPHA);
            roll=alpha*(roll+d.gyroX*dt)+(1-alpha)*gravity_roll;
            pitch=alpha*(pitch+d.gyroY*dt)+(1-alpha)*gravity_pitch;
            if (now - last_print >= 100000) {
                last_print = now;
                ESP_LOGI(TAG,"roll=%.2f pitch=%.2f alpha=%.2f",roll,pitch,(double)alpha);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void app_main(void)
{
    static const osrcore_param_t parameters[] = {
        {"alpha", 0.8, 0.99, 0.01, OSR_AHRS_ALPHA},
    };
    osrcore_params_init(parameters, sizeof(parameters)/sizeof(parameters[0]));
    osrcore_fw_update_start();

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
    qmi8658_calibrate_bias();
    xTaskCreatePinnedToCore(task_imu, "imu", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "Complementary attitude filter; heater disabled; keep still for calibration.");
}
