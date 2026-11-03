#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void osrcore_fw_network_start(void);
void osrcore_fw_record_log(const char *line);
bool osrcore_wifi_handle_line(const char *line);
bool osrcore_fw_try_lock(void);
void osrcore_fw_unlock(void);
bool osrcore_lesson_command(const char *line);

/* Starts a small stdin command task for examples that do not already own USB serial. */
void osrcore_fw_update_start(void);

/* Handles stream/status/fw commands. Returns true when the line was consumed. */
bool osrcore_fw_handle_line(const char *line);

#ifdef __cplusplus
}
#endif
