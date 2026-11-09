/**
 * Lesson 02: adjustable passive buzzer.
 * Hardware: GPIO42. LEDC TIMER_1 generates a 50% duty-cycle tone.
 * Changing frequency changes pitch; zero duty silences the buzzer.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "osrcore_fw_update.h"
#include "osrcore_params.h"
#include "osrcore_lesson_config.h"

#define BUZZER_PIN      42
#define BUZZER_TIMER    LEDC_TIMER_1
#define BUZZER_CHANNEL  LEDC_CHANNEL_2
#define BUZZER_RES      LEDC_TIMER_10_BIT  /* 1024 steps */

static const char *TAG = "buzzer";

static esp_timer_handle_t s_buzzer_timer = NULL;

static void buzzer_set_off(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL);
}

static void buzzer_timer_cb(void *arg)
{
    buzzer_set_off();
}

static void buzzer_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BUZZER_RES,
        .timer_num       = BUZZER_TIMER,
        .freq_hz         = 1000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num   = BUZZER_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = BUZZER_CHANNEL,
        .timer_sel  = BUZZER_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch);
}

/* Play a tone at freq Hz for duration_ms milliseconds (non-blocking) */
static void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz == 0 || duration_ms == 0) { buzzer_set_off(); return; }
    if (s_buzzer_timer == NULL) {
        esp_timer_create_args_t args = {};
        args.callback = buzzer_timer_cb;
        args.name = "buzzer";
        esp_timer_create(&args, &s_buzzer_timer);
    }
    esp_timer_stop(s_buzzer_timer);
    ledc_set_freq(LEDC_LOW_SPEED_MODE, BUZZER_TIMER, freq_hz);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, 512); /* 50% duty */
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL);
    esp_timer_start_once(s_buzzer_timer, (uint64_t)duration_ms * 1000);
}

/* One lesson, one configurable tone followed by 100 ms silence. */
void app_main(void)
{
    buzzer_init();
    static const osrcore_param_t parameters[] = {
        {"frequency", 220, 1760, 10, OSR_BUZZER_FREQUENCY},
        {"duration", 100, 1000, 50, OSR_BUZZER_DURATION},
    };
    osrcore_params_init(parameters, sizeof(parameters)/sizeof(parameters[0]));
    osrcore_fw_update_start();
    while (1) {
        int frequency = (int)osrcore_param_get("frequency", OSR_BUZZER_FREQUENCY);
        int duration = (int)osrcore_param_get("duration", OSR_BUZZER_DURATION);
        ESP_LOGI(TAG, "tone=%d Hz duration=%d ms", frequency, duration);
        buzzer_tone(frequency, duration);
        vTaskDelay(pdMS_TO_TICKS(duration + 100));
    }
}
