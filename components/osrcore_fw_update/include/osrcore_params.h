#pragma once
#include <stdbool.h>
#include <stddef.h>
struct cJSON;
typedef struct { const char *key; double min, max, step, initial; } osrcore_param_t;
/* Call once, before starting any serial or lesson tasks. Definitions must be static. */
void osrcore_params_init(const osrcore_param_t *definitions, size_t count);
double osrcore_param_get(const char *key, double fallback);
bool osrcore_params_handle_line(const char *line);
void osrcore_params_add_info(struct cJSON *info);
