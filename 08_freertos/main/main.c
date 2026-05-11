/**
 * 示例08：FreeRTOS 多任务架构
 *
 * 演示 ESP32-S3 双核 FreeRTOS 的核心概念：
 *   - 任务创建与优先级
 *   - 任务间通信（Queue、Mutex）
 *   - 双核绑定（Core 0 / Core 1）
 *   - 任务运行时统计
 *
 * 任务结构（对应 osrcore 的三任务架构）：
 *   Core 1  task_sensor  P5  100ms  — 模拟传感器采样，写入队列
 *   Core 1  task_control P4  20ms   — 从队列读取，执行控制逻辑
 *   Core 0  task_comm    P3  1000ms — 打印系统状态
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "freertos";

/* Shared state protected by mutex */
typedef struct {
    float sensor_val;
    float control_out;
    uint32_t tick;
} shared_state_t;

static shared_state_t g_state;
static SemaphoreHandle_t g_mutex;

/* Queue: sensor → control */
typedef struct { float value; int64_t timestamp_us; } sensor_msg_t;
static QueueHandle_t g_sensor_q;

/* ---- task_sensor: Core 1, P5, 100ms ---- */
static void task_sensor(void *arg)
{
    sensor_msg_t msg;
    float phase = 0.0f;

    while (1) {
        /* Simulate a sine-wave sensor reading */
        phase += 0.1f;
        msg.value        = 1.0f + 0.5f * (phase - (int)phase);  /* sawtooth 1.0-1.5 */
        msg.timestamp_us = esp_timer_get_time();

        xQueueOverwrite(g_sensor_q, &msg);

        xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_state.sensor_val = msg.value;
        g_state.tick++;
        xSemaphoreGive(g_mutex);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ---- task_control: Core 1, P4, 20ms ---- */
static void task_control(void *arg)
{
    sensor_msg_t msg;
    float integral = 0.0f;
    const float target = 1.25f;
    const float kp = 2.0f, ki = 0.1f;

    while (1) {
        if (xQueuePeek(g_sensor_q, &msg, 0) == pdTRUE) {
            float err = target - msg.value;
            integral += err * 0.02f;
            float out = kp * err + ki * integral;

            xSemaphoreTake(g_mutex, portMAX_DELAY);
            g_state.control_out = out;
            xSemaphoreGive(g_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ---- task_comm: Core 0, P3, 1000ms ---- */
static void task_comm(void *arg)
{
    while (1) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        float sv  = g_state.sensor_val;
        float co  = g_state.control_out;
        uint32_t t = g_state.tick;
        xSemaphoreGive(g_mutex);

        printf("[tick=%lu] sensor=%.3f  control_out=%.3f\n", t, sv, co);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    g_mutex    = xSemaphoreCreateMutex();
    g_sensor_q = xQueueCreate(1, sizeof(sensor_msg_t));

    ESP_LOGI(TAG, "Creating tasks...");

    /* Core 1: real-time tasks */
    xTaskCreatePinnedToCore(task_sensor,  "sensor",  4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(task_control, "control", 4096, NULL, 4, NULL, 1);

    /* Core 0: communication */
    xTaskCreatePinnedToCore(task_comm, "comm", 4096, NULL, 3, NULL, 0);

    ESP_LOGI(TAG, "All tasks running. Core 1: sensor+control, Core 0: comm");
}
