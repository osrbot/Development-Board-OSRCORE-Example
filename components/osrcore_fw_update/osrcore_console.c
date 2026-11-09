#include "osrcore_console.h"

bool osrcore_console_read_line(osrcore_console_reader_t *reader, FILE *stream,
                               char *line, size_t capacity)
{
    if (capacity < 2) return false;
    int ch;
    while ((ch = fgetc(stream)) != EOF) {
        if (ch == '\n' || ch == '\r') {
            bool complete = !reader->discard && reader->length > 0;
            line[reader->length] = '\0';
            reader->length = 0;
            reader->discard = false;
            if (complete) return true;
        } else if (!reader->discard) {
            if (ch == 0 || reader->length >= capacity - 1) {
                reader->length = 0;
                reader->discard = true;
            } else {
                line[reader->length++] = (char)ch;
            }
        }
    }
    /* USB VFS may report EOF/EAGAIN between packets, even within one command. */
    clearerr(stream);
    return false;
}
