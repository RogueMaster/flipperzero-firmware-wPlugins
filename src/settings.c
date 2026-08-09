#include "settings.h"

#include <flipper_format/flipper_format.h>
#include <furi.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SETTINGS_FILETYPE "Internet Time Screen"
#define SETTINGS_VERSION  1

void settings_init_defaults(AppSettings* settings) {
    furi_check(settings);
    settings->utc_offset_minutes = 0;
    settings->loaded = false;
}

uint8_t settings_offset_to_index(int16_t utc_offset_minutes) {
    if(!internet_time_offset_valid(utc_offset_minutes)) {
        utc_offset_minutes = 0;
    }
    return (uint8_t)((utc_offset_minutes - INTERNET_TIME_OFFSET_MIN_MINUTES) /
                     INTERNET_TIME_OFFSET_STEP_MINUTES);
}

int16_t settings_index_to_offset(uint8_t index) {
    if(index >= SETTINGS_OFFSET_COUNT) {
        index = settings_offset_to_index(0);
    }
    return (int16_t)(INTERNET_TIME_OFFSET_MIN_MINUTES +
                     ((int16_t)index * INTERNET_TIME_OFFSET_STEP_MINUTES));
}

void settings_format_offset(int16_t utc_offset_minutes, char* out, size_t out_size) {
    furi_check(out);
    furi_check(out_size >= 10);

    const char sign = (utc_offset_minutes < 0) ? '-' : '+';
    int16_t abs_minutes = (utc_offset_minutes < 0) ? (int16_t)(-utc_offset_minutes) :
                                                     utc_offset_minutes;
    unsigned hours = (unsigned)(abs_minutes / 60);
    unsigned mins = (unsigned)(abs_minutes % 60);
    snprintf(out, out_size, "UTC%c%02u:%02u", sign, hours, mins);
}

bool settings_load(Storage* storage, AppSettings* settings) {
    furi_check(storage);
    furi_check(settings);

    settings_init_defaults(settings);

    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* filetype = furi_string_alloc();
    bool ok = false;
    uint32_t version = 0;
    int32_t offset = 0;

    do {
        if(!flipper_format_file_open_existing(file, SETTINGS_CONFIG_PATH)) break;
        if(!flipper_format_read_header(file, filetype, &version)) break;
        if(furi_string_cmp_str(filetype, SETTINGS_FILETYPE) != 0) break;
        if(version != SETTINGS_VERSION) break;
        if(!flipper_format_read_int32(file, "UTC offset minutes", &offset, 1)) break;
        if(offset < INT16_MIN || offset > INT16_MAX) break;
        if(!internet_time_offset_valid((int16_t)offset)) break;

        settings->utc_offset_minutes = (int16_t)offset;
        settings->loaded = true;
        ok = true;
    } while(false);

    furi_string_free(filetype);
    flipper_format_free(file);
    return ok;
}

bool settings_save(Storage* storage, const AppSettings* settings) {
    furi_check(storage);
    furi_check(settings);

    FlipperFormat* file = flipper_format_file_alloc(storage);
    bool ok = false;
    int32_t offset = settings->utc_offset_minutes;

    do {
        if(!storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX)) {
            /* Directory may already exist; continue and try write. */
        }
        if(!flipper_format_file_open_always(file, SETTINGS_CONFIG_PATH)) break;
        if(!flipper_format_write_header_cstr(file, SETTINGS_FILETYPE, SETTINGS_VERSION)) break;
        if(!flipper_format_write_int32(file, "UTC offset minutes", &offset, 1)) break;
        ok = true;
    } while(false);

    flipper_format_free(file);
    return ok;
}
