/**
 * 示例03：PWM 舵机 / ESC 控制
 *
 * 硬件：
 *   GPIO1 — 油门通道（ESC）
 *   GPIO2 — 转向通道（舵机）
 *
 * 原理：标准 RC PWM 协议，50Hz 周期（20ms），脉宽 1000-2000µs。
 *   1000µs = 最小（反转/左满舵）
 *   1500µs = 中立（停止/居中）
 *   2000µs = 最大（全速/右满舵）
 *
 * LEDC TIMER_0，14-bit 分辨率，80MHz APB 时钟：
 *   period = 80_000_000 / 50 = 1_600_000 ticks
 *   1µs = 80 ticks
 *   duty = pulse_us * 80
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define THROTTLE_PIN  1
#define STEERING_PIN  2

#define PWM_FREQ_HZ   50
#define PWM_RES       LEDC_TIMER_14_BIT  /* 16384 steps */
#define PWM_TIMER     LEDC_TIMER_0
#define CH_THROTTLE   LEDC_CHANNEL_0
#define CH_STEERING   LEDC_CHANNEL_1

/* 80 MHz APB clock → 1 tick = 12.5 ns → 1 µs = 80 ticks */
#define US_TO_DUTY(us)  ((us) * 80)

static const char *TAG = "servo";

static void pwm_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_RES,
        .timer_num       = PWM_TIMER,
        .freq_hz         = PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch_throttle = {
        .gpio_num   = THROTTLE_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = CH_THROTTLE,
        .timer_sel  = PWM_TIMER,
        .duty       = US_TO_DUTY(1500),
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_throttle);

    ledc_channel_config_t ch_steering = {
        .gpio_num   = STEERING_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = CH_STEERING,
        .timer_sel  = PWM_TIMER,
        .duty       = US_TO_DUTY(1500),
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_steering);
}

static void set_pulse(ledc_channel_t ch, uint32_t pulse_us)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, US_TO_DUTY(pulse_us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

void app_main(void)
{
    pwm_init();
    ESP_LOGI(TAG, "PWM ready — throttle GPIO%d, steering GPIO%d", THROTTLE_PIN, STEERING_PIN);

    /* ESC arming: hold neutral for 2 s */
    set_pulse(CH_THROTTLE, 1500);
    set_pulse(CH_STEERING, 1500);
    ESP_LOGI(TAG, "ESC arming (2s neutral)...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Sweep steering left → center → right, throttle stays neutral */
    const uint32_t steering_pts[] = { 1500, 1200, 1500, 1800, 1500 };
    const int n = sizeof(steering_pts) / sizeof(steering_pts[0]);

    while (1) {
        for (int i = 0; i < n; i++) {
            ESP_LOGI(TAG, "steering %d us", (int)steering_pts[i]);
            set_pulse(CH_STEERING, steering_pts[i]);
            vTaskDelay(pdMS_TO_TICKS(800));
        }
    }
}
