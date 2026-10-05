#include "dnd_settings.h"
#include "dnd_fs.h"
#include "dnd_profile_handoff.h"

#include <stdio.h>
#include <string.h>

#define DND_SETTINGS_PATH        DND_CHARACTER_DATA_ROOT "/settings.txt"
#define DND_SETTINGS_TEMP_PATH   DND_CHARACTER_DATA_ROOT "/settings.tmp"
#define DND_SETTINGS_BACKUP_PATH DND_CHARACTER_DATA_ROOT "/settings.bak"

static bool dnd_settings_write_value_line(File* file, const char* key, uint8_t value) {
    char line[40];
    int length = snprintf(line, sizeof(line), "%s=%u\n", key, (unsigned int)value);
    return length > 0 && (size_t)length < sizeof(line) &&
           storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static bool dnd_settings_write_line(File* file, const char* key, uint8_t value) {
    char line[40];
    int length = snprintf(line, sizeof(line), "%s=%u\n", key, value ? 1U : 0U);
    return length > 0 && (size_t)length < sizeof(line) &&
           storage_file_write(file, line, (size_t)length) == (size_t)length;
}

void dnd_settings_defaults(DndSettings* settings) {
    if(!settings) return;
    settings->skip_dice_loading = 0U;
    settings->debug = 0U;
    settings->extra_items = 1U;
    settings->catalog_all = 0U;
    settings->homebrew = 1U;
    settings->menu_type = DndMenuTypeText;
}

void dnd_settings_shared_defaults(DndSharedSettings* settings) {
    if(!settings) return;
    settings->debug = 0U;
    settings->homebrew = 1U;
}

static bool dnd_settings_apply_bool(const char* line, const char* key, uint8_t* target) {
    if(!line || !key || !target) return false;
    size_t key_length = strlen(key);
    size_t line_length = strlen(line);
    if(line_length < key_length + 1U || strncmp(line, key, key_length) != 0 ||
       line[key_length] != '=')
        return false;
    if(line_length != key_length + 2U) return true;
    char value = line[key_length + 1U];
    if(value != '0' && value != '1') return true;
    *target = value == '1' ? 1U : 0U;
    return true;
}

bool dnd_settings_load_shared(Storage* storage, DndSharedSettings* settings) {
    if(!storage || !settings) return false;
    dnd_settings_shared_defaults(settings);
    if(!storage_file_exists(storage, DND_SETTINGS_PATH)) return true;

    File* file = storage_file_alloc(storage);
    if(!file) return true;
    if(!storage_file_open(file, DND_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return true;
    }

    /* Child FAPs currently need only Debug and Homebrew. Keep this projection
       deliberately tiny and stop as soon as both complete lines are recovered. */
    enum {
        DndSharedFoundDebug = 1U << 0,
        DndSharedFoundHomebrew = 1U << 1,
        DndSharedFoundAll = DndSharedFoundDebug | DndSharedFoundHomebrew,
    };
    char line[32];
    size_t used = 0U;
    bool overflow = false;
    uint8_t found = 0U;
    uint8_t buffer[64];
    while(found != DndSharedFoundAll) {
        size_t count = storage_file_read(file, buffer, sizeof(buffer));
        if(!count) break;
        for(size_t i = 0U; i < count; ++i) {
            char ch = (char)buffer[i];
            if(ch == '\r') continue;
            if(ch == '\n') {
                if(!overflow) {
                    line[used] = '\0';
                    if(dnd_settings_apply_bool(line, "Debug", &settings->debug))
                        found |= DndSharedFoundDebug;
                    else if(dnd_settings_apply_bool(line, "Homebrew", &settings->homebrew))
                        found |= DndSharedFoundHomebrew;
                }
                used = 0U;
                overflow = false;
                if(found == DndSharedFoundAll) break;
                continue;
            }
            if(overflow) continue;
            if(used + 1U < sizeof(line))
                line[used++] = ch;
            else
                overflow = true;
        }
    }
    if(found != DndSharedFoundAll && used && !overflow && storage_file_get_error(file) == FSE_OK) {
        line[used] = '\0';
        if(!(found & DndSharedFoundDebug) &&
           dnd_settings_apply_bool(line, "Debug", &settings->debug))
            found |= DndSharedFoundDebug;
        else if(
            !(found & DndSharedFoundHomebrew) &&
            dnd_settings_apply_bool(line, "Homebrew", &settings->homebrew))
            found |= DndSharedFoundHomebrew;
    }
    storage_file_close(file);
    storage_file_free(file);
    return true;
}

static bool dnd_settings_apply_menu_type(const char* line, uint8_t* target) {
    if(!line || !target) return false;
    const char* key = "MenuType";
    size_t key_length = strlen(key);
    size_t line_length = strlen(line);
    if(line_length < key_length + 1U || strncmp(line, key, key_length) != 0 ||
       line[key_length] != '=')
        return false;
    const char* value = line + key_length + 1U;
    *target = strcmp(value, "1") == 0 ? DndMenuTypeGraphical : DndMenuTypeText;
    return true;
}

static void dnd_settings_apply_line(DndSettings* settings, const char* line) {
    if(!settings || !line) return;
    if(dnd_settings_apply_bool(line, "SkipDiceLoading", &settings->skip_dice_loading)) return;
    if(dnd_settings_apply_bool(line, "Debug", &settings->debug)) return;
    if(dnd_settings_apply_bool(line, "ExtraItems", &settings->extra_items)) return;
    if(dnd_settings_apply_bool(line, "CatalogAll", &settings->catalog_all)) return;
    if(dnd_settings_apply_bool(line, "Homebrew", &settings->homebrew)) return;
    (void)dnd_settings_apply_menu_type(line, &settings->menu_type);
}

bool dnd_settings_load(Storage* storage, DndSettings* settings) {
    if(!storage || !settings) return false;
    dnd_settings_defaults(settings);
    if(!storage_file_exists(storage, DND_SETTINGS_PATH)) return true;

    File* file = storage_file_alloc(storage);
    if(!file) return true;
    if(!storage_file_open(file, DND_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return true;
    }

    char line[64];
    size_t used = 0U;
    bool overflow = false;
    uint8_t buffer[128];
    while(true) {
        size_t count = storage_file_read(file, buffer, sizeof(buffer));
        if(!count) break;
        for(size_t i = 0U; i < count; ++i) {
            char ch = (char)buffer[i];
            if(ch == '\r') continue;
            if(ch == '\n') {
                if(!overflow) {
                    line[used] = '\0';
                    dnd_settings_apply_line(settings, line);
                }
                used = 0U;
                overflow = false;
                continue;
            }
            if(overflow) continue;
            if(used + 1U < sizeof(line))
                line[used++] = ch;
            else
                overflow = true;
        }
    }
    bool read_complete = storage_file_get_error(file) == FSE_OK;
    if(used && !overflow && read_complete) {
        line[used] = '\0';
        dnd_settings_apply_line(settings, line);
    }
    storage_file_close(file);
    storage_file_free(file);
    return true;
}

bool dnd_settings_save(Storage* storage, const DndSettings* settings) {
    if(!storage || !settings) return false;
    if(!dnd_fs_ensure_directory(storage, DND_CHARACTER_DATA_ROOT)) return false;

    storage_common_remove(storage, DND_SETTINGS_TEMP_PATH);
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = storage_file_open(file, DND_SETTINGS_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              dnd_settings_write_line(file, "SkipDiceLoading", settings->skip_dice_loading) &&
              dnd_settings_write_line(file, "Debug", settings->debug) &&
              dnd_settings_write_line(file, "ExtraItems", settings->extra_items) &&
              dnd_settings_write_line(file, "CatalogAll", settings->catalog_all) &&
              dnd_settings_write_line(file, "Homebrew", settings->homebrew) &&
              dnd_settings_write_value_line(file, "MenuType", settings->menu_type) &&
              storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        storage_common_remove(storage, DND_SETTINGS_TEMP_PATH);
        return false;
    }

    if(storage_file_exists(storage, DND_SETTINGS_BACKUP_PATH) &&
       storage_common_remove(storage, DND_SETTINGS_BACKUP_PATH) != FSE_OK) {
        storage_common_remove(storage, DND_SETTINGS_TEMP_PATH);
        return false;
    }
    bool had_live = storage_file_exists(storage, DND_SETTINGS_PATH);
    if(had_live &&
       storage_common_rename(storage, DND_SETTINGS_PATH, DND_SETTINGS_BACKUP_PATH) != FSE_OK) {
        storage_common_remove(storage, DND_SETTINGS_TEMP_PATH);
        return false;
    }
    if(storage_common_rename(storage, DND_SETTINGS_TEMP_PATH, DND_SETTINGS_PATH) == FSE_OK) {
        if(had_live) storage_common_remove(storage, DND_SETTINGS_BACKUP_PATH);
        return true;
    }
    if(had_live) (void)storage_common_rename(storage, DND_SETTINGS_BACKUP_PATH, DND_SETTINGS_PATH);
    storage_common_remove(storage, DND_SETTINGS_TEMP_PATH);
    return false;
}
