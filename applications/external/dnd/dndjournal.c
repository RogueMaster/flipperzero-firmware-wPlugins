#include "dnd_fs.h"
#include "dnd_profile_handoff.h"

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
#define JOURNAL_CLASS_MAX 4U
#define JOURNAL_ITEM_PATH POCKET_D20_CHARACTER_DATA_ROOT "/inventory_%lu.txt"

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
    uint8_t detail_scroll;
    char class_names[JOURNAL_CLASS_MAX][JOURNAL_NAME_LEN];
    uint8_t class_levels[JOURNAL_CLASS_MAX];
    uint8_t class_count;
    uint8_t return_to_dnd;
    uint8_t launch_adventure;
    uint8_t have_profile;
    uint32_t profile;
    JournalScreen screen;
    JournalEdit edit;
    char edit_buffer[JOURNAL_BODY_LEN];
    char status[32];
} JournalApp;

static void dndjournal_set_status(JournalApp* app, const char* status);

static const char* const journal_category_names[JournalCategoryCount] = {
    "Quick",
    "Adventure",
    "Item",
    "Milestone",
};

static void dndjournal_copy(char* destination, size_t size, const char* source) {
    if(!destination || !size) return;
    if(!source) source = "";
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

static bool dndjournal_parse_u32(const char* text, uint32_t* output) {
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

static bool dndjournal_writef(File* file, const char* format, ...) {
    char line[128];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if(length < 0 || (size_t)length >= sizeof(line)) return false;
    return storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static uint8_t dndjournal_hex_value(char value) {
    if(value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if(value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
    if(value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    return 0xFFU;
}

static bool dndjournal_write_string(File* file, const char* key, const char* value) {
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

static bool dndjournal_reader_next(JournalReader* reader, char* value) {
    if(reader->position >= reader->count) {
        reader->count =
            (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
        reader->position = 0U;
        if(!reader->count) return false;
    }
    *value = (char)reader->buffer[reader->position++];
    return true;
}

static void dndjournal_skip_field_value(JournalReader* reader);

static bool dndjournal_read_field_key(JournalReader* reader, char* key, size_t key_size) {
    if(!reader || !key || key_size < 2U) return false;
    size_t used = 0U;
    bool overflow = false;
    char character = '\0';
    while(dndjournal_reader_next(reader, &character)) {
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
            dndjournal_skip_field_value(reader);
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

static bool dndjournal_read_raw_field_value(
    JournalReader* reader,
    char* value,
    size_t value_size) {
    if(!reader || !value || !value_size) return false;
    size_t used = 0U;
    bool overflow = false;
    bool consumed = false;
    char character = '\0';
    while(dndjournal_reader_next(reader, &character)) {
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

static bool dndjournal_read_decoded_field_value(
    JournalReader* reader,
    char* destination,
    size_t destination_size) {
    if(!reader || !destination || !destination_size) return false;
    size_t output = 0U;
    bool overflow = false;
    char character = '\0';
    while(dndjournal_reader_next(reader, &character)) {
        if(character == '\n') break;
        if(character == '\r') continue;
        uint8_t byte = (uint8_t)character;
        if(character == '%') {
            char high_char = '\0';
            char low_char = '\0';
            if(!dndjournal_reader_next(reader, &high_char) || !dndjournal_reader_next(reader, &low_char))
                break;
            uint8_t high = dndjournal_hex_value(high_char);
            uint8_t low = dndjournal_hex_value(low_char);
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

static void dndjournal_skip_field_value(JournalReader* reader) {
    char character = '\0';
    while(dndjournal_reader_next(reader, &character))
        if(character == '\n') break;
}

static bool dndjournal_parse_i32(const char* text, int32_t* output) {
    if(!text || !text[0] || !output) return false;
    bool negative = false;
    if(*text == '-' || *text == '+') {
        negative = *text == '-';
        ++text;
    }
    if(*text < '0' || *text > '9') return false;
    int32_t value = 0;
    while(*text) {
        if(*text < '0' || *text > '9') return false;
        int32_t digit = (int32_t)(*text - '0');
        if(value > (INT32_MAX - digit) / 10) return false;
        value = value * 10 + digit;
        ++text;
    }
    *output = negative ? -value : value;
    return true;
}

static uint8_t dndjournal_parse_csv_i32(char* text, int32_t* values, uint8_t capacity) {
    if(!text || !values || !capacity) return 0U;
    uint8_t count = 0U;
    char* cursor = text;
    while(count < capacity && *cursor) {
        char* comma = strchr(cursor, ',');
        if(comma) *comma = '\0';
        if(!dndjournal_parse_i32(cursor, &values[count])) return count;
        ++count;
        if(!comma) break;
        cursor = comma + 1U;
    }
    return count;
}

static bool dndjournal_class_key(const char* key, const char* suffix, uint8_t* index) {
    if(!key || !suffix || strncmp(key, "Class", 5U)) return false;
    const char* cursor = key + 5U;
    if(*cursor < '0' || *cursor > '9') return false;
    uint32_t value = 0U;
    while(*cursor >= '0' && *cursor <= '9') {
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(value > (UINT32_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
        ++cursor;
    }
    if(value >= JOURNAL_CLASS_MAX || strcmp(cursor, suffix)) return false;
    *index = (uint8_t)value;
    return true;
}

static bool dndjournal_load_classes(JournalApp* app) {
    if(!app || !app->storage || !app->have_profile) return false;
    memset(app->class_names, 0, sizeof(app->class_names));
    memset(app->class_levels, 0, sizeof(app->class_levels));
    app->class_count = 0U;
    char path[JOURNAL_PATH_LEN];
    if(!dnd_profile_ref_path(app->storage, app->profile, path, sizeof(path))) return false;
    File* file = storage_file_alloc(app->storage);
    if(!file || !storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(file) storage_file_free(file);
        return false;
    }
    JournalReader reader = {.file = file};
    char key[32];
    char value[192];
    while(dndjournal_read_field_key(&reader, key, sizeof(key))) {
        uint8_t index = 0U;
        if(dndjournal_class_key(key, "Name", &index)) {
            if(dndjournal_read_decoded_field_value(
                   &reader, app->class_names[index], sizeof(app->class_names[index]))) {
                if(index + 1U > app->class_count) app->class_count = (uint8_t)(index + 1U);
            }
        } else if(dndjournal_class_key(key, "Data", &index)) {
            if(dndjournal_read_raw_field_value(&reader, value, sizeof(value))) {
                char* comma = strchr(value, ',');
                if(comma) *comma = '\0';
                uint32_t level = 0U;
                if(dndjournal_parse_u32(value, &level) && level <= 20U) {
                    app->class_levels[index] = (uint8_t)level;
                    if(index + 1U > app->class_count) app->class_count = (uint8_t)(index + 1U);
                }
            }
        } else {
            dndjournal_skip_field_value(&reader);
        }
    }
    bool ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    while(app->class_count && !app->class_levels[app->class_count - 1U] &&
          !app->class_names[app->class_count - 1U][0])
        --app->class_count;
    return ok && app->class_count;
}

static uint8_t dndjournal_total_level(const JournalApp* app) {
    uint16_t total = 0U;
    if(!app) return 0U;
    for(uint8_t i = 0U; i < app->class_count; ++i) total += app->class_levels[i];
    return (uint8_t)(total > 20U ? 20U : total);
}

static bool dndjournal_profile_dir(char* output, size_t size, uint32_t profile) {
    int length = snprintf(output, size, JOURNAL_PROFILE_PATH, (unsigned long)profile);
    return length > 0 && (size_t)length < size;
}

static bool dndjournal_entry_path(
    char* output,
    size_t size,
    uint32_t profile,
    const char* file_name) {
    if(!file_name || !file_name[0] || strchr(file_name, '/') || strchr(file_name, '\\'))
        return false;
    char directory[JOURNAL_PATH_LEN];
    if(!dndjournal_profile_dir(directory, sizeof(directory), profile)) return false;
    int length = snprintf(output, size, "%s/%s", directory, file_name);
    return length > 0 && (size_t)length < size;
}

static bool dndjournal_file_name_valid(const char* file_name) {
    if(!file_name) return false;
    size_t length = strlen(file_name);
    return length >= 5U && length < JOURNAL_FILE_LEN && !strchr(file_name, '/') &&
           !strchr(file_name, '\\') && strcmp(file_name + length - 4U, ".txt") == 0;
}

static bool dndjournal_directory_exists(Storage* storage, const char* path) {
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK && file_info_is_dir(&info);
}

static bool dndjournal_ensure_directory(Storage* storage, uint32_t profile) {
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    char directory[JOURNAL_PATH_LEN];
    if(!dndjournal_profile_dir(directory, sizeof(directory), profile)) return false;
    storage_common_mkdir(storage, directory);
    return dndjournal_directory_exists(storage, directory);
}

static bool dndjournal_publish_temp(
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

static bool dndjournal_read_line(
    JournalReader* reader,
    char* line,
    size_t size,
    bool* overflow) {
    if(!reader || !line || size < 2U) return false;
    size_t used = 0U;
    bool consumed = false;
    bool too_long = false;
    char ch = '\0';
    while(dndjournal_reader_next(reader, &ch)) {
        consumed = true;
        if(ch == '\n') break;
        if(ch == '\r') continue;
        if(used + 1U < size)
            line[used++] = ch;
        else
            too_long = true;
    }
    line[used] = '\0';
    if(overflow) *overflow = too_long;
    return consumed;
}

static bool dndjournal_write_line(File* file, const char* line) {
    if(!file || !line) return false;
    size_t length = strlen(line);
    return storage_file_write(file, line, length) == length &&
           storage_file_write(file, "\n", 1U) == 1U;
}

static bool dndjournal_patch_milestone_class(JournalApp* app, uint8_t class_index) {
    if(!app || !app->storage || !app->have_profile || class_index >= JOURNAL_CLASS_MAX)
        return false;
    char path[JOURNAL_PATH_LEN];
    if(!dnd_profile_ref_path(app->storage, app->profile, path, sizeof(path))) return false;
    char temporary[JOURNAL_PATH_LEN], backup[JOURNAL_PATH_LEN];
    int tn = snprintf(temporary, sizeof(temporary), "%s.jtmp", path);
    int bn = snprintf(backup, sizeof(backup), "%s.jbak", path);
    if(tn <= 0 || bn <= 0 || (size_t)tn >= sizeof(temporary) || (size_t)bn >= sizeof(backup))
        return false;

    File* input = storage_file_alloc(app->storage);
    File* output = storage_file_alloc(app->storage);
    char* line = malloc(768U);
    char* parse = malloc(768U);
    if(!input || !output || !line || !parse) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        free(line);
        free(parse);
        return false;
    }
    bool ok = storage_file_open(input, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_open(output, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    bool class_touched = false;
    if(ok) {
        JournalReader reader = {.file = input};
        char target[24];
        snprintf(target, sizeof(target), "Class%uData", class_index);
        bool overflow = false;
        while(ok && dndjournal_read_line(&reader, line, 768U, &overflow)) {
            if(overflow) { ok = false; break; }
            char replacement[256];
            const char* out_line = line;
            dndjournal_copy(parse, 768U, line);
            char* value = strchr(parse, '=');
            if(value) {
                *value++ = '\0';
                if(!strcmp(parse, target)) {
                    int32_t values[16] = {0};
                    if(dndjournal_parse_csv_i32(value, values, 16U) == 16U && values[0] < 20) {
                        ++values[0];
                        if(values[2] < 20) ++values[2];
                        if(values[3] < 20) ++values[3];
                        int n = snprintf(
                            replacement,
                            sizeof(replacement),
                            "%s=%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld",
                            target,
                            (long)values[0], (long)values[1], (long)values[2], (long)values[3],
                            (long)values[4], (long)values[5], (long)values[6], (long)values[7],
                            (long)values[8], (long)values[9], (long)values[10], (long)values[11],
                            (long)values[12], (long)values[13], (long)values[14], (long)values[15]);
                        if(n > 0 && (size_t)n < sizeof(replacement)) {
                            out_line = replacement;
                            class_touched = true;
                        }
                    }
                } else if(!strcmp(parse, "Vitals")) {
                    int32_t values[12] = {0};
                    if(dndjournal_parse_csv_i32(value, values, 12U) == 12U) {
                        if(values[11] < 20) ++values[11];
                        int n = snprintf(
                            replacement,
                            sizeof(replacement),
                            "Vitals=%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld",
                            (long)values[0], (long)values[1], (long)values[2], (long)values[3],
                            (long)values[4], (long)values[5], (long)values[6], (long)values[7],
                            (long)values[8], (long)values[9], (long)values[10], (long)values[11]);
                        if(n > 0 && (size_t)n < sizeof(replacement)) out_line = replacement;
                    }
                }
            }
            ok = dndjournal_write_line(output, out_line);
        }
        if(ok) ok = storage_file_sync(output);
    }
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    free(line);
    free(parse);
    if(!ok || !class_touched) {
        storage_common_remove(app->storage, temporary);
        return false;
    }
    return dndjournal_publish_temp(app->storage, temporary, path, backup);
}

static bool dndjournal_write_collection_field(File* file, const char* value) {
    if(!file || !value) return false;
    static const char digits[] = "0123456789ABCDEF";
    char chunk[64];
    size_t used = 0U;
    for(size_t i = 0U; value[i]; ++i) {
        uint8_t byte = (uint8_t)value[i];
        bool escape = byte == '%' || byte == '|' || byte == '\n' || byte == '\r' || byte < 0x20U;
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
    return !used || storage_file_write(file, chunk, used) == used;
}

static bool dndjournal_ensure_item_sidecar(JournalApp* app, char* path, size_t size) {
    if(!app || !path || !size) return false;
    int n = snprintf(path, size, JOURNAL_ITEM_PATH, (unsigned long)app->profile);
    if(n <= 0 || (size_t)n >= size) return false;
    storage_common_mkdir(app->storage, POCKET_D20_CHARACTER_DATA_ROOT);
    bool needs_header = !storage_file_exists(app->storage, path);
    if(!needs_header) {
        File* existing = storage_file_alloc(app->storage);
        if(!existing) return false;
        bool opened = storage_file_open(existing, path, FSAM_READ, FSOM_OPEN_EXISTING);
        uint64_t file_size = opened ? storage_file_size(existing) : 0U;
        if(opened) storage_file_close(existing);
        storage_file_free(existing);
        if(!opened) return false;
        needs_header = file_size == 0U;
    }
    if(!needs_header) return true;
    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(file, "DNDItems=1\n", 11U) == 11U && storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool dndjournal_create_inventory_item(JournalApp* app) {
    if(!app || !app->current_loaded || app->current_entry.category != JournalCategoryItem)
        return false;
    char path[JOURNAL_PATH_LEN];
    if(!dndjournal_ensure_item_sidecar(app, path, sizeof(path))) return false;
    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_EXISTING);
    if(ok) {
        uint64_t end = storage_file_size(file);
        ok = end <= UINT32_MAX && storage_file_seek(file, (uint32_t)end, true);
    }
    if(ok) ok = storage_file_write(file, "I|", 2U) == 2U;
    if(ok) ok = dndjournal_write_collection_field(file, app->current_entry.title);
    if(ok) ok = storage_file_write(file, "|", 1U) == 1U;
    if(ok) ok = dndjournal_write_collection_field(file, app->current_entry.body);
    if(ok) ok = storage_file_write(file, "||", 2U) == 2U;
    if(ok) {
        const char* numeric =
            "1,0,0,0,0,0,0,0,1,6,0,0,0,1,0,6,0,0,0,-1,0,0,0,-1,0\n";
        size_t length = strlen(numeric);
        ok = storage_file_write(file, numeric, length) == length && storage_file_sync(file);
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool dndjournal_read_entry(
    Storage* storage,
    uint32_t profile,
    const char* file_name,
    JournalEntry* entry) {
    if(!dndjournal_file_name_valid(file_name) || !entry) return false;
    char path[JOURNAL_PATH_LEN];
    if(!dndjournal_entry_path(path, sizeof(path), profile, file_name)) return false;
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
    dndjournal_copy(parsed.title, sizeof(parsed.title), "Journal Entry");
    char key[32];
    char value[48];
    bool recognized = false;
    while(dndjournal_read_field_key(&reader, key, sizeof(key))) {
        if(!strcmp(key, "Title")) {
            if(dndjournal_read_decoded_field_value(&reader, parsed.title, sizeof(parsed.title)))
                recognized = true;
        } else if(!strcmp(key, "Body")) {
            if(dndjournal_read_decoded_field_value(&reader, parsed.body, sizeof(parsed.body)))
                recognized = true;
        } else if(!strcmp(key, "Category") || !strcmp(key, "Completed") ||
                  !strcmp(key, "LevelGranted") || !strcmp(key, "ClassIndex")) {
            if(dndjournal_read_raw_field_value(&reader, value, sizeof(value))) {
                uint32_t number = 0U;
                if(dndjournal_parse_u32(value, &number)) {
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
            dndjournal_skip_field_value(&reader);
        }
    }
    bool io_ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    if(!io_ok || !recognized) return false;
    dndjournal_copy(parsed.file_name, sizeof(parsed.file_name), file_name);
    *entry = parsed;
    return true;
}

static bool dndjournal_read_metadata(
    Storage* storage,
    uint32_t profile,
    const char* file_name,
    JournalEntryMeta* entry) {
    if(!dndjournal_file_name_valid(file_name) || !entry) return false;
    char path[JOURNAL_PATH_LEN];
    if(!dndjournal_entry_path(path, sizeof(path), profile, file_name)) return false;
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
    dndjournal_copy(parsed.title, sizeof(parsed.title), "Journal Entry");
    char key[32];
    char value[48];
    bool recognized = false;
    while(dndjournal_read_field_key(&reader, key, sizeof(key))) {
        if(!strcmp(key, "Title")) {
            if(dndjournal_read_decoded_field_value(&reader, parsed.title, sizeof(parsed.title)))
                recognized = true;
        } else if(!strcmp(key, "Body")) {
            dndjournal_skip_field_value(&reader);
        } else if(!strcmp(key, "Category") || !strcmp(key, "Completed") ||
                  !strcmp(key, "LevelGranted") || !strcmp(key, "ClassIndex")) {
            if(dndjournal_read_raw_field_value(&reader, value, sizeof(value))) {
                uint32_t number = 0U;
                if(dndjournal_parse_u32(value, &number)) {
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
            dndjournal_skip_field_value(&reader);
        }
    }
    bool io_ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    if(!io_ok || !recognized) return false;
    dndjournal_copy(parsed.file_name, sizeof(parsed.file_name), file_name);
    *entry = parsed;
    return true;
}

static void dndjournal_insert_descending(JournalApp* app, const JournalEntryMeta* entry) {
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

static void dndjournal_insert_oldest(JournalApp* app, const JournalEntryMeta* entry) {
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

static void dndjournal_hydrate_cache(JournalApp* app) {
    if(!app) return;
    for(uint8_t i = 0U; i < app->cache_count; ++i) {
        char file_name[JOURNAL_FILE_LEN];
        dndjournal_copy(file_name, sizeof(file_name), app->entries[i].file_name);
        JournalEntryMeta parsed;
        if(dndjournal_read_metadata(app->storage, app->profile, file_name, &parsed)) {
            app->entries[i] = parsed;
        } else {
            memset(&app->entries[i], 0, sizeof(app->entries[i]));
            dndjournal_copy(app->entries[i].file_name, sizeof(app->entries[i].file_name), file_name);
            dndjournal_copy(app->entries[i].title, sizeof(app->entries[i].title), "Unreadable entry");
            app->entries[i].category = JournalCategoryQuick;
        }
    }
}

static bool dndjournal_scan_cache(
    JournalApp* app,
    const char* boundary,
    bool newer,
    uint16_t anchor) {
    char directory_path[JOURNAL_PATH_LEN];
    if(!dndjournal_profile_dir(directory_path, sizeof(directory_path), app->profile)) return false;
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
        if(file_info_is_dir(&info) || !dndjournal_file_name_valid(file_name)) continue;
        if(boundary) {
            int compare = strcmp(file_name, boundary);
            if(newer ? compare <= 0 : compare >= 0) continue;
        }
        JournalEntryMeta parsed;
        memset(&parsed, 0, sizeof(parsed));
        dndjournal_copy(parsed.file_name, sizeof(parsed.file_name), file_name);
        if(newer)
            dndjournal_insert_oldest(app, &parsed);
        else
            dndjournal_insert_descending(app, &parsed);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(!app->cache_count) return false;
    dndjournal_hydrate_cache(app);
    app->cache_start = newer ?
                           (anchor >= app->cache_count ? (uint16_t)(anchor - app->cache_count) : 0U) :
                           anchor;
    return true;
}

static bool dndjournal_load(JournalApp* app) {
    app->count = 0U;
    app->cache_start = 0U;
    app->cache_count = 0U;
    app->current_loaded = 0U;
    char directory_path[JOURNAL_PATH_LEN];
    if(!dndjournal_profile_dir(directory_path, sizeof(directory_path), app->profile)) return false;
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
        if(file_info_is_dir(&info) || !dndjournal_file_name_valid(file_name)) continue;
        if(app->count == UINT16_MAX - 1U) {
            ok = false;
            break;
        }
        ++app->count;
        JournalEntryMeta parsed;
        memset(&parsed, 0, sizeof(parsed));
        dndjournal_copy(parsed.file_name, sizeof(parsed.file_name), file_name);
        dndjournal_insert_descending(app, &parsed);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(app->cache_count) dndjournal_hydrate_cache(app);
    return ok;
}

static const JournalEntryMeta* dndjournal_entry_at(JournalApp* app, uint16_t index) {
    if(!app || index >= app->count) return NULL;
    if(!app->cache_count && !dndjournal_load(app)) return NULL;
    uint16_t guard = 0U;
    while(index < app->cache_start && app->cache_count && guard++ < UINT16_MAX) {
        char boundary[JOURNAL_FILE_LEN];
        dndjournal_copy(boundary, sizeof(boundary), app->entries[0].file_name);
        uint16_t old_start = app->cache_start;
        if(!dndjournal_scan_cache(app, boundary, true, old_start) || app->cache_start >= old_start)
            return NULL;
    }
    guard = 0U;
    while(index >= (uint16_t)(app->cache_start + app->cache_count) &&
          app->cache_count && guard++ < UINT16_MAX) {
        char boundary[JOURNAL_FILE_LEN];
        dndjournal_copy(boundary, sizeof(boundary), app->entries[app->cache_count - 1U].file_name);
        uint16_t next_start = (uint16_t)(app->cache_start + app->cache_count);
        if(!dndjournal_scan_cache(app, boundary, false, next_start)) return NULL;
    }
    if(index < app->cache_start || index >= (uint16_t)(app->cache_start + app->cache_count))
        return NULL;
    return &app->entries[index - app->cache_start];
}

static bool dndjournal_window(JournalApp* app, uint16_t start) {
    if(!app || start >= app->count) return false;
    if(app->cache_count && app->cache_start == start) return true;
    if(start == 0U) return dndjournal_scan_cache(app, NULL, false, 0U);
    const JournalEntryMeta* previous = dndjournal_entry_at(app, start - 1U);
    if(!previous) return false;
    char boundary[JOURNAL_FILE_LEN];
    dndjournal_copy(boundary, sizeof(boundary), previous->file_name);
    return dndjournal_scan_cache(app, boundary, false, start);
}

static const JournalEntryMeta* dndjournal_cached_entry_at(
    const JournalApp* app, uint16_t index) {
    if(!app || index >= app->count || !app->cache_count || index < app->cache_start ||
       index >= (uint16_t)(app->cache_start + app->cache_count))
        return NULL;
    return &app->entries[index - app->cache_start];
}

static void dndjournal_prepare_list_window(JournalApp* app) {
    if(!app || !app->count || app->scroll >= app->count) return;
    (void)dndjournal_window(app, app->scroll);
}

static uint16_t dndjournal_index_of_file(JournalApp* app, const char* target) {
    char directory_path[JOURNAL_PATH_LEN];
    if(!target || !dndjournal_profile_dir(directory_path, sizeof(directory_path), app->profile))
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
        if(file_info_is_dir(&info) || !dndjournal_file_name_valid(file_name) ||
           strcmp(file_name, target) <= 0)
            continue;
        if(index < UINT16_MAX) ++index;
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return index;
}

static bool dndjournal_open_detail(JournalApp* app, uint16_t index) {
    const JournalEntryMeta* meta = dndjournal_entry_at(app, index);
    if(!meta) return false;
    char file_name[JOURNAL_FILE_LEN];
    dndjournal_copy(file_name, sizeof(file_name), meta->file_name);
    if(!dndjournal_read_entry(app->storage, app->profile, file_name, &app->current_entry)) return false;
    app->selection = index;
    app->current_loaded = 1U;
    app->screen = JournalScreenDetail;
    app->detail_field = 0U;
    app->detail_scroll = 0U;
    (void)dndjournal_load_classes(app);
    return true;
}

static void dndjournal_update_cached_current(JournalApp* app) {
    if(!app || !app->current_loaded) return;
    for(uint8_t i = 0U; i < app->cache_count; ++i) {
        JournalEntryMeta* meta = &app->entries[i];
        if(strcmp(meta->file_name, app->current_entry.file_name) != 0) continue;
        dndjournal_copy(meta->title, sizeof(meta->title), app->current_entry.title);
        meta->category = app->current_entry.category;
        meta->completed = app->current_entry.completed;
        meta->level_granted = app->current_entry.level_granted;
        meta->class_index = app->current_entry.class_index;
        break;
    }
}

static bool dndjournal_save_entry(JournalApp* app, JournalEntry* entry) {
    if(!app->have_profile) return false;
    if(!dndjournal_file_name_valid(entry->file_name) || entry->category >= JournalCategoryCount)
        return false;
    char path[JOURNAL_PATH_LEN];
    char temporary[JOURNAL_PATH_LEN];
    char backup[JOURNAL_PATH_LEN];
    if(!dndjournal_entry_path(path, sizeof(path), app->profile, entry->file_name)) return false;
    int temporary_length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    int backup_length = snprintf(backup, sizeof(backup), "%s.bak", path);
    if(temporary_length <= 0 || (size_t)temporary_length >= sizeof(temporary) ||
       backup_length <= 0 || (size_t)backup_length >= sizeof(backup) ||
       !dndjournal_ensure_directory(app->storage, app->profile))
        return false;
    storage_common_remove(app->storage, temporary);
    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    bool ok = storage_file_open(file, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              dndjournal_writef(file, "PocketD20Journal=1\n") &&
              dndjournal_writef(file, "CharacterId=%lu\n", (unsigned long)app->profile) &&
              dndjournal_write_string(file, "Title", entry->title) &&
              dndjournal_write_string(file, "Body", entry->body) &&
              dndjournal_writef(file, "Category=%u\n", entry->category) &&
              dndjournal_writef(file, "Completed=%u\n", entry->completed ? 1U : 0U) &&
              dndjournal_writef(file, "LevelGranted=%u\n", entry->level_granted) &&
              dndjournal_writef(file, "ClassIndex=%u\n", entry->class_index) &&
              dndjournal_writef(file, "End=OK\n") && storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        storage_common_remove(app->storage, temporary);
        return false;
    }
    return dndjournal_publish_temp(app->storage, temporary, path, backup);
}

static bool dndjournal_apply_milestone_level(JournalApp* app) {
    if(!app || !app->current_loaded) return false;
    JournalEntry* entry = &app->current_entry;
    if(entry->category != JournalCategoryMilestone) {
        dndjournal_set_status(app, "Set category Milestone");
        return false;
    }
    if(entry->level_granted) {
        dndjournal_set_status(app, "Level already applied");
        return false;
    }
    if(!dndjournal_load_classes(app) || !app->class_count) {
        dndjournal_set_status(app, "No class found");
        return false;
    }
    if(dndjournal_total_level(app) >= 20U) {
        dndjournal_set_status(app, "Maximum total level");
        return false;
    }
    if(entry->class_index >= app->class_count || !app->class_levels[entry->class_index])
        entry->class_index = 0U;
    if(entry->class_index >= app->class_count || !app->class_levels[entry->class_index]) {
        dndjournal_set_status(app, "Class unavailable");
        return false;
    }

    uint8_t prior_completed = entry->completed;
    uint8_t prior_granted = entry->level_granted;
    entry->completed = 1U;
    entry->level_granted = 1U;
    /* Persist the one-shot gate before changing the character. A write/power failure
       can therefore never turn a single milestone into two class levels. */
    if(!dndjournal_save_entry(app, entry)) {
        entry->completed = prior_completed;
        entry->level_granted = prior_granted;
        dndjournal_set_status(app, "Milestone save failed");
        return false;
    }
    if(!dndjournal_patch_milestone_class(app, entry->class_index)) {
        entry->completed = prior_completed;
        entry->level_granted = prior_granted;
        (void)dndjournal_save_entry(app, entry);
        dndjournal_set_status(app, "Level update failed");
        return false;
    }
    dndjournal_update_cached_current(app);
    (void)dndjournal_load_classes(app);
    dndjournal_set_status(app, "Class level increased");
    return true;
}

static bool dndjournal_create_entry(JournalApp* app, JournalEntry* entry) {
    if(!app->have_profile) return false;
    if(!dndjournal_ensure_directory(app->storage, app->profile)) return false;
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
        if(!dndjournal_entry_path(path, sizeof(path), app->profile, file_name)) return false;
        if(storage_file_exists(app->storage, path)) continue;
        dndjournal_copy(entry->file_name, sizeof(entry->file_name), file_name);
        if(dndjournal_save_entry(app, entry)) return true;
        entry->file_name[0] = '\0';
        return false;
    }
    return false;
}

static bool dndjournal_delete_entry(JournalApp* app, const JournalEntry* entry) {
    char path[JOURNAL_PATH_LEN];
    if(!dndjournal_entry_path(path, sizeof(path), app->profile, entry->file_name)) return false;
    return !storage_file_exists(app->storage, path) ||
           storage_common_remove(app->storage, path) == FSE_OK;
}

static void dndjournal_set_status(JournalApp* app, const char* status) {
    dndjournal_copy(app->status, sizeof(app->status), status);
}

static void dndjournal_refresh(JournalApp* app) {
    if(app && app->view) view_commit_model(app->view, true);
}

static void dndjournal_draw_header(Canvas* canvas, JournalApp* app, const char* title) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, title);
    if(app->status[0]) {
        uint16_t width = canvas_string_width(canvas, app->status);
        if(width < 62U) canvas_draw_str(canvas, 126U - width, 8, app->status);
    }
    if(app->screen == JournalScreenList && app->profile != UINT32_MAX) {
        char profile_id[16];
        snprintf(profile_id, sizeof(profile_id), "[%lu]", (unsigned long)app->profile);
        uint16_t id_width = canvas_string_width(canvas, profile_id);
        uint8_t id_x = id_width < 125U ? (uint8_t)(126U - id_width) : 1U;
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, id_x > 1U ? (uint8_t)(id_x - 1U) : 0U, 0,
                        (uint8_t)(id_width + 2U), 10);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, id_x, 8, profile_id);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void dndjournal_draw_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = (uint8_t)(11U + row * 10U);
    if(selected) {
        canvas_draw_box(canvas, 0, y, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    char shown[27];
    dndjournal_copy(shown, sizeof(shown), text);
    canvas_draw_str(canvas, 2, (uint8_t)(y + 8U), shown);
    if(selected) canvas_set_color(canvas, ColorBlack);
}

static void dndjournal_draw_list(Canvas* canvas, JournalApp* app) {
    dndjournal_draw_header(canvas, app, "DNDJournal");
    uint32_t total = (uint32_t)app->count + 1U;
    for(uint8_t row = 0U; row < 5U; ++row) {
        uint16_t index = (uint16_t)(app->scroll + row);
        if(index >= total) break;
        char text[48];
        if(index == app->count) {
            dndjournal_copy(text, sizeof(text), "+ New Entry");
        } else {
            const JournalEntryMeta* entry = dndjournal_cached_entry_at(app, index);
            if(entry) {
                snprintf(
                    text,
                    sizeof(text),
                    "%c %.9s: %.24s",
                    entry->completed ? 'X' : ' ',
                    journal_category_names[entry->category],
                    entry->title);
            } else {
                dndjournal_copy(text, sizeof(text), "Entry unavailable");
            }
        }
        dndjournal_draw_row(canvas, row, index == app->selection, text);
    }
}

static void dndjournal_draw_detail(Canvas* canvas, JournalApp* app) {
    dndjournal_draw_header(canvas, app, "Journal Entry");
    if(!app->current_loaded) return;
    const JournalEntry* entry = &app->current_entry;
    char rows[9][48];
    snprintf(rows[0], sizeof(rows[0]), "Category: %s", journal_category_names[entry->category]);
    snprintf(rows[1], sizeof(rows[1]), "Title: %.32s", entry->title);
    snprintf(rows[2], sizeof(rows[2]), "Body: %.32s", entry->body);
    snprintf(rows[3], sizeof(rows[3]), "Complete: %s", entry->completed ? "Yes" : "No");
    if(entry->category == JournalCategoryMilestone) {
        const char* class_name = entry->class_index < app->class_count && app->class_names[entry->class_index][0] ? app->class_names[entry->class_index] : "Primary";
        snprintf(rows[4], sizeof(rows[4]), "Level class: %.24s", class_name);
        dndjournal_copy(rows[5], sizeof(rows[5]), entry->level_granted ? "Level already applied" : "Apply milestone level");
        dndjournal_copy(rows[7], sizeof(rows[7]), "Continue active Adventure");
    } else {
        dndjournal_copy(rows[4], sizeof(rows[4]), "Level class: --");
        dndjournal_copy(rows[5], sizeof(rows[5]), "Milestone level: --");
        dndjournal_copy(rows[7], sizeof(rows[7]), "Adventure: --");
    }
    dndjournal_copy(rows[6], sizeof(rows[6]), entry->category == JournalCategoryItem ? "Create inventory item" : "Inventory item: --");
    dndjournal_copy(rows[8], sizeof(rows[8]), "Delete Entry");
    for(uint8_t row = 0U; row < 5U; ++row) {
        uint8_t index = (uint8_t)(app->detail_scroll + row);
        if(index >= 9U) break;
        dndjournal_draw_row(canvas, row, index == app->detail_field, rows[index]);
    }
}

static void dndjournal_draw(Canvas* canvas, void* model) {
    JournalApp* app = *(JournalApp**)model;
    canvas_clear(canvas);
    if(app->screen == JournalScreenList)
        dndjournal_draw_list(canvas, app);
    else
        dndjournal_draw_detail(canvas, app);
}

static void dndjournal_text_done(void* context) {
    JournalApp* app = context;
    if(app->current_loaded) {
        JournalEntry* entry = &app->current_entry;
        if(app->edit == JournalEditTitle)
            dndjournal_copy(entry->title, sizeof(entry->title), app->edit_buffer);
        else if(app->edit == JournalEditBody)
            dndjournal_copy(entry->body, sizeof(entry->body), app->edit_buffer);
        bool saved = dndjournal_save_entry(app, entry);
        if(saved) dndjournal_update_cached_current(app);
        dndjournal_set_status(app, saved ? "Saved" : "Save failed");
    }
    app->edit = JournalEditNone;
    view_dispatcher_switch_to_view(app->dispatcher, JournalViewMain);
    dndjournal_refresh(app);
}

static void dndjournal_begin_text(JournalApp* app, JournalEdit edit) {
    if(!app->current_loaded) return;
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) {
            dndjournal_set_status(app, "Memory low");
            return;
        }
        view_dispatcher_add_view(
            app->dispatcher, JournalViewText, text_input_get_view(app->text_input));
    }
    JournalEntry* entry = &app->current_entry;
    app->edit = edit;
    dndjournal_copy(
        app->edit_buffer,
        sizeof(app->edit_buffer),
        edit == JournalEditTitle ? entry->title : entry->body);
    text_input_reset(app->text_input);
    text_input_set_header_text(
        app->text_input, edit == JournalEditTitle ? "Journal title" : "Journal note");
    text_input_set_result_callback(
        app->text_input,
        dndjournal_text_done,
        app,
        app->edit_buffer,
        edit == JournalEditTitle ? JOURNAL_NAME_LEN : JOURNAL_BODY_LEN,
        false);
    view_dispatcher_switch_to_view(app->dispatcher, JournalViewText);
}

static void dndjournal_move_list(JournalApp* app, int8_t delta) {
    uint32_t total = (uint32_t)app->count + 1U;
    int32_t next = (int32_t)app->selection + delta;
    if(next < 0) next = (int32_t)total - 1;
    if((uint32_t)next >= total) next = 0;
    app->selection = (uint16_t)next;
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + 5U) app->scroll = (uint16_t)(app->selection - 4U);
    dndjournal_prepare_list_window(app);
}

static void dndjournal_return_to_dnd(JournalApp* app) {
    app->return_to_dnd = 1U;
    view_dispatcher_stop(app->dispatcher);
}

static void dndjournal_delete_current(JournalApp* app) {
    if(!app->current_loaded) return;
    uint16_t old_selection = app->selection;
    if(!dndjournal_delete_entry(app, &app->current_entry)) {
        dndjournal_set_status(app, "Delete failed");
        return;
    }
    bool loaded = dndjournal_load(app);
    app->selection = old_selection < app->count ? old_selection : app->count;
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + 5U) app->scroll = (uint16_t)(app->selection - 4U);
    dndjournal_prepare_list_window(app);
    app->screen = JournalScreenList;
    app->current_loaded = 0U;
    dndjournal_set_status(app, loaded ? "Deleted" : "Deleted; index partial");
}

static bool dndjournal_input(InputEvent* event, void* context) {
    JournalApp* app = context;
    if((event->type == InputTypeShort || event->type == InputTypeLong ||
        event->type == InputTypeRepeat) && !strcmp(app->status, "Saved"))
        app->status[0] = '\0';
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        app->return_to_dnd = 0U;
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    if(app->screen == JournalScreenList) {
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyUp)
            dndjournal_move_list(app, -1);
        else if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
                event->key == InputKeyDown)
            dndjournal_move_list(app, 1);
        else if(event->type == InputTypeShort && event->key == InputKeyBack)
            dndjournal_return_to_dnd(app);
        else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection == app->count) {
                if(!app->have_profile) {
                    dndjournal_set_status(app, "Create character in DNDolphins");
                } else if(app->count >= UINT16_MAX - 1U) {
                    dndjournal_set_status(app, "Journal index full");
                } else {
                    JournalEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    dndjournal_copy(entry.title, sizeof(entry.title), "New Entry");
                    if(dndjournal_create_entry(app, &entry)) {
                        char created[JOURNAL_FILE_LEN];
                        dndjournal_copy(created, sizeof(created), entry.file_name);
                        bool indexed = dndjournal_load(app);
                        app->selection = dndjournal_index_of_file(app, created);
                        app->current_entry = entry;
                        app->current_loaded = 1U;
                        app->screen = JournalScreenDetail;
                        app->detail_field = 0U;
                        app->detail_scroll = 0U;
                        (void)dndjournal_load_classes(app);
                        dndjournal_set_status(app, indexed ? "Created" : "Created; index partial");
                    } else {
                        dndjournal_set_status(app, "Create failed");
                    }
                }
            } else if(dndjournal_open_detail(app, app->selection)) {
                app->status[0] = '\0';
            } else {
                dndjournal_set_status(app, "Entry load failed");
            }
        }
    } else {
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyUp) {
            app->detail_field = app->detail_field ? (uint8_t)(app->detail_field - 1U) : 8U;
        } else if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
                  event->key == InputKeyDown) {
            app->detail_field = (uint8_t)((app->detail_field + 1U) % 9U);
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->screen = JournalScreenList;
            app->current_loaded = 0U;
            app->status[0] = '\0';
            dndjournal_prepare_list_window(app);
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
                if(entry->category == JournalCategoryMilestone) (void)dndjournal_load_classes(app);
                changed = true;
            } else if(app->detail_field == 3U) {
                entry->completed = !entry->completed;
                changed = true;
            } else if(app->detail_field == 4U &&
                      entry->category == JournalCategoryMilestone && app->class_count) {
                int16_t index = (int16_t)entry->class_index +
                                (event->key == InputKeyRight ? 1 : -1);
                if(index < 0) index = (int16_t)app->class_count - 1;
                if(index >= app->class_count) index = 0;
                entry->class_index = (uint8_t)index;
                changed = true;
            }
            if(changed) {
                bool saved = dndjournal_save_entry(app, entry);
                if(saved) dndjournal_update_cached_current(app);
                dndjournal_set_status(app, saved ? "Saved" : "Save failed");
            }
        } else if(event->type == InputTypeShort && event->key == InputKeyOk &&
                  app->current_loaded) {
            JournalEntry* entry = &app->current_entry;
            if(app->detail_field == 1U) {
                dndjournal_begin_text(app, JournalEditTitle);
            } else if(app->detail_field == 2U) {
                dndjournal_begin_text(app, JournalEditBody);
            } else if(app->detail_field == 0U || app->detail_field == 3U) {
                if(app->detail_field == 0U) {
                    entry->category = (uint8_t)((entry->category + 1U) % JournalCategoryCount);
                    if(entry->category == JournalCategoryMilestone) (void)dndjournal_load_classes(app);
                } else {
                    entry->completed = !entry->completed;
                }
                bool saved = dndjournal_save_entry(app, entry);
                if(saved) dndjournal_update_cached_current(app);
                dndjournal_set_status(app, saved ? "Saved" : "Save failed");
            } else if(app->detail_field == 4U && entry->category == JournalCategoryMilestone) {
                if(app->class_count) {
                    entry->class_index = (uint8_t)((entry->class_index + 1U) % app->class_count);
                    bool saved = dndjournal_save_entry(app, entry);
                    if(saved) dndjournal_update_cached_current(app);
                    dndjournal_set_status(app, saved ? "Saved" : "Save failed");
                } else {
                    dndjournal_set_status(app, "No class found");
                }
            } else if(app->detail_field == 5U && entry->category == JournalCategoryMilestone) {
                (void)dndjournal_apply_milestone_level(app);
            } else if(app->detail_field == 6U && entry->category == JournalCategoryItem) {
                dndjournal_set_status(
                    app,
                    dndjournal_create_inventory_item(app) ? "Inventory item created" :
                                                         "Inventory add failed");
            } else if(app->detail_field == 7U && entry->category == JournalCategoryMilestone) {
                app->launch_adventure = 1U;
                view_dispatcher_stop(app->dispatcher);
                return true;
            } else if(app->detail_field == 8U) {
                dndjournal_delete_current(app);
            }
        }
        if(app->screen == JournalScreenDetail) {
            if(app->detail_field < app->detail_scroll) app->detail_scroll = app->detail_field;
            if(app->detail_field >= app->detail_scroll + 5U)
                app->detail_scroll = (uint8_t)(app->detail_field - 4U);
        }
    }
    dndjournal_refresh(app);
    return true;
}

static bool dndjournal_navigation(void* context) {
    JournalApp* app = context;
    if(app->edit != JournalEditNone) {
        app->edit = JournalEditNone;
        view_dispatcher_switch_to_view(app->dispatcher, JournalViewMain);
        dndjournal_refresh(app);
    } else if(app->screen == JournalScreenDetail) {
        app->screen = JournalScreenList;
        app->current_loaded = 0U;
        dndjournal_prepare_list_window(app);
        dndjournal_refresh(app);
    } else {
        dndjournal_return_to_dnd(app);
    }
    return true;
}

static JournalApp* dndjournal_app_alloc(const char* args) {
    JournalApp* app = malloc(sizeof(JournalApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(*app));
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->gui || !app->storage) goto fail;
    /* Journal always follows DNDolphins' persisted Active= character.
       Launch arguments never select a character and a stale Active= reference
       does not fall forward to another profile. */
    UNUSED(args);
    bool have_profile = dnd_profile_ref_active_exact(app->storage, &app->profile);
    app->have_profile = have_profile ? 1U : 0U;
    if(!have_profile) app->profile = 0U;
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    if(!app->dispatcher || !app->view) goto fail;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, dndjournal_navigation);
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(JournalApp*));
    JournalApp** model = view_get_model(app->view);
    if(!model) goto fail;
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, dndjournal_draw);
    view_set_input_callback(app->view, dndjournal_input);
    if(!app->have_profile)
        dndjournal_set_status(app, "No character; Back to DNDolphins");
    else if(!dndjournal_load(app))
        dndjournal_set_status(app, "Partial load/memory");
    app->screen = JournalScreenList;
    dndjournal_prepare_list_window(app);
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

static void dndjournal_app_free(JournalApp* app) {
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
    JournalApp* app = dndjournal_app_alloc(context);
    if(!app) return -1;
    view_dispatcher_switch_to_view(app->dispatcher, JournalViewMain);
    view_dispatcher_run(app->dispatcher);
    uint8_t launch_adventure = app->launch_adventure;
    uint8_t return_to_dnd = !launch_adventure && app->return_to_dnd;
    uint32_t profile = app->profile;
    dndjournal_app_free(app);
    if(launch_adventure || return_to_dnd) {
        char args[32];
        const char* launch_args = NULL;
        const char* launch_path = DNDOLPHINS_FAP_PATH;
        if(launch_adventure) {
            UNUSED(profile);
            dndjournal_copy(args, sizeof(args), POCKET_D20_HANDOFF_ADVENTURE_CONTINUE);
            launch_args = args;
            launch_path = DNDADVENTURE_FAP_PATH;
        }
        if(launch_adventure) {
            if(!dnd_handoff_launch(launch_path, launch_args)) return -1;
        } else {
            (void)dnd_handoff_launch_if_present(
                launch_path, POCKET_D20_RETURN_FOCUS_JOURNAL);
        }
    }
    return 0;
}
