/**
 * 示例11：完整机器人示例 (osrcore 架构)
 *
 * 三任务双核架构：
 *   Core 1  task_imu     P5  1ms   — QMI8658 采样 → Madgwick AHRS → 里程计积分
 *   Core 1  task_control P4  20ms  — 编码器测速 → PID → ESC 输出 → ADC 电压监测
 *   Core 0  task_comm    P3  1ms   — SBUS 解码 / USB CDC 命令 / 数据上报
 *
 * 控制优先级：SBUS 遥控 > USB CDC 串口命令
 *   SBUS CH7（index 6）< 1500 → 遥控模式
 *   SBUS CH7（index 6）≥ 1500 → 串口控制模式
 *   SBUS CH8（index 7）< 1500 → 限速 15%
 *
 * USB CDC 命令：
 *   v <vx> <steer_deg>  — 设置速度(m/s)和转向角度(deg)
 *   i                   — 打印 IMU
 *   o                   — 打印里程计
 *   status              — 完整状态
 *   kp/ki/kd <val>      — 修改 PID 参数
 *   kr                  — 重置 PID
 *   reset               — 重启
 *   help                — 命令列表
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2c_master.h"
#include "driver/pulse_cnt.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "qmi8658.h"
#include "madgwick.h"
#include "pid.h"
#include "imu_heater.h"

/* ---- Pin / parameter config ---- */
#define I2C_SDA             10
#define I2C_SCL             11
#define IMU_ADDR            0x6B
#define EA                  3
#define EB                  9
#define THROTTLE_PIN        1
#define STEERING_PIN        2
#define VOLTAGE_PIN         4
#define BUZZER_PIN          42
#define LED_PIN             45
#define IO_CTRL_BAT         16
#define SBUS_RX_PIN         44
#define SBUS_TX_PIN         43

/* ---- Encoder ---- */
#define ENCODER_PPR         1024
#define GEAR_RATIO          10.55f
#define WHEEL_RADIUS        0.0425f
#define WHEEL_CIRCUMFERENCE (2.0f * 3.14159265f * WHEEL_RADIUS)

/* ---- PWM ---- */
#define PWM_FREQ_HZ         50
#define PWM_RES_BITS        14
#define PWM_RES             LEDC_TIMER_14_BIT
/* duty = pulse_us / 20000 * 16383 */
#define DUTY_FROM_PULSE(us) ((uint32_t)((us) / 20000.0f * 16383.0f))

/* ---- Servo limits ---- */
#define THROTTLE_NEUTRAL    1500
#define THROTTLE_MIN        1000
#define THROTTLE_MAX        2000
#define STEERING_CENTER     1500
#define STEERING_MIN        1000
#define STEERING_MAX        2000

/* ---- Control ---- */
#define CONTROL_INTERVAL_MS 20
#define SPEED_CALC_INTERVAL_MS 20
#define SPEED_LPF_ALPHA     0.15f
#define SPEED_STOP_THRESHOLD 0.05f
#define MAX_SPEED_FORWARD   6.0f
#define MAX_SPEED_REVERSE   6.0f

/* ---- PID defaults ---- */
#define PID_KP_DEFAULT      447.0f
#define PID_KI_DEFAULT      4.7f
#define PID_KD_DEFAULT      47.0f
#define PID_MAX_INTEGRAL    1000.0f
#define PID_DEADBAND        0.05f

/* ---- SBUS ---- */
#define SBUS_FRAME          25
#define SBUS_MIN            240
#define SBUS_MAX            1810
#define CONTROL_MODE_CH     6   /* index 6: < 1500 → remote */
#define SPEED_MODE_CH       7   /* index 7: < 1500 → limit 15% */
#define STEERING_CH         0
#define THROTTLE_CH         2
#define RC_STEERING_DEADZONE 10
#define RC_THROTTLE_DEADZONE 30
#define STEERING_REVERSE    1
#define THROTTLE_REVERSE    0

/* ---- ADC voltage ---- */
#define LOW_VOLTAGE_THRESHOLD 11.3f
#define NUM_ADC_POINTS        9

/* ---- Timeouts ---- */
#define SERIAL_TIMEOUT_MS   500
#define RC_TIMEOUT_MS       200

/* ---- Serial TX queue ---- */
#define SERIAL_TX_LINE_MAX  192
#define SERIAL_TX_QUEUE_LEN 32

/* ---- Steering angle → pulse ---- */
/* steer_deg: -45..+45 maps to STEERING_MIN..STEERING_MAX */
#define STEER_DEG_TO_PULSE(deg) \
    ((int)(STEERING_CENTER + (int)((deg) / 45.0f * (STEERING_MAX - STEERING_CENTER))))

static const char *TAG = "full";

/* ---- ADC lookup table ---- */
static const int   ADC_PTS[]  = {1017,1135,1197,1257,1379,1450,1495,1555,1615};
static const float VOLT_PTS[] = {9.0f,10.0f,10.5f,11.0f,12.0f,12.6f,13.0f,13.5f,14.0f};

/* ---- app_state_t ---- */
typedef struct {
    float  target_speed;          // m/s, written by comm task
    float  filtered_speed;        // m/s, written by control task
    float  kp, ki, kd;
    float  odom_pos[2];           // m, x/y
    float  odom_yaw;              // rad
    float  quat[4];               // w x y z
    float  accel[3];              // m/s²
    float  gyro[3];               // rad/s
    float  voltage;               // V
    float  temp;                  // °C
    int    steering_pulse;        // µs
    int    rc_ch[10];
    bool   remote_active;
    bool   failsafe;
    bool   speed_full_mode;
    bool   serial_control_active;
    unsigned long last_serial_cmd_ms;
} app_state_t;

static app_state_t g_state;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

/* ---- Hardware handles ---- */
static pcnt_unit_handle_t s_pcnt;
static madgwick_t g_ahrs;
static pid_ctrl_t g_pid;
static i2c_master_dev_handle_t g_imu_dev;
static adc_oneshot_unit_handle_t s_adc;

/* ---- Serial TX queue ---- */
typedef struct { char line[SERIAL_TX_LINE_MAX]; } serial_tx_msg_t;
static QueueHandle_t s_serial_tx_queue = NULL;

static void serial_tx_task_fn(void *arg)
{
    serial_tx_msg_t msg;
    while (1) {
        if (xQueueReceive(s_serial_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            fputs(msg.line, stdout);
            fflush(stdout);
        }
    }
}

static void serial_tx_init(void)
{
    s_serial_tx_queue = xQueueCreate(SERIAL_TX_QUEUE_LEN, sizeof(serial_tx_msg_t));
    xTaskCreatePinnedToCore(serial_tx_task_fn, "serial_tx", 4096, NULL, 1, NULL, 0);
}

static void serial_tx_printf(const char *fmt, ...)
{
    if (!s_serial_tx_queue) return;
    serial_tx_msg_t msg;
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg.line, SERIAL_TX_LINE_MAX, fmt, args);
    va_end(args);
    xQueueSend(s_serial_tx_queue, &msg, 0);
}

/* ---- ADC voltage lookup ---- */
static float adc_to_voltage(int raw)
{
    if (raw <= ADC_PTS[0]) return VOLT_PTS[0];
    if (raw >= ADC_PTS[NUM_ADC_POINTS - 1]) return VOLT_PTS[NUM_ADC_POINTS - 1];
    for (int i = 0; i < NUM_ADC_POINTS - 1; i++) {
        if (raw >= ADC_PTS[i] && raw < ADC_PTS[i + 1]) {
            float r = (float)(raw - ADC_PTS[i]) / (ADC_PTS[i + 1] - ADC_PTS[i]);
            return VOLT_PTS[i] + r * (VOLT_PTS[i + 1] - VOLT_PTS[i]);
        }
    }
    return 0.0f;
}

/* ---- Utility ---- */
static inline int iclamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int map_range(int x, int in_min, int in_max, int out_min, int out_max)
{
    return (int)((float)(x - in_min) * (out_max - out_min) /
                 (in_max - in_min) + out_min);
}

static inline int apply_deadzone(int v, int center, int dz)
{
    return (abs(v - center) < dz) ? center : v;
}

/* ---- PWM helpers ---- */
static void set_throttle(int us)
{
    us = iclamp(us, THROTTLE_MIN, THROTTLE_MAX);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, DUTY_FROM_PULSE(us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void set_steering(int us)
{
    us = iclamp(us, STEERING_MIN, STEERING_MAX);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, DUTY_FROM_PULSE(us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* ---- Buzzer (passive PWM on LEDC_CHANNEL_2) ---- */
void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz == 0) {
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        return;
    }
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, freq_hz);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 2048);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
}

/* ---- LED (single GPIO on LED_PIN) ---- */
/* This board uses a single LED on GPIO45; there is no RGB WS2812B driver.   */
/* led_set_color maps: any non-zero channel → LED on, all zero → LED off.    */
/* The heater milestone colours (green/red) are communicated via buzzer tone  */
/* and printf; the LED simply stays on whenever heater is active.             */
void led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    int level = (r || g || b) ? 1 : 0;
    gpio_set_level(LED_PIN, level);
    printf("INFO: led color r=%u g=%u b=%u\n", r, g, b);
}

static void led_set_blue(void)
{
    gpio_set_level(LED_PIN, 1);
}

/* ---- Init helpers ---- */
static void init_i2c_imu(void)
{
    i2c_master_bus_config_t bc = {
        .i2c_port              = I2C_NUM_0,
        .sda_io_num            = I2C_SDA,
        .scl_io_num            = I2C_SCL,
        .clk_source            = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt     = 7,
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
    /* accum_count=1: no overflow callback needed for PPR=1024 */
    pcnt_unit_config_t uc = { .low_limit = -32768, .high_limit = 32767 };
    uc.flags.accum_count = 1;
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
    pcnt_unit_enable(s_pcnt);
    pcnt_unit_clear_count(s_pcnt);
    pcnt_unit_start(s_pcnt);
}

static void init_pwm(void)
{
    /* Timer 0: throttle + steering at 50Hz */
    ledc_timer_config_t t0 = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_RES,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t0);

    /* Timer 1: buzzer (variable freq) */
    ledc_timer_config_t t1 = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num       = LEDC_TIMER_1,
        .freq_hz         = 1000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t1);

    ledc_channel_config_t ch_thr = {
        .gpio_num   = THROTTLE_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = DUTY_FROM_PULSE(THROTTLE_NEUTRAL),
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_thr);

    ledc_channel_config_t ch_str = {
        .gpio_num   = STEERING_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_1,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = DUTY_FROM_PULSE(STEERING_CENTER),
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_str);

    ledc_channel_config_t ch_buz = {
        .gpio_num   = BUZZER_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_2,
        .timer_sel  = LEDC_TIMER_1,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_buz);
}

static void init_sbus(void)
{
    uart_config_t cfg = {
        .baud_rate  = 100000,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_EVEN,
        .stop_bits  = UART_STOP_BITS_2,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &cfg);
    uart_set_pin(UART_NUM_0, SBUS_TX_PIN, SBUS_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_line_inverse(UART_NUM_0,
                          UART_SIGNAL_RXD_INV | UART_SIGNAL_TXD_INV);
}

static void init_adc(void)
{
    adc_oneshot_unit_init_cfg_t adc_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&adc_cfg, &s_adc);

    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(s_adc, ADC_CHANNEL_3, &ch_cfg);
}

/* ---- SBUS parse ---- */
/* Returns true on valid frame. lost_frame = bit2, failsafe = bit3 of flags byte */
static bool sbus_parse(const uint8_t *buf, uint16_t *ch, bool *lost_frame, bool *failsafe)
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
    ch[8]  = ((buf[11] >> 8 | buf[12] << 0) & 0x7FF);
    ch[9]  = ((buf[12] >> 3 | buf[13] << 5) & 0x7FF);
    *lost_frame = (buf[23] & (1 << 2)) != 0;
    *failsafe   = (buf[23] & (1 << 3)) != 0;
    return true;
}

/* ---- task_imu: Core 1, P5, 1ms ---- */
static void task_imu(void *arg)
{
    TickType_t last      = xTaskGetTickCount();
    TickType_t last_odom = xTaskGetTickCount();

    while (1) {
        qmi8658_data_t d;
        if (qmi8658_read(&d)) {
            madgwick_update_imu(&g_ahrs,
                                d.gyroX, d.gyroY, d.gyroZ,
                                d.accelX, d.accelY, d.accelZ);

            float qw = g_ahrs.q0, qx = g_ahrs.q1;
            float qy = g_ahrs.q2, qz = g_ahrs.q3;

            TickType_t now_t = xTaskGetTickCount();
            float dt = (float)(now_t - last_odom) * portTICK_PERIOD_MS / 1000.0f;
            last_odom = now_t;

            portENTER_CRITICAL(&g_mux);

            g_state.quat[0] = qw;
            g_state.quat[1] = qx;
            g_state.quat[2] = qy;
            g_state.quat[3] = qz;

            g_state.accel[0] = d.accelX * 9.80665f;
            g_state.accel[1] = d.accelY * 9.80665f;
            g_state.accel[2] = d.accelZ * 9.80665f;

            g_state.gyro[0] = d.gyroX * 0.0174533f;
            g_state.gyro[1] = d.gyroY * 0.0174533f;
            g_state.gyro[2] = d.gyroZ * 0.0174533f;

            g_state.temp = d.temp;

            /* Odometry: integrate speed × dt using yaw from quaternion */
            if (dt > 0.001f && dt < 0.1f) {
                float siny = 2.0f * (qw * qz + qx * qy);
                float cosy = 1.0f - 2.0f * (qy * qy + qz * qz);
                g_state.odom_yaw = atan2f(siny, cosy);

                float spd = g_state.filtered_speed;
                g_state.odom_pos[0] += spd * dt * cosf(g_state.odom_yaw);
                g_state.odom_pos[1] += spd * dt * sinf(g_state.odom_yaw);
            }

            portEXIT_CRITICAL(&g_mux);

            imu_heater_update_temp(d.temp);
        }

        vTaskDelayUntil(&last, pdMS_TO_TICKS(1));
    }
}

/* ---- task_control: Core 1, P4, 20ms ---- */
static void task_control(void *arg)
{
    TickType_t last       = xTaskGetTickCount();
    TickType_t last_spd_t = xTaskGetTickCount();
    TickType_t last_ctrl_t = xTaskGetTickCount();
    int32_t    last_count = 0;
    unsigned long last_volt_ms = 0;

    while (1) {
        TickType_t now_t = xTaskGetTickCount();

        /* Speed calculation every SPEED_CALC_INTERVAL_MS */
        float dt_ms = (float)(now_t - last_spd_t) * portTICK_PERIOD_MS;
        if (dt_ms >= (float)SPEED_CALC_INTERVAL_MS) {
            int raw = 0;
            pcnt_unit_get_count(s_pcnt, &raw);
            int32_t cnt   = (int32_t)raw;
            int32_t delta = cnt - last_count;
            last_count = cnt;

            float ratio   = (float)ENCODER_PPR * GEAR_RATIO;
            float rps     = (float)delta / ratio / (dt_ms / 1000.0f);
            float raw_spd = rps * WHEEL_CIRCUMFERENCE;

            portENTER_CRITICAL(&g_mux);
            g_state.filtered_speed =
                SPEED_LPF_ALPHA * raw_spd +
                (1.0f - SPEED_LPF_ALPHA) * g_state.filtered_speed;
            portEXIT_CRITICAL(&g_mux);

            last_spd_t = now_t;
        }

        /* PID control */
        float ctrl_dt = (float)(now_t - last_ctrl_t) * portTICK_PERIOD_MS / 1000.0f;
        if (ctrl_dt <= 0.0f) ctrl_dt = CONTROL_INTERVAL_MS / 1000.0f;
        last_ctrl_t = now_t;

        float target, spd;
        int   steer;
        portENTER_CRITICAL(&g_mux);
        target = g_state.target_speed;
        spd    = g_state.filtered_speed;
        steer  = g_state.steering_pulse;
        portEXIT_CRITICAL(&g_mux);

        int throttle;
        if (fabsf(target) < SPEED_STOP_THRESHOLD) {
            throttle = THROTTLE_NEUTRAL;
            portENTER_CRITICAL(&g_mux);
            pid_reset(&g_pid);
            portEXIT_CRITICAL(&g_mux);
        } else {
            portENTER_CRITICAL(&g_mux);
            throttle = pid_update(&g_pid, target, spd, THROTTLE_NEUTRAL, ctrl_dt);
            portEXIT_CRITICAL(&g_mux);
        }

        set_throttle(throttle);
        set_steering(steer);

        /* ADC voltage every 500ms */
        unsigned long now_ms = (unsigned long)(esp_timer_get_time() / 1000);
        if (now_ms - last_volt_ms >= 500) {
            int raw_adc = 0;
            if (adc_oneshot_read(s_adc, ADC_CHANNEL_3, &raw_adc) == ESP_OK) {
                float voltage = adc_to_voltage(raw_adc);
                portENTER_CRITICAL(&g_mux);
                g_state.voltage = voltage;
                portEXIT_CRITICAL(&g_mux);
                if (voltage < LOW_VOLTAGE_THRESHOLD) {
                    serial_tx_printf("WARN: low battery %.2fV\n", voltage);
                }
            }
            last_volt_ms = now_ms;
        }

        vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROL_INTERVAL_MS));
    }
}

/* ---- Serial command processing ---- */
static void process_serial_cmd(const char *line)
{
    float v, s;
    float val;

    portENTER_CRITICAL(&g_mux);
    bool remote = g_state.remote_active;
    portEXIT_CRITICAL(&g_mux);

    if (remote) return;  /* remote has priority */

    if (sscanf(line, "v %f %f", &v, &s) == 2) {
        /* v <vx_m_s> <steer_deg>: steer_deg maps to pulse */
        int pulse = STEER_DEG_TO_PULSE(s);
        pulse = iclamp(pulse, STEERING_MIN, STEERING_MAX);
        portENTER_CRITICAL(&g_mux);
        g_state.target_speed   = v;
        g_state.steering_pulse = pulse;
        g_state.last_serial_cmd_ms =
            (unsigned long)(esp_timer_get_time() / 1000);
        portEXIT_CRITICAL(&g_mux);
        serial_tx_printf("OK v=%.2f steer=%d\n", v, pulse);

    } else if (strcmp(line, "i") == 0) {
        float quat[4], accel[3], gyro[3];
        portENTER_CRITICAL(&g_mux);
        for (int i = 0; i < 4; i++) quat[i]  = g_state.quat[i];
        for (int i = 0; i < 3; i++) accel[i] = g_state.accel[i];
        for (int i = 0; i < 3; i++) gyro[i]  = g_state.gyro[i];
        portEXIT_CRITICAL(&g_mux);
        float roll  = atan2f(2.0f*(quat[0]*quat[1]+quat[2]*quat[3]),
                             1.0f-2.0f*(quat[1]*quat[1]+quat[2]*quat[2])) * 57.296f;
        float pitch = asinf(2.0f*(quat[0]*quat[2]-quat[3]*quat[1])) * 57.296f;
        float yaw   = atan2f(2.0f*(quat[0]*quat[3]+quat[1]*quat[2]),
                             1.0f-2.0f*(quat[2]*quat[2]+quat[3]*quat[3])) * 57.296f;
        serial_tx_printf("IMU roll=%.1f pitch=%.1f yaw=%.1f "
                         "ax=%.3f ay=%.3f az=%.3f "
                         "gx=%.3f gy=%.3f gz=%.3f\n",
                         roll, pitch, yaw,
                         accel[0], accel[1], accel[2],
                         gyro[0], gyro[1], gyro[2]);

    } else if (strcmp(line, "o") == 0) {
        float pos[2], yaw;
        portENTER_CRITICAL(&g_mux);
        pos[0] = g_state.odom_pos[0];
        pos[1] = g_state.odom_pos[1];
        yaw    = g_state.odom_yaw;
        portEXIT_CRITICAL(&g_mux);
        serial_tx_printf("ODOM x=%.4f y=%.4f yaw=%.4f\n", pos[0], pos[1], yaw);

    } else if (strcmp(line, "status") == 0) {
        float spd, volt, temp;
        int   steer;
        bool  rem, fs, full;
        portENTER_CRITICAL(&g_mux);
        spd   = g_state.filtered_speed;
        volt  = g_state.voltage;
        temp  = g_state.temp;
        steer = g_state.steering_pulse;
        rem   = g_state.remote_active;
        fs    = g_state.failsafe;
        full  = g_state.speed_full_mode;
        portEXIT_CRITICAL(&g_mux);
        serial_tx_printf("STATUS spd=%.2f steer=%d volt=%.2f temp=%.1f "
                         "remote=%d failsafe=%d full_speed=%d\n",
                         spd, steer, volt, temp, rem, fs, full);

    } else if (sscanf(line, "kp %f", &val) == 1) {
        portENTER_CRITICAL(&g_mux);
        g_state.kp = val;
        g_pid.kp   = val;
        portEXIT_CRITICAL(&g_mux);
        serial_tx_printf("OK kp=%.3f\n", val);

    } else if (sscanf(line, "ki %f", &val) == 1) {
        portENTER_CRITICAL(&g_mux);
        g_state.ki = val;
        g_pid.ki   = val;
        portEXIT_CRITICAL(&g_mux);
        serial_tx_printf("OK ki=%.3f\n", val);

    } else if (sscanf(line, "kd %f", &val) == 1) {
        portENTER_CRITICAL(&g_mux);
        g_state.kd = val;
        g_pid.kd   = val;
        portEXIT_CRITICAL(&g_mux);
        serial_tx_printf("OK kd=%.3f\n", val);

    } else if (strcmp(line, "kr") == 0) {
        portENTER_CRITICAL(&g_mux);
        pid_reset(&g_pid);
        portEXIT_CRITICAL(&g_mux);
        serial_tx_printf("OK pid reset\n");

    } else if (strcmp(line, "reset") == 0) {
        serial_tx_printf("INFO: rebooting...\n");
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();

    } else if (strcmp(line, "help") == 0) {
        serial_tx_printf(
            "Commands:\n"
            "  v <vx> <steer_deg>  set speed(m/s) and steering angle(deg)\n"
            "  i                   print IMU\n"
            "  o                   print odometry\n"
            "  status              full status\n"
            "  kp/ki/kd <val>      set PID gains\n"
            "  kr                  reset PID\n"
            "  reset               reboot\n"
            "  help                this list\n"
        );

    } else {
        serial_tx_printf("ERR: unknown command '%s'\n", line);
    }
}

/* ---- task_comm: Core 0, P3, 1ms ---- */
static void task_comm(void *arg)
{
    uint8_t  sbus_buf[SBUS_FRAME];
    uint16_t rc_ch[16];
    bool     lost_frame, failsafe_flag;
    char     line_buf[128];
    int      line_len = 0;
    bool     rc_initialized = false;
    unsigned long last_rc_ok_ms = 0;

    while (1) {
        unsigned long now_ms = (unsigned long)(esp_timer_get_time() / 1000);

        /* --- SBUS receive (non-blocking) --- */
        uint8_t byte;
        if (uart_read_bytes(UART_NUM_0, &byte, 1, 0) == 1 && byte == 0x0F) {
            sbus_buf[0] = byte;
            if (uart_read_bytes(UART_NUM_0, sbus_buf + 1, SBUS_FRAME - 1,
                                pdMS_TO_TICKS(5)) == SBUS_FRAME - 1) {
                if (sbus_parse(sbus_buf, rc_ch, &lost_frame, &failsafe_flag)) {
                    last_rc_ok_ms  = now_ms;
                    rc_initialized = true;

                    if (failsafe_flag || lost_frame) {
                        if (!g_state.failsafe) {
                            portENTER_CRITICAL(&g_mux);
                            g_state.failsafe             = true;
                            g_state.remote_active        = false;
                            g_state.serial_control_active = true;
                            g_state.target_speed         = 0.0f;
                            g_state.steering_pulse       = STEERING_CENTER;
                            portEXIT_CRITICAL(&g_mux);
                            serial_tx_printf("WARN: RC failsafe\n");
                        }
                    } else {
                        portENTER_CRITICAL(&g_mux);
                        g_state.failsafe = false;
                        for (int i = 0; i < 10; i++) {
                            g_state.rc_ch[i] = rc_ch[i];
                        }
                        portEXIT_CRITICAL(&g_mux);

                        if (rc_ch[CONTROL_MODE_CH] < 1500) {
                            /* Remote control mode */
                            if (!g_state.remote_active) {
                                portENTER_CRITICAL(&g_mux);
                                g_state.remote_active        = true;
                                g_state.serial_control_active = false;
                                portEXIT_CRITICAL(&g_mux);
                                serial_tx_printf("INFO: remote control\n");
                            }
                        } else {
                            /* Serial control mode */
                            if (g_state.remote_active) {
                                portENTER_CRITICAL(&g_mux);
                                g_state.remote_active        = false;
                                g_state.serial_control_active = true;
                                g_state.target_speed         = 0.0f;
                                g_state.steering_pulse       = STEERING_CENTER;
                                portEXIT_CRITICAL(&g_mux);
                                pid_reset(&g_pid);
                                serial_tx_printf("INFO: serial control\n");
                            }
                        }

                        if (g_state.remote_active) {
                            int steer_raw = map_range(rc_ch[STEERING_CH],
                                                      SBUS_MIN, SBUS_MAX,
                                                      STEERING_MIN, STEERING_MAX);
                            steer_raw = apply_deadzone(steer_raw, STEERING_CENTER,
                                                       RC_STEERING_DEADZONE);
                            if (STEERING_REVERSE) {
                                steer_raw = STEERING_MAX - (steer_raw - STEERING_MIN);
                            }
                            steer_raw = iclamp(steer_raw, STEERING_MIN, STEERING_MAX);

                            int thr_raw = map_range(rc_ch[THROTTLE_CH],
                                                    SBUS_MIN, SBUS_MAX,
                                                    THROTTLE_MIN, THROTTLE_MAX);
                            thr_raw = apply_deadzone(thr_raw, THROTTLE_NEUTRAL,
                                                     RC_THROTTLE_DEADZONE);
                            if (THROTTLE_REVERSE) {
                                thr_raw = THROTTLE_MAX - (thr_raw - THROTTLE_MIN);
                            }

                            float spd = 0.0f;
                            if (thr_raw > THROTTLE_NEUTRAL) {
                                spd = (float)(thr_raw - THROTTLE_NEUTRAL) /
                                      (float)(THROTTLE_MAX - THROTTLE_NEUTRAL) *
                                      MAX_SPEED_FORWARD;
                            } else if (thr_raw < THROTTLE_NEUTRAL) {
                                spd = (float)(thr_raw - THROTTLE_NEUTRAL) /
                                      (float)(THROTTLE_NEUTRAL - THROTTLE_MIN) *
                                      (-MAX_SPEED_REVERSE);
                            }

                            /* Speed mode channel: ch[7] < 1500 → limit to 15% */
                            bool full = (rc_ch[SPEED_MODE_CH] >= 1500);
                            if (!full) spd *= 0.15f;

                            portENTER_CRITICAL(&g_mux);
                            g_state.speed_full_mode  = full;
                            g_state.target_speed     = spd;
                            g_state.steering_pulse   = steer_raw;
                            portEXIT_CRITICAL(&g_mux);
                        }
                    }
                }
            }
        }

        /* RC timeout: no valid frame for 200ms while remote_active → failsafe */
        if (rc_initialized && g_state.remote_active) {
            if (now_ms - last_rc_ok_ms >= RC_TIMEOUT_MS) {
                portENTER_CRITICAL(&g_mux);
                g_state.failsafe             = true;
                g_state.remote_active        = false;
                g_state.serial_control_active = true;
                g_state.target_speed         = 0.0f;
                g_state.steering_pulse       = STEERING_CENTER;
                portEXIT_CRITICAL(&g_mux);
                serial_tx_printf("WARN: RC signal lost\n");
            }
        }

        /* --- USB CDC command (non-blocking byte-by-byte) --- */
        uint8_t ch_byte;
        if (usb_serial_jtag_read_bytes(&ch_byte, 1, 0) == 1) {
            char c = (char)ch_byte;
            if (c == '\n') {
                if (line_len > 0) {
                    /* strip trailing CR/space */
                    while (line_len > 0 &&
                           (line_buf[line_len-1] == '\r' ||
                            line_buf[line_len-1] == ' ')) {
                        line_len--;
                    }
                    if (line_len > 0) {
                        line_buf[line_len] = '\0';
                        process_serial_cmd(line_buf);
                    }
                    line_len = 0;
                }
            } else if (line_len < (int)sizeof(line_buf) - 1) {
                line_buf[line_len++] = c;
            }
        }

        /* Serial control timeout: 500ms without command → stop */
        if (g_state.serial_control_active && !g_state.remote_active) {
            if (g_state.last_serial_cmd_ms > 0 &&
                now_ms - g_state.last_serial_cmd_ms > SERIAL_TIMEOUT_MS) {
                portENTER_CRITICAL(&g_mux);
                g_state.target_speed   = 0.0f;
                g_state.steering_pulse = STEERING_CENTER;
                portEXIT_CRITICAL(&g_mux);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* ---- app_main ---- */
void app_main(void)
{
    /* USB CDC console */
    usb_serial_jtag_driver_config_t usb_cfg = {
        .rx_buffer_size = 512,
        .tx_buffer_size = 512,
    };
    usb_serial_jtag_driver_install(&usb_cfg);

    serial_tx_init();

    /* Battery enable: set GPIO16 LOW (osrcore convention) */
    gpio_config_t io_cfg = {};
    io_cfg.pin_bit_mask = (1ULL << IO_CTRL_BAT);
    io_cfg.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_cfg);
    gpio_set_level(IO_CTRL_BAT, 0);

    /* LED pin as output */
    gpio_config_t led_cfg = {};
    led_cfg.pin_bit_mask = (1ULL << LED_PIN);
    led_cfg.mode = GPIO_MODE_OUTPUT;
    gpio_config(&led_cfg);

    ESP_ERROR_CHECK(nvs_flash_init());

    init_i2c_imu();
    init_encoder();
    init_pwm();
    init_sbus();
    init_adc();

    madgwick_init(&g_ahrs, 0.1f);

    imu_heater_init(56.0f);

    /* Init state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.quat[0]           = 1.0f;
    g_state.steering_pulse    = STEERING_CENTER;
    g_state.serial_control_active = true;
    g_state.failsafe          = true;
    g_state.kp                = PID_KP_DEFAULT;
    g_state.ki                = PID_KI_DEFAULT;
    g_state.kd                = PID_KD_DEFAULT;

    pid_init(&g_pid, PID_KP_DEFAULT, PID_KI_DEFAULT, PID_KD_DEFAULT,
             PID_MAX_INTEGRAL, PID_DEADBAND);

    /* Startup sequence: LED blue → buzzer 4-note scale → print ready */
    led_set_blue();

    static const uint32_t scale[] = {262, 330, 392, 523};
    for (int i = 0; i < 4; i++) {
        buzzer_tone(scale[i], 150);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* ESC arming */
    set_throttle(THROTTLE_NEUTRAL);
    set_steering(STEERING_CENTER);
    vTaskDelay(pdMS_TO_TICKS(2000));

    serial_tx_printf("INFO: osrcore ready\n");
    ESP_LOGI(TAG, "osrcore ready");

    /* Start IMU task first so the heater can receive temperature updates */
    xTaskCreatePinnedToCore(task_imu, "imu", 4096, NULL, 5, NULL, 1);

    /* Wait until heater reaches warm threshold (38°C) before starting control */
    serial_tx_printf("INFO: Waiting for IMU heater warm...\n");
    while (!imu_heater_warm()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    serial_tx_printf("INFO: IMU heater warm — starting control tasks\n");

    xTaskCreatePinnedToCore(task_control, "ctrl",    4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(task_comm,    "comm",    8192, NULL, 3, NULL, 0);

    vTaskDelete(NULL);
}
