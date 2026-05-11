/**
 * 示例11：完整机器人示例
 *
 * 整合所有外设，实现与 osrcore 相同的三任务架构：
 *
 *   Core 1  task_imu     P5  5ms   — QMI8658 采样 → Madgwick AHRS
 *   Core 1  task_control P4  20ms  — 编码器测速 → PID → ESC 输出
 *   Core 0  task_comm    P3  1ms   — SBUS 解码 / USB CDC 命令 / 数据上报
 *
 * 控制优先级：SBUS 遥控 > USB CDC 串口命令
 *   SBUS CH7（index 6）< 1500 → 遥控模式
 *   SBUS CH7（index 6）≥ 1500 → 串口控制模式
 *
 * USB CDC 命令：
 *   v <vx> <steer>  — 设置速度(m/s)和转向脉宽(us)
 *   status          — 打印系统状态
 *   kp/ki/kd <val>  — 修改 PID 参数
 *   stop            — 停止
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/pulse_cnt.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "qmi8658.h"
#include "madgwick.h"
#include "pid.h"

/* ---- Pin / parameter config ---- */
#define I2C_SDA         10
#define I2C_SCL         11
#define IMU_ADDR        0x6B
#define EA              3
#define EB              9
#define THROTTLE_PIN    1
#define STEERING_PIN    2
#define BUZZER_PIN      42
#define LED_PIN         45
#define IO_CTRL_BAT     16
#define SBUS_RX_PIN     44
#define SBUS_TX_PIN     43

#define ENCODER_PPR     512
#define GEAR_RATIO      10.55f
#define WHEEL_RADIUS    0.0425f
#define PULSES_PER_REV  (ENCODER_PPR * GEAR_RATIO)
#define CIRCUMFERENCE   (2.0f * 3.14159265f * WHEEL_RADIUS)
#define DIST_PER_PULSE  (CIRCUMFERENCE / PULSES_PER_REV)

#define PWM_FREQ_HZ     50
#define PWM_RES         LEDC_TIMER_14_BIT
#define US_TO_DUTY(us)  ((us) * 80)
#define THROTTLE_NEUTRAL 1500
#define THROTTLE_MIN     1000
#define THROTTLE_MAX     2000
#define STEERING_CENTER  1500

#define CONTROL_DT      0.020f
#define LPF_ALPHA       0.15f
#define SBUS_FRAME      25
#define CONTROL_MODE_CH 6   /* SBUS channel index for mode switch */

static const char *TAG = "full";

/* ---- Global state ---- */
typedef struct {
    float target_speed;
    uint32_t steering_pulse;
    float filtered_speed;
    float quat[4];
    float accel[3], gyro[3];
    bool remote_active;
    bool failsafe;
    bool serial_control;
} app_state_t;

static app_state_t g_state;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

/* ---- Hardware handles ---- */
static pcnt_unit_handle_t s_pcnt;
static int32_t s_pcnt_accum = 0;
static madgwick_t g_ahrs;
static pid_t g_pid;
static i2c_master_dev_handle_t g_imu_dev;

/* ---- PCNT overflow ---- */
static bool IRAM_ATTR pcnt_overflow_cb(pcnt_unit_handle_t u,
                                        const pcnt_watch_event_data_t *e,
                                        void *ctx)
{
    int32_t *acc = (int32_t *)ctx;
    *acc += e->watch_point_val;
    return false;
}

/* ---- Init helpers ---- */
static void init_i2c_imu(void)
{
    i2c_master_bus_config_t bc = {
        .i2c_port = I2C_NUM_0, .sda_io_num = I2C_SDA, .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bc, &bus));

    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = IMU_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dc, &g_imu_dev));
    qmi8658_init(g_imu_dev);
}

static void init_encoder(void)
{
    pcnt_unit_config_t uc = { .low_limit = -32768, .high_limit = 32767 };
    pcnt_new_unit(&uc, &s_pcnt);

    pcnt_chan_config_t cc = { .edge_gpio_num = EA, .level_gpio_num = EB };
    pcnt_channel_handle_t ch;
    pcnt_new_channel(s_pcnt, &cc, &ch);
    pcnt_channel_set_edge_action(ch, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                     PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_channel_set_level_action(ch, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_unit_add_watch_point(s_pcnt,  32767);
    pcnt_unit_add_watch_point(s_pcnt, -32768);
    pcnt_event_callbacks_t cbs = { .on_reach = pcnt_overflow_cb };
    pcnt_unit_register_event_callbacks(s_pcnt, &cbs, &s_pcnt_accum);
    pcnt_unit_enable(s_pcnt);
    pcnt_unit_clear_count(s_pcnt);
    pcnt_unit_start(s_pcnt);
}

static void init_pwm(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = PWM_RES,
        .timer_num = LEDC_TIMER_0, .freq_hz = PWM_FREQ_HZ, .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);

    ledc_channel_config_t ch_thr = {
        .gpio_num = THROTTLE_PIN, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0,
        .duty = US_TO_DUTY(THROTTLE_NEUTRAL), .hpoint = 0,
    };
    ledc_channel_config(&ch_thr);

    ledc_channel_config_t ch_str = {
        .gpio_num = STEERING_PIN, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1, .timer_sel = LEDC_TIMER_0,
        .duty = US_TO_DUTY(STEERING_CENTER), .hpoint = 0,
    };
    ledc_channel_config(&ch_str);
}

static void init_sbus(void)
{
    uart_config_t cfg = {
        .baud_rate = 100000, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN, .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &cfg);
    uart_set_pin(UART_NUM_0, SBUS_TX_PIN, SBUS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_line_inverse(UART_NUM_0, UART_SIGNAL_RXD_INV | UART_SIGNAL_TXD_INV);
}

static void set_throttle(uint32_t us)
{
    if (us < THROTTLE_MIN) us = THROTTLE_MIN;
    if (us > THROTTLE_MAX) us = THROTTLE_MAX;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, US_TO_DUTY(us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void set_steering(uint32_t us)
{
    if (us < THROTTLE_MIN) us = THROTTLE_MIN;
    if (us > THROTTLE_MAX) us = THROTTLE_MAX;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, US_TO_DUTY(us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* ---- task_imu: Core 1, P5, 5ms ---- */
static void task_imu(void *arg)
{
    qmi8658_data_t d;
    while (1) {
        if (qmi8658_read(&d)) {
            madgwick_update_imu(&g_ahrs, d.gx, d.gy, d.gz, d.ax, d.ay, d.az);
            portENTER_CRITICAL(&g_mux);
            g_state.quat[0] = g_ahrs.q0; g_state.quat[1] = g_ahrs.q1;
            g_state.quat[2] = g_ahrs.q2; g_state.quat[3] = g_ahrs.q3;
            g_state.accel[0] = d.ax; g_state.accel[1] = d.ay; g_state.accel[2] = d.az;
            g_state.gyro[0]  = d.gx; g_state.gyro[1]  = d.gy; g_state.gyro[2]  = d.gz;
            portEXIT_CRITICAL(&g_mux);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* ---- task_control: Core 1, P4, 20ms ---- */
static void task_control(void *arg)
{
    int raw = 0;
    pcnt_unit_get_count(s_pcnt, &raw);
    int32_t last_count = s_pcnt_accum + raw;
    float filtered = 0.0f;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));

        pcnt_unit_get_count(s_pcnt, &raw);
        int32_t now   = s_pcnt_accum + raw;
        int32_t delta = now - last_count;
        last_count = now;

        float speed = (delta * DIST_PER_PULSE) / CONTROL_DT;
        filtered = LPF_ALPHA * speed + (1.0f - LPF_ALPHA) * filtered;

        portENTER_CRITICAL(&g_mux);
        float target  = g_state.target_speed;
        uint32_t steer = g_state.steering_pulse;
        g_state.filtered_speed = filtered;
        portEXIT_CRITICAL(&g_mux);

        float out = pid_calc(&g_pid, target, filtered, CONTROL_DT);
        int32_t pulse = THROTTLE_NEUTRAL + (int32_t)out;
        set_throttle((uint32_t)pulse);
        set_steering(steer);
    }
}

/* ---- SBUS parse ---- */
static bool sbus_parse(const uint8_t *buf, uint16_t *ch, bool *fs)
{
    if (buf[0] != 0x0F || buf[24] != 0x00) return false;
    ch[0]  = ((buf[1]       | buf[2]  << 8) & 0x7FF);
    ch[1]  = ((buf[2]  >> 3 | buf[3]  << 5) & 0x7FF);
    ch[2]  = ((buf[3]  >> 6 | buf[4]  << 2 | buf[5] << 10) & 0x7FF);
    ch[3]  = ((buf[5]  >> 1 | buf[6]  << 7) & 0x7FF);
    ch[4]  = ((buf[6]  >> 4 | buf[7]  << 4) & 0x7FF);
    ch[5]  = ((buf[7]  >> 7 | buf[8]  << 1 | buf[9] << 9) & 0x7FF);
    ch[6]  = ((buf[9]  >> 2 | buf[10] << 6) & 0x7FF);
    ch[7]  = ((buf[10] >> 5 | buf[11] << 3) & 0x7FF);
    *fs = (buf[23] & (1 << 2)) != 0;
    return true;
}

#define SBUS_TO_US(v) ((uint32_t)(((v) - 240) * 1000 / 1570 + 1000))

/* ---- task_comm: Core 0, P3 ---- */
static void task_comm(void *arg)
{
    uint8_t sbus_buf[SBUS_FRAME];
    uint16_t ch[16];
    bool fs;
    char line[64];
    int64_t last_serial_cmd = 0;
    int64_t last_print = 0;

    while (1) {
        /* --- SBUS receive (non-blocking peek) --- */
        uint8_t byte;
        if (uart_read_bytes(UART_NUM_0, &byte, 1, 0) == 1 && byte == 0x0F) {
            sbus_buf[0] = byte;
            if (uart_read_bytes(UART_NUM_0, sbus_buf + 1, SBUS_FRAME - 1,
                                pdMS_TO_TICKS(5)) == SBUS_FRAME - 1) {
                if (sbus_parse(sbus_buf, ch, &fs)) {
                    bool remote = (ch[CONTROL_MODE_CH] < 1500);
                    portENTER_CRITICAL(&g_mux);
                    g_state.failsafe     = fs;
                    g_state.remote_active = remote && !fs;
                    if (remote && !fs) {
                        /* Map SBUS ch2 (throttle) and ch0 (steering) to PWM */
                        float vx = ((float)ch[2] - 1024.0f) / 784.0f * 6.0f;
                        g_state.target_speed   = vx;
                        g_state.steering_pulse = SBUS_TO_US(ch[0]);
                    }
                    portEXIT_CRITICAL(&g_mux);
                }
            }
        }

        /* --- USB CDC command --- */
        if (fgets(line, sizeof(line), stdin)) {
            line[strcspn(line, "\r\n")] = '\0';
            float v, s;
            portENTER_CRITICAL(&g_mux);
            bool serial_ok = !g_state.remote_active;
            portEXIT_CRITICAL(&g_mux);

            if (serial_ok) {
                if (sscanf(line, "v %f %f", &v, &s) == 2) {
                    portENTER_CRITICAL(&g_mux);
                    g_state.target_speed   = v;
                    g_state.steering_pulse = (uint32_t)s;
                    portEXIT_CRITICAL(&g_mux);
                    last_serial_cmd = esp_timer_get_time();
                } else if (strcmp(line, "stop") == 0) {
                    portENTER_CRITICAL(&g_mux);
                    g_state.target_speed   = 0.0f;
                    g_state.steering_pulse = STEERING_CENTER;
                    portEXIT_CRITICAL(&g_mux);
                } else if (strcmp(line, "status") == 0) {
                    portENTER_CRITICAL(&g_mux);
                    float spd = g_state.filtered_speed;
                    bool rem  = g_state.remote_active;
                    bool fail = g_state.failsafe;
                    portEXIT_CRITICAL(&g_mux);
                    printf("speed=%.2f remote=%d failsafe=%d\n", spd, rem, fail);
                }
            }
        }

        /* Serial control timeout: 500ms without command → stop */
        if (!g_state.remote_active && last_serial_cmd > 0) {
            if ((esp_timer_get_time() - last_serial_cmd) > 500000) {
                portENTER_CRITICAL(&g_mux);
                g_state.target_speed   = 0.0f;
                g_state.steering_pulse = STEERING_CENTER;
                portEXIT_CRITICAL(&g_mux);
                last_serial_cmd = 0;
            }
        }

        /* Periodic IMU print (every 100ms) */
        int64_t now = esp_timer_get_time();
        if (now - last_print > 100000) {
            last_print = now;
            portENTER_CRITICAL(&g_mux);
            float q0 = g_state.quat[0], q1 = g_state.quat[1];
            float q2 = g_state.quat[2], q3 = g_state.quat[3];
            float spd = g_state.filtered_speed;
            portEXIT_CRITICAL(&g_mux);
            float roll  = atan2f(2.0f*(q0*q1+q2*q3), 1.0f-2.0f*(q1*q1+q2*q2)) * 57.296f;
            float pitch = asinf(2.0f*(q0*q2-q3*q1)) * 57.296f;
            printf("spd=%.2f R=%.1f P=%.1f\n", spd, roll, pitch);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_main(void)
{
    /* USB CDC console */
    usb_serial_jtag_driver_config_t usb_cfg = { .rx_buffer_size = 512, .tx_buffer_size = 512 };
    usb_serial_jtag_driver_install(&usb_cfg);
    esp_vfs_usb_serial_jtag_use_driver();

    /* Battery enable */
    gpio_set_direction(IO_CTRL_BAT, GPIO_MODE_OUTPUT);
    gpio_set_level(IO_CTRL_BAT, 1);

    ESP_ERROR_CHECK(nvs_flash_init());

    init_i2c_imu();
    init_encoder();
    init_pwm();
    init_sbus();

    madgwick_init(&g_ahrs, 0.1f);
    pid_init(&g_pid, 447.0f, 4.7f, 47.0f, 1000.0f, 0.05f);

    g_state.steering_pulse = STEERING_CENTER;
    g_state.target_speed   = 0.0f;

    ESP_LOGI(TAG, "ESC arming (2s)...");
    set_throttle(THROTTLE_NEUTRAL);
    set_steering(STEERING_CENTER);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Ready");

    xTaskCreatePinnedToCore(task_imu,     "imu",     4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(task_control, "control", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(task_comm,    "comm",    8192, NULL, 3, NULL, 0);
}
