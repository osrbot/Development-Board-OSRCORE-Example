// Exercise the actual firmware parser and NVS transaction with host-side platform stubs.
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const json=path.join(root,'01_blink_led/managed_components/espressif__cjson/cJSON');
const dir=await fs.mkdtemp(path.join(os.tmpdir(),'osrcore-runtime-'));
try{
 await fs.mkdir(path.join(dir,'freertos'));
 const headers={
 'freertos/FreeRTOS.h':'#include <assert.h>\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(x) ((void)(x))\n#define portEXIT_CRITICAL(x) ((void)(x))\n#define configASSERT assert\n',
 'esp_app_desc.h':'typedef struct {const char *project_name;} esp_app_desc_t;\nconst esp_app_desc_t *esp_app_get_description(void);\n',
 'nvs.h':'#include <stddef.h>\ntypedef int nvs_handle_t;\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define NVS_READONLY 0\n#define NVS_READWRITE 1\nint nvs_open(const char*,int,nvs_handle_t*);\nint nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);\nint nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);\nint nvs_commit(nvs_handle_t);\nvoid nvs_close(nvs_handle_t);\n',
 'nvs_flash.h':'int nvs_flash_init(void);\n',
 };
 for(const [name,content] of Object.entries(headers))await fs.writeFile(path.join(dir,name),content);
 await fs.writeFile(path.join(dir,'test.c'),String.raw`
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "osrcore_params.h"
#include "esp_app_desc.h"
#include "nvs.h"
static unsigned char committed[256],staged[256];static size_t stored,stage_size;static int fail_commit;
const esp_app_desc_t *esp_app_get_description(void){static esp_app_desc_t app={"01_blink_led"};return &app;}
int nvs_flash_init(void){return 0;}
int nvs_open(const char*n,int mode,nvs_handle_t*h){*h=1;return 0;}
int nvs_get_blob(nvs_handle_t h,const char*k,void*v,size_t*l){if(!stored)return -1;assert(*l>=stored);memcpy(v,committed,stored);*l=stored;return 0;}
int nvs_set_blob(nvs_handle_t h,const char*k,const void*v,size_t l){assert(l<256);memcpy(staged,v,l);stage_size=l;return 0;}
int nvs_commit(nvs_handle_t h){if(fail_commit)return -1;memcpy(committed,staged,stage_size);stored=stage_size;return 0;}
void nvs_close(nvs_handle_t h){}
int main(void){
 static const osrcore_param_t defs[]={{"interval",100,2000,50,500},{"brightness",0,255,1,32}};
 osrcore_params_init(defs,2);
 assert(!osrcore_params_handle_line("status"));
 assert(osrcore_params_handle_line("osrcore params {\"id\":\"apply\",\"lesson\":\"01_blink_led\",\"values\":{\"interval\":100,\"brightness\":80},\"save\":false}"));
 assert(osrcore_param_get("interval",0)==100 && osrcore_param_get("brightness",0)==80);
 osrcore_params_handle_line("osrcore params {\"id\":\"invalid\",\"lesson\":\"01_blink_led\",\"values\":{\"interval\":200,\"brightness\":999},\"save\":false}");
 assert(osrcore_param_get("interval",0)==100); // Entire update is rejected, not partially applied.
 osrcore_params_handle_line("osrcore params {\"id\":\"wrong\",\"lesson\":\"02_buzzer\",\"values\":{\"interval\":200,\"brightness\":90},\"save\":false}");
 osrcore_params_handle_line("osrcore params {\"id\":\"step\",\"lesson\":\"01_blink_led\",\"values\":{\"interval\":101,\"brightness\":90},\"save\":false}");
 osrcore_params_handle_line("osrcore params {\"id\":\"unknown\",\"lesson\":\"01_blink_led\",\"values\":{\"interval\":200,\"other\":90},\"save\":false}");
 osrcore_params_handle_line("osrcore params {\"id\":\"duplicate\",\"lesson\":\"01_blink_led\",\"values\":{\"interval\":200,\"interval\":300},\"save\":false}");
 osrcore_params_handle_line("osrcore params {\"id\":\"save\",\"lesson\":\"01_blink_led\",\"values\":{\"interval\":100,\"brightness\":80},\"save\":true}");
 fail_commit=1;
 osrcore_params_handle_line("osrcore params {\"id\":\"savefail\",\"lesson\":\"01_blink_led\",\"values\":{\"interval\":200,\"brightness\":70},\"save\":true}");
 assert(osrcore_param_get("interval",0)==100 && osrcore_param_get("brightness",0)==80);
 osrcore_params_init(defs,2); // Emulate startup loading the committed snapshot.
 assert(osrcore_param_get("interval",0)==100 && osrcore_param_get("brightness",0)==80);
 osrcore_params_handle_line("osrcore params malformed");
 osrcore_params_handle_line("osrcore params {\"id\":\"trailing\"} junk");
 return 0;
}
`);
 const binary=path.join(dir,'runtime-test');
 execFileSync('cc',['-std=c11','-O2','-I'+dir,'-I'+json,'-I'+path.join(root,'components/osrcore_fw_update/include'),path.join(dir,'test.c'),path.join(root,'components/osrcore_fw_update/osrcore_params.c'),path.join(json,'cJSON.c'),'-lm','-o',binary]);
 const replies=execFileSync(binary,{encoding:'utf8'}).trim().split('\n').map(line=>JSON.parse(line.slice(15)));
 assert.equal(replies.find(r=>r.id==='apply').ok,true);
 assert.equal(replies.find(r=>r.id==='save').saved,true);
 for(const id of ['invalid','wrong','step','unknown','duplicate','savefail'])assert.equal(replies.find(r=>r.id===id).ok,false,id);
 console.log('Firmware parser passed: ranges, step, identity, atomic rejection, persistence, save failure and malformed requests.');
}finally{await fs.rm(dir,{recursive:true,force:true});}
