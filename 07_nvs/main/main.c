/** Lesson 07: apply a gain, save it, restart, and read it back. */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_system.h"
#include "osrcore_fw_update.h"
#include "osrcore_console.h"
#include "osrcore_params.h"
#include "osrcore_lesson_config.h"
bool osrcore_lesson_command(const char *line){
    if(strcmp(line,"save")!=0)return false;
    char request[192];
    snprintf(request,sizeof(request),"osrcore params {\"id\":\"console\",\"lesson\":\"07_nvs\",\"values\":{\"kp\":%.4f},\"save\":true}",osrcore_param_get("kp",OSR_NVS_KP));
    return osrcore_params_handle_line(request);
}
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    static osrcore_param_t parameters[] = {{"kp",0,20,0.1,OSR_NVS_KP}};
    // Retain gains saved by earlier versions until a runtime parameter snapshot is saved.
    nvs_handle_t legacy;
    if(nvs_open("lesson7",NVS_READONLY,&legacy)==ESP_OK){
        float kp;size_t length=sizeof(kp);
        if(nvs_get_blob(legacy,"kp",&kp,&length)==ESP_OK && kp>=0 && kp<=20)parameters[0].initial=kp;
        nvs_close(legacy);
    }
    osrcore_params_init(parameters,1);
    osrcore_fw_network_start();
    ESP_LOGI("nvs", "Loaded Kp=%.2f. Commands: save, reboot", osrcore_param_get("kp",OSR_NVS_KP));
    char line[384];
    osrcore_console_reader_t reader = {0};
    while(1) {
        if(!osrcore_console_read_line(&reader, stdin, line, sizeof(line))){vTaskDelay(pdMS_TO_TICKS(20));continue;}
        line[strcspn(line,"\r\n")]=0;
        if(osrcore_fw_handle_line(line))continue;
        if(osrcore_lesson_command(line))continue;
        else if(strcmp(line,"reboot")==0){fflush(stdout);esp_restart();}
    }
}
