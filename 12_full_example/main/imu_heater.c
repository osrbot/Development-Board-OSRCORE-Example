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
#include <stdint.h>

/* ── Constants ──────────────────────────────────────────────────── */
#define TARGET_C            56.0f
#define WARM_C              38.0f
#define STABLE_C            54.0f
#define PREHEAT_MARGIN      8.0f
#define HYSTERESIS          1.0f
#define SLOPE_WINDOW        6
#define SLOPE_MAX           0.1f
#define READY_TIMEOUT_MS    300000UL
#define LOG_INTERVAL        10      /* ticks (10 × 500 ms = 5 s) */

/* GPIO46 sits in the high GPIO bank (GPIO32–GPIO63) */
#define HEATER_PIN_BIT (1UL << (46 - 32))

/* ── Extern helpers defined in main.c ──────────────────────────── */
/* buzzer_tone and led_set_color are made non-static in main.c     */
extern void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms);
extern void led_set_color(uint8_t r, uint8_t g, uint8_t b);

/* ── State ──────────────────────────────────────────────────────── */
static float s_target  = TARGET_C;
static float s_temp    = 0.0f;
static bool  s_on      = false;
static bool  s_warm    = false;
static bool  s_stable  = false;
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

    uint32_t tick = 0;
    float slope_hist[SLOPE_WINDOW];
    memset(slope_hist, 0, sizeof(slope_hist));
    int   slope_idx  = 0;
    int   slope_cnt  = 0;
    float prev_temp  = 0.0f;
    bool  prev_valid = false;

    while (1) {
        float temp = s_temp; /* written atomically by imu_heater_update_temp() */

        /* Phase 1 — preheat: full-on until within PREHEAT_MARGIN of target */
        /* Phase 2 — regulate: bang-bang near target                        */
        bool near_target = (temp >= s_target - PREHEAT_MARGIN);
        if (!near_target) {
            heater_set(true);
        } else {
            if (s_on  && temp >= s_target + HYSTERESIS) heater_set(false);
            if (!s_on && temp <  s_target - HYSTERESIS) heater_set(true);
        }

        /* Log every 5 s while not yet ready */
        if (!s_ready && (tick % LOG_INTERVAL == 0)) {
            printf("INFO: heater warming temp=%.1fC target=%.1fC heater=%s\n",
                   temp, s_target, s_on ? "ON" : "OFF");
        }

        /* Milestone 1 — 38°C: warm enough to use */
        if (!s_warm && temp >= WARM_C) {
            s_warm = true;
            printf("INFO: heater warm, temp=%.1fC — ready to use\n", temp);
            buzzer_tone(880, 200);
            led_set_color(0, 255, 0); /* green */
        }

        /* Milestone 2 — 54°C: thermal steady state */
        if (!s_stable && temp >= STABLE_C) {
            s_stable = true;
            printf("INFO: heater stable, temp=%.1fC — reset for best gyro bias\n", temp);
            buzzer_tone(1200, 150);
            vTaskDelay(pdMS_TO_TICKS(200));
            buzzer_tone(600, 300);
            led_set_color(255, 0, 0); /* red */
        }

        /* Slope tracking: accumulate per-sample delta once near target */
        if (!s_ready && near_target) {
            if (prev_valid) {
                float delta = temp - prev_temp;
                slope_hist[slope_idx] = delta;
                slope_idx = (slope_idx + 1) % SLOPE_WINDOW;
                if (slope_cnt < SLOPE_WINDOW) slope_cnt++;
            }
            prev_temp  = temp;
            prev_valid = true;
        }

        /* Ready condition: in band + slope window full + all |delta| < threshold,
         * OR timeout as fallback                                               */
        if (!s_ready) {
            bool in_band   = (temp >= s_target - HYSTERESIS);
            bool timed_out = ((tick * 500UL) >= READY_TIMEOUT_MS);
            bool slope_ok  = false;

            if (slope_cnt == SLOPE_WINDOW) {
                slope_ok = true;
                for (int i = 0; i < SLOPE_WINDOW; i++) {
                    if (fabsf(slope_hist[i]) >= SLOPE_MAX) {
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

void imu_heater_init(float target_c)
{
    s_target = target_c;

    /* GPIO46 is a strapping pin — configure output directly via registers */
    PIN_FUNC_SELECT(IO_MUX_GPIO46_REG, FUNC_GPIO46_GPIO46);
    REG_WRITE(GPIO_ENABLE1_W1TS_REG, HEATER_PIN_BIT);

    heater_set(false);

    xTaskCreate(task_heater, "heater", 2048, NULL, 2, NULL);
}

void imu_heater_update_temp(float t) { s_temp = t; }
bool imu_heater_is_on(void)          { return s_on; }
bool imu_heater_warm(void)           { return s_warm; }
bool imu_heater_stable(void)         { return s_stable; }
bool imu_heater_ready(void)          { return s_ready; }
