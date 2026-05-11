/**
 * 示例06：正交编码器速度测量
 *
 * 硬件：GPIO3(EA)，GPIO9(EB)，512 PPR，减速比 10.55，轮径 42.5mm
 * 原理：ESP-IDF PCNT（脉冲计数）新 API，单边沿模式（EA 上升沿计数，EB 控制方向）。
 *       每 20ms 读取计数差，换算为线速度（m/s）。
 *
 * 速度公式：
 *   pulses_per_rev = PPR * GEAR_RATIO = 512 * 10.55 = 5401.6
 *   distance_per_pulse = circumference / pulses_per_rev
 *   speed = (delta_pulses * distance_per_pulse) / dt
 */

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"

#define EA              3
#define EB              9
#define ENCODER_PPR     512
#define GEAR_RATIO      10.55f
#define WHEEL_RADIUS    0.0425f
#define CALC_INTERVAL   20   /* ms */

#define PULSES_PER_REV  (ENCODER_PPR * GEAR_RATIO)
#define CIRCUMFERENCE   (2.0f * 3.14159265f * WHEEL_RADIUS)
#define DIST_PER_PULSE  (CIRCUMFERENCE / PULSES_PER_REV)

static const char *TAG = "encoder";

static pcnt_unit_handle_t s_unit;
static int32_t s_accum = 0;

/* PCNT overflow callback: accumulate high-order counts */
static bool IRAM_ATTR pcnt_overflow_cb(pcnt_unit_handle_t unit,
                                        const pcnt_watch_event_data_t *edata,
                                        void *user_ctx)
{
    int32_t *accum = (int32_t *)user_ctx;
    *accum += edata->watch_point_val;
    return false;
}

static void encoder_init(void)
{
    pcnt_unit_config_t unit_cfg = {
        .low_limit  = -32768,
        .high_limit =  32767,
    };
    pcnt_new_unit(&unit_cfg, &s_unit);

    /* EA: count on rising edge; EB: controls direction */
    pcnt_chan_config_t chan_a = {
        .edge_gpio_num  = EA,
        .level_gpio_num = EB,
    };
    pcnt_channel_handle_t ch_a;
    pcnt_new_channel(s_unit, &chan_a, &ch_a);
    pcnt_channel_set_edge_action(ch_a, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                       PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_channel_set_level_action(ch_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    /* Watch points for overflow accumulation */
    pcnt_unit_add_watch_point(s_unit,  32767);
    pcnt_unit_add_watch_point(s_unit, -32768);

    pcnt_event_callbacks_t cbs = { .on_reach = pcnt_overflow_cb };
    pcnt_unit_register_event_callbacks(s_unit, &cbs, &s_accum);

    pcnt_unit_enable(s_unit);
    pcnt_unit_clear_count(s_unit);
    pcnt_unit_start(s_unit);
}

static int32_t encoder_get_count(void)
{
    int raw = 0;
    pcnt_unit_get_count(s_unit, &raw);
    return s_accum + raw;
}

void app_main(void)
{
    encoder_init();
    ESP_LOGI(TAG, "Encoder ready EA=GPIO%d EB=GPIO%d", EA, EB);

    int32_t last = encoder_get_count();
    const float dt = CALC_INTERVAL / 1000.0f;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(CALC_INTERVAL));

        int32_t now   = encoder_get_count();
        int32_t delta = now - last;
        last = now;

        float speed = delta * DIST_PER_PULSE / dt;
        printf("count=%ld  delta=%ld  speed=%.3f m/s\n", now, delta, speed);
    }
}
