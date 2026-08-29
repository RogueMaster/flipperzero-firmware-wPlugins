#include "dnd_fs.h"
#include "dnd_handoff.h"
#include "dnd_profile_ref.h"

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "DndJournal"
#define JOURNAL_NAME_LEN 32U
#define JOURNAL_BODY_LEN 192U
#define JOURNAL_FILE_LEN 48U
#define JOURNAL_PATH_LEN 128U
#define JOURNAL_CACHE_SIZE 8U
#define JOURNAL_READ_BUFFER 256U
#define JOURNAL_PROFILE_PATH APP_DATA_PATH("ch_%lu")

typedef enum {
    JournalCategoryQuick,
    JournalCategoryAdventure,
    JournalCategoryItem,
    JournalCategoryMilestone,
    JournalCategoryCount,
} JournalCategory;

typedef struct {
    char title[JOURNAL_NAME_LEN];
    char file_name[JOURNAL_FILE_LEN];
    uint8_t category;
    uint8_t completed;
    uint8_t level_granted;
    uint8_t class_index;
} JournalEntryMeta;

typedef struct {
    char title[JOURNAL_NAME_LEN];
    char body[JOURNAL_BODY_LEN];
    char file_name[JOURNAL_FILE_LEN];
    uint8_t category;
    uint8_t completed;
    uint8_t level_granted;
    uint8_t class_index;
} JournalEntry;

typedef enum {
    JournalViewMain,
    JournalViewText,
} JournalViewId;

typedef enum {
    JournalScreenList,
    JournalScreenDetail,
} JournalScreen;

typedef enum {
    JournalEditNone,
    JournalEditTitle,
    JournalEditBody,
} JournalEdit;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    View* view;
    TextInput* text_input;
    JournalEntryMeta entries[JOURNAL_CACHE_SIZE];
    JournalEntry current_entry;
    uint16_t count;
    uint16_t cache_start;
    uint16_t selection;
    uint16_t scroll;
    uint8_t cache_count;
    uint8_t current_loaded;
    uint8_t detail_field;
    uint8_t return_to_dnd;
    uint8_t launch_adventure;
    uint32_t profile;
    JournalScreen screen;
    JournalEdit edit;
    char edit_buffer[JOURNAL_BODY_LEN];
    char status[32];
} JournalApp;

static const char* const journal_category_names[JournalCategoryCount] = {
    "Quick",
    "Adventure",
    "Item",
    "Milestone",
};

static void journal_copy(char* destination, size_t size, const char* source) {
    if(!destination || !size) return;
    if(!source) source = "";
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

static bool journal_parse_u32(const char* text, uint32_t* output) {
    if(!text || !text[0] || !output) return false;
    uint32_t value = 0U;
    for(const char* p = text; *p; ++p) {
        if(*p < '0' || *p > '9') return false;
        uint32_t digit = (uint32_t)(*p - '0');
        if(value > UINT32_MAX / 10U ||
           (value == UINT32_MAX / 10U && digit > UINT32_MAX % 10U))
            return false;
        value = value * 10U + digit;
    }
    *output = value;
    return true;
}

static bool journal_writef(File* file, const char* format, ...) {
    char line[128];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if(length < 0 || (size_t)length >= sizeof(line)) return false;
    return storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static uint8_t journal_hex_value(char value) {
    if(value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if(value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
    if(value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    return 0xFFU;
}

static bool journal_write_string(File* file, const char* key, const char* value) {
    const size_t key_length = strlen(key);
    if(storage_file_write(file, key, key_length) != key_length ||
       storage_file_write(file, "=", 1U) != 1U)
        return false;
    static const char digits[] = "0123456789ABCDEF";
    char chunk[64];
    size_t used = 0U;
    for(size_t i = 0U; value[i]; ++i) {
        uint8_t byte = (uint8_t)value[i];
        bool escape = byte == '%' || byte == '\n' || byte == '\r' || byte < 0x20U;
        size_t needed = escape ? 3U : 1U;
        if(used + needed > sizeof(chunk)) {
            if(storage_file_write(file, chunk, used) != used) return false;
            used = 0U;
        }
        if(escape) {
            chunk[used++] = '%';
            chunk[used++] = digits[byte >> 4U];
            chunk[used++] = digits[byte & 0x0FU];
        } else {
            chunk[used++] = (char)byte;
        }
    }
    if(used && storage_file_write(file, chunk, used) != used) return false;
    return storage_file_write(file, "\n", 1U) == 1U;
}

typedef struct {
    File* file;
    uint8_t buffer[JOURNAL_READ_BUFFER];
    uint16_t position;
    uint16_t count;
} JournalReader;

static bool journal_reader_next(JournalReader* reader, char* value) {
    if(reader->position >= reader->count) {
        reader->count =
            (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
        reader->position = 0U;
        if(!reader->count) return false;
    }
    *value = (char)reader->buffer[reader->position++];
    return true;
}

static void journal_skip_field_value(JournalReader* reader);

static bool journal_read_field_key(JournalReader* reader, char* key, size_t key_size) {
    if(!reader || !key || key_size < 2U) return false;
    size_t used = 0U;
    bool overflow = false;
    char character = '\0';
    while(journal_reader_next(reader, &character)) {
        if(character == '\r') continue;
        if(character == '\n') {
            /* A malformed/unknown non-key line cannot stop the rest of the file. */
            used = 0U;
            overflow = false;
            continue;
        }
        if(character == '=') {
            if(used && !overflow) {
                key[used] = '\0';
                return true;
            }
            journal_skip_field_value(reader);
            used = 0U;
            overflow = false;
            continue;
        }
        if(used + 1U < key_size)
            key[used++] = character;
        else
            overflow = true;
    }
    key[0] = '\0';
    return false;
}

static bool journal_read_raw_field_value(
    JournalReader* reader,
    char* value,
    size_t value_size) {
    if(!reader || !value || !value_size) return false;
    size_t used = 0U;
    bool overflow = false;
    bool consumed = false;
    char character = '\0';
    while(journal_reader_next(reader, &character)) {
        if(character == '\n') break;
        if(character == '\r') continue;
        consumed = true;
        if(used + 1U < value_size)
            value[used++] = character;
        else
            overflow = true;
    }
    value[used] = '\0';
    return consumed && !overflow;
}

static bool journal_read_decoded_field_value(
    JournalReader* reader,
    char* destination,
    size_t destination_size) {
    if(!reader || !destination || !destination_size) return false;
    size_t output = 0U;
    bool overflow = false;
    char character = '\0';
    while(journal_reader_next(reader, &character)) {
        if(character == '\n') break;
        if(character == '\r') continue;
        uint8_t byte = (uint8_t)character;
        if(character == '%') {
            char high_char = '\0';
            char low_char = '\0';
            if(!journal_reader_next(reader, &high_char) || !journal_reader_next(reader, &low_char))
                break;
            uint8_t high = journal_hex_value(high_char);
            uint8_t low = journal_hex_value(low_char);
            if(high != 0xFFU && low != 0xFFU) {
                byte = (uint8_t)((high << 4U) | low);
            } else {
                byte = (uint8_t)'%';
                /* Invalid escapes are preserved best-effort rather than failing the entry. */
            }
        }
        if(output + 1U < destination_size)
            destination[output++] = (char)byte;
        else
            overflow = true;
    }
    destination[output] = '\0';
    return !overflow;
}

static void journal_skip_field_value(JournalReader* reader) {
    char character = '\0';
    while(journal_reader_next(reader, &character))
        if(character == '\n') break;
}

static bool journal_profile_dir(char* output, size_t size, uint32_t profile) {
    int length = snprintf(output, size, JOURNAL_PROFILE_PATH, (unsigned long)profile);
    return length > 0 && (size_t)length < size;
}

static bool journal_entry_path(
    char* output,
    size_t size,
    uint32_t profile,
    const char* file_name) {
    if(!file_name || !file_name[0] || strchr(file_name, '/') || strchr(file_name, '\\'))
        return false;
    char directory[JOURNAL_PATH_LEN];
    if(!journal_profile_dir(directory, sizeof(directory), profile)) return false;
    int length = snprintf(output, size, "%s/%s", directory, file_name);
    return length > 0 && (size_t)length < size;
}

static bool journal_file_name_valid(const char* file_name) {
    if(!file_name) return false;
    size_t length = strlen(file_name);
    return length >= 5U && length < JOURNAL_FILE_LEN && !strchr(file_name, '/') &&
           !strchr(file_name, '\\') && strcmp(file_name + length - 4U, ".txt") == 0;
}

static bool journal_directory_exists(Storage* storage, const char* path) {
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK && file_info_is_dir(&info);
}

static bool journal_ensure_directory(Storage* storage, uint32_t profile) {
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    char directory[JOURNAL_PATH_LEN];
    if(!journal_profile_dir(directory, sizeof(directory), profile)) return false;
    storage_common_mkdir(storage, directory);
    return journal_directory_exists(storage, directory);
}

static bool journal_publish_temp(
    Storage* storage,
    const char* temporary,
    const char* destination,
    const char* backup) {
    bool had_destination = storage_file_exists(storage, destination);
    if(had_destination) {
        if(storage_file_exists(storage, backup)) storage_common_remove(storage, backup);
        if(storage_common_rename(storage, destination, backup) != FSE_OK) return false;
    }
    if(storage_common_rename(storage, temporary, destination) == FSE_OK) {
        if(had_destination) storage_common_remove(storage, backup);
        return true;
    }
    if(had_destination) storage_common_rename(storage, backup, destination);
    storage_common_remove(storage, temporary);
    return false;
}

static bool journal_read_entry(
    Storage* storage,
    uint32_t profile,
    const char* file_name,
    JournalEntry* entry) {
    if(!journal_file_name_valid(file_name) || !entry) return false;
    char path[JOURNAL_PATH_LEN];
    if(!journal_entry_path(path, sizeof(path), profile, file_name)) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    JournalReader reader = {.file = file};
    JournalEntry parsed;
    memset(&parsed, 0, sizeof(parsed));
    parsed.category = JournalCategoryQuick;
    journal_copy(parsed.title, sizeof(parsed.title), "Journal Entry");
    char key[32];
    char value[48];
    bool recognized = false;
    while(journal_read_field_key(&reader, key, sizeof(key))) {
        if(!strcmp(key, "Title")) {
            if(journal_read_decoded_field_value(&reader, parsed.title, sizeof(parsed.title)))
                recognized = true;
        } else if(!strcmp(key, "Body")) {
            if(journal_read_decoded_field_value(&reader, parsed.body, sizeof(parsed.body)))
                recognized = true;
        } else if(!strcmp(key, "Category") || !strcmp(key, "Completed") ||
                  !strcmp(key, "LevelGranted") || !strcmp(key, "ClassIndex")) {
            if(journal_read_raw_field_value(&reader, value, sizeof(value))) {
                uint32_t number = 0U;
                if(journal_parse_u32(value, &number)) {
                    if(!strcmp(key, "Category") && number < JournalCategoryCount)
                        parsed.category = (uint8_t)number;
                    else if(!strcmp(key, "Completed") && number <= 1U)
                        parsed.completed = (uint8_t)number;
                    else if(!strcmp(key, "LevelGranted") && number <= 20U)
                        parsed.level_granted = (uint8_t)number;
                    else if(!strcmp(key, "ClassIndex") && number < 4U)
                        parsed.class_index = (uint8_t)number;
                    else
                        continue;
                    recognized = true;
                }
            }
        } else {
            /* Version, CharacterId, End and unknown future fields are informational. */
            journal_skip_field_value(&reader);
        }
    }
    bool io_ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    if(!io_ok || !recognized) return false;
    journal_copy(parsed.file_name, sizeof(parsed.file_name), file_name);
    *entry = parsed;
    return true;
}

static bool journal_read_metadata(
    Storage* storage,
    uint32_t profile,
    const char* file_name,
    JournalEntryMeta* entry) {
    if(!journal_file_name_valid(file_name) || !entry) return false;
    char path[JOURNAL_PATH_LEN];
    if(!journal_entry_path(path, sizeof(path), profile, file_name)) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    JournalReader reader = {.file = file};
    JournalEntryMeta parsed;
    memset(&parsed, 0, sizeof(parsed));
    parsed.category = JournalCategoryQuick;
    journal_copy(parsed.title, sizeof(parsed.title), "Journal Entry");
    char key[32];
    char value[48];
    bool recognized = false;
    while(journal_read_field_key(&reader, key, sizeof(key))) {
        if(!strcmp(key, "Title")) {
            if(journal_read_decoded_field_value(&reader, parsed.title, sizeof(parsed.title)))
                recognized = true;
        } else if(!strcmp(key, "Body")) {
            journal_skip_field_value(&reader);
        } else if(!strcmp(key, "Category") || !strcmp(key, "Completed") ||
                  !strcmp(key, "LevelGranted") || !strcmp(key, "ClassIndex")) {
            if(journal_read_raw_field_value(&reader, value, sizeof(value))) {
                uint32_t number = 0U;
                if(journal_parse_u32(value, &number)) {
                    if(!strcmp(key, "Category") && number < JournalCategoryCount)
                        parsed.category = (uint8_t)number;
                    else if(!strcmp(key, "Completed") && number <= 1U)
                        parsed.completed = (uint8_t)number;
                    else if(!strcmp(key, "LevelGranted") && number <= 20U)
                        parsed.level_granted = (uint8_t)number;
                    else if(!strcmp(key, "ClassIndex") && number < 4U)
                        parsed.class_index = (uint8_t)number;
                    else
                        continue;
                    recognized = true;
                }
            }
        } else {
            journal_skip_field_value(&reader);
        }
    }
    bool io_ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    if(!io_ok || !recognized) return false;
    journal_copy(parsed.file_name, sizeof(parsed.file_name), file_name);
    *entry = parsed;
    return true;
}

static void journal_insert_descending(JournalApp* app, const JournalEntryMeta* entry) {
    if(app->cache_count < JOURNAL_CACHE_SIZE) {
        app->entries[app->cache_count++] = *entry;
    } else {
        if(strcmp(entry->file_name, app->entries[app->cache_count - 1U].file_name) <= 0) return;
        app->entries[app->cache_count - 1U] = *entry;
    }
    uint8_t position = (uint8_t)(app->cache_count - 1U);
    while(position > 0U &&
          strcmp(app->entries[position - 1U].file_name, app->entries[position].file_name) < 0) {
        JournalEntryMeta swap = app->entries[position - 1U];
        app->entries[position - 1U] = app->entries[position];
        app->entries[position] = swap;
        --position;
    }
}

static void journal_insert_oldest(JournalApp* app, const JournalEntryMeta* entry) {
    if(app->cache_count < JOURNAL_CACHE_SIZE) {
        app->entries[app->cache_count++] = *entry;
        uint8_t position = (uint8_t)(app->cache_count - 1U);
        while(position > 0U &&
              strcmp(app->entries[position - 1U].file_name, app->entries[position].file_name) < 0) {
            JournalEntryMeta swap = app->entries[position - 1U];
            app->entries[position - 1U] = app->entries[position];
            app->entries[position] = swap;
            --position;
        }
        return;
    }
    if(strcmp(entry->file_name, app->entries[0].file_name) >= 0) return;
    app->entries[0] = *entry;
    uint8_t position = 0U;
    while(position + 1U < app->cache_count &&
          strcmp(app->entries[position].file_name, app->entries[position + 1U].file_name) < 0) {
        JournalEntryMeta swap = app->entries[position + 1U];
        app->entries[position + 1U] = app->entries[position];
        app->entries[position] = swap;
        ++position;
    }
}

static void journal_hydrate_cache(JournalApp* app) {
    if(!app) return;
    for(uint8_t i = 0U; i < app->cache_count; ++i) {
        char file_name[JOURNAL_FILE_LEN];
        journal_copy(file_name, sizeof(file_name), app->entries[i].file_name);
        JournalEntryMeta parsed;
        if(journal_read_metadata(app->storage, app->profile, file_name, &parsed)) {
            app->entries[i] = parsed;
        } else {
            memset(&app->entries[i], 0, sizeof(app->entries[i]));
            journal_copy(app->entries[i].file_name, sizeof(app->entries[i].file_name), file_name);
            journal_copy(app->entries[i].title, sizeof(app->entries[i].title), "Unreadable entry");
            app->entries[i].category = JournalCategoryQuick;
        }
    }
}

static bool journal_scan_cache(
    JournalApp* app,
    const char* boundary,
    bool newer,
    uint16_t anchor) {
    char directory_path[JOURNAL_PATH_LEN];
    if(!journal_profile_dir(directory_path, sizeof(directory_path), app->profile)) return false;
    File* directory = storage_file_alloc(app->storage);
    if(!directory) return false;
    if(!storage_dir_open(directory, directory_path)) {
        storage_file_free(directory);
        app->cache_count = 0U;
        return false;
    }
    app->cache_count = 0U;
    FileInfo info;
    char file_name[JOURNAL_FILE_LEN];
    while(storage_dir_read(directory, &info, file_name, sizeof(file_name))) {
        if(file_info_is_dir(&info) || !journal_file_name_valid(file_name)) continue;
        if(boundary) {
            int compare = strcmp(file_name, boundary);
            if(newer ? compare <= 0 : compare >= 0) continue;
        }
        JournalEntryMeta parsed;
        memset(&parsed, 0, sizeof(parsed));
        journal_copy(parsed.file_name, sizeof(parsed.file_name), file_name);
        if(newer)
            journal_insert_oldest(app, &parsed);
        else
            journal_insert_descending(app, &parsed);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(!app->cache_count) return false;
    journal_hydrate_cache(app);
    app->cache_start = newer ?
                           (anchor >= app->cache_count ? (uint16_t)(anchor - app->cache_count) : 0U) :
                           anchor;
    return true;
}

static bool journal_load(JournalApp* app) {
    app->count = 0U;
    app->cache_start = 0U;
    app->cache_count = 0U;
    app->current_loaded = 0U;
    char directory_path[JOURNAL_PATH_LEN];
    if(!journal_profile_dir(directory_path, sizeof(directory_path), app->profile)) return false;
    File* directory = storage_file_alloc(app->storage);
    if(!directory) return false;
    if(!storage_dir_open(directory, directory_path)) {
        storage_file_free(directory);
        return true;
    }
    FileInfo info;
    char file_name[JOURNAL_FILE_LEN];
    bool ok = true;
    while(storage_dir_read(directory, &info, file_name, sizeof(file_name))) {
        if(file_info_is_dir(&info) || !journal_file_name_valid(file_name)) continue;
        if(app->count == UINT16_MAX - 1U) {
            ok = false;
            break;
        }
        ++app->count;
        JournalEntryMeta parsed;
        memset(&parsed, 0, sizeof(parsed));
        journal_copy(parsed.file_name, sizeof(parsed.file_name), file_name);
        journal_insert_descending(app, &parsed);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(app->cache_count) journal_hydrate_cache(app);
    return ok;
}

static const JournalEntryMeta* journal_entry_at(JournalApp* app, uint16_t index) {
    if(!app || index >= app->count) return NULL;
    if(!app->cache_count && !journal_load(app)) return NULL;
    uint16_t guard = 0U;
    while(index < app->cache_start && app->cache_count && guard++ < UINT16_MAX) {
        char boundary[JOURNAL_FILE_LEN];
        journal_copy(boundary, sizeof(boundary), app->entries[0].file_name);
        uint16_t old_start = app->cache_start;
        if(!journal_scan_cache(app, boundary, true, old_start) || app->cache_start >= old_start)
            return NULL;
    }
    guard = 0U;
    while(index >= (uint16_t)(app->cache_start + app->cache_count) &&
          app->cache_count && guard++ < UINT16_MAX) {
        char boundary[JOURNAL_FILE_LEN];
        journal_copy(boundary, sizeof(boundary), app->entries[app->cache_count - 1U].file_name);
        uint16_t next_start = (uint16_t)(app->cache_start + app->cache_count);
        if(!journal_scan_cache(app, boundary, false, next_start)) return NULL;
    }
    if(index < app->cache_start || index >= (uint16_t)(app->cache_start + app->cache_count))
        return NULL;
    return &app->entries[index - app->cache_start];
}

static bool journal_window(JournalApp* app, uint16_t start) {
    if(!app || start >= app->count) return false;
    if(app->cache_count && app->cache_start == start) return true;
    if(start == 0U) return journal_scan_cache(app, NULL, false, 0U);
    const JournalEntryMeta* previous = journal_entry_at(app, start - 1U);
    if(!previous) return false;
    char boundary[JOURNAL_FILE_LEN];
    journal_copy(boundary, sizeof(boundary), previous->file_name);
    return journal_scan_cache(app, boundary, false, start);
}

static uint16_t journal_index_of_file(JournalApp* app, const char* target) {
    char directory_path[JOURNAL_PATH_LEN];
    if(!target || !journal_profile_dir(directory_path, sizeof(directory_path), app->profile))
        return 0U;
    File* directory = storage_file_alloc(app->storage);
    if(!directory || !storage_dir_open(directory, directory_path)) {
        if(directory) storage_file_free(directory);
        return 0U;
    }
    uint16_t index = 0U;
    FileInfo info;
    char file_name[JOURNAL_FILE_LEN];
    while(storage_dir_read(directory, &info, file_name, sizeof(file_name))) {
        if(file_info_is_dir(&info) || !journal_file_name_valid(file_name) ||
           strcmp(file_name, target) <= 0)
            continue;
        if(index < UINT16_MAX) ++index;
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return index;
}

static bool journal_open_detail(JournalApp* app, uint16_t index) {
    const JournalEntryMeta* meta = journal_entry_at(app, index);
    if(!meta) return false;
    char file_name[JOURNAL_FILE_LEN];
    journal_copy(file_name, sizeof(file_name), meta->file_name);
    if(!journal_read_entry(app->storage, app->profile, file_name, &app->current_entry)) return false;
    app->selection = index;
    app->current_loaded = 1U;
    app->screen = JournalScreenDetail;
    app->detail_field = 0U;
    return true;
}

static void journal_update_cached_current(JournalApp* app) {
    if(!app || !app->current_loaded) return;
    for(uint8_t i = 0U; i < app->cache_count; ++i) {
        JournalEntryMeta* meta = &app->entries[i];
        if(strcmp(meta->file_name, app->current_entry.file_name) != 0) continue;
        journal_copy(meta->title, sizeof(meta->title), app->current_entry.title);
        meta->category = app->current_entry.category;
        meta->completed = app->current_entry.completed;
        meta->level_granted = app->current_entry.level_granted;
        meta->class_index = app->current_entry.class_index;
        break;
    }
}

static bool journal_save_entry(JournalApp* app, JournalEntry* entry) {
    if(!journal_file_name_valid(entry->file_name) || entry->category >= JournalCategoryCount)
        return false;
    char path[JOURNAL_PATH_LEN];
    char temporary[JOURNAL_PATH_LEN];
    char backup[JOURNAL_PATH_LEN];
    if(!journal_entry_path(path, sizeof(path), app->profile, entry->file_name)) return false;
    int temporary_length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    int backup_length = snprintf(backup, sizeof(backup), "%s.bak", path);
    if(temporary_length <= 0 || (size_t)temporary_length >= sizeof(temporary) ||
       backup_length <= 0 || (size_t)backup_length >= sizeof(backup) ||
       !journal_ensure_directory(app->storage, app->profile))
        return false;
    storage_common_remove(app->storage, temporary);
    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    bool ok = storage_file_open(file, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              journal_writef(file, "PocketD20Journal=1\n") &&
              journal_writef(file, "CharacterId=%lu\n", (unsigned long)app->profile) &&
              journal_write_string(file, "Title", entry->title) &&
              journal_write_string(file, "Body", entry->body) &&
              journal_writef(file, "Category=%u\n", entry->category) &&
              journal_writef(file, "Completed=%u\n", entry->completed ? 1U : 0U) &&
              journal_writef(file, "LevelGranted=%u\n", entry->level_granted) &&
              journal_writef(file, "ClassIndex=%u\n", entry->class_index) &&
              journal_writef(file, "End=OK\n") && storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        storage_common_remove(app->storage, temporary);
        return false;
    }
    return journal_publish_temp(app->storage, temporary, path, backup);
}

static bool journal_create_entry(JournalApp* app, JournalEntry* entry) {
    if(!journal_ensure_directory(app->storage, app->profile)) return false;
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    for(uint8_t suffix = 0U; suffix < 100U; ++suffix) {
        char file_name[JOURNAL_FILE_LEN];
        int length = snprintf(
            file_name,
            sizeof(file_name),
            "ch_%lu_%04u%02u%02u_%02u%02u%02u_%02u.txt",
            (unsigned long)app->profile,
            (unsigned int)now.year,
            (unsigned int)now.month,
            (unsigned int)now.day,
            (unsigned int)now.hour,
            (unsigned int)now.minute,
            (unsigned int)now.second,
            (unsigned int)suffix);
        if(length <= 0 || (size_t)length >= sizeof(file_name)) return false;
        char path[JOURNAL_PATH_LEN];
        if(!journal_entry_path(path, sizeof(path), app->profile, file_name)) return false;
        if(storage_file_exists(app->storage, path)) continue;
        journal_copy(entry->file_name, sizeof(entry->file_name), file_name);
        if(journal_save_entry(app, entry)) return true;
        entry->file_name[0] = '\0';
        return false;
    }
    return false;
}

static bool journal_delete_entry(JournalApp* app, const JournalEntry* entry) {
    char path[JOURNAL_PATH_LEN];
    if(!journal_entry_path(path, sizeof(path), app->profile, entry->file_name)) return false;
    return !storage_file_exists(app->storage, path) ||
           storage_common_remove(app->storage, path) == FSE_OK;
}

static void journal_set_status(JournalApp* app, const char* status) {
    journal_copy(app->status, sizeof(app->status), status);
}

static void journal_refresh(JournalApp* app) {
    if(app && app->view) view_commit_model(app->view, true);
}

static void journal_draw_header(Canvas* canvas, JournalApp* app, const char* title) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, title);
    if(app->status[0]) {
        uint16_t width = canvas_string_width(canvas, app->status);
        if(width < 62U) canvas_draw_str(canvas, 126U - width, 8, app->status);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void journal_draw_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = (uint8_t)(11U + row * 10U);
    if(selected) {
        canvas_draw_box(canvas, 0, y, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    char shown[27];
    journal_copy(shown, sizeof(shown), text);
    canvas_draw_str(canvas, 2, (uint8_t)(y + 8U), shown);
    if(selected) canvas_set_color(canvas, ColorBlack);
}

static void journal_draw_list(Canvas* canvas, JournalApp* app) {
    if(app->scroll < app->count) (void)journal_window(app, app->scroll);
    journal_draw_header(canvas, app, "DNDJournal");
    uint32_t total = (uint32_t)app->count + 1U;
    for(uint8_t row = 0U; row < 5U; ++row) {
        uint16_t index = (uint16_t)(app->scroll + row);
        if(index >= total) break;
        char text[48];
        if(index == app->count) {
            journal_copy(text, sizeof(text), "+ New Entry");
        } else {
            const JournalEntryMeta* entry = journal_entry_at(app, index);
            if(entry) {
                snprintf(
                    text,
                    sizeof(text),
                    "%c %.9s: %.24s",
                    entry->completed ? 'X' : ' ',
                    journal_category_names[entry->category],
                    entry->title);
            } else {
                journal_copy(text, sizeof(text), "Entry unavailable");
            }
        }
        journal_draw_row(canvas, row, index == app->selection, text);
    }
}

static void journal_draw_detail(Canvas* canvas, JournalApp* app) {
    journal_draw_header(canvas, app, "Journal Entry");
    if(!app->current_loaded) return;
    const JournalEntry* entry = &app->current_entry;
    char rows[5][48];
    snprintf(rows[0], sizeof(rows[0]), "Category: %s", journal_category_names[entry->category]);
    snprintf(rows[1], sizeof(rows[1]), "Title: %.32s", entry->title);
    snprintf(rows[2], sizeof(rows[2]), "Body: %.32s", entry->body);
    snprintf(rows[3], sizeof(rows[3]), "Complete: %s", entry->completed ? "Yes" : "No");
    journal_copy(
        rows[4],
        sizeof(rows[4]),
        entry->category == JournalCategoryMilestone ? "Continue Adventure" : "Delete Entry");
    for(uint8_t row = 0U; row < 5U; ++row)
        journal_draw_row(canvas, row, row == app->detail_field, rows[row]);
}

static void journal_draw(Canvas* canvas, void* model) {
    JournalApp* app = *(JournalApp**)model;
    canvas_clear(canvas);
    if(app->screen == JournalScreenList)
        journal_draw_list(canvas, app);
    else
        journal_draw_detail(canvas, app);
}

static void journal_text_done(void* context) {
    JournalApp* app = context;
    if(app->current_loaded) {
        JournalEntry* entry = &app->current_entry;
        if(app->edit == JournalEditTitle)
            journal_copy(entry->title, sizeof(entry->title), app->edit_buffer);
        else if(app->edit == JournalEditBody)
            journal_copy(entry->body, sizeof(entry->body), app->edit_buffer);
        bool saved = journal_save_entry(app, entry);
        if(saved) journal_update_cached_current(app);
        journal_set_status(app, saved ? "Saved" : "Save failed");
    }
    app->edit = JournalEditNone;
    view_dispatcher_switch_to_view(app->dispatcher, JournalViewMain);
    journal_refresh(app);
}

static void journal_begin_text(JournalApp* app, JournalEdit edit) {
    if(!app->current_loaded) return;
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) {
            journal_set_status(app, "Memory low");
            return;
        }
        view_dispatcher_add_view(
            app->dispatcher, JournalViewText, text_input_get_view(app->text_input));
    }
    JournalEntry* entry = &app->current_entry;
    app->edit = edit;
    journal_copy(
        app->edit_buffer,
        sizeof(app->edit_buffer),
        edit == JournalEditTitle ? entry->title : entry->body);
    text_input_reset(app->text_input);
    text_input_set_header_text(
        app->text_input, edit == JournalEditTitle ? "Journal title" : "Journal note");
    text_input_set_result_callback(
        app->text_input,
        journal_text_done,
        app,
        app->edit_buffer,
        edit == JournalEditTitle ? JOURNAL_NAME_LEN : JOURNAL_BODY_LEN,
        false);
    view_dispatcher_switch_to_view(app->dispatcher, JournalViewText);
}

static void journal_move_list(JournalApp* app, int8_t delta) {
    uint32_t total = (uint32_t)app->count + 1U;
    int32_t next = (int32_t)app->selection + delta;
    if(next < 0) next = (int32_t)total - 1;
    if((uint32_t)next >= total) next = 0;
    app->selection = (uint16_t)next;
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + 5U) app->scroll = (uint16_t)(app->selection - 4U);
    if(app->selection < app->count) (void)journal_entry_at(app, app->selection);
}

static void journal_return_to_dnd(JournalApp* app) {
    app->return_to_dnd = 1U;
    view_dispatcher_stop(app->dispatcher);
}

static void journal_delete_current(JournalApp* app) {
    if(!app->current_loaded) return;
    uint16_t old_selection = app->selection;
    if(!journal_delete_entry(app, &app->current_entry)) {
        journal_set_status(app, "Delete failed");
        return;
    }
    bool loaded = journal_load(app);
    app->selection = old_selection < app->count ? old_selection : app->count;
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + 5U) app->scroll = (uint16_t)(app->selection - 4U);
    app->screen = JournalScreenList;
    app->current_loaded = 0U;
    journal_set_status(app, loaded ? "Deleted" : "Deleted; index partial");
}

static bool journal_input(InputEvent* event, void* context) {
    JournalApp* app = context;
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        journal_return_to_dnd(app);
        return true;
    }
    if(app->screen == JournalScreenList) {
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyUp)
            journal_move_list(app, -1);
        else if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
                event->key == InputKeyDown)
            journal_move_list(app, 1);
        else if(event->type == InputTypeShort && event->key == InputKeyBack)
            journal_return_to_dnd(app);
        else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection == app->count) {
                if(app->count >= UINT16_MAX - 1U) {
                    journal_set_status(app, "Journal index full");
                } else {
                    JournalEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    journal_copy(entry.title, sizeof(entry.title), "New Entry");
                    if(journal_create_entry(app, &entry)) {
                        char created[JOURNAL_FILE_LEN];
                        journal_copy(created, sizeof(created), entry.file_name);
                        bool indexed = journal_load(app);
                        app->selection = journal_index_of_file(app, created);
                        app->current_entry = entry;
                        app->current_loaded = 1U;
                        app->screen = JournalScreenDetail;
                        app->detail_field = 0U;
                        journal_set_status(app, indexed ? "Created" : "Created; index partial");
                    } else {
                        journal_set_status(app, "Create failed");
                    }
                }
            } else if(journal_open_detail(app, app->selection)) {
                app->status[0] = '\0';
            } else {
                journal_set_status(app, "Entry load failed");
            }
        }
    } else {
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyUp) {
            app->detail_field = app->detail_field ? (uint8_t)(app->detail_field - 1U) : 4U;
        } else if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
                  event->key == InputKeyDown) {
            app->detail_field = (uint8_t)((app->detail_field + 1U) % 5U);
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->screen = JournalScreenList;
            app->current_loaded = 0U;
            app->status[0] = '\0';
        } else if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
                  (event->key == InputKeyLeft || event->key == InputKeyRight) &&
                  app->current_loaded) {
            JournalEntry* entry = &app->current_entry;
            bool changed = false;
            if(app->detail_field == 0U) {
                int16_t category = entry->category + (event->key == InputKeyRight ? 1 : -1);
                if(category < 0) category = JournalCategoryCount - 1;
                if(category >= JournalCategoryCount) category = 0;
                entry->category = (uint8_t)category;
                changed = true;
            } else if(app->detail_field == 3U) {
                entry->completed = !entry->completed;
                changed = true;
            }
            if(changed) {
                bool saved = journal_save_entry(app, entry);
                if(saved) journal_update_cached_current(app);
                journal_set_status(app, saved ? "Saved" : "Save failed");
            }
        } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
                  app->current_loaded && app->detail_field == 4U &&
                  app->current_entry.category == JournalCategoryMilestone) {
            journal_delete_current(app);
        } else if(event->type == InputTypeShort && event->key == InputKeyOk &&
                  app->current_loaded) {
            if(app->detail_field == 1U)
                journal_begin_text(app, JournalEditTitle);
            else if(app->detail_field == 2U)
                journal_begin_text(app, JournalEditBody);
            else if(app->detail_field == 0U || app->detail_field == 3U) {
                JournalEntry* entry = &app->current_entry;
                if(app->detail_field == 0U)
                    entry->category = (uint8_t)((entry->category + 1U) % JournalCategoryCount);
                else
                    entry->completed = !entry->completed;
                bool saved = journal_save_entry(app, entry);
                if(saved) journal_update_cached_current(app);
                journal_set_status(app, saved ? "Saved" : "Save failed");
            } else if(app->detail_field == 4U) {
                if(app->current_entry.category == JournalCategoryMilestone) {
                    app->launch_adventure = 1U;
                    view_dispatcher_stop(app->dispatcher);
                } else {
                    journal_delete_current(app);
                }
            }
        }
    }
    journal_refresh(app);
    return true;
}

static bool journal_navigation(void* context) {
    JournalApp* app = context;
    if(app->edit != JournalEditNone) {
        app->edit = JournalEditNone;
        view_dispatcher_switch_to_view(app->dispatcher, JournalViewMain);
        journal_refresh(app);
    } else if(app->screen == JournalScreenDetail) {
        app->screen = JournalScreenList;
        app->current_loaded = 0U;
        journal_refresh(app);
    } else {
        journal_return_to_dnd(app);
    }
    return true;
}

static JournalApp* journal_app_alloc(const char* args) {
    JournalApp* app = malloc(sizeof(JournalApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(*app));
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->gui || !app->storage) goto fail;
    /* Explicit handoff ID wins. A direct launch uses DNDolphins' persisted
       active character when available, otherwise character 0. */
    if(!(args && journal_parse_u32(args, &app->profile)) &&
       !dnd_profile_ref_active(app->storage, &app->profile))
        app->profile = 0U;
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    if(!app->dispatcher || !app->view) goto fail;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, journal_navigation);
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(JournalApp*));
    JournalApp** model = view_get_model(app->view);
    if(!model) goto fail;
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, journal_draw);
    view_set_input_callback(app->view, journal_input);
    if(!journal_load(app)) journal_set_status(app, "Partial load/memory");
    app->screen = JournalScreenList;
    view_dispatcher_add_view(app->dispatcher, JournalViewMain, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;

fail:
    if(app->text_input) text_input_free(app->text_input);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
    return NULL;
}

static void journal_app_free(JournalApp* app) {
    if(!app) return;
    if(app->dispatcher && app->text_input)
        view_dispatcher_remove_view(app->dispatcher, JournalViewText);
    if(app->dispatcher && app->view)
        view_dispatcher_remove_view(app->dispatcher, JournalViewMain);
    if(app->text_input) text_input_free(app->text_input);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t dndjournal_app(void* context) {
    JournalApp* app = journal_app_alloc(context);
    if(!app) return -1;
    view_dispatcher_switch_to_view(app->dispatcher, JournalViewMain);
    view_dispatcher_run(app->dispatcher);
    uint8_t launch_adventure = app->launch_adventure;
    uint8_t return_to_dnd = !launch_adventure && app->return_to_dnd;
    uint32_t profile = app->profile;
    journal_app_free(app);
    if(launch_adventure || return_to_dnd) {
        char args[16];
        const char* launch_args = NULL;
        const char* launch_path = DNDOLPHINS_FAP_PATH;
        if(launch_adventure) {
            int length = snprintf(args, sizeof(args), "%lu", (unsigned long)profile);
            if(length <= 0 || (size_t)length >= sizeof(args)) return -1;
            launch_args = args;
            launch_path = DNDADVENTURE_FAP_PATH;
        }
        if(!dnd_handoff_launch(launch_path, launch_args)) return -1;
    }
    return 0;
}
