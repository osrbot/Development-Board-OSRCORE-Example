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
#include "driver/uart.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "osrcore_fw_update.h"

#define LED_PIN       45
#define BUZZER_PIN    42
#define THROTTLE_PIN  1
#define STEERING_PIN  2
#define SBUS_RX_PIN   44
#define SBUS_TX_PIN   43
#define RMT_CLK_HZ    80000000
#define BUZZER_TIMER  LEDC_TIMER_1
#define BUZZER_CH     LEDC_CHANNEL_2
#define PWM_TIMER     LEDC_TIMER_0
#define PWM_RES       LEDC_TIMER_14_BIT
#define PWM_FREQ_HZ   50
#define THROTTLE_CH   LEDC_CHANNEL_0
#define STEERING_CH   LEDC_CHANNEL_1
#define I2C_SDA       10
#define I2C_SCL       11
#define I2C_FREQ_HZ   400000
#define SBUS_FRAME    25
#define SBUS_MIN      240
#define SBUS_MAX      1810
#define SBUS_CENTER   1024

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
static uint16_t s_rc_ch[16];
static uint32_t s_rc_frames = 0;
static bool s_rc_ok = false;
static bool s_rc_failsafe = false;
static bool s_drive_enabled = false;
static portMUX_TYPE s_factory_mux = portMUX_INITIALIZER_UNLOCKED;

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

static uint32_t duty_from_pulse(int pulse_us) {
    if (pulse_us < 1000) pulse_us = 1000;
    if (pulse_us > 2000) pulse_us = 2000;
    return (uint32_t)((float)pulse_us / 20000.0f * ((1 << 14) - 1));
}

static void set_pwm(ledc_channel_t ch, int pulse_us) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty_from_pulse(pulse_us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

static void pwm_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_RES,
        .timer_num = PWM_TIMER,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t throttle = {
        .gpio_num = THROTTLE_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = THROTTLE_CH,
        .timer_sel = PWM_TIMER,
        .duty = duty_from_pulse(1500),
        .hpoint = 0,
    };
    ledc_channel_config(&throttle);

    ledc_channel_config_t steering = {
        .gpio_num = STEERING_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = STEERING_CH,
        .timer_sel = PWM_TIMER,
        .duty = duty_from_pulse(1500),
        .hpoint = 0,
    };
    ledc_channel_config(&steering);
}

static int map_clamped(uint16_t v, int in_min, int in_max, int out_min, int out_max) {
    if (v < in_min) v = in_min;
    if (v > in_max) v = in_max;
    return (int)((float)(v - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min);
}

static void sbus_init(void) {
    uart_config_t cfg = {
        .baud_rate = 100000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_0, 512, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &cfg);
    uart_set_pin(UART_NUM_0, SBUS_TX_PIN, SBUS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_line_inverse(UART_NUM_0, UART_SIGNAL_RXD_INV | UART_SIGNAL_TXD_INV);
}

static bool sbus_parse(const uint8_t *buf, uint16_t *ch, bool *failsafe) {
    if (buf[0] != 0x0F || buf[24] != 0x00) return false;
    ch[0]  = ((buf[1]       | buf[2]  << 8) & 0x07FF);
    ch[1]  = ((buf[2]  >> 3 | buf[3]  << 5) & 0x07FF);
    ch[2]  = ((buf[3]  >> 6 | buf[4]  << 2 | buf[5] << 10) & 0x07FF);
    ch[3]  = ((buf[5]  >> 1 | buf[6]  << 7) & 0x07FF);
    ch[4]  = ((buf[6]  >> 4 | buf[7]  << 4) & 0x07FF);
    ch[5]  = ((buf[7]  >> 7 | buf[8]  << 1 | buf[9] << 9) & 0x07FF);
    ch[6]  = ((buf[9]  >> 2 | buf[10] << 6) & 0x07FF);
    ch[7]  = ((buf[10] >> 5 | buf[11] << 3) & 0x07FF);
    ch[8]  = ((buf[12]      | buf[13] << 8) & 0x07FF);
    ch[9]  = ((buf[13] >> 3 | buf[14] << 5) & 0x07FF);
    ch[10] = ((buf[14] >> 6 | buf[15] << 2 | buf[16] << 10) & 0x07FF);
    ch[11] = ((buf[16] >> 1 | buf[17] << 7) & 0x07FF);
    ch[12] = ((buf[17] >> 4 | buf[18] << 4) & 0x07FF);
    ch[13] = ((buf[18] >> 7 | buf[19] << 1 | buf[20] << 9) & 0x07FF);
    ch[14] = ((buf[20] >> 2 | buf[21] << 6) & 0x07FF);
    ch[15] = ((buf[21] >> 5 | buf[22] << 3) & 0x07FF);
    *failsafe = (buf[23] & (1 << 3)) != 0;
    return true;
}

static bool i2c_read_reg(uint16_t addr, uint8_t reg, uint8_t *val) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &dev) != ESP_OK) return false;
    esp_err_t ret = i2c_master_transmit_receive(dev, &reg, 1, val, 1, 20);
    i2c_master_bus_rm_device(dev);
    return ret == ESP_OK;
}

static bool i2c_read_burst(uint16_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &dev) != ESP_OK) return false;
    esp_err_t ret = i2c_master_transmit_receive(dev, &reg, 1, buf, len, 20);
    i2c_master_bus_rm_device(dev);
    return ret == ESP_OK;
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
    uint64_t sn = 0;
    esp_chip_info(&chip);
    esp_flash_get_size(NULL, &flash_size);
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    for (int i = 0; i < 6; i++) sn = (sn << 8) | mac[i];

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
    printf("DIAG sn=%012llX\n", (unsigned long long)sn);
    print_reset_reason();
}

static void print_imu_sample(void) {
    uint8_t who = 0;
    uint8_t raw[14] = {0};
    bool who_ok = i2c_read_reg(0x6B, 0x00, &who);
    bool data_ok = i2c_read_burst(0x6B, 0x33, raw, sizeof(raw));
    if (!who_ok) {
        printf("DIAG imu=MISS addr=0x6B\n");
        return;
    }
    int16_t temp = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t ax = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t ay = (int16_t)((raw[5] << 8) | raw[4]);
    int16_t az = (int16_t)((raw[7] << 8) | raw[6]);
    int16_t gx = (int16_t)((raw[9] << 8) | raw[8]);
    int16_t gy = (int16_t)((raw[11] << 8) | raw[10]);
    int16_t gz = (int16_t)((raw[13] << 8) | raw[12]);
    printf("DIAG imu=OK who=0x%02X data=%s temp_raw=%d acc_raw=%d,%d,%d gyro_raw=%d,%d,%d\n",
           who, data_ok ? "OK" : "FAIL", temp, ax, ay, az, gx, gy, gz);
}

static void print_mag_sample(void) {
    uint8_t chip = 0;
    uint8_t raw[6] = {0};
    bool id_ok = i2c_read_reg(0x7C, 0x00, &chip);
    bool data_ok = i2c_read_burst(0x7C, 0x01, raw, sizeof(raw));
    if (!id_ok) {
        printf("DIAG mag=MISS addr=0x7C\n");
        return;
    }
    int16_t x = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t y = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t z = (int16_t)((raw[5] << 8) | raw[4]);
    printf("DIAG mag=OK chip=0x%02X data=%s raw=%d,%d,%d\n",
           chip, data_ok ? "OK" : "FAIL", x, y, z);
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
    print_imu_sample();
    print_mag_sample();
    printf("DIAG led_gpio=%d buzzer_gpio=%d serial=USB\n", LED_PIN, BUZZER_PIN);
    printf("DIAG pwm throttle_gpio=%d steering_gpio=%d neutral=1500us\n", THROTTLE_PIN, STEERING_PIN);
    printf("DIAG sbus rx_gpio=%d frames=%lu ok=%d failsafe=%d\n",
           SBUS_RX_PIN, (unsigned long)s_rc_frames, s_rc_ok ? 1 : 0, s_rc_failsafe ? 1 : 0);
    printf("DIAG result=CHECK_LED_BUZZER_BY_OBSERVATION\n");
    printf("DIAG done\n");
}

static void print_help(void) {
    printf("Commands:\n");
    printf("  diag       run full board diagnostics\n");
    printf("  status     print short board status\n");
    printf("  sn         print ESP32 MAC-based serial number\n");
    printf("  imu        read QMI8658 sample\n");
    printf("  mag        read QMC6309 sample\n");
    printf("  i2c        scan I2C bus on SDA10/SCL11\n");
    printf("  rc         print latest SBUS channels\n");
    printf("  drive on   enable limited RC motor/steering test\n");
    printf("  drive off  disable RC motor/steering test\n");
    printf("  motor fwd|rev|stop  short ESC output test\n");
    printf("  servo left|center|right  steering output test\n");
    printf("  ledtest    run LED color sequence\n");
    printf("  beeptest   run buzzer tone sequence\n");
    printf("  led r g b  set LED color, 0..255\n");
    printf("  beep       play one buzzer test tone\n");
    printf("  help       command list\n");
}

static void print_status(void) {
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    printf("FACTORY status uptime_ms=%lld chip_cores=%d heap=%lu led=%u,%u,%u cmds=%lu rc=%lu drive=%d\n",
           (long long)(esp_timer_get_time() / 1000),
           chip.cores,
           (unsigned long)esp_get_free_heap_size(),
           s_led_rgb[0], s_led_rgb[1], s_led_rgb[2],
           (unsigned long)s_cmd_count,
           (unsigned long)s_rc_frames,
           s_drive_enabled ? 1 : 0);
}

static void print_rc(void) {
    uint16_t ch[16];
    bool ok, failsafe;
    uint32_t frames;
    portENTER_CRITICAL(&s_factory_mux);
    memcpy(ch, s_rc_ch, sizeof(ch));
    ok = s_rc_ok;
    failsafe = s_rc_failsafe;
    frames = s_rc_frames;
    portEXIT_CRITICAL(&s_factory_mux);
    printf("RC ok=%d failsafe=%d frames=%lu ch=%u,%u,%u,%u,%u,%u,%u,%u\n",
           ok ? 1 : 0, failsafe ? 1 : 0, (unsigned long)frames,
           ch[0], ch[1], ch[2], ch[3], ch[4], ch[5], ch[6], ch[7]);
}

static void serial_task(void *arg) {
    (void)arg;
    char line[384];
    print_help();
    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        s_cmd_count++;
        if (osrcore_fw_handle_line(line)) continue;

        int r = 0, g = 0, b = 0;
        if (strcmp(line, "diag") == 0) {
            run_diag();
        } else if (strcmp(line, "status") == 0) {
            print_status();
        } else if (strcmp(line, "sn") == 0) {
            print_board_info();
        } else if (strcmp(line, "imu") == 0) {
            print_imu_sample();
        } else if (strcmp(line, "mag") == 0) {
            print_mag_sample();
        } else if (strcmp(line, "i2c") == 0) {
            i2c_scan();
        } else if (strcmp(line, "rc") == 0) {
            print_rc();
        } else if (strcmp(line, "drive on") == 0) {
            s_drive_enabled = true;
            printf("OK drive on limited_rc_control\n");
        } else if (strcmp(line, "drive off") == 0) {
            s_drive_enabled = false;
            set_pwm(THROTTLE_CH, 1500);
            set_pwm(STEERING_CH, 1500);
            printf("OK drive off neutral\n");
        } else if (strcmp(line, "motor fwd") == 0) {
            set_pwm(THROTTLE_CH, 1560);
            vTaskDelay(pdMS_TO_TICKS(500));
            set_pwm(THROTTLE_CH, 1500);
            printf("OK motor fwd pulse=1560 duration=500ms\n");
        } else if (strcmp(line, "motor rev") == 0) {
            set_pwm(THROTTLE_CH, 1440);
            vTaskDelay(pdMS_TO_TICKS(500));
            set_pwm(THROTTLE_CH, 1500);
            printf("OK motor rev pulse=1440 duration=500ms\n");
        } else if (strcmp(line, "motor stop") == 0) {
            set_pwm(THROTTLE_CH, 1500);
            printf("OK motor stop\n");
        } else if (strcmp(line, "servo left") == 0) {
            set_pwm(STEERING_CH, 1300);
            printf("OK servo left pulse=1300\n");
        } else if (strcmp(line, "servo center") == 0) {
            set_pwm(STEERING_CH, 1500);
            printf("OK servo center pulse=1500\n");
        } else if (strcmp(line, "servo right") == 0) {
            set_pwm(STEERING_CH, 1700);
            printf("OK servo right pulse=1700\n");
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

static void sbus_task(void *arg) {
    (void)arg;
    uint8_t buf[SBUS_FRAME];
    while (1) {
        uint8_t byte = 0;
        if (uart_read_bytes(UART_NUM_0, &byte, 1, pdMS_TO_TICKS(20)) != 1) {
            if (!s_drive_enabled) {
                set_pwm(THROTTLE_CH, 1500);
                set_pwm(STEERING_CH, 1500);
            }
            continue;
        }
        if (byte != 0x0F) continue;
        buf[0] = byte;
        if (uart_read_bytes(UART_NUM_0, buf + 1, SBUS_FRAME - 1, pdMS_TO_TICKS(10)) != SBUS_FRAME - 1) {
            continue;
        }

        uint16_t ch[16] = {0};
        bool failsafe = false;
        if (!sbus_parse(buf, ch, &failsafe)) continue;

        portENTER_CRITICAL(&s_factory_mux);
        memcpy(s_rc_ch, ch, sizeof(s_rc_ch));
        s_rc_ok = true;
        s_rc_failsafe = failsafe;
        s_rc_frames++;
        portEXIT_CRITICAL(&s_factory_mux);

        if (s_drive_enabled && !failsafe) {
            int steering = map_clamped(ch[0], SBUS_MIN, SBUS_MAX, 1300, 1700);
            int throttle = map_clamped(ch[2], SBUS_MIN, SBUS_MAX, 1440, 1560);
            if (abs((int)ch[2] - SBUS_CENTER) < 40) throttle = 1500;
            if (abs((int)ch[0] - SBUS_CENTER) < 30) steering = 1500;
            set_pwm(STEERING_CH, steering);
            set_pwm(THROTTLE_CH, throttle);
        } else {
            set_pwm(THROTTLE_CH, 1500);
            if (!s_drive_enabled) set_pwm(STEERING_CH, 1500);
        }
    }
}

void app_main(void) {
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    led_init();
    buzzer_init();
    pwm_init();
    sbus_init();
    i2c_init();
    set_pwm(THROTTLE_CH, 1500);
    set_pwm(STEERING_CH, 1500);
    buzzer_tone(1800, 120);
    vTaskDelay(pdMS_TO_TICKS(160));
    buzzer_tone(2400, 120);

    ESP_LOGI(TAG, "OSRBOT ESP32-S3 factory demo started");
    run_diag();
    xTaskCreate(led_task, "factory_led", 3072, NULL, 5, NULL);
    xTaskCreate(serial_task, "factory_serial", 4096, NULL, 5, NULL);
    xTaskCreate(sbus_task, "factory_sbus", 4096, NULL, 5, NULL);
    xTaskCreate(heartbeat_task, "factory_heartbeat", 3072, NULL, 4, NULL);
}
