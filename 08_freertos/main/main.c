/** Lesson 08: observe two periodic tasks. No fake sensor or motor control. */
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "osrcore_fw_update.h"
#include "osrcore_lesson_config.h"
static void periodic_task(void *arg) {
    const int id=(intptr_t)arg;
    const TickType_t period=pdMS_TO_TICKS(id==0?OSR_TASK_A_MS:OSR_TASK_B_MS);
    TickType_t last=xTaskGetTickCount(); unsigned count=0;
    while(1){ESP_LOGI("tasks","task=%c count=%u tick=%lu",'A'+id,++count,(unsigned long)last);vTaskDelayUntil(&last,period);}
}
void app_main(void) {
    osrcore_fw_update_start();
    configASSERT(xTaskCreate(periodic_task,"task_A",3072,(void*)0,3,NULL)==pdPASS);
    configASSERT(xTaskCreate(periodic_task,"task_B",3072,(void*)1,3,NULL)==pdPASS);
}
