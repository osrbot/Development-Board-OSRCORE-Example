/* Local-network lesson service. Pair over USB; credentials never appear in HTTP URLs. */
#include "osrcore_fw_update.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION_MAJOR >= 6
#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#include "mbedtls/private/sha256.h"
#else
#include "mbedtls/sha256.h"
#endif
static bool initialized, wifi_ready, update_busy;
static esp_netif_t *netif;
static char pairing[33];
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static char log_lines[48][192];
static unsigned log_sequence;
static vprintf_like_t previous_log;
static httpd_handle_t server;

bool osrcore_fw_try_lock(void){bool ok;portENTER_CRITICAL(&mux);ok=!update_busy;if(ok)update_busy=true;portEXIT_CRITICAL(&mux);return ok;}
void osrcore_fw_unlock(void){portENTER_CRITICAL(&mux);update_busy=false;portEXIT_CRITICAL(&mux);}
void osrcore_fw_record_log(const char *line){
    portENTER_CRITICAL(&mux);strlcpy(log_lines[log_sequence%48],line,192);log_sequence++;portEXIT_CRITICAL(&mux);
}
static int capture_log(const char *format,va_list args){
    char line[192];va_list copy;va_copy(copy,args);vsnprintf(line,sizeof(line),format,copy);va_end(copy);
    osrcore_fw_record_log(line);
    return previous_log?previous_log(format,args):vprintf(format,args);
}
static esp_err_t json_reply(httpd_req_t *req,cJSON *data){char *text=cJSON_PrintUnformatted(data);cJSON_Delete(data);if(!text)return ESP_ERR_NO_MEM;httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");esp_err_t err=httpd_resp_sendstr(req,text);free(text);return err;}
static bool authorized(httpd_req_t *req){char header[48],expected[48];snprintf(expected,sizeof(expected),"Bearer %s",pairing);if(!pairing[0]||httpd_req_get_hdr_value_str(req,"Authorization",header,sizeof(header))!=ESP_OK||strcmp(header,expected)){httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Pair via USB first");return false;}return true;}
static esp_err_t info_handler(httpd_req_t *req){
    if(!authorized(req))return ESP_OK;
    const esp_app_desc_t *app=esp_app_get_description();cJSON *o=cJSON_CreateObject();
    cJSON_AddStringToObject(o,"board","OSRCORE");cJSON_AddNumberToObject(o,"protocol",1);cJSON_AddStringToObject(o,"project",app->project_name);cJSON_AddStringToObject(o,"version",app->version);
    cJSON_AddBoolToObject(o,"ota",true);return json_reply(req,o);
}
static esp_err_t logs_handler(httpd_req_t *req){
    if(!authorized(req))return ESP_OK;
    unsigned after=0;char query[48],number[16];if(httpd_req_get_url_query_str(req,query,sizeof(query))==ESP_OK&&httpd_query_key_value(query,"after",number,sizeof(number))==ESP_OK)after=strtoul(number,NULL,10);
    cJSON *o=cJSON_CreateObject(),*lines=cJSON_AddArrayToObject(o,"lines");unsigned end;portENTER_CRITICAL(&mux);end=log_sequence;portEXIT_CRITICAL(&mux);
    unsigned start=end>48?end-48:0;if(after>start&&after<=end)start=after;
    for(unsigned i=start;i<end;i++){char line[192];portENTER_CRITICAL(&mux);strlcpy(line,log_lines[i%48],sizeof(line));portEXIT_CRITICAL(&mux);cJSON_AddItemToArray(lines,cJSON_CreateString(line));}
    cJSON_AddNumberToObject(o,"next",end);return json_reply(req,o);
}
static void restart_task(void *arg){vTaskDelay(pdMS_TO_TICKS(500));esp_restart();}
__attribute__((weak)) bool osrcore_lesson_command(const char *line){return false;}
static esp_err_t command_handler(httpd_req_t *req){
    if(!authorized(req))return ESP_OK;
    char command[16];if(req->content_len<1||req->content_len>=sizeof(command))return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid command");
    int offset=0;while(offset<req->content_len){int n=httpd_req_recv(req,command+offset,req->content_len-offset);if(n<=0)return ESP_FAIL;offset+=n;}command[offset]=0;
    if(strcmp(command,"reboot")==0){httpd_resp_sendstr(req,"{\"ok\":true}");xTaskCreate(restart_task,"reboot",2048,NULL,2,NULL);return ESP_OK;}
    if(strcmp(command,"save")==0&&osrcore_lesson_command(command))return httpd_resp_sendstr(req,"{\"ok\":true}");
    return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Unsupported lesson command");
}
static esp_err_t ota_handler(httpd_req_t *req){
    if(!authorized(req))return ESP_OK;
    const esp_partition_t *part=esp_ota_get_next_update_partition(NULL);char expected[65];
    if(!part||req->content_len<32||req->content_len>part->size||httpd_req_get_hdr_value_str(req,"X-Firmware-SHA256",expected,sizeof(expected))!=ESP_OK||strlen(expected)!=64)return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid image metadata");
    if(!osrcore_fw_try_lock()){httpd_resp_set_status(req,"409 Conflict");return httpd_resp_sendstr(req,"Update already active");}
    esp_ota_handle_t handle=0;esp_err_t err=esp_ota_begin(part,req->content_len,&handle);bool opened=err==ESP_OK;
    mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts(&sha,0);
    char buffer[2048];int remaining=req->content_len;
    while(err==ESP_OK&&remaining>0){int n=httpd_req_recv(req,buffer,remaining>sizeof(buffer)?sizeof(buffer):remaining);if(n<=0){err=ESP_FAIL;break;}mbedtls_sha256_update(&sha,(unsigned char*)buffer,n);err=esp_ota_write(handle,buffer,n);remaining-=n;}
    unsigned char digest[32];char actual[65];mbedtls_sha256_finish(&sha,digest);mbedtls_sha256_free(&sha);for(int i=0;i<32;i++)sprintf(actual+i*2,"%02x",digest[i]);
    if(strcmp(actual,expected)!=0)err=ESP_ERR_INVALID_CRC;
    if(err==ESP_OK){err=esp_ota_end(handle);opened=false;}
    if(err==ESP_OK)err=esp_ota_set_boot_partition(part);
    if(err!=ESP_OK){if(opened)esp_ota_abort(handle);osrcore_fw_unlock();return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Image validation/write failed; old app retained");}
    httpd_resp_set_type(req,"application/json");httpd_resp_sendstr(req,"{\"ok\":true,\"rebooting\":true}");xTaskCreate(restart_task,"ota_reboot",2048,NULL,2,NULL);return ESP_OK;
}
static void start_http(void){
    if(server){return;}
    httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.stack_size=8192;config.recv_wait_timeout=10;config.max_uri_handlers=5;
    if(httpd_start(&server,&config)!=ESP_OK)return;
    httpd_uri_t routes[]={
        {.uri="/api/info",.method=HTTP_GET,.handler=info_handler},
        {.uri="/api/logs",.method=HTTP_GET,.handler=logs_handler},
        {.uri="/api/ota",.method=HTTP_POST,.handler=ota_handler},
        {.uri="/api/command",.method=HTTP_POST,.handler=command_handler}};
    for(unsigned i=0;i<4;i++)httpd_register_uri_handler(server,&routes[i]);
}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data){
    if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_START)esp_wifi_connect();
    if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED)esp_wifi_connect();
    if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){ip_event_got_ip_t *event=data;ESP_LOGI("lesson_wifi","Connected: " IPSTR,IP2STR(&event->ip_info.ip));start_http();}
}
static esp_err_t start_wifi(const char *ssid,const char *password){
    if(!wifi_ready){
        ESP_RETURN_ON_ERROR(esp_netif_init(),"wifi","netif");
        esp_err_t err=esp_event_loop_create_default();if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE)return err;
        netif=esp_netif_create_default_wifi_sta();wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
        ESP_RETURN_ON_ERROR(esp_wifi_init(&init),"wifi","init");
        esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL);esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL);
        esp_wifi_set_storage(WIFI_STORAGE_RAM);esp_wifi_set_mode(WIFI_MODE_STA);wifi_ready=true;
    }else esp_wifi_stop();
    wifi_config_t config={0};memcpy(config.sta.ssid,ssid,strnlen(ssid,sizeof(config.sta.ssid)));strlcpy((char*)config.sta.password,password,sizeof(config.sta.password));
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA,&config),"wifi","config");return esp_wifi_start();
}
static bool unhex(const char *text,char *out,size_t capacity){size_t n=strlen(text);if(strcmp(text,"-")==0){out[0]=0;return true;}if(n%2||n/2>=capacity)return false;for(size_t i=0;i<n;i+=2){char pair[3]={text[i],text[i+1],0},*end;long value=strtol(pair,&end,16);if(*end||value==0)return false;out[i/2]=value;}out[n/2]=0;return true;}
void osrcore_fw_network_start(void){
    if(initialized){return;}
    initialized=true;
    esp_err_t err=nvs_flash_init();if(err!=ESP_OK){ESP_LOGE("lesson_wifi","NVS unavailable: %s",esp_err_to_name(err));return;}
    nvs_handle_t nvs;if(nvs_open("lesson_wifi",NVS_READWRITE,&nvs)!=ESP_OK)return;
    size_t size=sizeof(pairing);if(nvs_get_str(nvs,"pair",pairing,&size)!=ESP_OK){unsigned char bytes[16];esp_fill_random(bytes,sizeof(bytes));for(int i=0;i<16;i++)sprintf(pairing+i*2,"%02x",bytes[i]);nvs_set_str(nvs,"pair",pairing);nvs_commit(nvs);}
    char ssid[33]={0},password[65]={0};size=sizeof(ssid);nvs_get_str(nvs,"ssid",ssid,&size);size=sizeof(password);nvs_get_str(nvs,"password",password,&size);nvs_close(nvs);
    previous_log=esp_log_set_vprintf(capture_log);if(ssid[0])start_wifi(ssid,password);
}
bool osrcore_wifi_handle_line(const char *line){
    if(strncmp(line,"wifi ",5)!=0){return false;}
    osrcore_fw_network_start();
    if(strcmp(line,"wifi status")==0){esp_netif_ip_info_t ip={0};if(netif)esp_netif_get_ip_info(netif,&ip);printf("WIFI ip=" IPSTR " token=%s protocol=1\n",IP2STR(&ip.ip),pairing);return true;}
    char ssid_hex[65],pass_hex[129],ssid[33],password[65];
    if(sscanf(line,"wifi config %64s %128s",ssid_hex,pass_hex)==2&&unhex(ssid_hex,ssid,sizeof(ssid))&&ssid[0]&&unhex(pass_hex,password,sizeof(password))){
        nvs_handle_t nvs;esp_err_t err=nvs_open("lesson_wifi",NVS_READWRITE,&nvs);if(err==ESP_OK){err=nvs_set_str(nvs,"ssid",ssid);if(err==ESP_OK)err=nvs_set_str(nvs,"password",password);if(err==ESP_OK)err=nvs_commit(nvs);nvs_close(nvs);}if(err==ESP_OK)err=start_wifi(ssid,password);printf("%s wifi config\n",err==ESP_OK?"OK":"ERROR");
    }else printf("ERROR wifi command\n");return true;
}
