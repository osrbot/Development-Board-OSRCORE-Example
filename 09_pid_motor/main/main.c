/**
 * 示例09：PID 电机速度闭环控制
 *
 * 硬件：
 *   编码器  GPIO3(EA) / GPIO9(EB)，512 PPR，减速比 10.55，轮径 42.5mm
 *   ESC     GPIO1，LEDC 50Hz 14-bit，脉宽 1000-2000µs
 *
 * 控制逻辑（20ms 周期）：
 *   1. 读编码器计数差 → 换算线速度（m/s），一阶低通滤波
 *   2. PID 计算输出（单位：µs 偏移量）
 *   3. 输出 = THROTTLE_NEUTRAL(1500) + pid_out，钳位到 [1000, 2000]
 *
 * 目标速度通过 USB CDC 串口命令设置：
 *   v <speed_m_s>   — 设置目标速度（-6.0 ~ 6.0 m/s）
 *   stop            — 停止
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/pulse_cnt.h"
#include "driver/ledc.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_log.h"
#include "pid.h"

/* ---- Hardware config ---- */
#define EA              3
#define EB              9
#define ENCODER_PPR     512
#define GEAR_RATIO      10.55f
#define WHEEL_RADIUS    0.0425f
#define THROTTLE_PIN    1
#define PWM_FREQ_HZ     50
#define PWM_RES         LEDC_TIMER_14_BIT
#define PWM_TIMER       LEDC_TIMER_0
#define CH_THROTTLE     LEDC_CHANNEL_0

/* ---- Derived constants ---- */
#define PULSES_PER_REV  (ENCODER_PPR * GEAR_RATIO)
#define CIRCUMFERENCE   (2.0f * 3.14159265f * WHEEL_RADIUS)
#define DIST_PER_PULSE  (CIRCUMFERENCE / PULSES_PER_REV)
#define CONTROL_DT      0.020f   /* 20ms */
#define LPF_ALPHA       0.15f
#define THROTTLE_NEUTRAL 1500
#define THROTTLE_MIN     1000
#define THROTTLE_MAX     2000
#define US_TO_DUTY(us)  ((us) * 80)

static const char *TAG = "pid_motor";

static pcnt_unit_handle_t s_pcnt;
static int32_t s_accum = 0;
static float   s_target_speed = 0.0f;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* ---- Encoder ---- */
static bool IRAM_ATTR pcnt_overflow_cb(pcnt_unit_handle_t u,
                                        const pcnt_watch_event_data_t *e,
                                        void *ctx)
{
    int32_t *acc = (int32_t *)ctx;
    *acc += e->watch_point_val;
    return false;
}

static void encoder_init(void)
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
    pcnt_unit_register_event_callbacks(s_pcnt, &cbs, &s_accum);
    pcnt_unit_enable(s_pcnt);
    pcnt_unit_clear_count(s_pcnt);
    pcnt_unit_start(s_pcnt);
}

static int32_t encoder_count(void)
{
    int raw = 0;
    pcnt_unit_get_count(s_pcnt, &raw);
    return s_accum + raw;
}

/* ---- ESC PWM ---- */
static void esc_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = PWM_RES,
        .timer_num = PWM_TIMER, .freq_hz = PWM_FREQ_HZ, .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);

    ledc_channel_config_t c = {
        .gpio_num = THROTTLE_PIN, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = CH_THROTTLE, .timer_sel = PWM_TIMER,
        .duty = US_TO_DUTY(THROTTLE_NEUTRAL), .hpoint = 0,
    };
    ledc_channel_config(&c);
}

static void esc_set(uint32_t pulse_us)
{
    if (pulse_us < THROTTLE_MIN) pulse_us = THROTTLE_MIN;
    if (pulse_us > THROTTLE_MAX) pulse_us = THROTTLE_MAX;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, CH_THROTTLE, US_TO_DUTY(pulse_us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, CH_THROTTLE);
}

/* ---- Control task ---- */
static void task_control(void *arg)
{
    pid_t pid;
    pid_init(&pid, 447.0f, 4.7f, 47.0f, 1000.0f, 0.05f);

    int32_t last_count = encoder_count();
    float filtered_speed = 0.0f;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));

        int32_t now   = encoder_count();
        int32_t delta = now - last_count;
        last_count = now;

        float raw_speed = (delta * DIST_PER_PULSE) / CONTROL_DT;
        filtered_speed  = LPF_ALPHA * raw_speed + (1.0f - LPF_ALPHA) * filtered_speed;

        portENTER_CRITICAL(&s_mux);
        float target = s_target_speed;
        portEXIT_CRITICAL(&s_mux);

        float out = pid_calc(&pid, target, filtered_speed, CONTROL_DT);
        int32_t pulse = THROTTLE_NEUTRAL + (int32_t)out;
        esc_set((uint32_t)pulse);

        printf("target=%.2f  actual=%.2f  pulse=%ld\n", target, filtered_speed, pulse);
    }
}

/* ---- Command task ---- */
static void task_cmd(void *arg)
{
    char line[32];
    while (1) {
        if (!fgets(line, sizeof(line), stdin)) continue;
        line[strcspn(line, "\r\n")] = '\0';

        float v;
        if (sscanf(line, "v %f", &v) == 1) {
            if (v >  6.0f) v =  6.0f;
            if (v < -6.0f) v = -6.0f;
            portENTER_CRITICAL(&s_mux);
            s_target_speed = v;
            portEXIT_CRITICAL(&s_mux);
            printf("target speed set to %.2f m/s\n", v);
        } else if (strcmp(line, "stop") == 0) {
            portENTER_CRITICAL(&s_mux);
            s_target_speed = 0.0f;
            portEXIT_CRITICAL(&s_mux);
            printf("stopped\n");
        }
    }
}

void app_main(void)
{
    usb_serial_jtag_driver_config_t usb_cfg = { .rx_buffer_size = 256, .tx_buffer_size = 256 };
    usb_serial_jtag_driver_install(&usb_cfg);
    esp_vfs_usb_serial_jtag_use_driver();

    encoder_init();
    esc_init();

    ESP_LOGI(TAG, "ESC arming (2s neutral)...");
    esc_set(THROTTLE_NEUTRAL);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Ready. Commands: v <speed>, stop");

    xTaskCreatePinnedToCore(task_control, "control", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(task_cmd,     "cmd",     4096, NULL, 3, NULL, 0);
}
