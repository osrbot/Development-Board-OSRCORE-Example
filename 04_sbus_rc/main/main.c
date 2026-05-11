/**
 * 示例04：SBUS 遥控接收
 *
 * 硬件：UART0，GPIO44(RX)，GPIO43(TX)
 *
 * SBUS 协议：
 *   波特率 100000，8E2（8数据位，偶校验，2停止位）
 *   信号电平反相（逻辑1=低电平）
 *   帧格式：0x0F + 22字节数据 + 1字节标志 + 0x00，共25字节
 *   16通道，每通道11位，范围 240-1810（中立约1024）
 *   标志字节 bit2=failsafe，bit3=lost_frame
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#define SBUS_UART     UART_NUM_0
#define SBUS_RX_PIN   44
#define SBUS_TX_PIN   43
#define SBUS_BAUD     100000
#define SBUS_FRAME    25

static const char *TAG = "sbus";

typedef struct {
    uint16_t ch[16];
    bool failsafe;
    bool lost_frame;
} sbus_data_t;

static void sbus_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = SBUS_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_EVEN,
        .stop_bits  = UART_STOP_BITS_2,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(SBUS_UART, 256, 0, 0, NULL, 0);
    uart_param_config(SBUS_UART, &cfg);
    uart_set_pin(SBUS_UART, SBUS_TX_PIN, SBUS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    /* SBUS signal is inverted */
    uart_set_line_inverse(SBUS_UART, UART_SIGNAL_RXD_INV | UART_SIGNAL_TXD_INV);
}

static bool sbus_parse(const uint8_t *buf, sbus_data_t *out)
{
    if (buf[0] != 0x0F || buf[24] != 0x00)
        return false;

    out->ch[0]  = ((buf[1]       | buf[2]  << 8) & 0x07FF);
    out->ch[1]  = ((buf[2]  >> 3 | buf[3]  << 5) & 0x07FF);
    out->ch[2]  = ((buf[3]  >> 6 | buf[4]  << 2 | buf[5] << 10) & 0x07FF);
    out->ch[3]  = ((buf[5]  >> 1 | buf[6]  << 7) & 0x07FF);
    out->ch[4]  = ((buf[6]  >> 4 | buf[7]  << 4) & 0x07FF);
    out->ch[5]  = ((buf[7]  >> 7 | buf[8]  << 1 | buf[9] << 9) & 0x07FF);
    out->ch[6]  = ((buf[9]  >> 2 | buf[10] << 6) & 0x07FF);
    out->ch[7]  = ((buf[10] >> 5 | buf[11] << 3) & 0x07FF);
    out->ch[8]  = ((buf[12]      | buf[13] << 8) & 0x07FF);
    out->ch[9]  = ((buf[13] >> 3 | buf[14] << 5) & 0x07FF);
    out->ch[10] = ((buf[14] >> 6 | buf[15] << 2 | buf[16] << 10) & 0x07FF);
    out->ch[11] = ((buf[16] >> 1 | buf[17] << 7) & 0x07FF);
    out->ch[12] = ((buf[17] >> 4 | buf[18] << 4) & 0x07FF);
    out->ch[13] = ((buf[18] >> 7 | buf[19] << 1 | buf[20] << 9) & 0x07FF);
    out->ch[14] = ((buf[20] >> 2 | buf[21] << 6) & 0x07FF);
    out->ch[15] = ((buf[21] >> 5 | buf[22] << 3) & 0x07FF);

    out->failsafe  = (buf[23] & (1 << 2)) != 0;
    out->lost_frame = (buf[23] & (1 << 3)) != 0;
    return true;
}

static void sbus_task(void *arg)
{
    uint8_t buf[SBUS_FRAME];
    sbus_data_t data;

    while (1) {
        /* Sync to frame header */
        uint8_t byte;
        do {
            uart_read_bytes(SBUS_UART, &byte, 1, portMAX_DELAY);
        } while (byte != 0x0F);

        buf[0] = byte;
        int got = uart_read_bytes(SBUS_UART, buf + 1, SBUS_FRAME - 1, pdMS_TO_TICKS(10));
        if (got != SBUS_FRAME - 1)
            continue;

        if (!sbus_parse(buf, &data))
            continue;

        if (data.failsafe) {
            ESP_LOGW(TAG, "FAILSAFE");
            continue;
        }

        printf("ch: %4d %4d %4d %4d %4d %4d %4d %4d | fs=%d lf=%d\n",
               data.ch[0], data.ch[1], data.ch[2], data.ch[3],
               data.ch[4], data.ch[5], data.ch[6], data.ch[7],
               data.failsafe, data.lost_frame);
    }
}

void app_main(void)
{
    sbus_init();
    ESP_LOGI(TAG, "SBUS ready on UART%d RX=GPIO%d", SBUS_UART, SBUS_RX_PIN);
    xTaskCreate(sbus_task, "sbus", 4096, NULL, 5, NULL);
}
