/**
 * 示例07：NVS 参数持久化存储
 *
 * 演示：将 PID 参数（kp/ki/kd）以 blob 形式存入 NVS Flash，
 *       重启后自动恢复，通过 USB CDC 串口命令修改并保存。
 *
 * 命令（USB CDC 终端输入）：
 *   kp <val>   — 设置 Kp
 *   ki <val>   — 设置 Ki
 *   kd <val>   — 设置 Kd
 *   show       — 打印当前参数
 *   reset      — 恢复默认值
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"

#define NVS_NS      "pid_params"
#define NVS_KEY     "params"
#define NVS_INIT    "init"

#define KP_DEFAULT  447.0f
#define KI_DEFAULT    4.7f
#define KD_DEFAULT   47.0f

static const char *TAG = "nvs";

typedef struct {
    float kp, ki, kd;
} pid_params_t;

static pid_params_t g_params;

static void params_load(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) goto defaults;

    uint8_t inited = 0;
    nvs_get_u8(h, NVS_INIT, &inited);
    if (!inited) { nvs_close(h); goto defaults; }

    size_t sz = sizeof(pid_params_t);
    err = nvs_get_blob(h, NVS_KEY, &g_params, &sz);
    nvs_close(h);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded kp=%.2f ki=%.2f kd=%.2f", g_params.kp, g_params.ki, g_params.kd);
        return;
    }

defaults:
    g_params.kp = KP_DEFAULT;
    g_params.ki = KI_DEFAULT;
    g_params.kd = KD_DEFAULT;
    ESP_LOGI(TAG, "Using defaults kp=%.2f ki=%.2f kd=%.2f", g_params.kp, g_params.ki, g_params.kd);
}

static void params_save(void)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NVS_NS, NVS_READWRITE, &h));
    nvs_set_blob(h, NVS_KEY, &g_params, sizeof(pid_params_t));
    nvs_set_u8(h, NVS_INIT, 1);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Saved kp=%.2f ki=%.2f kd=%.2f", g_params.kp, g_params.ki, g_params.kd);
}

static void cmd_task(void *arg)
{
    char line[64];
    while (1) {
        if (!fgets(line, sizeof(line), stdin))
            continue;
        line[strcspn(line, "\r\n")] = '\0';

        float val;
        if (sscanf(line, "kp %f", &val) == 1) {
            g_params.kp = val; params_save();
        } else if (sscanf(line, "ki %f", &val) == 1) {
            g_params.ki = val; params_save();
        } else if (sscanf(line, "kd %f", &val) == 1) {
            g_params.kd = val; params_save();
        } else if (strcmp(line, "show") == 0) {
            printf("kp=%.2f ki=%.2f kd=%.2f\n", g_params.kp, g_params.ki, g_params.kd);
        } else if (strcmp(line, "reset") == 0) {
            g_params.kp = KP_DEFAULT;
            g_params.ki = KI_DEFAULT;
            g_params.kd = KD_DEFAULT;
            params_save();
        }
    }
}

void app_main(void)
{
    /* USB CDC console */
    usb_serial_jtag_driver_config_t usb_cfg = {
        .rx_buffer_size = 256,
        .tx_buffer_size = 256,
    };
    usb_serial_jtag_driver_install(&usb_cfg);
    esp_vfs_usb_serial_jtag_use_driver();

    ESP_ERROR_CHECK(nvs_flash_init());
    params_load();

    xTaskCreate(cmd_task, "cmd", 4096, NULL, 3, NULL);

    printf("NVS example ready. Commands: kp/ki/kd <val>, show, reset\n");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
