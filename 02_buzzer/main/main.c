/**
 * 示例02：蜂鸣器播放旋律
 *
 * 硬件：GPIO42，无源蜂鸣器
 * 原理：LEDC TIMER_1 动态调整频率，50% 占空比驱动蜂鸣器发声。
 *       改变频率即改变音调，静音时将占空比置 0。
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define BUZZER_PIN      42
#define BUZZER_TIMER    LEDC_TIMER_1
#define BUZZER_CHANNEL  LEDC_CHANNEL_2
#define BUZZER_RES      LEDC_TIMER_10_BIT  /* 1024 steps */

static const char *TAG = "buzzer";

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

/* Play a tone at freq Hz for duration_ms milliseconds */
static void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    ledc_set_freq(LEDC_LOW_SPEED_MODE, BUZZER_TIMER, freq_hz);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, 512); /* 50% duty */
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL);
}

/* ---- Note frequencies (Hz) ---- */
#define C4  262
#define D4  294
#define E4  330
#define F4  349
#define G4  392
#define A4  440
#define B4  494
#define C5  523

void app_main(void)
{
    buzzer_init();
    ESP_LOGI(TAG, "Buzzer ready on GPIO%d", BUZZER_PIN);

    /* C major scale up and down */
    const uint32_t scale[] = { C4, D4, E4, F4, G4, A4, B4, C5,
                                B4, A4, G4, F4, E4, D4, C4 };
    const int n = sizeof(scale) / sizeof(scale[0]);

    while (1) {
        for (int i = 0; i < n; i++) {
            ESP_LOGI(TAG, "note %d Hz", (int)scale[i]);
            buzzer_tone(scale[i], 150);
            vTaskDelay(pdMS_TO_TICKS(30)); /* gap between notes */
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
