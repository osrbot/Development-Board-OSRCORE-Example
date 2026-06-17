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
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/rmt_tx.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"

#define LED_PIN       45
#define BUZZER_PIN    42
#define RMT_CLK_HZ    80000000
#define BUZZER_TIMER  LEDC_TIMER_1
#define BUZZER_CH     LEDC_CHANNEL_2
#define I2C_SDA       10
#define I2C_SCL       11
#define I2C_FREQ_HZ   400000

#define T0H (400 / 12)
#define T0L (850 / 12)
#define T1H (800 / 12)
#define T1L (450 / 12)
#define RESET_TICKS (50000 / 12)

static const char *TAG = "factory";
static rmt_channel_handle_t s_led_channel;
static rmt_encoder_handle_t s_led_encoder;
static esp_timer_handle_t s_buzzer_timer;
static i2c_master_bus_handle_t s_i2c_bus;
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

static void i2c_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (ret != ESP_OK) {
        printf("DIAG i2c init=FAIL err=%s\n", esp_err_to_name(ret));
    }
}

static void print_reset_reason(void) {
    const char *name = "unknown";
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON: name = "poweron"; break;
    case ESP_RST_EXT: name = "external"; break;
    case ESP_RST_SW: name = "software"; break;
    case ESP_RST_PANIC: name = "panic"; break;
    case ESP_RST_INT_WDT: name = "interrupt_wdt"; break;
    case ESP_RST_TASK_WDT: name = "task_wdt"; break;
    case ESP_RST_WDT: name = "other_wdt"; break;
    case ESP_RST_DEEPSLEEP: name = "deepsleep"; break;
    case ESP_RST_BROWNOUT: name = "brownout"; break;
    case ESP_RST_SDIO: name = "sdio"; break;
    default: break;
    }
    printf("DIAG reset=%s\n", name);
}

static void print_board_info(void) {
    esp_chip_info_t chip;
    uint32_t flash_size = 0;
    uint8_t mac[6] = {0};
    esp_chip_info(&chip);
    esp_flash_get_size(NULL, &flash_size);
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    printf("DIAG chip=ESP32-S3 cores=%d rev=%d features=0x%lx\n",
           chip.cores, chip.revision, (unsigned long)chip.features);
    printf("DIAG flash=%luKB heap_free=%lu heap_min=%lu heap_dma=%lu\n",
           (unsigned long)(flash_size / 1024),
           (unsigned long)esp_get_free_heap_size(),
           (unsigned long)esp_get_minimum_free_heap_size(),
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA));
    printf("DIAG mac=%02X:%02X:%02X:%02X:%02X:%02X random=0x%08lX uptime_ms=%lld\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
           (unsigned long)esp_random(),
           (long long)(esp_timer_get_time() / 1000));
    print_reset_reason();
}

static void i2c_scan(void) {
    if (!s_i2c_bus) {
        printf("DIAG i2c scan=FAIL bus_not_ready\n");
        return;
    }
    printf("DIAG i2c sda=%d scl=%d freq=%d scan_begin\n", I2C_SDA, I2C_SCL, I2C_FREQ_HZ);
    int found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (i2c_master_probe(s_i2c_bus, addr, 20) == ESP_OK) {
            printf("DIAG i2c addr=0x%02X%s\n", addr,
                   addr == 0x6B ? " QMI8658" : "");
            found++;
        }
    }
    printf("DIAG i2c qmi8658=%s\n", i2c_master_probe(s_i2c_bus, 0x6B, 20) == ESP_OK ? "OK" : "MISS");
    printf("DIAG i2c qmc6309_7bit_0x7C=%s\n", i2c_master_probe(s_i2c_bus, 0x7C, 20) == ESP_OK ? "OK" : "MISS");
    printf("DIAG i2c scan_done found=%d\n", found);
}

static void led_test(void) {
    const uint8_t colors[][3] = {
        { 48, 0, 0 },
        { 0, 48, 0 },
        { 0, 0, 48 },
        { 48, 32, 0 },
        { 0, 0, 0 },
    };
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        led_set(colors[i][0], colors[i][1], colors[i][2]);
        vTaskDelay(pdMS_TO_TICKS(180));
    }
    printf("OK ledtest\n");
}

static void beep_test(void) {
    const uint32_t tones[] = { 1600, 2200, 2800 };
    for (size_t i = 0; i < sizeof(tones) / sizeof(tones[0]); i++) {
        buzzer_tone(tones[i], 90);
        vTaskDelay(pdMS_TO_TICKS(140));
    }
    printf("OK beeptest\n");
}

static void run_diag(void) {
    printf("DIAG begin\n");
    print_board_info();
    i2c_scan();
    printf("DIAG led_gpio=%d buzzer_gpio=%d serial=USB\n", LED_PIN, BUZZER_PIN);
    printf("DIAG result=CHECK_LED_BUZZER_BY_OBSERVATION\n");
    printf("DIAG done\n");
}

static void print_help(void) {
    printf("Commands:\n");
    printf("  diag       run full board diagnostics\n");
    printf("  status     print short board status\n");
    printf("  i2c        scan I2C bus on SDA10/SCL11\n");
    printf("  ledtest    run LED color sequence\n");
    printf("  beeptest   run buzzer tone sequence\n");
    printf("  led r g b  set LED color, 0..255\n");
    printf("  beep       play one buzzer test tone\n");
    printf("  help       command list\n");
}

static void print_status(void) {
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    printf("FACTORY status uptime_ms=%lld chip_cores=%d heap=%lu led=%u,%u,%u cmds=%lu\n",
           (long long)(esp_timer_get_time() / 1000),
           chip.cores,
           (unsigned long)esp_get_free_heap_size(),
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
        if (strcmp(line, "diag") == 0) {
            run_diag();
        } else if (strcmp(line, "status") == 0) {
            print_status();
        } else if (strcmp(line, "i2c") == 0) {
            i2c_scan();
        } else if (strcmp(line, "ledtest") == 0) {
            led_test();
        } else if (strcmp(line, "beeptest") == 0) {
            beep_test();
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
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    led_init();
    buzzer_init();
    i2c_init();
    buzzer_tone(1800, 120);
    vTaskDelay(pdMS_TO_TICKS(160));
    buzzer_tone(2400, 120);

    ESP_LOGI(TAG, "OSRBOT ESP32-S3 factory demo started");
    run_diag();
    xTaskCreate(led_task, "factory_led", 3072, NULL, 5, NULL);
    xTaskCreate(serial_task, "factory_serial", 4096, NULL, 5, NULL);
    xTaskCreate(heartbeat_task, "factory_heartbeat", 3072, NULL, 4, NULL);
}
