/**
 * 示例07：NVS 参数持久化存储
 *
 * 演示 osrcore 中全部 4 个 NVS 命名空间的读写：
 *   1. pid_params  — kp / ki / kd
 *   2. mag_calib   — 硬铁偏移 (3 floats) + 软铁矩阵 (9 floats)
 *   3. cf_params   — 互补滤波参数 alpha_static / alpha_moving / speed_thr
 *   4. level_cal   — 加速度计水平校准偏移 ox / oy / oz
 *
 * USB CDC 串口命令：
 *   kp <val>                    — 设置 Kp 并保存
 *   ki <val>                    — 设置 Ki 并保存
 *   kd <val>                    — 设置 Kd 并保存
 *   mag hi <x> <y> <z>          — 设置硬铁偏移并保存
 *   cf <alpha_s> <alpha_m> <thr>— 设置互补滤波参数并保存
 *   level <ox> <oy> <oz>        — 设置水平校准偏移并保存
 *   show                        — 打印全部 4 个命名空间的当前值
 *   reset                       — 恢复全部默认值并保存
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

static const char *TAG = "nvs_example";

/* ------------------------------------------------------------------ */
/*  Namespace / key constants (mirrors osrcore config.h)               */
/* ------------------------------------------------------------------ */

/* pid_params */
#define NS_PID          "pid_params"
#define KEY_KP          "kp"
#define KEY_KI          "ki"
#define KEY_KD          "kd"
#define KEY_PID_INIT    "init"

#define KP_DEFAULT      447.0f
#define KI_DEFAULT        4.7f
#define KD_DEFAULT       47.0f

/* mag_calib */
#define NS_MAG          "mag_calib"
#define KEY_MAG_HI      "hi"       /* hard-iron: 3×float blob  */
#define KEY_MAG_SI      "si"       /* soft-iron: 9×float blob  */
#define KEY_MAG_INIT    "init"

/* cf_params */
#define NS_CF           "cf_params"
#define KEY_CF_AS       "alpha_s"
#define KEY_CF_AM       "alpha_m"
#define KEY_CF_THR      "spd_thr"
#define KEY_CF_INIT     "init"

#define CF_ALPHA_STATIC_DEFAULT  0.98f
#define CF_ALPHA_MOVING_DEFAULT  0.995f
#define CF_SPEED_THR_DEFAULT     0.5f

/* level_cal */
#define NS_LEVEL        "level_cal"
#define KEY_LEVEL_OX    "ox"
#define KEY_LEVEL_OY    "oy"
#define KEY_LEVEL_OZ    "oz"
#define KEY_LEVEL_INIT  "init"

/* ------------------------------------------------------------------ */
/*  In-RAM parameter structs                                           */
/* ------------------------------------------------------------------ */

static struct {
    float kp, ki, kd;
} g_pid = { KP_DEFAULT, KI_DEFAULT, KD_DEFAULT };

static struct {
    float hard[3];          /* hard-iron offset (Gauss)          */
    float soft[9];          /* soft-iron matrix, row-major 3×3   */
} g_mag = {
    .hard = { 0.0f, 0.0f, 0.0f },
    .soft = { 1.0f, 0.0f, 0.0f,
              0.0f, 1.0f, 0.0f,
              0.0f, 0.0f, 1.0f },
};

static struct {
    float alpha_static;
    float alpha_moving;
    float speed_thr;
} g_cf = { CF_ALPHA_STATIC_DEFAULT, CF_ALPHA_MOVING_DEFAULT, CF_SPEED_THR_DEFAULT };

static struct {
    float ox, oy, oz;       /* accel mounting offsets in g       */
} g_level = { 0.0f, 0.0f, 0.0f };

/* ------------------------------------------------------------------ */
/*  pid_params — save / load                                           */
/* ------------------------------------------------------------------ */

static void pid_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NS_PID, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, KEY_KP, &g_pid.kp, sizeof(float));
    nvs_set_blob(h, KEY_KI, &g_pid.ki, sizeof(float));
    nvs_set_blob(h, KEY_KD, &g_pid.kd, sizeof(float));
    nvs_set_u8(h, KEY_PID_INIT, 1);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "[pid_params] saved kp=%.2f ki=%.2f kd=%.2f",
             g_pid.kp, g_pid.ki, g_pid.kd);
}

static void pid_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NS_PID, NVS_READONLY, &h) != ESP_OK) goto defaults;

    uint8_t inited = 0;
    nvs_get_u8(h, KEY_PID_INIT, &inited);
    if (!inited) { nvs_close(h); goto defaults; }

    size_t sz = sizeof(float);
    nvs_get_blob(h, KEY_KP, &g_pid.kp, &sz);
    nvs_get_blob(h, KEY_KI, &g_pid.ki, &sz);
    nvs_get_blob(h, KEY_KD, &g_pid.kd, &sz);
    nvs_close(h);
    ESP_LOGI(TAG, "[pid_params] loaded kp=%.2f ki=%.2f kd=%.2f",
             g_pid.kp, g_pid.ki, g_pid.kd);
    return;

defaults:
    g_pid.kp = KP_DEFAULT;
    g_pid.ki = KI_DEFAULT;
    g_pid.kd = KD_DEFAULT;
    ESP_LOGI(TAG, "[pid_params] using defaults");
}

/* ------------------------------------------------------------------ */
/*  mag_calib — save / load                                            */
/* ------------------------------------------------------------------ */

static void mag_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NS_MAG, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, KEY_MAG_HI, g_mag.hard, 3 * sizeof(float));
    nvs_set_blob(h, KEY_MAG_SI, g_mag.soft, 9 * sizeof(float));
    nvs_set_u8(h, KEY_MAG_INIT, 1);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "[mag_calib] saved hi=[%.4f %.4f %.4f]",
             g_mag.hard[0], g_mag.hard[1], g_mag.hard[2]);
}

static void mag_load(void)
{
    static const float identity[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };

    nvs_handle_t h;
    if (nvs_open(NS_MAG, NVS_READONLY, &h) != ESP_OK) goto defaults;

    uint8_t inited = 0;
    nvs_get_u8(h, KEY_MAG_INIT, &inited);
    if (!inited) { nvs_close(h); goto defaults; }

    size_t sz_hi = 3 * sizeof(float);
    size_t sz_si = 9 * sizeof(float);
    nvs_get_blob(h, KEY_MAG_HI, g_mag.hard, &sz_hi);
    nvs_get_blob(h, KEY_MAG_SI, g_mag.soft, &sz_si);
    nvs_close(h);
    ESP_LOGI(TAG, "[mag_calib] loaded hi=[%.4f %.4f %.4f]",
             g_mag.hard[0], g_mag.hard[1], g_mag.hard[2]);
    return;

defaults:
    memset(g_mag.hard, 0, sizeof(g_mag.hard));
    memcpy(g_mag.soft, identity, sizeof(g_mag.soft));
    ESP_LOGI(TAG, "[mag_calib] using defaults (zero hard-iron, identity soft-iron)");
}

/* ------------------------------------------------------------------ */
/*  cf_params — save / load                                            */
/* ------------------------------------------------------------------ */

static void cf_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NS_CF, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, KEY_CF_AS,  &g_cf.alpha_static,  sizeof(float));
    nvs_set_blob(h, KEY_CF_AM,  &g_cf.alpha_moving,  sizeof(float));
    nvs_set_blob(h, KEY_CF_THR, &g_cf.speed_thr,     sizeof(float));
    nvs_set_u8(h, KEY_CF_INIT, 1);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "[cf_params] saved alpha_s=%.4f alpha_m=%.4f thr=%.3f",
             g_cf.alpha_static, g_cf.alpha_moving, g_cf.speed_thr);
}

static void cf_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NS_CF, NVS_READONLY, &h) != ESP_OK) goto defaults;

    uint8_t inited = 0;
    nvs_get_u8(h, KEY_CF_INIT, &inited);
    if (!inited) { nvs_close(h); goto defaults; }

    size_t sz = sizeof(float);
    nvs_get_blob(h, KEY_CF_AS,  &g_cf.alpha_static, &sz);
    nvs_get_blob(h, KEY_CF_AM,  &g_cf.alpha_moving, &sz);
    nvs_get_blob(h, KEY_CF_THR, &g_cf.speed_thr,    &sz);
    nvs_close(h);
    ESP_LOGI(TAG, "[cf_params] loaded alpha_s=%.4f alpha_m=%.4f thr=%.3f",
             g_cf.alpha_static, g_cf.alpha_moving, g_cf.speed_thr);
    return;

defaults:
    g_cf.alpha_static = CF_ALPHA_STATIC_DEFAULT;
    g_cf.alpha_moving = CF_ALPHA_MOVING_DEFAULT;
    g_cf.speed_thr    = CF_SPEED_THR_DEFAULT;
    ESP_LOGI(TAG, "[cf_params] using defaults");
}

/* ------------------------------------------------------------------ */
/*  level_cal — save / load                                            */
/* ------------------------------------------------------------------ */

static void level_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NS_LEVEL, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, KEY_LEVEL_OX, &g_level.ox, sizeof(float));
    nvs_set_blob(h, KEY_LEVEL_OY, &g_level.oy, sizeof(float));
    nvs_set_blob(h, KEY_LEVEL_OZ, &g_level.oz, sizeof(float));
    nvs_set_u8(h, KEY_LEVEL_INIT, 1);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "[level_cal] saved offset=[%.4f %.4f %.4f]g",
             g_level.ox, g_level.oy, g_level.oz);
}

static void level_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NS_LEVEL, NVS_READONLY, &h) != ESP_OK) goto defaults;

    uint8_t inited = 0;
    nvs_get_u8(h, KEY_LEVEL_INIT, &inited);
    if (!inited) { nvs_close(h); goto defaults; }

    size_t sz = sizeof(float);
    nvs_get_blob(h, KEY_LEVEL_OX, &g_level.ox, &sz);
    nvs_get_blob(h, KEY_LEVEL_OY, &g_level.oy, &sz);
    nvs_get_blob(h, KEY_LEVEL_OZ, &g_level.oz, &sz);
    nvs_close(h);
    ESP_LOGI(TAG, "[level_cal] loaded offset=[%.4f %.4f %.4f]g",
             g_level.ox, g_level.oy, g_level.oz);
    return;

defaults:
    g_level.ox = 0.0f;
    g_level.oy = 0.0f;
    g_level.oz = 0.0f;
    ESP_LOGI(TAG, "[level_cal] using defaults (zero offsets)");
}

/* ------------------------------------------------------------------ */
/*  reset all namespaces to defaults and persist                       */
/* ------------------------------------------------------------------ */

static void all_reset(void)
{
    g_pid.kp = KP_DEFAULT;
    g_pid.ki = KI_DEFAULT;
    g_pid.kd = KD_DEFAULT;
    pid_save();

    memset(g_mag.hard, 0, sizeof(g_mag.hard));
    static const float identity[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };
    memcpy(g_mag.soft, identity, sizeof(g_mag.soft));
    mag_save();

    g_cf.alpha_static = CF_ALPHA_STATIC_DEFAULT;
    g_cf.alpha_moving = CF_ALPHA_MOVING_DEFAULT;
    g_cf.speed_thr    = CF_SPEED_THR_DEFAULT;
    cf_save();

    g_level.ox = 0.0f;
    g_level.oy = 0.0f;
    g_level.oz = 0.0f;
    level_save();

    printf("All namespaces reset to defaults.\n");
}

/* ------------------------------------------------------------------ */
/*  show all params                                                     */
/* ------------------------------------------------------------------ */

static void all_show(void)
{
    printf("\n--- pid_params ---\n");
    printf("  kp=%.4f  ki=%.4f  kd=%.4f\n",
           g_pid.kp, g_pid.ki, g_pid.kd);

    printf("--- mag_calib ---\n");
    printf("  hard-iron: [%.4f  %.4f  %.4f]\n",
           g_mag.hard[0], g_mag.hard[1], g_mag.hard[2]);
    printf("  soft-iron: [%.4f %.4f %.4f]\n"
           "             [%.4f %.4f %.4f]\n"
           "             [%.4f %.4f %.4f]\n",
           g_mag.soft[0], g_mag.soft[1], g_mag.soft[2],
           g_mag.soft[3], g_mag.soft[4], g_mag.soft[5],
           g_mag.soft[6], g_mag.soft[7], g_mag.soft[8]);

    printf("--- cf_params ---\n");
    printf("  alpha_static=%.4f  alpha_moving=%.4f  speed_thr=%.3f\n",
           g_cf.alpha_static, g_cf.alpha_moving, g_cf.speed_thr);

    printf("--- level_cal ---\n");
    printf("  ox=%.4f  oy=%.4f  oz=%.4f  (g)\n\n",
           g_level.ox, g_level.oy, g_level.oz);
}

/* ------------------------------------------------------------------ */
/*  USB CDC command task                                               */
/* ------------------------------------------------------------------ */

static void cmd_task(void *arg)
{
    char line[128];

    printf("\nNVS example ready. Commands:\n");
    printf("  kp/ki/kd <val>\n");
    printf("  mag hi <x> <y> <z>\n");
    printf("  cf <alpha_s> <alpha_m> <thr>\n");
    printf("  level <ox> <oy> <oz>\n");
    printf("  show\n");
    printf("  reset\n\n");

    while (1) {
        if (!fgets(line, sizeof(line), stdin))
            continue;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        float a, b, c;

        /* --- pid_params --- */
        if (sscanf(line, "kp %f", &a) == 1) {
            g_pid.kp = a; pid_save();
        } else if (sscanf(line, "ki %f", &a) == 1) {
            g_pid.ki = a; pid_save();
        } else if (sscanf(line, "kd %f", &a) == 1) {
            g_pid.kd = a; pid_save();

        /* --- mag_calib --- */
        } else if (sscanf(line, "mag hi %f %f %f", &a, &b, &c) == 3) {
            g_mag.hard[0] = a;
            g_mag.hard[1] = b;
            g_mag.hard[2] = c;
            mag_save();

        /* --- cf_params --- */
        } else if (sscanf(line, "cf %f %f %f", &a, &b, &c) == 3) {
            g_cf.alpha_static = a;
            g_cf.alpha_moving = b;
            g_cf.speed_thr    = c;
            cf_save();

        /* --- level_cal --- */
        } else if (sscanf(line, "level %f %f %f", &a, &b, &c) == 3) {
            g_level.ox = a;
            g_level.oy = b;
            g_level.oz = c;
            level_save();

        /* --- show / reset --- */
        } else if (strcmp(line, "show") == 0) {
            all_show();
        } else if (strcmp(line, "reset") == 0) {
            all_reset();
        } else {
            printf("Unknown command: %s\n", line);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  app_main                                                           */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    /* USB CDC console */
    usb_serial_jtag_driver_config_t usb_cfg = {
        .rx_buffer_size = 256,
        .tx_buffer_size = 256,
    };
    usb_serial_jtag_driver_install(&usb_cfg);
    esp_vfs_usb_serial_jtag_use_driver();

    /* NVS flash init — erase on version mismatch */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition erased and re-initialised");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    /* Load all namespaces (falls back to defaults if not yet written) */
    pid_load();
    mag_load();
    cf_load();
    level_load();

    xTaskCreate(cmd_task, "cmd", 4096, NULL, 3, NULL);

    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
