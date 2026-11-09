/** Lesson 08: observe two periodic tasks. No fake sensor or motor control. */
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "osrcore_fw_update.h"
#include "osrcore_params.h"
#include "osrcore_lesson_config.h"
static void periodic_task(void *arg) {
    const int id=(intptr_t)arg;

    TickType_t last=xTaskGetTickCount(); unsigned count=0;
    while(1){const TickType_t period=pdMS_TO_TICKS((int)osrcore_param_get(id==0?"taskA":"taskB",id==0?OSR_TASK_A_MS:OSR_TASK_B_MS));ESP_LOGI("tasks","task=%c count=%u tick=%lu",'A'+id,++count,(unsigned long)last);vTaskDelayUntil(&last,period);}
}
void app_main(void) {
    static const osrcore_param_t parameters[] = {
        {"taskA", 100, 1500, 100, OSR_TASK_A_MS},
        {"taskB", 100, 1500, 100, OSR_TASK_B_MS},
    };
    osrcore_params_init(parameters, sizeof(parameters)/sizeof(parameters[0]));
    osrcore_fw_update_start();
    configASSERT(xTaskCreate(periodic_task,"task_A",3072,(void*)0,3,NULL)==pdPASS);
    configASSERT(xTaskCreate(periodic_task,"task_B",3072,(void*)1,3,NULL)==pdPASS);
}
