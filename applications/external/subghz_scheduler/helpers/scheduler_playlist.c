#include "scheduler_playlist.h"

#include <flipper_format/flipper_format.h>
#include <string.h>
#include <limits.h>

static bool playlist_parse_wait_seconds(const char* text, uint32_t* seconds) {
    while(*text == ' ' || *text == '\t')
        text++;
    if(*text < '0' || *text > '9') return false;

    uint32_t value = 0;
    do {
        const uint32_t digit = *text++ - '0';
        if(value > (UINT32_MAX - digit) / 10) return false;
        value = value * 10 + digit;
    } while(*text >= '0' && *text <= '9');

    while(*text == ' ' || *text == '\t')
        text++;
    if(*text || value > SCHEDULER_PLAYLIST_MAX_WAIT_SECONDS) return false;
    *seconds = value;
    return true;
}

static bool playlist_parse_line(
    char* line,
    SchedulerPlaylistTiming* timing,
    uint32_t* pending,
    bool* waiting_for_sub) {
    while(*line == ' ' || *line == '\t')
        line++;
    if(!strncmp(line, "time:", 5)) {
        if(!playlist_parse_wait_seconds(line + 5, pending)) return false;
        *waiting_for_sub = true;
        timing->has_timing = true;
    } else if(!strncmp(line, "sub:", 4)) {
        line += 4;
        while(*line == ' ' || *line == '\t')
            line++;
        if(!*line || timing->count == SCHEDULER_MAX_LINES) return false;
        timing->wait_seconds[timing->count++] = *pending;
        *pending = 0;
        *waiting_for_sub = false;
    }
    return true;
}

bool scheduler_playlist_read_timing(
    Storage* storage,
    const char* playlist_path,
    SchedulerPlaylistTiming* timing) {
    memset(timing, 0, sizeof(*timing));
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, playlist_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    char line[512];
    size_t length = 0;
    uint32_t pending = 0;
    bool waiting_for_sub = false;
    bool valid = true;
    uint8_t byte;
    while(storage_file_read(file, &byte, 1) == 1) {
        if(byte == '\n') {
            line[length] = '\0';
            valid = playlist_parse_line(line, timing, &pending, &waiting_for_sub);
            length = 0;
            if(!valid) break;
        } else if(byte != '\r') {
            if(length == sizeof(line) - 1) {
                valid = false;
                break;
            }
            line[length++] = byte;
        }
    }
    if(valid && length) {
        line[length] = '\0';
        valid = playlist_parse_line(line, timing, &pending, &waiting_for_sub);
    }
    if(timing->count == 0 || waiting_for_sub) valid = false;
    storage_file_close(file);
    storage_file_free(file);
    return valid;
}

int scheduler_playlist_count(Storage* storage, const char* playlist_path) {
    FlipperFormat* format = flipper_format_file_alloc(storage);
    FuriString* data = furi_string_alloc();
    int count = -1;

    if(flipper_format_file_open_existing(format, playlist_path)) {
        count = 0;
        while(flipper_format_read_string(format, "sub", data)) {
            ++count;
        }
    }

    flipper_format_file_close(format);
    flipper_format_free(format);
    furi_string_free(data);
    return count;
}
