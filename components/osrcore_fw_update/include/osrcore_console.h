#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* One state per serial reader; retain fragments between nonblocking polls. */
typedef struct {
    size_t length;
    bool discard;
} osrcore_console_reader_t;

/* Returns only complete, nonempty lines. Oversized or binary lines are discarded. */
bool osrcore_console_read_line(osrcore_console_reader_t *reader, FILE *stream,
                               char *line, size_t capacity);
