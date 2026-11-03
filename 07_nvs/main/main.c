/** Lesson 07: save one gain, restart, and read it back. */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_system.h"
#include "osrcore_fw_update.h"
#include "osrcore_lesson_config.h"
static nvs_handle_t handle;
bool osrcore_lesson_command(const char *line){
 if(strcmp(line,"save")!=0)return false;
 float kp=OSR_NVS_KP;ESP_ERROR_CHECK(nvs_set_blob(handle,"kp",&kp,sizeof(kp)));ESP_ERROR_CHECK(nvs_commit(handle));ESP_LOGI("nvs","Saved Kp=%.2f",kp);return true;
}
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(nvs_open("lesson7", NVS_READWRITE, &handle));
    osrcore_fw_network_start();
    float kp = OSR_NVS_KP; size_t length=sizeof(kp);
    esp_err_t err=nvs_get_blob(handle,"kp",&kp,&length);
    if(err!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND) ESP_ERROR_CHECK(err);
    ESP_LOGI("nvs", "Loaded Kp=%.2f. Commands: save, reboot", kp);
    char line[384];
    while(1) {
        if(!fgets(line,sizeof(line),stdin)){vTaskDelay(pdMS_TO_TICKS(20));continue;}
        line[strcspn(line,"\r\n")]=0;
        if(osrcore_fw_handle_line(line))continue;
        if(osrcore_lesson_command(line))continue;
        else if(strcmp(line,"reboot")==0){fflush(stdout);esp_restart();}
    }
}
