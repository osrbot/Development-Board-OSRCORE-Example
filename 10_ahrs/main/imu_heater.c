/*
 * IMU Heater — GPIO46 resistor heater control
 *
 * Hardware prerequisite: V_IN (9–26 V) must be connected.
 * The heater resistor is powered via the onboard TPS54540 5 V rail.
 * USB power alone is insufficient for full heating power.
 *
 * GPIO46 is a boot strapping pin. ESP-IDF gpio_config() silently
 * fails to enable the output driver on this pin. Output must be
 * controlled directly via GPIO register writes.
 */

#include "imu_heater.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* ── Constants ──────────────────────────────────────────────────── */
#define HEATER_WARM_C           38.0f   /* milestone: warm enough to use   */
#define HEATER_PREHEAT_MARGIN   8.0f    /* °C below target: full-on zone   */
#define HEATER_HYSTERESIS       1.0f    /* bang-bang ±1°C                  */
#define HEATER_SLOPE_WINDOW     6       /* samples (6 × 500 ms = 3 s)      */
#define HEATER_SLOPE_MAX        0.1f    /* °C/sample threshold              */
#define HEATER_READY_TIMEOUT_MS 300000  /* ms: fallback timeout             */
#define HEATER_LOG_INTERVAL     10      /* ticks (10 × 500 ms = 5 s)       */

/* GPIO46 sits in the high GPIO bank (GPIO32–GPIO63) */
#define HEATER_PIN_BIT (1UL << (46 - 32))

/* ── State ──────────────────────────────────────────────────────── */
static float s_target  = 56.0f;
static float s_temp    = 0.0f;   /* fed by imu_heater_update_temp()  */
static bool  s_on      = false;
static bool  s_warm    = false;
static bool  s_ready   = false;

/* ── Helpers ────────────────────────────────────────────────────── */
static void heater_set(bool on)
{
    s_on = on;
    if (on) {
        REG_WRITE(GPIO_OUT1_W1TS_REG, HEATER_PIN_BIT);
    } else {
        REG_WRITE(GPIO_OUT1_W1TC_REG, HEATER_PIN_BIT);
    }
}

/* ── Background task ────────────────────────────────────────────── */
static void task_heater(void *arg)
{
    (void)arg;

    uint32_t tick      = 0;
    float slope_hist[HEATER_SLOPE_WINDOW];
    memset(slope_hist, 0, sizeof(slope_hist));
    int   slope_idx    = 0;
    int   slope_cnt    = 0;
    float prev_temp    = 0.0f;
    bool  prev_valid   = false;

    while (1) {
        float temp = s_temp;   /* written atomically by update_temp() */

        /* Phase 1 — preheat: full-on until within PREHEAT_MARGIN of target */
        /* Phase 2 — regulate: bang-bang near target                        */
        bool near_target = (temp >= s_target - HEATER_PREHEAT_MARGIN);
        if (!near_target) {
            heater_set(true);
        } else {
            if (s_on  && temp >= s_target + HEATER_HYSTERESIS) heater_set(false);
            if (!s_on && temp <  s_target - HEATER_HYSTERESIS) heater_set(true);
        }

        /* Log every 5 s while not yet ready */
        if (!s_ready && (tick % HEATER_LOG_INTERVAL == 0)) {
            printf("INFO: heater warming temp=%.1fC target=%.1fC heater=%s\n",
                   temp, s_target, s_on ? "ON" : "OFF");
        }

        /* Milestone — 38°C: warm enough to use */
        if (!s_warm && temp >= HEATER_WARM_C) {
            s_warm = true;
            printf("INFO: heater warm, temp=%.1fC — ready to use\n", temp);
        }

        /* Slope tracking: accumulate per-sample delta once near target */
        if (!s_ready && near_target) {
            if (prev_valid) {
                float delta = temp - prev_temp;
                slope_hist[slope_idx] = delta;
                slope_idx = (slope_idx + 1) % HEATER_SLOPE_WINDOW;
                if (slope_cnt < HEATER_SLOPE_WINDOW) slope_cnt++;
            }
            prev_temp  = temp;
            prev_valid = true;
        }

        /* Ready condition: in band + slope window full + all |delta| < threshold,
         * OR timeout as fallback                                               */
        if (!s_ready) {
            bool in_band   = (temp >= s_target - HEATER_HYSTERESIS);
            bool timed_out = ((tick * 500UL) >= HEATER_READY_TIMEOUT_MS);
            bool slope_ok  = false;

            if (slope_cnt == HEATER_SLOPE_WINDOW) {
                slope_ok = true;
                for (int i = 0; i < HEATER_SLOPE_WINDOW; i++) {
                    if (fabsf(slope_hist[i]) >= HEATER_SLOPE_MAX) {
                        slope_ok = false;
                        break;
                    }
                }
            }

            if ((in_band && slope_ok) || timed_out) {
                s_ready = true;
                if (timed_out && !(in_band && slope_ok)) {
                    printf("WARN: heater timeout, temp=%.1fC (target %.1fC) — proceeding\n",
                           temp, s_target);
                } else {
                    printf("INFO: heater ready, temp=%.1fC (slope stable)\n", temp);
                }
            }
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ── Public API ─────────────────────────────────────────────────── */

void imu_heater_init(float temp_c)
{
    s_target = temp_c;

    /* GPIO46 is a strapping pin — configure output directly via registers */
    PIN_FUNC_SELECT(IO_MUX_GPIO46_REG, FUNC_GPIO46_GPIO46);
    REG_WRITE(GPIO_ENABLE1_W1TS_REG, HEATER_PIN_BIT);

    heater_set(false);

    xTaskCreate(task_heater, "heater", 2048, NULL, 2, NULL);
}

void  imu_heater_update_temp(float t) { s_temp = t; }
float imu_heater_get_temp(void)       { return s_temp; }
bool  imu_heater_is_on(void)          { return s_on; }
bool  imu_heater_warm(void)           { return s_warm; }
