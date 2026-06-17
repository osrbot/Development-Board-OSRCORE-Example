#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Starts a small stdin command task for examples that do not already own USB serial. */
void osrcore_fw_update_start(void);

/* Handles stream/status/fw commands. Returns true when the line was consumed. */
bool osrcore_fw_handle_line(const char *line);

#ifdef __cplusplus
}
#endif
