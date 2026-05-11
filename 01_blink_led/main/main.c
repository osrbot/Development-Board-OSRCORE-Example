/**
 * 示例01：WS2812B LED 颜色循环
 *
 * 硬件：GPIO45，1颗 WS2812B
 * 原理：使用 ESP-IDF RMT TX 驱动，自定义编码器将 GRB 字节流转换为 RMT 符号。
 *
 * WS2812B 时序（80MHz RMT 时钟，12.5ns/tick）：
 *   T0H = 32 ticks (~400ns)   T0L = 68 ticks (~850ns)
 *   T1H = 64 ticks (~800ns)   T1L = 36 ticks (~450ns)
 *   Reset >= 4000 ticks (50us)
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"

#define LED_PIN      45
#define RMT_CLK_HZ   80000000  /* 80 MHz */

/* WS2812B bit timing in RMT ticks (1 tick = 12.5 ns @ 80 MHz) */
#define T0H  32
#define T0L  68
#define T1H  64
#define T1L  36
#define RESET_TICKS 4000

static const char *TAG = "led";

/* ---- Custom RMT encoder for WS2812B ---- */

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    rmt_symbol_word_t reset_symbol;
    int state;
} ws2812_encoder_t;

static size_t ws2812_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                            const void *data, size_t data_size,
                            rmt_encode_state_t *ret_state)
{
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
                                            &ws->reset_symbol, sizeof(rmt_symbol_word_t),
                                            &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws->state = RMT_ENCODING_RESET;
            *ret_state = RMT_ENCODING_COMPLETE;
        }
    }
    return encoded;
}

static esp_err_t ws2812_del(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws = __containerof(encoder, ws2812_encoder_t, base);
    rmt_del_encoder(ws->bytes_encoder);
    rmt_del_encoder(ws->copy_encoder);
    free(ws);
    return ESP_OK;
}

static esp_err_t ws2812_reset(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_reset(ws->bytes_encoder);
    rmt_encoder_reset(ws->copy_encoder);
    ws->state = 0;
    return ESP_OK;
}

static esp_err_t ws2812_encoder_new(rmt_encoder_handle_t *ret)
{
    ws2812_encoder_t *ws = calloc(1, sizeof(ws2812_encoder_t));
    ws->base.encode  = ws2812_encode;
    ws->base.del     = ws2812_del;
    ws->base.reset   = ws2812_reset;

    /* Bit-level encoder: maps each byte to RMT symbols */
    rmt_bytes_encoder_config_t bcfg = {
        .bit0 = { .level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L },
        .bit1 = { .level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L },
        .flags.msb_first = 1,
    };
    rmt_new_bytes_encoder(&bcfg, &ws->bytes_encoder);

    /* Copy encoder for the reset pulse */
    rmt_copy_encoder_config_t ccfg = {};
    rmt_new_copy_encoder(&ccfg, &ws->copy_encoder);

    /* Reset symbol: low for >= 50 us */
    ws->reset_symbol.level0    = 0;
    ws->reset_symbol.duration0 = RESET_TICKS / 2;
    ws->reset_symbol.level1    = 0;
    ws->reset_symbol.duration1 = RESET_TICKS / 2;

    *ret = &ws->base;
    return ESP_OK;
}

/* ---- LED helpers ---- */

static rmt_channel_handle_t s_channel;
static rmt_encoder_handle_t s_encoder;

static void led_init(void)
{
    rmt_tx_channel_config_t ch_cfg = {
        .gpio_num        = LED_PIN,
        .clk_src         = RMT_CLK_SRC_DEFAULT,
        .resolution_hz   = RMT_CLK_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    rmt_new_tx_channel(&ch_cfg, &s_channel);
    ws2812_encoder_new(&s_encoder);
    rmt_enable(s_channel);
}

/* data: GRB byte order */
static void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t grb[3] = { g, r, b };
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    rmt_transmit(s_channel, s_encoder, grb, sizeof(grb), &tx_cfg);
    rmt_tx_wait_all_done(s_channel, portMAX_DELAY);
}

/* ---- Main ---- */

void app_main(void)
{
    led_init();
    ESP_LOGI(TAG, "WS2812B ready on GPIO%d", LED_PIN);

    /* Cycle: red -> green -> blue -> off */
    const struct { uint8_t r, g, b; const char *name; } colors[] = {
        { 32,  0,  0, "red"   },
        {  0, 32,  0, "green" },
        {  0,  0, 32, "blue"  },
        {  0,  0,  0, "off"   },
    };
    int idx = 0;

    while (1) {
        ESP_LOGI(TAG, "%s", colors[idx].name);
        led_set(colors[idx].r, colors[idx].g, colors[idx].b);
        idx = (idx + 1) % 4;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
