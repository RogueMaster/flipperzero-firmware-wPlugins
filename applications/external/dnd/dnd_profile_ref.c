#include "dnd_profile_ref.h"
#include "dnd_handoff.h"

#include <stdio.h>
#include <string.h>

#define DND_ACTIVE_PROFILE_PATH "/ext/apps_data/dndolphins/custom_active_profile.txt"

static bool dnd_profile_ref_parse_u32(const char* text, uint32_t* output) {
    if(!text || !*text || !output) return false;
    uint32_t value = 0U;
    const char* cursor = text;
    while(*cursor >= '0' && *cursor <= '9') {
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(value > (UINT32_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
        ++cursor;
    }
    if(cursor == text) return false;
    while(*cursor == '\r' || *cursor == '\n' || *cursor == ' ' || *cursor == '\t') ++cursor;
    if(*cursor) return false;
    *output = value;
    return true;
}


static bool dnd_profile_ref_primary_exists(Storage* storage, uint32_t profile) {
    File* directory = storage_file_alloc(storage);
    if(!directory) return false;
    if(!storage_dir_open(directory, POCKET_D20_CHARACTER_DATA_ROOT)) {
        storage_file_free(directory);
        return false;
    }
    char prefix[32];
    int prefix_length = snprintf(prefix, sizeof(prefix), "ch_%lu_", (unsigned long)profile);
    bool found = false;
    if(prefix_length > 0 && (size_t)prefix_length < sizeof(prefix)) {
        FileInfo info;
        char filename[128];
        while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
            if(file_info_is_dir(&info)) continue;
            size_t length = strlen(filename);
            if(length >= 5U && !strncmp(filename, prefix, (size_t)prefix_length) &&
               !strcmp(filename + length - 4U, ".txt")) {
                found = true;
                break;
            }
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return found;
}
bool dnd_profile_ref_active(Storage* storage, uint32_t* profile) {
    if(!storage || !profile) return false;

    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, DND_ACTIVE_PROFILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    bool found = false;
    char line[96];
    size_t used = 0U;
    while(true) {
        char ch = '\0';
        size_t count = storage_file_read(file, &ch, 1U);
        if(count != 1U) {
            if(used) {
                line[used] = '\0';
                char* equal = strchr(line, '=');
                if(equal) {
                    *equal++ = '\0';
                    uint32_t parsed = 0U;
                    if(strcmp(line, "Active") == 0 && dnd_profile_ref_parse_u32(equal, &parsed)) {
                        *profile = parsed;
                        found = true;
                    }
                }
            }
            break;
        }
        if(ch == '\r') continue;
        if(ch == '\n') {
            line[used] = '\0';
            char* equal = strchr(line, '=');
            if(equal) {
                *equal++ = '\0';
                uint32_t parsed = 0U;
                if(strcmp(line, "Active") == 0 && dnd_profile_ref_parse_u32(equal, &parsed)) {
                    *profile = parsed;
                    found = true;
                }
            }
            used = 0U;
            continue;
        }
        if(used + 1U < sizeof(line)) line[used++] = ch;
    }

    bool io_ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    return io_ok && found && dnd_profile_ref_primary_exists(storage, *profile);
}
