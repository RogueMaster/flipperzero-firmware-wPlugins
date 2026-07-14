#include "period_tracker_string_utils.h"
#include <string.h>

// Initialize buffered line reader
void file_line_reader_init(FileLineReader* reader, File* file) {
    reader->file = file;
    reader->len = 0;
    reader->pos = 0;
}

// Read next complete line from file, handling chunk boundaries
bool file_line_reader_next(FileLineReader* reader, char* line, size_t line_size) {
    if(!reader || !line || line_size == 0) return false;

    size_t out = 0;
    bool got_any = false;

    for(;;) {
        if(reader->pos >= reader->len) {
            uint16_t bytes_read =
                storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
            if(bytes_read == 0) break; // EOF
            reader->len = bytes_read;
            reader->pos = 0;
        }

        got_any = true;
        char c = reader->buffer[reader->pos++];

        if(c == '\n') {
            line[out] = '\0';
            if(out > 0 && line[out - 1] == '\r') line[out - 1] = '\0';
            return true;
        }

        if(out < line_size - 1) {
            line[out++] = c;
        }
        // Characters beyond line_size - 1 are consumed but dropped
    }

    // EOF: return final line if the file didn't end with a newline
    line[out] = '\0';
    if(out > 0 && line[out - 1] == '\r') line[out - 1] = '\0';
    return got_any;
}

// Split CSV line into fields
int string_split_csv(char* line, char* fields[], int max_fields) {
    if(!line || !fields || max_fields == 0) return 0;

    int field_count = 0;
    char* ptr = line;

    while(*ptr != '\0' && field_count < max_fields) {
        // Skip leading whitespace
        while(*ptr == ' ' || *ptr == '\t')
            ptr++;

        if(*ptr == '\0') break;

        fields[field_count] = ptr;
        field_count++;

        // Find next comma or end of line
        while(*ptr != '\0' && *ptr != ',') {
            ptr++;
        }

        // Null-terminate field
        if(*ptr == ',') {
            *ptr = '\0';
            ptr++;
        }
    }

    return field_count;
}
