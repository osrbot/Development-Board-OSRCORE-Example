/**
 * Lesson 01: WS2812B RGB color cycle.
 * Hardware: one WS2812B on GPIO45.
 * The RMT TX driver encodes GRB bytes into pulse symbols.
 * At 80 MHz, one tick is 12.5 ns: T0H=32, T0L=68, T1H=64, T1L=36.
 * The reset interval is at least 50 us (4000 ticks).
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "osrcore_fw_update.h"
#include "osrcore_params.h"
#include "osrcore_lesson_config.h"

#define LED_PIN      45
#define RMT_CLK_HZ   80000000  /* 80 MHz */

/* WS2812B bit timing in RMT ticks (1 tick = 12.5 ns @ 80 MHz) */
#define T0H  (400 * 80 / 1000)   // 32 ticks
#define T0L  (850 * 80 / 1000)   // 68 ticks
#define T1H  (800 * 80 / 1000)   // 64 ticks
#define T1L  (450 * 80 / 1000)   // 36 ticks
#define RESET_TICKS (50000 * 80 / 1000)  // 4000 ticks

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
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded = 0;
    if (ws->state == 0) {
        encoded += ws->bytes_encoder->encode(ws->bytes_encoder, channel,
                                             data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) ws->state = 1;
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto done;
        }
    }
    if (ws->state == 1) {
        session_state = RMT_ENCODING_RESET;
        encoded += ws->copy_encoder->encode(ws->copy_encoder, channel,
                    &ws->reset_symbol, sizeof(ws->reset_symbol), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws->state = 0;
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) state |= RMT_ENCODING_MEM_FULL;
    }
done:
    *ret_state = state;
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
    if (!ws) return ESP_ERR_NO_MEM;
    ws->base.encode  = ws2812_encode;
    ws->base.del     = ws2812_del;
    ws->base.reset   = ws2812_reset;

    /* Bit-level encoder: maps each byte to RMT symbols */
    rmt_bytes_encoder_config_t bcfg = {
        .bit0 = { .level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L },
        .bit1 = { .level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L },
        .flags.msb_first = 1,
    };
    esp_err_t err = rmt_new_bytes_encoder(&bcfg, &ws->bytes_encoder);
    if (err != ESP_OK) { free(ws); return err; }

    /* Copy encoder for the reset pulse */
    rmt_copy_encoder_config_t ccfg = {};
    err = rmt_new_copy_encoder(&ccfg, &ws->copy_encoder);
    if (err != ESP_OK) { rmt_del_encoder(ws->bytes_encoder); free(ws); return err; }

    /* Reset symbol: low for >= 50 us */
    ws->reset_symbol = (rmt_symbol_word_t){
        .level0    = 0,
        .duration0 = RESET_TICKS / 2,
        .level1    = 0,
        .duration1 = RESET_TICKS / 2,
    };

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
    ESP_ERROR_CHECK(rmt_new_tx_channel(&ch_cfg, &s_channel));
    ESP_ERROR_CHECK(ws2812_encoder_new(&s_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_channel));
}

/* data: GRB byte order */
static void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t grb[3] = { g, r, b };
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    ESP_ERROR_CHECK(rmt_transmit(s_channel, s_encoder, grb, sizeof(grb), &tx_cfg));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_channel, -1));
}

/* ---- Main ---- */

void app_main(void)
{
    led_init();
    static const osrcore_param_t parameters[] = {
        {"interval", 100, 2000, 50, OSR_LED_INTERVAL},
        {"brightness", 0, 255, 1, OSR_LED_BRIGHTNESS},
    };
    osrcore_params_init(parameters, sizeof(parameters)/sizeof(parameters[0]));
    osrcore_fw_update_start();
    ESP_LOGI(TAG, "WS2812B ready on GPIO%d", LED_PIN);

    const char *names[] = {"red", "green", "blue", "off"};
    int idx = 0;
    while (1) {
        uint8_t brightness = (uint8_t)osrcore_param_get("brightness", OSR_LED_BRIGHTNESS);
        ESP_LOGI(TAG, "%s", names[idx]);
        led_set(idx==0?brightness:0, idx==1?brightness:0, idx==2?brightness:0);
        idx = (idx + 1) % 4;
        vTaskDelay(pdMS_TO_TICKS((int)osrcore_param_get("interval", OSR_LED_INTERVAL)));
    }
}
