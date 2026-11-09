/**
 * Lesson 03: servo and ESC PWM.
 * GPIO1 is the ESC throttle channel; GPIO2 is the steering servo channel.
 * RC PWM uses a 50 Hz period (20 ms) and a 1000-2000 us pulse.
 * 1500 us is the nominal neutral position; actual travel depends on the device.
 * LEDC TIMER_0 uses 14-bit resolution and an 80 MHz APB clock.
 * Only the steering pulse is adjustable in this lesson; throttle stays neutral.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "osrcore_fw_update.h"
#include "osrcore_params.h"
#include "osrcore_lesson_config.h"

#define THROTTLE_PIN  1
#define STEERING_PIN  2

#define PWM_FREQ_HZ   50
#define PWM_RES       LEDC_TIMER_14_BIT  /* 16384 steps */
#define PWM_TIMER     LEDC_TIMER_0
#define CH_THROTTLE   LEDC_CHANNEL_0
#define CH_STEERING   LEDC_CHANNEL_1

static const char *TAG = "servo";

/* Map a pulse width in µs to a 14-bit LEDC duty value.
 * Clamps to the valid RC range [1000, 2000] µs.
 * Formula: pulse_us / 20000.0 * (2^14 - 1) */
static int duty_from_pulse(int pulse_us)
{
    if (pulse_us < 1000) pulse_us = 1000;
    if (pulse_us > 2000) pulse_us = 2000;
    return (int)((float)pulse_us / 20000.0f * ((1 << 14) - 1));
}

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
        .duty       = duty_from_pulse(1500),
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_throttle);

    ledc_channel_config_t ch_steering = {
        .gpio_num   = STEERING_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = CH_STEERING,
        .timer_sel  = PWM_TIMER,
        .duty       = duty_from_pulse(1500),
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_steering);
}

static void set_pulse(ledc_channel_t ch, uint32_t pulse_us)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty_from_pulse((int)pulse_us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

void app_main(void)
{
    pwm_init();
    static const osrcore_param_t parameters[] = {
        {"pulse", 1000, 2000, 10, OSR_SERVO_PULSE},
    };
    osrcore_params_init(parameters, sizeof(parameters)/sizeof(parameters[0]));
    osrcore_fw_update_start();
    ESP_LOGI(TAG, "PWM ready — throttle GPIO%d, steering GPIO%d", THROTTLE_PIN, STEERING_PIN);

    /* ESC arming: hold neutral for 2 s */
    set_pulse(CH_THROTTLE, 1500);
    set_pulse(CH_STEERING, 1500);
    ESP_LOGI(TAG, "ESC arming (2s neutral)...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Only steering moves. ESC stays neutral. */
    while (1) {
        int pulse = (int)osrcore_param_get("pulse", OSR_SERVO_PULSE);
        set_pulse(CH_STEERING, pulse);
        ESP_LOGI(TAG, "steering=%d us, throttle=1500 us", pulse);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
