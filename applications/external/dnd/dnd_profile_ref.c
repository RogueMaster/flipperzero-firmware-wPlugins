#include "dnd_profile_ref.h"
#include "dnd_fs.h"
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


static bool dnd_profile_ref_filename_is_primary(const char* filename, const char* prefix, size_t prefix_length) {
    if(!filename || !prefix) return false;
    size_t length = strlen(filename);
    if(length < prefix_length + 6U || strncmp(filename, prefix, prefix_length) != 0 ||
       strcmp(filename + length - 4U, ".txt") != 0)
        return false;
    /* Primary profile names end in _<level>.txt. Sidecars do not. */
    const char* extension = filename + length - 4U;
    const char* level = extension;
    while(level > filename && level[-1] != '_') --level;
    if(level <= filename || level >= extension) return false;
    for(const char* p = level; p < extension; ++p)
        if(*p < '0' || *p > '9') return false;
    return true;
}


static bool dnd_profile_ref_filename_id(const char* filename, uint32_t* output) {
    if(!filename || !output || strncmp(filename, "ch_", 3U) != 0) return false;
    const char* id_begin = filename + 3U;
    const char* id_end = strchr(id_begin, '_');
    if(!id_end || id_end == id_begin) return false;
    char text[16];
    size_t length = (size_t)(id_end - id_begin);
    if(length >= sizeof(text)) return false;
    memcpy(text, id_begin, length);
    text[length] = '\0';
    return dnd_profile_ref_parse_u32(text, output);
}

static bool dnd_profile_ref_fallback(Storage* storage, uint32_t after, bool have_after, uint32_t* profile) {
    if(!storage || !profile) return false;
    File* directory = storage_file_alloc(storage);
    if(!directory) return false;
    if(!storage_dir_open(directory, POCKET_D20_CHARACTER_DATA_ROOT)) {
        storage_file_free(directory);
        return false;
    }
    bool have_first = false, have_next = false;
    uint32_t first = UINT32_MAX, next = UINT32_MAX;
    FileInfo info;
    char filename[128];
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info)) continue;
        uint32_t id = 0U;
        char prefix[32];
        if(!dnd_profile_ref_filename_id(filename, &id)) continue;
        int prefix_length = snprintf(prefix, sizeof(prefix), "ch_%lu_", (unsigned long)id);
        if(prefix_length <= 0 || (size_t)prefix_length >= sizeof(prefix)) continue;
        if(!dnd_profile_ref_filename_is_primary(filename, prefix, (size_t)prefix_length)) continue;
        if(!have_first || id < first) {
            first = id;
            have_first = true;
        }
        if(have_after && id > after && (!have_next || id < next)) {
            next = id;
            have_next = true;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(have_next) {
        *profile = next;
        return true;
    }
    if(have_first) {
        *profile = first;
        return true;
    }
    return false;
}

bool dnd_profile_ref_path(Storage* storage, uint32_t profile, char* output, size_t size) {
    if(!storage || !output || !size) return false;
    output[0] = '\0';
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
            if(!dnd_profile_ref_filename_is_primary(filename, prefix, (size_t)prefix_length)) continue;
            if(pocket_d20_child_path(output, size, POCKET_D20_CHARACTER_DATA_ROOT, NULL, filename)) {
                found = true;
                break;
            }
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return found;
}

static bool dnd_profile_ref_primary_exists(Storage* storage, uint32_t profile) {
    char path[160];
    return dnd_profile_ref_path(storage, profile, path, sizeof(path));
}
bool dnd_profile_ref_active(Storage* storage, uint32_t* profile) {
    if(!storage || !profile) return false;

    bool found = false;
    bool io_ok = true;
    File* file = storage_file_alloc(storage);
    if(file && storage_file_open(file, DND_ACTIVE_PROFILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
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
                        if(strcmp(line, "Active") == 0 &&
                           dnd_profile_ref_parse_u32(equal, &parsed)) {
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
                    if(strcmp(line, "Active") == 0 &&
                       dnd_profile_ref_parse_u32(equal, &parsed)) {
                        *profile = parsed;
                        found = true;
                    }
                }
                used = 0U;
                continue;
            }
            if(used + 1U < sizeof(line)) line[used++] = ch;
        }
        io_ok = storage_file_get_error(file) == FSE_OK;
        storage_file_close(file);
    }
    if(file) storage_file_free(file);

    if(io_ok && found && dnd_profile_ref_primary_exists(storage, *profile)) return true;
    uint32_t after = found ? *profile : 0U;
    return dnd_profile_ref_fallback(storage, after, found, profile);
}
