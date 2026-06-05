/**
 * 示例12：QMC6309 磁力计读取
 *
 * 硬件：I2C，SDA=GPIO10，SCL=GPIO11，地址 0x7C
 * 输出：航向角（度）、XYZ 磁场（Gauss），每 100ms 打印一次
 * 控制台：USB CDC，支持 cal / heading / help 命令
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "qmc6309.h"

#define I2C_SDA      10
#define I2C_SCL      11
#define IO_CTRL_BAT  16

#define I2C_FREQ_HZ  400000
#define CMD_BUF_LEN  64

static const char *TAG = "mag";

/* ------------------------------------------------------------------ */
/*  Command parsing                                                     */
/* ------------------------------------------------------------------ */

static void handle_command(const char *line)
{
    /* strip trailing whitespace */
    char buf[CMD_BUF_LEN];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == ' '))
        buf[--len] = '\0';

    if (strncmp(buf, "cal", 3) == 0) {
        uint32_t sec = 30; /* default 30 s */
        if (buf[3] == ' ' && buf[4] != '\0')
            sec = (uint32_t)atoi(&buf[4]);
        printf("Calibrating for %lu s — rotate board 360°...\n", (unsigned long)sec);
        qmc6309_calibrate(sec * 1000);
        float hard[3], soft[9];
        qmc6309_get_calibration(hard, soft);
        printf("hard-iron: x=%.4f y=%.4f z=%.4f\n", hard[0], hard[1], hard[2]);

    } else if (strcmp(buf, "heading") == 0) {
        qmc6309_data_t d;
        if (qmc6309_read(&d))
            printf("heading=%.1f\n", d.heading);
        else
            printf("no data\n");

    } else if (strcmp(buf, "help") == 0) {
        printf("commands:\n"
               "  cal [sec]  — calibrate hard-iron (default 30 s)\n"
               "  heading    — print current heading\n"
               "  help       — this message\n");

    } else if (len > 0) {
        printf("unknown command: %s\n", buf);
    }
}

/* ------------------------------------------------------------------ */
/*  Main task                                                           */
/* ------------------------------------------------------------------ */

static void mag_task(void *arg)
{
    char cmd_buf[CMD_BUF_LEN];
    int  cmd_pos = 0;
    uint8_t rx;

    TickType_t last_read = xTaskGetTickCount();

    while (1) {
        /* --- 100 ms mag read --- */
        TickType_t now = xTaskGetTickCount();
        if ((now - last_read) >= pdMS_TO_TICKS(100)) {
            last_read = now;
            qmc6309_data_t d;
            if (qmc6309_read(&d))
                printf("hdg=%.1f x=%.4f y=%.4f z=%.4f\n",
                       d.heading, d.x, d.y, d.z);
        }

        /* --- non-blocking USB CDC byte read --- */
        int n = usb_serial_jtag_read_bytes(&rx, 1, 0);
        if (n == 1) {
            if (rx == '\n' || rx == '\r') {
                cmd_buf[cmd_pos] = '\0';
                if (cmd_pos > 0)
                    handle_command(cmd_buf);
                cmd_pos = 0;
            } else if (cmd_pos < CMD_BUF_LEN - 1) {
                cmd_buf[cmd_pos++] = (char)rx;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    /* Battery enable — GPIO16 HIGH */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << IO_CTRL_BAT),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_cfg);
    gpio_set_level(IO_CTRL_BAT, 1);

    /* USB CDC console */
    usb_serial_jtag_driver_config_t usb_cfg = {
        .rx_buffer_size = 256,
        .tx_buffer_size = 256,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));
    usb_serial_jtag_vfs_use_driver();

    /* I2C bus */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port             = I2C_NUM_0,
        .sda_io_num           = I2C_SDA,
        .scl_io_num           = I2C_SCL,
        .clk_source           = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt    = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    /* QMC6309 device */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = QMC6309_ADDR,
        .scl_speed_hz    = I2C_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev));

    qmc6309_init(dev);
    ESP_LOGI(TAG, "QMC6309 ready. Type 'help' for commands.");

    xTaskCreatePinnedToCore(mag_task, "mag_task", 4096, NULL, 5, NULL, 0);
}
