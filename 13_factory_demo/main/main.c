/*
 * OSRBOT ESP32-S3 factory demo.
 *
 * Safe board-level factory test:
 * - WS2812B status LED on GPIO45 cycles colors.
 * - Passive buzzer on GPIO42 plays a short boot beep.
 * - USB serial prints heartbeat and accepts simple text commands.
 *
 * This demo intentionally does not drive ESC or steering PWM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/rmt_tx.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_timer.h"

#define LED_PIN       45
#define BUZZER_PIN    42
#define RMT_CLK_HZ    80000000
#define BUZZER_TIMER  LEDC_TIMER_1
#define BUZZER_CH     LEDC_CHANNEL_2

#define T0H (400 / 12)
#define T0L (850 / 12)
#define T1H (800 / 12)
#define T1L (450 / 12)
#define RESET_TICKS (50000 / 12)

static const char *TAG = "factory";
static rmt_channel_handle_t s_led_channel;
static rmt_encoder_handle_t s_led_encoder;
static esp_timer_handle_t s_buzzer_timer;
static uint8_t s_led_rgb[3] = {0, 32, 0};
static uint32_t s_cmd_count = 0;

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    rmt_symbol_word_t reset_symbol;
    int state;
} ws2812_encoder_t;

static size_t ws2812_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                            const void *data, size_t data_size,
                            rmt_encode_state_t *ret_state) {
    ws2812_encoder_t *ws = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    size_t encoded = 0;

    if (ws->state == 0) {
        encoded += ws->bytes_encoder->encode(ws->bytes_encoder, channel,
                                             data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws->state = 1;
            session_state = RMT_ENCODING_RESET;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = RMT_ENCODING_MEM_FULL;
            return encoded;
        }
    }

    if (ws->state == 1) {
        encoded += ws->copy_encoder->encode(ws->copy_encoder, channel,
                                            &ws->reset_symbol, sizeof(ws->reset_symbol),
                                            &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws->state = 0;
            *ret_state = RMT_ENCODING_COMPLETE;
        }
    }

    return encoded;
}

static esp_err_t ws2812_del(rmt_encoder_t *encoder) {
    ws2812_encoder_t *ws = __containerof(encoder, ws2812_encoder_t, base);
    rmt_del_encoder(ws->bytes_encoder);
    rmt_del_encoder(ws->copy_encoder);
    free(ws);
    return ESP_OK;
}

static esp_err_t ws2812_reset(rmt_encoder_t *encoder) {
    ws2812_encoder_t *ws = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_reset(ws->bytes_encoder);
    rmt_encoder_reset(ws->copy_encoder);
    ws->state = 0;
    return ESP_OK;
}

static void led_encoder_new(rmt_encoder_handle_t *ret) {
    ws2812_encoder_t *ws = calloc(1, sizeof(ws2812_encoder_t));
    ws->base.encode = ws2812_encode;
    ws->base.del = ws2812_del;
    ws->base.reset = ws2812_reset;

    rmt_bytes_encoder_config_t bcfg = {
        .bit0 = { .level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L },
        .bit1 = { .level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L },
        .flags.msb_first = 1,
    };
    rmt_new_bytes_encoder(&bcfg, &ws->bytes_encoder);

    rmt_copy_encoder_config_t ccfg = {};
    rmt_new_copy_encoder(&ccfg, &ws->copy_encoder);

    ws->reset_symbol = (rmt_symbol_word_t){
        .level0 = 0,
        .duration0 = RESET_TICKS,
        .level1 = 0,
        .duration1 = 0,
    };
    *ret = &ws->base;
}

static void led_init(void) {
    rmt_tx_channel_config_t ch_cfg = {
        .gpio_num = LED_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_CLK_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    rmt_new_tx_channel(&ch_cfg, &s_led_channel);
    led_encoder_new(&s_led_encoder);
    rmt_enable(s_led_channel);
}

static void led_set(uint8_t r, uint8_t g, uint8_t b) {
    s_led_rgb[0] = r;
    s_led_rgb[1] = g;
    s_led_rgb[2] = b;
    uint8_t grb[3] = { g, r, b };
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    rmt_transmit(s_led_channel, s_led_encoder, grb, sizeof(grb), &tx_cfg);
    rmt_tx_wait_all_done(s_led_channel, portMAX_DELAY);
}

static void buzzer_off(void) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CH, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CH);
}

static void buzzer_timer_cb(void *arg) {
    (void)arg;
    buzzer_off();
}

static void buzzer_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BUZZER_TIMER,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BUZZER_CH,
        .timer_sel = BUZZER_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch);

    esp_timer_create_args_t args = {
        .callback = buzzer_timer_cb,
        .name = "factory_buzzer",
    };
    esp_timer_create(&args, &s_buzzer_timer);
}

static void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms) {
    esp_timer_stop(s_buzzer_timer);
    ledc_set_freq(LEDC_LOW_SPEED_MODE, BUZZER_TIMER, freq_hz);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CH, 512);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CH);
    esp_timer_start_once(s_buzzer_timer, (uint64_t)duration_ms * 1000);
}

static void print_help(void) {
    printf("Commands:\n");
    printf("  status    print board test status\n");
    printf("  led r g b set LED color, 0..255\n");
    printf("  beep      play buzzer test tone\n");
    printf("  help      command list\n");
}

static void print_status(void) {
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    printf("FACTORY status uptime_ms=%lld chip_cores=%d led=%u,%u,%u cmds=%lu\n",
           (long long)(esp_timer_get_time() / 1000),
           chip.cores,
           s_led_rgb[0], s_led_rgb[1], s_led_rgb[2],
           (unsigned long)s_cmd_count);
}

static void serial_task(void *arg) {
    (void)arg;
    char line[96];
    print_help();
    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        s_cmd_count++;

        int r = 0, g = 0, b = 0;
        if (strcmp(line, "status") == 0) {
            print_status();
        } else if (strcmp(line, "beep") == 0) {
            buzzer_tone(2000, 120);
            printf("OK beep\n");
        } else if (sscanf(line, "led %d %d %d", &r, &g, &b) == 3) {
            if (r < 0) r = 0;
            if (r > 255) r = 255;
            if (g < 0) g = 0;
            if (g > 255) g = 255;
            if (b < 0) b = 0;
            if (b > 255) b = 255;
            led_set((uint8_t)r, (uint8_t)g, (uint8_t)b);
            printf("OK led %d %d %d\n", r, g, b);
        } else if (strcmp(line, "help") == 0) {
            print_help();
        } else {
            printf("ERROR unknown command\n");
        }
    }
}

static void led_task(void *arg) {
    (void)arg;
    const uint8_t colors[][3] = {
        { 24, 0, 0 },
        { 0, 24, 0 },
        { 0, 0, 24 },
        { 24, 16, 0 },
    };
    size_t idx = 0;
    while (1) {
        led_set(colors[idx][0], colors[idx][1], colors[idx][2]);
        idx = (idx + 1) % (sizeof(colors) / sizeof(colors[0]));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void heartbeat_task(void *arg) {
    (void)arg;
    while (1) {
        print_status();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    led_init();
    buzzer_init();
    buzzer_tone(1800, 120);
    vTaskDelay(pdMS_TO_TICKS(160));
    buzzer_tone(2400, 120);

    ESP_LOGI(TAG, "OSRBOT ESP32-S3 factory demo started");
    xTaskCreate(led_task, "factory_led", 3072, NULL, 5, NULL);
    xTaskCreate(serial_task, "factory_serial", 4096, NULL, 5, NULL);
    xTaskCreate(heartbeat_task, "factory_heartbeat", 3072, NULL, 4, NULL);
}
