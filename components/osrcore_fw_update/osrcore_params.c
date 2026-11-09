#include "osrcore_params.h"
#include "cJSON.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"

#define MAX_PARAMS 4
static const osrcore_param_t *defs;
static size_t count;
static double current[MAX_PARAMS];
static bool saved;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
typedef struct { uint32_t version, count; double values[MAX_PARAMS]; } stored_t;

static bool valid(size_t i, double value) {
    const osrcore_param_t *d = &defs[i];
    double steps = (value - d->min) / d->step;
    return isfinite(value) && value >= d->min && value <= d->max && fabs(steps - round(steps)) < 1e-6;
}
static void snapshot(double *values, bool *persisted) {
    portENTER_CRITICAL(&mux);
    memcpy(values, current, sizeof(current));
    *persisted = saved;
    portEXIT_CRITICAL(&mux);
}
static cJSON *values_json(const double *values) {
    cJSON *object = cJSON_CreateObject();
    for (size_t i = 0; i < count; i++) cJSON_AddNumberToObject(object, defs[i].key, values[i]);
    return object;
}
void osrcore_params_init(const osrcore_param_t *definitions, size_t n) {
    configASSERT(n <= MAX_PARAMS);
    defs = definitions; count = n; saved = false;
    memset(current, 0, sizeof(current));
    for (size_t i = 0; i < count; i++) current[i] = defs[i].initial;
    if (nvs_flash_init() != ESP_OK) return;
    nvs_handle_t handle;
    if (nvs_open("lesson_params", NVS_READONLY, &handle) != ESP_OK) return;
    stored_t data; size_t length = sizeof(data);
    esp_err_t err = nvs_get_blob(handle, esp_app_get_description()->project_name, &data, &length);
    nvs_close(handle);
    if (err != ESP_OK || length != sizeof(data) || data.version != 1 || data.count != count) return;
    for (size_t i = 0; i < count; i++) if (!valid(i, data.values[i])) return;
    memcpy(current, data.values, sizeof(current)); saved = true;
}
double osrcore_param_get(const char *key, double fallback) {
    double value = fallback;
    portENTER_CRITICAL(&mux);
    for (size_t i = 0; i < count; i++) if (!strcmp(key, defs[i].key)) { value = current[i]; break; }
    portEXIT_CRITICAL(&mux);
    return value;
}
void osrcore_params_add_info(cJSON *info) {
    double values[MAX_PARAMS]; bool persisted;
    snapshot(values, &persisted);
    cJSON_AddNumberToObject(info, "runtimeParameters", 1);
    cJSON_AddItemToObject(info, "parameters", values_json(values));
    cJSON_AddBoolToObject(info, "parametersSaved", persisted);
}
bool osrcore_params_handle_line(const char *line) {
    const char *prefix = "osrcore params ";
    if (strncmp(line, prefix, strlen(prefix))) return false;
    const char *end = NULL;
    cJSON *request = cJSON_ParseWithOpts(line + strlen(prefix), &end, true);
    cJSON *id = cJSON_GetObjectItemCaseSensitive(request, "id");
    cJSON *lesson = cJSON_GetObjectItemCaseSensitive(request, "lesson");
    cJSON *values = cJSON_GetObjectItemCaseSensitive(request, "values");
    cJSON *save = cJSON_GetObjectItemCaseSensitive(request, "save");
    const char *project = esp_app_get_description()->project_name;
    const char *error = NULL;
    double next[MAX_PARAMS] = {0};
    if (!cJSON_IsString(id) || strlen(id->valuestring) > 64 || !cJSON_IsString(lesson) || strcmp(lesson->valuestring, project)) error = "lesson_or_request_mismatch";
    else if (!count) error = "no_runtime_parameters";
    else if (!cJSON_IsObject(values) || cJSON_GetArraySize(values) != count || !cJSON_IsBool(save)) error = "invalid_parameters";
    if (!error) {
        for (size_t i = 0; i < count; i++) {
            cJSON *value = cJSON_GetObjectItemCaseSensitive(values, defs[i].key);
            if (!cJSON_IsNumber(value) || !valid(i, value->valuedouble)) { error = "parameter_out_of_range"; break; }
            next[i] = value->valuedouble;
        }
        // Reject duplicate names, even if the object has the expected size.
        for (cJSON *a = values->child; a && !error; a = a->next)
            for (cJSON *b = a->next; b; b = b->next)
                if (!strcmp(a->string, b->string)) { error = "duplicate_parameter"; break; }
    }
    if (!error && cJSON_IsTrue(save)) {
        nvs_handle_t handle;
        esp_err_t err = nvs_open("lesson_params", NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            stored_t data = {.version = 1, .count = count};
            memcpy(data.values, next, sizeof(next));
            err = nvs_set_blob(handle, project, &data, sizeof(data));
            if (err == ESP_OK) err = nvs_commit(handle);
            nvs_close(handle);
        }
        if (err != ESP_OK) error = "save_failed";
    }
    if (!error) {
        portENTER_CRITICAL(&mux);
        memcpy(current, next, sizeof(current)); saved = cJSON_IsTrue(save);
        portEXIT_CRITICAL(&mux);
    }
    cJSON *reply = cJSON_CreateObject();
    if (reply) {
        double actual[MAX_PARAMS]; bool persisted; snapshot(actual, &persisted);
        cJSON_AddStringToObject(reply, "id", cJSON_IsString(id) ? id->valuestring : "");
        cJSON_AddStringToObject(reply, "project", project);
        cJSON_AddBoolToObject(reply, "ok", error == NULL);
        cJSON_AddBoolToObject(reply, "saved", persisted);
        cJSON_AddItemToObject(reply, "values", values_json(actual));
        if (error) cJSON_AddStringToObject(reply, "error", error);
        char *json = cJSON_PrintUnformatted(reply);
        if (json) { printf("OSRCORE_PARAMS %s\n", json); cJSON_free(json); }
        cJSON_Delete(reply); fflush(stdout);
    }
    cJSON_Delete(request);
    return true;
}
