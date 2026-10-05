#include "dnd_storage.h"
#include "dnd_fs.h"
#include "dnd_profile_handoff.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DND_STORAGE_TEXT_VERSION        5U
#define DND_STORAGE_VALUE_LINE_LEN      320U
#define DND_STORAGE_ENCODED_LINE_LEN    ((DND_DETAIL_LEN * 3U) + 64U)
#define DND_STORAGE_FORMAT_LINE_LEN     256U
#define DND_STORAGE_READ_BUFFER         256U
#define DND_STORAGE_COLLECTION_LINE_LEN 1280U
#define DND_STORAGE_ITEM_SEED_LINE_LEN  256U
#define DND_STORAGE_DATA_DIR            DND_CHARACTER_DATA_ROOT
#define DND_STORAGE_EXPORT_DIR          DND_CHARACTER_DATA_ROOT "/exports"
#define DND_STORAGE_ARCHIVE_DIR         DND_CHARACTER_DATA_ROOT "/archive"

#define DND_STORAGE_LEGACY_PROFILE_DIR "/ext/apps_data/dungeons_and_dolphins/profiles"

#define DND_STORAGE_ACTIVE_PROFILE_PATH        DND_CHARACTER_DATA_ROOT "/custom_active_profile.txt"
#define DND_STORAGE_ACTIVE_PROFILE_TEMP_PATH   DND_CHARACTER_DATA_ROOT "/custom_active_profile.tmp"
#define DND_STORAGE_ACTIVE_PROFILE_BACKUP_PATH DND_CHARACTER_DATA_ROOT "/custom_active_profile.bak"

static void dnd_storage_copy(char* destination, size_t size, const char* source) {
    if(size == 0U) return;
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

static bool dnd_storage_parse_u32_span(
    const char* begin,
    const char* end,
    uint32_t maximum,
    uint32_t* output) {
    if(!begin || !end || !output || begin >= end) return false;
    uint32_t value = 0U;
    for(const char* cursor = begin; cursor < end; ++cursor) {
        if(*cursor < '0' || *cursor > '9') return false;
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(value > maximum / 10U || (value == maximum / 10U && digit > maximum % 10U))
            return false;
        value = value * 10U + digit;
    }
    *output = value;
    return true;
}

static bool dnd_storage_parse_u32_range(const char* text, uint32_t maximum, uint32_t* output) {
    return text && dnd_storage_parse_u32_span(text, text + strlen(text), maximum, output);
}

static bool dnd_storage_publish_temp(
    Storage* storage,
    const char* temporary,
    const char* destination,
    const char* backup) {
    if(!storage || !temporary || !destination || !backup) return false;
    bool had_destination = storage_file_exists(storage, destination);
    if(had_destination) {
        if(storage_file_exists(storage, backup) &&
           storage_common_remove(storage, backup) != FSE_OK)
            return false;
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

static bool dnd_storage_copy_file(
    Storage* storage,
    const char* source,
    const char* destination,
    const char* temporary);

static uint8_t dnd_storage_character_level(const DndCharacter* character) {
    uint16_t total = 0U;
    for(uint8_t i = 0U; i < character->class_count; ++i)
        total += character->classes[i].level;
    if(total < 1U) total = 1U;
    return total > 255U ? 255U : (uint8_t)total;
}

static void dnd_storage_filename_name(char* output, size_t size, const char* name) {
    if(size == 0U) return;
    size_t position = 0U;
    for(size_t i = 0U; name[i] && position + 1U < size; ++i) {
        char value = name[i];
        if((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '-') {
            output[position++] = value;
        } else if(position && output[position - 1U] != '_') {
            output[position++] = '_';
        }
    }
    while(position && output[position - 1U] == '_')
        --position;
    if(position == 0U) {
        dnd_storage_copy(output, size, "Unnamed");
        return;
    }
    output[position] = '\0';
}

static void dnd_storage_profile_path(
    char* output,
    size_t size,
    uint32_t profile,
    const DndCharacter* character) {
    char safe_name[DND_CHARACTER_NAME_LEN];
    dnd_storage_filename_name(safe_name, sizeof(safe_name), character->name);
    snprintf(
        output,
        size,
        "%s/ch_%lu_%s_%u.txt",
        DND_STORAGE_DATA_DIR,
        (unsigned long)profile,
        safe_name,
        dnd_storage_character_level(character));
}

static bool dnd_storage_writef(File* file, const char* format, ...) {
    char line[DND_STORAGE_FORMAT_LINE_LEN];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if(length < 0 || (size_t)length >= sizeof(line)) return false;
    return storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static uint8_t dnd_storage_hex_value(char value) {
    if(value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if(value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
    if(value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    return 0xFFU;
}

static bool dnd_storage_write_string(File* file, const char* key, const char* value) {
    if(!file || !key || !value) return false;
    const size_t key_length = strlen(key);
    if(storage_file_write(file, key, key_length) != key_length ||
       storage_file_write(file, "=", 1U) != 1U)
        return false;
    static const char digits[] = "0123456789ABCDEF";
    char chunk[64];
    size_t used = 0U;
    for(size_t i = 0U; value[i] != '\0'; ++i) {
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

static void dnd_storage_decode_string(char* destination, size_t size, const char* value) {
    size_t output = 0U;
    for(size_t input = 0U; value[input] != '\0' && output + 1U < size; ++input) {
        if(value[input] == '%' && value[input + 1U] && value[input + 2U]) {
            uint8_t high = dnd_storage_hex_value(value[input + 1U]);
            uint8_t low = dnd_storage_hex_value(value[input + 2U]);
            if(high != 0xFFU && low != 0xFFU) {
                destination[output++] = (char)((high << 4U) | low);
                input += 2U;
                continue;
            }
        }
        destination[output++] = value[input];
    }
    if(size) destination[output] = '\0';
}

static bool
    dnd_storage_write_i8_array(File* file, const char* key, const int8_t* values, size_t count) {
    char line[256];
    size_t position = (size_t)snprintf(line, sizeof(line), "%s=", key);
    for(size_t i = 0U; i < count; ++i) {
        int written =
            snprintf(line + position, sizeof(line) - position, "%s%d", i ? "," : "", values[i]);
        if(written < 0 || (size_t)written >= sizeof(line) - position) return false;
        position += (size_t)written;
    }
    line[position++] = '\n';
    return storage_file_write(file, line, position) == position;
}

static bool
    dnd_storage_write_u8_array(File* file, const char* key, const uint8_t* values, size_t count) {
    char line[256];
    size_t position = (size_t)snprintf(line, sizeof(line), "%s=", key);
    for(size_t i = 0U; i < count; ++i) {
        int written = snprintf(
            line + position,
            sizeof(line) - position,
            "%s%u",
            i ? "," : "",
            (unsigned int)values[i]);
        if(written < 0 || (size_t)written >= sizeof(line) - position) return false;
        position += (size_t)written;
    }
    line[position++] = '\n';
    return storage_file_write(file, line, position) == position;
}

typedef struct {
    File* file;
    uint8_t buffer[DND_STORAGE_READ_BUFFER];
    uint16_t position;
    uint16_t count;
    uint32_t raw_offset;
    bool eof;
} DndDolphinsReader;

static void
    dnd_storage_reader_init_at(DndDolphinsReader* reader, File* file, uint32_t raw_offset) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
    reader->raw_offset = raw_offset;
}

static void dnd_storage_reader_init(DndDolphinsReader* reader, File* file) {
    dnd_storage_reader_init_at(reader, file, 0U);
}

static bool dnd_storage_reader_next(DndDolphinsReader* reader, char* value) {
    if(reader->position >= reader->count) {
        reader->count =
            (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
        reader->position = 0U;
        if(!reader->count) {
            reader->eof = true;
            return false;
        }
    }
    *value = (char)reader->buffer[reader->position++];
    if(reader->raw_offset != UINT32_MAX) ++reader->raw_offset;
    return true;
}

static bool dnd_storage_read_line(DndDolphinsReader* reader, char* line, size_t size) {
    size_t position = 0U;
    char character = '\0';
    while(position + 1U < size) {
        if(!dnd_storage_reader_next(reader, &character)) break;
        if(character == '\n') break;
        if(character != '\r') line[position++] = character;
    }
    line[position] = '\0';
    return position > 0U || character == '\n';
}

static bool dnd_storage_parse_i32_span(const char* begin, const char* end, int32_t* output) {
    if(!begin || !end || !output || begin >= end) return false;
    bool negative = false;
    if(*begin == '-') {
        negative = true;
        ++begin;
        if(begin >= end) return false;
    }
    uint32_t maximum = negative ? (uint32_t)INT32_MAX + 1U : (uint32_t)INT32_MAX;
    uint32_t value = 0U;
    for(const char* cursor = begin; cursor < end; ++cursor) {
        if(*cursor < '0' || *cursor > '9') return false;
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(value > maximum / 10U || (value == maximum / 10U && digit > maximum % 10U))
            return false;
        value = value * 10U + digit;
    }
    if(negative) {
        *output = value == (uint32_t)INT32_MAX + 1U ? INT32_MIN : -(int32_t)value;
    } else {
        *output = (int32_t)value;
    }
    return true;
}

static size_t dnd_storage_parse_numbers(const char* value, int32_t* numbers, size_t maximum) {
    if(!value || !numbers || !maximum || !value[0]) return 0U;
    size_t count = 0U;
    const char* cursor = value;
    while(count < maximum) {
        const char* separator = strchr(cursor, ',');
        const char* end = separator ? separator : cursor + strlen(cursor);
        if(!dnd_storage_parse_i32_span(cursor, end, &numbers[count])) return 0U;
        ++count;
        if(!separator) return count;
        cursor = separator + 1U;
        if(!cursor[0]) return 0U;
    }
    /* More fields than expected are malformed rather than silently ignored. */
    return cursor[0] ? 0U : count;
}

static bool dnd_storage_indexed_key(
    const char* key,
    const char* prefix,
    const char* suffix,
    uint8_t maximum,
    uint8_t* index) {
    if(!key || !prefix || !suffix || !index) return false;
    size_t prefix_length = strlen(prefix);
    if(strncmp(key, prefix, prefix_length) != 0) return false;
    const char* cursor = key + prefix_length;
    const char* digits = cursor;
    uint32_t value = 0U;
    while(*cursor >= '0' && *cursor <= '9') {
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(value > (UINT32_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
        ++cursor;
    }
    if(cursor == digits || strcmp(cursor, suffix) != 0 || value >= maximum) return false;
    *index = (uint8_t)value;
    return true;
}

static void dnd_storage_spellbook_path(char* output, size_t size, uint32_t profile) {
    snprintf(output, size, "%s/spellbook_%lu.txt", DND_STORAGE_DATA_DIR, (unsigned long)profile);
}

static bool dnd_storage_inventory_bag_is_main(const char* bag) {
    return !bag || !bag[0] || !strcmp(bag, "Main");
}

static bool dnd_storage_inventory_bag_is_group(const char* bag) {
    return bag && !strcmp(bag, "Group");
}

static void
    dnd_storage_items_bag_path(char* output, size_t size, uint32_t profile, const char* bag) {
    if(dnd_storage_inventory_bag_is_main(bag)) {
        snprintf(
            output, size, "%s/inventory_%lu.txt", DND_STORAGE_DATA_DIR, (unsigned long)profile);
        return;
    }
    char safe[DND_INVENTORY_BAG_NAME_LEN];
    dnd_storage_filename_name(safe, sizeof(safe), bag);
    snprintf(output, size, "%s/inv%s_%lu.txt", DND_STORAGE_DATA_DIR, safe, (unsigned long)profile);
}

static void dnd_storage_items_path(char* output, size_t size, uint32_t profile) {
    dnd_storage_items_bag_path(output, size, profile, "Main");
}

static void dnd_storage_collection_path(
    char* output,
    size_t size,
    uint32_t profile,
    const char* collection) {
    if(!strcmp(collection, "spellbook"))
        dnd_storage_spellbook_path(output, size, profile);
    else if(!strcmp(collection, "items"))
        dnd_storage_items_path(output, size, profile);
    else if(!strcmp(collection, "feats"))
        snprintf(output, size, "%s/feats_%lu.txt", DND_STORAGE_DATA_DIR, (unsigned long)profile);
    else
        snprintf(
            output,
            size,
            "%s/%s_%lu.txt",
            DND_STORAGE_DATA_DIR,
            collection,
            (unsigned long)profile);
}

/* Collection files are established only when a real write requires one.
   Appends then operate on an existing file, so Add New never depends on
   OPEN_ALWAYS semantics or a post-append reread to create its backing store. */
static bool
    dnd_storage_ensure_collection_sidecar(Storage* storage, const char* path, const char* header) {
    if(!storage || !path || !header) return false;

    /* Match the proven character-save path used before collection sidecars:
       ensure the known app-data directory, then let storage_file_open() be the
       authority. Do not gate writes through recursive stat()/mkdir checks on
       /ext; those checks can reject a valid mounted path before the file open is
       ever attempted on some firmware builds. */
    storage_common_mkdir(storage, DND_STORAGE_DATA_DIR);

    if(storage_file_exists(storage, path)) {
        File* existing = storage_file_alloc(storage);
        if(!existing) return false;
        bool opened = storage_file_open(existing, path, FSAM_READ, FSOM_OPEN_EXISTING);
        uint64_t size = opened ? storage_file_size(existing) : 0U;
        if(opened) storage_file_close(existing);
        storage_file_free(existing);
        if(!opened) return false;
        if(size) return true;

        /* An empty interrupted file contains no user records, so repairing only
           its collection header is safe. */
        File* repair = storage_file_alloc(storage);
        if(!repair) return false;
        bool success = storage_file_open(repair, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                       storage_file_write(repair, header, strlen(header)) == strlen(header) &&
                       storage_file_sync(repair);
        storage_file_close(repair);
        storage_file_free(repair);
        return success && storage_file_exists(storage, path);
    }

    /* Flipper storage writes throughout this project use CREATE_ALWAYS.
       CREATE_NEW proved unreliable for these first-use collection files on
       device, leaving the editor with a RAM-only record and no backing file.
       We already established that the destination does not exist above, so
       CREATE_ALWAYS cannot overwrite a live collection here. */
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool opened = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    bool success = opened && storage_file_write(file, header, strlen(header)) == strlen(header) &&
                   storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!success) {
        /* Remove only a file that this create attempt actually opened. A
           pre-existing non-file collision (for example a directory with this
           name) must not be deleted as collateral cleanup. */
        if(opened) (void)storage_common_remove(storage, path);
        return false;
    }
    return storage_file_exists(storage, path);
}

static bool dnd_storage_write_raw(File* file, const char* value);
static void dnd_storage_collection_snapshot_path(
    char* output,
    size_t size,
    uint32_t profile,
    const DndCharacter* character,
    const char* collection);
static File* dnd_storage_open_collection_snapshot(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const char* collection,
    char* snapshot,
    size_t snapshot_size,
    char* live,
    size_t live_size);

static bool dnd_storage_ensure_spellbook_sidecar(Storage* storage, uint32_t profile) {
    char path[DND_FS_PATH_LEN];
    dnd_storage_spellbook_path(path, sizeof(path), profile);
    return dnd_storage_ensure_collection_sidecar(storage, path, "DNDSpellbook=1\n");
}

static bool dnd_storage_ensure_items_sidecar(Storage* storage, uint32_t profile) {
    char path[DND_FS_PATH_LEN];
    dnd_storage_items_path(path, sizeof(path), profile);
    return dnd_storage_ensure_collection_sidecar(storage, path, "DNDItems=1\n");
}

static bool
    dnd_storage_ensure_items_bag_sidecar(Storage* storage, uint32_t profile, const char* bag) {
    if(dnd_storage_inventory_bag_is_main(bag))
        return dnd_storage_ensure_items_sidecar(storage, profile);
    char path[DND_FS_PATH_LEN];
    dnd_storage_items_bag_path(path, sizeof(path), profile, bag);
    if(storage_file_exists(storage, path)) return true;
    storage_common_mkdir(storage, DND_STORAGE_DATA_DIR);
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool opened = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    bool success = opened && dnd_storage_write_raw(file, "DNDItems=1\n") &&
                   dnd_storage_writef(file, "BagName=%s\n", bag ? bag : "") &&
                   storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!success) {
        /* Remove only an incomplete file created by this attempt; never remove
           a pre-existing non-file path that merely blocked file creation. */
        if(opened) (void)storage_common_remove(storage, path);
        return false;
    }
    return storage_file_exists(storage, path);
}

static File* dnd_storage_open_items_bag_snapshot(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const char* bag,
    char* snapshot,
    size_t snapshot_size,
    char* live,
    size_t live_size) {
    if(dnd_storage_inventory_bag_is_main(bag))
        return dnd_storage_open_collection_snapshot(
            storage, profile, owner, "items", snapshot, snapshot_size, live, live_size);
    dnd_storage_items_bag_path(live, live_size, profile, bag);
    char safe[DND_INVENTORY_BAG_NAME_LEN];
    dnd_storage_filename_name(safe, sizeof(safe), bag);
    char suffix[40];
    snprintf(suffix, sizeof(suffix), "items_%s", safe);
    dnd_storage_collection_snapshot_path(snapshot, snapshot_size, profile, owner, suffix);
    storage_common_mkdir(storage, DND_STORAGE_DATA_DIR);
    File* file = storage_file_alloc(storage);
    if(!file) return NULL;
    if(!storage_file_open(file, snapshot, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        return NULL;
    }
    return file;
}

static void dnd_storage_collection_snapshot_path(
    char* output,
    size_t size,
    uint32_t profile,
    const DndCharacter* character,
    const char* collection) {
    char safe_name[DND_CHARACTER_NAME_LEN];
    dnd_storage_filename_name(
        safe_name,
        sizeof(safe_name),
        character && character->name[0] ? character->name : "Unnamed");
    snprintf(
        output,
        size,
        "%s/ch_%lu_%s_%u_%s.shd",
        DND_STORAGE_DATA_DIR,
        (unsigned long)profile,
        safe_name,
        character ? dnd_storage_character_level(character) : 1U,
        collection);
}

static bool dnd_storage_write_raw(File* file, const char* value) {
    if(!file || !value) return false;
    size_t length = strlen(value);
    return storage_file_write(file, value, length) == length;
}

static bool dnd_storage_write_collection_field(File* file, const char* value) {
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

static uint8_t dnd_storage_split_collection_line(char* line, char** fields, uint8_t capacity) {
    if(!line || !fields || !capacity) return 0U;
    uint8_t count = 0U;
    char* cursor = line;
    while(count < capacity) {
        fields[count++] = cursor;
        char* separator = strchr(cursor, '|');
        if(!separator) break;
        *separator = '\0';
        cursor = separator + 1U;
    }
    return count;
}

static bool dnd_storage_close_synced_file(File* file, bool success) {
    if(file) {
        if(success) success = storage_file_sync(file);
        storage_file_close(file);
        storage_file_free(file);
    }
    return success;
}

static bool
    dnd_storage_copy_file_direct(Storage* storage, const char* source, const char* destination) {
    if(!storage || !source || !destination) return false;
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    if(!input || !output) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        return false;
    }
    bool success = storage_file_open(input, source, FSAM_READ, FSOM_OPEN_EXISTING) &&
                   storage_file_open(output, destination, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    uint8_t buffer[256];
    while(success) {
        size_t count = storage_file_read(input, buffer, sizeof(buffer));
        if(!count) break;
        success = storage_file_write(output, buffer, count) == count;
    }
    if(success) success = storage_file_get_error(input) == FSE_OK && storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    return success && storage_file_exists(storage, destination);
}

static File* dnd_storage_open_collection_snapshot(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const char* collection,
    char* snapshot,
    size_t snapshot_size,
    char* live,
    size_t live_size) {
    if(!storage || !owner || !collection || !snapshot || !live) return NULL;
    if(!strcmp(collection, "spellbook"))
        dnd_storage_spellbook_path(live, live_size, profile);
    else
        dnd_storage_items_path(live, live_size, profile);
    dnd_storage_collection_snapshot_path(snapshot, snapshot_size, profile, owner, collection);
    /* The collection and its SHD snapshot both live directly in the canonical
       DNDolphins data directory. Use the same best-effort mkdir pattern as the
       working character save path instead of the recursive parent validator. */
    storage_common_mkdir(storage, DND_STORAGE_DATA_DIR);
    File* file = storage_file_alloc(storage);
    if(!file) return NULL;
    if(!storage_file_open(file, snapshot, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        return NULL;
    }
    return file;
}

static bool dnd_storage_publish_collection_snapshot(
    Storage* storage,
    File* file,
    const char* snapshot,
    const char* live,
    bool success) {
    success = dnd_storage_close_synced_file(file, success);
    if(!success) return false;
    char temporary[DND_FS_LONG_PATH_LEN], backup[DND_FS_LONG_PATH_LEN];
    snprintf(temporary, sizeof(temporary), "%s.publish.tmp", live);
    snprintf(backup, sizeof(backup), "%s.publish.bak", live);
    if(!dnd_storage_copy_file_direct(storage, snapshot, temporary)) {
        storage_common_remove(storage, temporary);
        return false;
    }
    return dnd_storage_publish_temp(storage, temporary, live, backup);
}

static bool dnd_storage_copy_live_collection_to_snapshot(
    Storage* storage,
    const char* live,
    File* output,
    uint64_t* copied_size,
    bool* needs_separator) {
    if(copied_size) *copied_size = 0U;
    if(needs_separator) *needs_separator = false;
    if(!storage || !live || !output || !copied_size || !needs_separator) return false;
    if(!storage_file_exists(storage, live)) return true;

    File* input = storage_file_alloc(storage);
    if(!input) return false;
    bool success = storage_file_open(input, live, FSAM_READ, FSOM_OPEN_EXISTING);
    uint8_t buffer[256];
    char last = '\0';
    while(success) {
        size_t count = storage_file_read(input, buffer, sizeof(buffer));
        if(!count) break;
        if(storage_file_write(output, buffer, count) != count) {
            success = false;
            break;
        }
        *copied_size += count;
        last = (char)buffer[count - 1U];
    }
    if(success) success = storage_file_get_error(input) == FSE_OK;
    storage_file_close(input);
    storage_file_free(input);
    if(success && *copied_size) *needs_separator = last != '\n';
    return success;
}

static bool dnd_storage_write_spell_record(
    File* file,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max) {
    if(!file || !spell) return false;
    return dnd_storage_write_raw(file, "S|") &&
           dnd_storage_write_collection_field(file, spell->name) &&
           dnd_storage_write_raw(file, "|") &&
           dnd_storage_write_collection_field(file, spell->detail) &&
           dnd_storage_write_raw(file, "|") &&
           dnd_storage_write_collection_field(file, spell->stable_id) &&
           dnd_storage_write_raw(file, "|") &&
           dnd_storage_write_collection_field(file, spell->source) &&
           dnd_storage_write_raw(file, "|") &&
           dnd_storage_write_collection_field(file, spell->school) &&
           dnd_storage_write_raw(file, "|") &&
           dnd_storage_write_collection_field(file, spell->grant_name) &&
           dnd_storage_writef(
               file,
               "|%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
               spell->level,
               spell->class_index,
               spell->prepared,
               spell->ritual,
               known,
               always_prepared,
               free_casts_current,
               free_casts_max,
               spell->grant_source,
               spell->favorite);
}

static bool dnd_storage_parse_spell_record(
    char* line,
    DndSpell* spell,
    uint8_t* known,
    uint8_t* always_prepared,
    uint8_t* free_casts_current,
    uint8_t* free_casts_max) {
    if(!line || !spell || !known || !always_prepared || !free_casts_current || !free_casts_max)
        return false;
    char* fields[8];
    uint8_t field_count = dnd_storage_split_collection_line(line, fields, 8U);
    if(field_count != 8U || strcmp(fields[0], "S")) return false;
    int32_t n[10] = {0};
    uint8_t number_count = dnd_storage_parse_numbers(fields[7], n, 10U);
    if(number_count != 9U && number_count != 10U) return false;
    memset(spell, 0, sizeof(*spell));
    dnd_storage_decode_string(spell->name, sizeof(spell->name), fields[1]);
    dnd_storage_decode_string(spell->detail, sizeof(spell->detail), fields[2]);
    dnd_storage_decode_string(spell->stable_id, sizeof(spell->stable_id), fields[3]);
    dnd_storage_decode_string(spell->source, sizeof(spell->source), fields[4]);
    dnd_storage_decode_string(spell->school, sizeof(spell->school), fields[5]);
    dnd_storage_decode_string(spell->grant_name, sizeof(spell->grant_name), fields[6]);
    spell->level = (uint8_t)n[0];
    spell->class_index = (uint8_t)n[1];
    spell->prepared = n[2] ? 1U : 0U;
    spell->ritual = n[3] ? 1U : 0U;
    *known = n[4] ? 1U : 0U;
    *always_prepared = n[5] ? 1U : 0U;
    *free_casts_current = (uint8_t)n[6];
    *free_casts_max = (uint8_t)n[7];
    spell->grant_source = (uint8_t)n[8];
    spell->favorite = number_count >= 10U && n[9] ? 1U : 0U;
    if(spell->level > 9U) spell->level = 9U;
    if(spell->class_index >= DND_MAX_CLASSES) spell->class_index = 0U;
    if(*free_casts_max > 20U) *free_casts_max = 20U;
    if(*free_casts_current > *free_casts_max) *free_casts_current = *free_casts_max;
    if(spell->grant_source >= DndGrantSourceCount) spell->grant_source = DndGrantSpecies;
    return true;
}

static bool dnd_storage_write_item_record(File* file, const DndItem* item) {
    if(!file || !item) return false;
    return dnd_storage_write_raw(file, "I|") &&
           dnd_storage_write_collection_field(file, item->name) &&
           dnd_storage_write_raw(file, "|") &&
           dnd_storage_write_collection_field(file, item->detail) &&
           dnd_storage_write_raw(file, "|") &&
           dnd_storage_write_collection_field(file, item->ammunition_group) &&
           dnd_storage_writef(
               file,
               "|%d,%d,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%u,%d,%u\n",
               item->quantity,
               item->weight_tenths,
               item->equipped,
               item->attuned,
               item->is_weapon,
               item->attack_ability,
               item->proficient,
               item->magic_bonus,
               item->damage_dice,
               item->damage_die,
               item->versatile_die,
               item->use_versatile,
               item->damage_type,
               item->add_ability_damage,
               item->extra_dice,
               item->extra_die,
               item->weapon_properties,
               item->ammo_current,
               item->ammo_max,
               (int)item->container_index,
               item->charges_current,
               item->charges_max,
               item->armor_base,
               item->armor_dex_cap,
               item->shield_bonus);
}

static bool dnd_storage_parse_item_record(char* line, DndItem* item) {
    if(!line || !item) return false;
    char* fields[5];
    uint8_t field_count = dnd_storage_split_collection_line(line, fields, 5U);
    if(field_count != 5U || strcmp(fields[0], "I")) return false;
    int32_t n[25];
    if(dnd_storage_parse_numbers(fields[4], n, 25U) != 25U) return false;
    memset(item, 0, sizeof(*item));
    dnd_storage_decode_string(item->name, sizeof(item->name), fields[1]);
    dnd_storage_decode_string(item->detail, sizeof(item->detail), fields[2]);
    dnd_storage_decode_string(item->ammunition_group, sizeof(item->ammunition_group), fields[3]);
    item->quantity = (int16_t)n[0];
    item->weight_tenths = (int16_t)n[1];
    item->equipped = n[2] ? 1U : 0U;
    item->attuned = n[3] ? 1U : 0U;
    item->is_weapon = n[4] ? 1U : 0U;
    item->attack_ability = (uint8_t)n[5];
    item->proficient = n[6] ? 1U : 0U;
    item->magic_bonus = (int8_t)n[7];
    item->damage_dice = (uint8_t)n[8];
    item->damage_die = (uint8_t)n[9];
    item->versatile_die = (uint8_t)n[10];
    item->use_versatile = n[11] ? 1U : 0U;
    item->damage_type = (uint8_t)n[12];
    item->add_ability_damage = n[13] ? 1U : 0U;
    item->extra_dice = (uint8_t)n[14];
    item->extra_die = (uint8_t)n[15];
    item->weapon_properties = (uint16_t)n[16];
    item->ammo_current = (int16_t)n[17];
    item->ammo_max = (int16_t)n[18];
    item->container_index = n[19];
    item->charges_current = (int16_t)n[20];
    item->charges_max = (int16_t)n[21];
    item->armor_base = (uint8_t)n[22];
    item->armor_dex_cap = (int8_t)n[23];
    item->shield_bonus = (uint8_t)n[24];
    if(item->attack_ability > DndAttackAbilityBest) item->attack_ability = DndAttackAbilityAuto;
    if(item->damage_type >= DndDamageTypeCount) item->damage_type = DndDamageBludgeoning;
    if(item->damage_dice > 20U) item->damage_dice = 20U;
    if(item->extra_dice > 20U) item->extra_dice = 20U;
    if(item->container_index < -1) item->container_index = -1;
    if(item->armor_dex_cap < -1 || item->armor_dex_cap > 9) item->armor_dex_cap = -1;
    return true;
}

bool dnd_storage_visit_spells(
    Storage* storage,
    uint32_t profile,
    DndDolphinsSpellRecordVisitor visitor,
    void* context,
    uint16_t* total_count) {
    if(!storage) return false;
    if(total_count) *total_count = 0U;
    char path[DND_FS_PATH_LEN];
    dnd_storage_spellbook_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    char* line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    DndDolphinsReader reader;
    dnd_storage_reader_init(&reader, file);
    bool success = true;
    uint16_t logical = 0U;
    while(dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) {
        if(strncmp(line, "S|", 2U)) continue;
        DndSpell parsed;
        uint8_t known = 0U, always = 0U, free_current = 0U, free_max = 0U;
        if(!dnd_storage_parse_spell_record(
               line, &parsed, &known, &always, &free_current, &free_max))
            continue;
        bool keep_scanning = true;
        if(visitor)
            keep_scanning =
                visitor(logical, &parsed, known, always, free_current, free_max, context);
        if(logical < UINT16_MAX) ++logical;
        if(!keep_scanning) break;
    }
    if(storage_file_get_error(file) != FSE_OK) success = false;
    if(total_count) *total_count = logical;
    free(line);
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

static bool dnd_storage_load_spellbook_window_internal(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_STORAGE_COLLECTION_PAGE_COUNT],
    uint8_t* valid_pages) {
    if(!storage || !character || !total_count) return false;
    dnd_data_clear_spells(character);

    const uint16_t page_index = start / DND_STORAGE_COLLECTION_CACHE_SIZE;
    const bool indexed = page_offsets && valid_pages;
    const bool direct = indexed && page_index < DND_STORAGE_COLLECTION_PAGE_COUNT &&
                        *valid_pages > page_index && *total_count >= start;
    const uint16_t known_total = *total_count;
    if(!direct) {
        *total_count = 0U;
        if(indexed) {
            memset(page_offsets, 0, sizeof(uint32_t) * DND_STORAGE_COLLECTION_PAGE_COUNT);
            *valid_pages = 0U;
        }
    }

    char path[DND_FS_PATH_LEN];
    dnd_storage_spellbook_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    uint32_t initial_offset = 0U;
    if(direct) {
        initial_offset = page_offsets[page_index];
        if(initial_offset && !storage_file_seek(file, initial_offset, true)) {
            storage_file_close(file);
            storage_file_free(file);
            return false;
        }
    }

    char* line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    DndDolphinsReader reader;
    dnd_storage_reader_init_at(&reader, file, initial_offset);
    bool success = true;
    uint16_t logical = direct ? start : 0U;
    const uint16_t window_end = start + DND_STORAGE_COLLECTION_CACHE_SIZE;
    while(logical < UINT16_MAX) {
        uint32_t line_offset = reader.raw_offset;
        if(!dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) break;
        if(strncmp(line, "S|", 2U)) continue;
        DndSpell parsed;
        uint8_t known = 0U, always = 0U, free_current = 0U, free_max = 0U;
        if(!dnd_storage_parse_spell_record(
               line, &parsed, &known, &always, &free_current, &free_max))
            continue;

        if(!direct && indexed && (logical % DND_STORAGE_COLLECTION_CACHE_SIZE) == 0U) {
            uint16_t discovered_page = logical / DND_STORAGE_COLLECTION_CACHE_SIZE;
            if(discovered_page < DND_STORAGE_COLLECTION_PAGE_COUNT) {
                page_offsets[discovered_page] = line_offset;
                if(*valid_pages <= discovered_page) *valid_pages = (uint8_t)(discovered_page + 1U);
            }
        }

        if(logical >= start && logical < window_end) {
            if(character->spell_count >= character->spell_capacity &&
               !dnd_data_reserve_spells(character, (uint8_t)(character->spell_count + 1U))) {
                success = false;
                break;
            }
            uint8_t local = character->spell_count++;
            character->spells[local] = parsed;
            character->spell_known[local] = known;
            character->spell_always_prepared[local] = always;
            character->spell_free_casts_current[local] = free_current;
            character->spell_free_casts_max[local] = free_max;
        }
        ++logical;
        if(direct && logical >= window_end) break;
    }
    if(storage_file_get_error(file) != FSE_OK) success = false;
    *total_count = direct ? known_total : logical;
    free(line);
    storage_file_close(file);
    storage_file_free(file);
    if(!success) dnd_data_clear_spells(character);
    return success;
}

bool dnd_storage_load_spellbook_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count) {
    return dnd_storage_load_spellbook_window_internal(
        storage, profile, start, character, total_count, NULL, NULL);
}

bool dnd_storage_load_spellbook_window_indexed(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_STORAGE_COLLECTION_PAGE_COUNT],
    uint8_t* valid_pages) {
    return dnd_storage_load_spellbook_window_internal(
        storage, profile, start, character, total_count, page_offsets, valid_pages);
}

static bool dnd_storage_rewrite_spellbook(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    uint16_t replace_start,
    const DndCharacter* replacement,
    int32_t delete_index,
    const DndSpell* append_spell,
    uint8_t append_known,
    uint8_t append_always,
    uint8_t append_free_current,
    uint8_t append_free_max) {
    if(!owner) return false;
    char path[DND_FS_PATH_LEN], snapshot[DND_FS_PATH_LEN];
    File* output = dnd_storage_open_collection_snapshot(
        storage, profile, owner, "spellbook", snapshot, sizeof(snapshot), path, sizeof(path));
    if(!output) return false;
    bool success = dnd_storage_write_raw(output, "DNDSpellbook=1\n");
    File* input = NULL;
    char* line = NULL;
    uint16_t logical = 0U;
    if(success && storage_file_exists(storage, path)) {
        input = storage_file_alloc(storage);
        if(!input || !storage_file_open(input, path, FSAM_READ, FSOM_OPEN_EXISTING))
            success = false;
        if(success) {
            line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
            if(!line) success = false;
        }
        if(success) {
            DndDolphinsReader reader;
            dnd_storage_reader_init(&reader, input);
            while(success &&
                  dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) {
                if(!strncmp(line, "DNDSpellbook=", 13U)) continue;
                if(strncmp(line, "S|", 2U)) {
                    success = dnd_storage_write_raw(output, line) &&
                              dnd_storage_write_raw(output, "\n");
                    continue;
                }
                /* Logical indexes are based only on valid records, exactly like the
                   page loader. Preserve malformed/manual lines without allowing them
                   to shift which later valid spell is edited or deleted. */
                DndSpell parsed;
                uint8_t parsed_known = 0U, parsed_always = 0U;
                uint8_t parsed_free_current = 0U, parsed_free_max = 0U;
                if(!dnd_storage_parse_spell_record(
                       line,
                       &parsed,
                       &parsed_known,
                       &parsed_always,
                       &parsed_free_current,
                       &parsed_free_max)) {
                    success = dnd_storage_write_raw(output, line) &&
                              dnd_storage_write_raw(output, "\n");
                    continue;
                }
                if(delete_index >= 0 && logical == (uint16_t)delete_index) {
                    ++logical;
                    continue;
                }
                if(replacement && logical >= replace_start &&
                   logical < (uint16_t)(replace_start + replacement->spell_count)) {
                    uint8_t local = (uint8_t)(logical - replace_start);
                    success = dnd_storage_write_spell_record(
                        output,
                        &replacement->spells[local],
                        replacement->spell_known[local],
                        replacement->spell_always_prepared[local],
                        replacement->spell_free_casts_current[local],
                        replacement->spell_free_casts_max[local]);
                } else {
                    /* Parsing tokenizes the input line in-place, so re-emit a valid
                       untouched record from the parsed fields instead of copying the
                       now-split buffer. */
                    success = dnd_storage_write_spell_record(
                        output,
                        &parsed,
                        parsed_known,
                        parsed_always,
                        parsed_free_current,
                        parsed_free_max);
                }
                ++logical;
            }
            if(storage_file_get_error(input) != FSE_OK) success = false;
        }
    }
    if(input) {
        storage_file_close(input);
        storage_file_free(input);
    }
    free(line);
    /* A staged UI record may extend the resident replacement window past the
       current end of the live sidecar. Emit that tail here so an editor-first
       Add New is persisted by the ordinary window-save path. */
    if(success && replacement) {
        uint16_t replacement_end = (uint16_t)replace_start + replacement->spell_count;
        uint16_t append_from = logical > replace_start ? logical : replace_start;
        for(uint16_t logical_index = append_from; success && logical_index < replacement_end;
            ++logical_index) {
            uint8_t local = (uint8_t)(logical_index - replace_start);
            success = dnd_storage_write_spell_record(
                output,
                &replacement->spells[local],
                replacement->spell_known[local],
                replacement->spell_always_prepared[local],
                replacement->spell_free_casts_current[local],
                replacement->spell_free_casts_max[local]);
        }
    }
    if(success && append_spell)
        success = dnd_storage_write_spell_record(
            output,
            append_spell,
            append_known,
            append_always,
            append_free_current,
            append_free_max);
    return dnd_storage_publish_collection_snapshot(storage, output, snapshot, path, success);
}

bool dnd_storage_save_spellbook_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    const DndCharacter* character) {
    if(!storage || !character) return false;
    if(character->spell_count &&
       (!character->spells || !character->spell_known || !character->spell_always_prepared ||
        !character->spell_free_casts_current || !character->spell_free_casts_max))
        return false;
    return dnd_storage_rewrite_spellbook(
        storage, profile, character, start, character, -1, NULL, 0U, 0U, 0U, 0U);
}

bool dnd_storage_append_spell(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max) {
    if(!storage || !owner || !spell) return false;
    if(!dnd_storage_ensure_spellbook_sidecar(storage, profile)) return false;

    char live[DND_FS_PATH_LEN], snapshot[DND_FS_PATH_LEN];
    File* output = dnd_storage_open_collection_snapshot(
        storage, profile, owner, "spellbook", snapshot, sizeof(snapshot), live, sizeof(live));
    if(!output) return false;

    uint64_t copied_size = 0U;
    bool needs_separator = false;
    bool success = dnd_storage_copy_live_collection_to_snapshot(
        storage, live, output, &copied_size, &needs_separator);
    if(success && !copied_size) success = dnd_storage_write_raw(output, "DNDSpellbook=1\n");
    if(success && needs_separator) success = dnd_storage_write_raw(output, "\n");
    if(success)
        success = dnd_storage_write_spell_record(
            output, spell, known, always_prepared, free_casts_current, free_casts_max);
    return dnd_storage_publish_collection_snapshot(storage, output, snapshot, live, success);
}

bool dnd_storage_delete_spell(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    uint16_t index) {
    if(!storage || !owner) return false;
    return dnd_storage_rewrite_spellbook(
        storage, profile, owner, 0U, NULL, index, NULL, 0U, 0U, 0U, 0U);
}

static bool dnd_storage_rewrite_spellbook_maintenance(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    bool reset_free_casts,
    bool remap_classes,
    uint8_t removed_class) {
    if(!storage || !owner) return false;
    char live[DND_FS_PATH_LEN];
    dnd_storage_spellbook_path(live, sizeof(live), profile);
    if(!storage_file_exists(storage, live)) return true;

    char snapshot[DND_FS_PATH_LEN];
    File* output = dnd_storage_open_collection_snapshot(
        storage, profile, owner, "spellbook", snapshot, sizeof(snapshot), live, sizeof(live));
    if(!output) return false;
    bool success = dnd_storage_write_raw(output, "DNDSpellbook=1\n");
    File* input = NULL;
    char* line = NULL;
    if(success) {
        input = storage_file_alloc(storage);
        if(!input || !storage_file_open(input, live, FSAM_READ, FSOM_OPEN_EXISTING))
            success = false;
    }
    if(success) {
        line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
        if(!line) success = false;
    }
    if(success) {
        DndDolphinsReader reader;
        dnd_storage_reader_init(&reader, input);
        while(success && dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) {
            if(!strncmp(line, "DNDSpellbook=", 13U)) continue;
            if(strncmp(line, "S|", 2U)) {
                success = dnd_storage_write_raw(output, line) &&
                          dnd_storage_write_raw(output, "\n");
                continue;
            }
            DndSpell spell;
            uint8_t known = 0U, always = 0U, free_current = 0U, free_max = 0U;
            if(!dnd_storage_parse_spell_record(
                   line, &spell, &known, &always, &free_current, &free_max)) {
                success = dnd_storage_write_raw(output, line) &&
                          dnd_storage_write_raw(output, "\n");
                continue;
            }
            if(reset_free_casts) free_current = free_max;
            if(remap_classes) {
                if(spell.class_index == removed_class)
                    spell.class_index = 0U;
                else if(spell.class_index > removed_class)
                    --spell.class_index;
            }
            success = dnd_storage_write_spell_record(
                output, &spell, known, always, free_current, free_max);
        }
        if(storage_file_get_error(input) != FSE_OK) success = false;
    }
    if(input) {
        storage_file_close(input);
        storage_file_free(input);
    }
    free(line);
    return dnd_storage_publish_collection_snapshot(storage, output, snapshot, live, success);
}

bool dnd_storage_reset_spell_free_casts(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner) {
    return dnd_storage_rewrite_spellbook_maintenance(storage, profile, owner, true, false, 0U);
}

bool dnd_storage_remap_spell_classes(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    uint8_t removed_class) {
    return dnd_storage_rewrite_spellbook_maintenance(
        storage, profile, owner, false, true, removed_class);
}

bool dnd_storage_visit_items_bag(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    DndDolphinsItemRecordVisitor visitor,
    void* context,
    uint16_t* total_count) {
    if(!storage) return false;
    if(total_count) *total_count = 0U;
    char path[DND_FS_PATH_LEN];
    dnd_storage_items_bag_path(path, sizeof(path), profile, bag);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    char* line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    DndDolphinsReader reader;
    dnd_storage_reader_init(&reader, file);
    bool success = true;
    uint16_t logical = 0U;
    while(dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) {
        if(strncmp(line, "I|", 2U)) continue;
        DndItem parsed;
        if(!dnd_storage_parse_item_record(line, &parsed)) continue;
        bool keep_scanning = true;
        if(visitor) keep_scanning = visitor(logical, &parsed, context);
        if(logical < UINT16_MAX) ++logical;
        if(!keep_scanning) break;
    }
    if(storage_file_get_error(file) != FSE_OK) success = false;
    if(total_count) *total_count = logical;
    free(line);
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

bool dnd_storage_visit_items(
    Storage* storage,
    uint32_t profile,
    DndDolphinsItemRecordVisitor visitor,
    void* context,
    uint16_t* total_count) {
    return dnd_storage_visit_items_bag(storage, profile, "Main", visitor, context, total_count);
}

bool dnd_storage_items_exist_bag(Storage* storage, uint32_t profile, const char* bag) {
    if(!storage) return false;
    char path[DND_FS_PATH_LEN];
    dnd_storage_items_bag_path(path, sizeof(path), profile, bag);
    return storage_file_exists(storage, path);
}

bool dnd_storage_items_exist(Storage* storage, uint32_t profile) {
    return dnd_storage_items_exist_bag(storage, profile, "Main");
}

bool dnd_storage_remove_live_items(Storage* storage, uint32_t profile) {
    if(!storage) return false;
    char path[DND_FS_PATH_LEN];
    dnd_storage_items_path(path, sizeof(path), profile);
    return !storage_file_exists(storage, path) || storage_common_remove(storage, path) == FSE_OK;
}

static bool dnd_storage_write_inventory_currency(File* file, const int32_t currency[5]) {
    if(!file || !currency) return false;
    return dnd_storage_writef(
        file,
        "Currency=%ld,%ld,%ld,%ld,%ld\n",
        (long)currency[0],
        (long)currency[1],
        (long)currency[2],
        (long)currency[3],
        (long)currency[4]);
}

bool dnd_storage_load_inventory_currency(
    Storage* storage,
    uint32_t profile,
    int32_t currency[5],
    bool* found) {
    if(found) *found = false;
    if(currency) memset(currency, 0, 5U * sizeof(currency[0]));
    if(!storage || !currency) return false;
    char path[DND_FS_PATH_LEN];
    dnd_storage_items_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    char line[128];
    DndDolphinsReader reader;
    dnd_storage_reader_init(&reader, file);
    bool success = true;
    while(dnd_storage_read_line(&reader, line, sizeof(line))) {
        if(strncmp(line, "Currency=", 9U)) continue;
        int32_t parsed[5];
        if(dnd_storage_parse_numbers(line + 9U, parsed, 5U) == 5U) {
            memcpy(currency, parsed, sizeof(parsed));
            if(found) *found = true;
        }
        break;
    }
    if(storage_file_get_error(file) != FSE_OK) success = false;
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

bool dnd_storage_inventory_initial_grant_state(Storage* storage, uint32_t profile, uint8_t* state) {
    if(state) *state = 0U;
    if(!storage || !state) return false;
    char path[DND_FS_PATH_LEN];
    dnd_storage_items_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    char line[64];
    DndDolphinsReader reader;
    dnd_storage_reader_init(&reader, file);
    bool success = true;
    while(dnd_storage_read_line(&reader, line, sizeof(line))) {
        if(!strcmp(line, "InitialInventory=2")) {
            *state = 2U;
            break;
        }
        if(!strcmp(line, "InitialInventory=1")) *state = 1U;
    }
    if(storage_file_get_error(file) != FSE_OK) success = false;
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

bool dnd_storage_inventory_initial_granted(Storage* storage, uint32_t profile, bool* granted) {
    if(granted) *granted = false;
    if(!granted) return false;
    uint8_t state = 0U;
    if(!dnd_storage_inventory_initial_grant_state(storage, profile, &state)) return false;
    *granted = state != 0U;
    return true;
}

static int32_t dnd_storage_add_currency_saturated(int32_t current, int32_t addition) {
    int64_t total = (int64_t)current + addition;
    if(total > INT32_MAX) return INT32_MAX;
    if(total < INT32_MIN) return INT32_MIN;
    return (int32_t)total;
}

static bool dnd_storage_compose_item_asset(
    Storage* storage,
    const char* path,
    const char* match,
    File* output,
    uint16_t* item_count,
    int32_t currency[5]) {
    if(!storage || !path || !match || !output || !item_count || !currency) return false;
    if(!match[0] || !storage_file_exists(storage, path)) return true;
    File* input = storage_file_alloc(storage);
    if(!input) return false;
    if(!storage_file_open(input, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(input);
        return false;
    }
    char* line = malloc(DND_STORAGE_ITEM_SEED_LINE_LEN);
    if(!line) {
        storage_file_close(input);
        storage_file_free(input);
        return false;
    }
    DndDolphinsReader reader;
    dnd_storage_reader_init(&reader, input);
    bool success = true;
    while(success && dnd_storage_read_line(&reader, line, DND_STORAGE_ITEM_SEED_LINE_LEN)) {
        if(!line[0] || line[0] == '#') continue;
        char* separator = strchr(line, '|');
        if(!separator) continue;
        *separator = '\0';
        if(strcmp(line, match)) continue;
        char* payload = separator + 1U;
        if(!strncmp(payload, "C|", 2U)) {
            int32_t values[5];
            if(dnd_storage_parse_numbers(payload + 2U, values, 5U) != 5U) continue;
            for(uint8_t i = 0U; i < 5U; ++i)
                currency[i] = dnd_storage_add_currency_saturated(currency[i], values[i]);
        } else if(!strncmp(payload, "I|", 2U)) {
            DndItem item;
            if(!dnd_storage_parse_item_record(payload, &item)) continue;
            success = dnd_storage_write_item_record(output, &item);
            if(success) ++(*item_count);
        }
    }
    if(storage_file_get_error(input) != FSE_OK) success = false;
    free(line);
    storage_file_close(input);
    storage_file_free(input);
    return success;
}

bool dnd_storage_create_items_from_assets(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const DndDolphinsItemSeedAsset* assets,
    uint8_t asset_count,
    int32_t currency_total[5],
    bool* created) {
    if(created) *created = false;
    if(!storage || !owner || (!assets && asset_count) || !currency_total) return false;
    if(dnd_storage_items_exist(storage, profile)) return true;

    char live[DND_FS_PATH_LEN];
    dnd_storage_items_path(live, sizeof(live), profile);
    storage_common_mkdir(storage, DND_STORAGE_DATA_DIR);
    File* output = storage_file_alloc(storage);
    if(!output) return false;
    /* items_exist() above is the ownership guard. Use the same proven create
       mode as the rest of the app rather than CREATE_NEW, which can fail on
       first use before inventory_<id>.txt ever appears on the SD card. */
    bool opened = storage_file_open(output, live, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!opened) {
        storage_file_free(output);
        return false;
    }

    bool success = dnd_storage_write_raw(output, "DNDItems=1\n");
    uint16_t item_count = 0U;
    for(uint8_t index = 0U; success && index < asset_count; ++index) {
        if(!assets[index].path || !assets[index].match) {
            success = false;
            break;
        }
        success = dnd_storage_compose_item_asset(
            storage, assets[index].path, assets[index].match, output, &item_count, currency_total);
    }
    if(success) success = dnd_storage_write_inventory_currency(output, currency_total);
    if(success) success = dnd_storage_write_raw(output, "InitialInventory=1\n");
    if(success) success = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    if(!success) {
        /* The destination was confirmed absent before this create attempt.
           Remove only the incomplete regular file we successfully opened; a
           pre-existing directory/path collision must survive a failed open. */
        if(opened) (void)storage_common_remove(storage, live);
        return false;
    }
    if(created) *created = true;
    return true;
}

bool dnd_storage_regrant_items_from_assets(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const DndDolphinsItemSeedAsset* assets,
    uint8_t asset_count,
    const DndDolphinsItemSeedAsset* fallback_asset,
    int32_t currency_total[5],
    bool* applied) {
    if(currency_total) memset(currency_total, 0, 5U * sizeof(currency_total[0]));
    if(applied) *applied = false;
    if(!storage || !owner || (!assets && asset_count) || !currency_total || !applied) return false;

    char live[DND_FS_PATH_LEN];
    dnd_storage_items_path(live, sizeof(live), profile);
    if(!storage_file_exists(storage, live)) return false;

    int32_t current_currency[5] = {0, 0, 0, 0, 0};
    bool currency_found = false;
    if(!dnd_storage_load_inventory_currency(storage, profile, current_currency, &currency_found))
        return false;
    if(!currency_found) {
        current_currency[0] = owner->currency_cp;
        current_currency[1] = owner->currency_sp;
        current_currency[2] = owner->currency_ep;
        current_currency[3] = owner->currency_gp;
        current_currency[4] = owner->currency_pp;
    }

    uint16_t existing_items = 0U;
    if(!dnd_storage_visit_items(storage, profile, NULL, NULL, &existing_items)) return false;

    char snapshot[DND_FS_PATH_LEN], path[DND_FS_PATH_LEN];
    File* output = dnd_storage_open_collection_snapshot(
        storage, profile, owner, "items", snapshot, sizeof(snapshot), path, sizeof(path));
    if(!output) return false;

    bool success = dnd_storage_write_raw(output, "DNDItems=1\n");
    File* input = NULL;
    char* line = NULL;
    if(success) {
        input = storage_file_alloc(storage);
        if(!input || !storage_file_open(input, live, FSAM_READ, FSOM_OPEN_EXISTING))
            success = false;
    }
    if(success) {
        line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
        if(!line) success = false;
    }
    if(success) {
        DndDolphinsReader reader;
        dnd_storage_reader_init(&reader, input);
        while(success && dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) {
            if(!strncmp(line, "DNDItems=", 9U) || !strncmp(line, "Currency=", 9U) ||
               !strncmp(line, "InitialInventory=", 17U))
                continue;
            success = dnd_storage_write_raw(output, line) && dnd_storage_write_raw(output, "\n");
        }
        if(storage_file_get_error(input) != FSE_OK) success = false;
    }
    if(input) {
        storage_file_close(input);
        storage_file_free(input);
    }
    free(line);

    uint16_t item_count = existing_items;
    int32_t delta[5] = {0, 0, 0, 0, 0};
    for(uint8_t index = 0U; success && index < asset_count; ++index) {
        if(!assets[index].path || !assets[index].match) {
            success = false;
            break;
        }
        success = dnd_storage_compose_item_asset(
            storage, assets[index].path, assets[index].match, output, &item_count, delta);
    }

    bool changed = item_count > existing_items;
    for(uint8_t i = 0U; i < 5U; ++i)
        if(delta[i] != 0) changed = true;
    if(success && !changed && fallback_asset && fallback_asset->path && fallback_asset->match) {
        success = dnd_storage_compose_item_asset(
            storage, fallback_asset->path, fallback_asset->match, output, &item_count, delta);
        changed = item_count > existing_items;
        for(uint8_t i = 0U; i < 5U; ++i)
            if(delta[i] != 0) changed = true;
    }

    int32_t combined[5];
    for(uint8_t i = 0U; i < 5U; ++i) {
        combined[i] = dnd_storage_add_currency_saturated(current_currency[i], delta[i]);
    }
    if(success && changed) success = dnd_storage_write_inventory_currency(output, combined);
    if(success && changed) success = dnd_storage_write_raw(output, "InitialInventory=2\n");
    if(!changed) success = false;
    success = dnd_storage_publish_collection_snapshot(storage, output, snapshot, path, success);
    if(success) {
        memcpy(currency_total, combined, sizeof(combined));
        *applied = true;
    } else {
        memset(currency_total, 0, 5U * sizeof(currency_total[0]));
    }
    return success;
}

static bool dnd_storage_load_items_window_internal(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_STORAGE_COLLECTION_PAGE_COUNT],
    uint8_t* valid_pages) {
    if(!storage || !character || !total_count) return false;
    dnd_data_clear_items(character);

    const uint16_t page_index = start / DND_STORAGE_COLLECTION_CACHE_SIZE;
    const bool indexed = page_offsets && valid_pages;
    const bool direct = indexed && page_index < DND_STORAGE_COLLECTION_PAGE_COUNT &&
                        *valid_pages > page_index && *total_count >= start;
    const uint16_t known_total = *total_count;
    if(!direct) {
        *total_count = 0U;
        if(indexed) {
            memset(page_offsets, 0, sizeof(uint32_t) * DND_STORAGE_COLLECTION_PAGE_COUNT);
            *valid_pages = 0U;
        }
    }

    char path[DND_FS_PATH_LEN];
    dnd_storage_items_bag_path(path, sizeof(path), profile, bag);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    uint32_t initial_offset = 0U;
    if(direct) {
        initial_offset = page_offsets[page_index];
        if(initial_offset && !storage_file_seek(file, initial_offset, true)) {
            storage_file_close(file);
            storage_file_free(file);
            return false;
        }
    }

    char* line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    DndDolphinsReader reader;
    dnd_storage_reader_init_at(&reader, file, initial_offset);
    bool success = true;
    uint16_t logical = direct ? start : 0U;
    const uint16_t window_end = start + DND_STORAGE_COLLECTION_CACHE_SIZE;
    while(logical < UINT16_MAX) {
        uint32_t line_offset = reader.raw_offset;
        if(!dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) break;
        if(strncmp(line, "I|", 2U)) continue;
        DndItem parsed;
        if(!dnd_storage_parse_item_record(line, &parsed)) continue;

        if(!direct && indexed && (logical % DND_STORAGE_COLLECTION_CACHE_SIZE) == 0U) {
            uint16_t discovered_page = logical / DND_STORAGE_COLLECTION_CACHE_SIZE;
            if(discovered_page < DND_STORAGE_COLLECTION_PAGE_COUNT) {
                page_offsets[discovered_page] = line_offset;
                if(*valid_pages <= discovered_page) *valid_pages = (uint8_t)(discovered_page + 1U);
            }
        }

        if(logical >= start && logical < window_end) {
            if(character->item_count >= character->item_capacity &&
               !dnd_data_reserve_items(character, (uint8_t)(character->item_count + 1U))) {
                success = false;
                break;
            }
            character->items[character->item_count++] = parsed;
        }
        ++logical;
        if(direct && logical >= window_end) break;
    }
    if(storage_file_get_error(file) != FSE_OK) success = false;
    *total_count = direct ? known_total : logical;
    free(line);
    storage_file_close(file);
    storage_file_free(file);
    if(!success) dnd_data_clear_items(character);
    return success;
}

bool dnd_storage_load_items_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count) {
    return dnd_storage_load_items_window_internal(
        storage, profile, "Main", start, character, total_count, NULL, NULL);
}

bool dnd_storage_load_items_window_indexed(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_STORAGE_COLLECTION_PAGE_COUNT],
    uint8_t* valid_pages) {
    return dnd_storage_load_items_window_internal(
        storage, profile, "Main", start, character, total_count, page_offsets, valid_pages);
}

bool dnd_storage_load_items_window_indexed_bag(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_STORAGE_COLLECTION_PAGE_COUNT],
    uint8_t* valid_pages) {
    return dnd_storage_load_items_window_internal(
        storage, profile, bag, start, character, total_count, page_offsets, valid_pages);
}

static bool dnd_storage_rewrite_items(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    const DndCharacter* owner,
    uint16_t replace_start,
    const DndCharacter* replacement,
    int32_t delete_index,
    const DndItem* append_item) {
    if(!owner) return false;
    int32_t currency[5] = {0, 0, 0, 0, 0};
    bool currency_found = false;
    if(dnd_storage_inventory_bag_is_main(bag) &&
       !dnd_storage_load_inventory_currency(storage, profile, currency, &currency_found))
        return false;
    /* Inventory sidecar currency is authoritative. Other FAPs preserve the
       absence of Currency=; DNDInventory creates that metadata when it opens
       an item-only sidecar. */
    char path[DND_FS_PATH_LEN], snapshot[DND_FS_PATH_LEN];
    File* output = dnd_storage_open_items_bag_snapshot(
        storage, profile, owner, bag, snapshot, sizeof(snapshot), path, sizeof(path));
    if(!output) return false;
    bool success = dnd_storage_write_raw(output, "DNDItems=1\n");
    if(success && !dnd_storage_inventory_bag_is_main(bag))
        success = dnd_storage_writef(output, "BagName=%s\n", bag ? bag : "");
    if(success && currency_found) success = dnd_storage_write_inventory_currency(output, currency);
    File* input = NULL;
    char* line = NULL;
    uint16_t logical = 0U;
    if(success && storage_file_exists(storage, path)) {
        input = storage_file_alloc(storage);
        if(!input || !storage_file_open(input, path, FSAM_READ, FSOM_OPEN_EXISTING))
            success = false;
        if(success) {
            line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
            if(!line) success = false;
        }
        if(success) {
            DndDolphinsReader reader;
            dnd_storage_reader_init(&reader, input);
            while(success &&
                  dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) {
                if(!strncmp(line, "DNDItems=", 9U)) continue;
                if(!strncmp(line, "Currency=", 9U)) continue;
                if(!strncmp(line, "BagName=", 8U)) continue;
                if(strncmp(line, "I|", 2U)) {
                    success = dnd_storage_write_raw(output, line) &&
                              dnd_storage_write_raw(output, "\n");
                    continue;
                }
                /* Keep malformed/manual item lines, but do not count them as logical
                   records when locating a page replacement or delete target. */
                DndItem parsed;
                if(!dnd_storage_parse_item_record(line, &parsed)) {
                    success = dnd_storage_write_raw(output, line) &&
                              dnd_storage_write_raw(output, "\n");
                    continue;
                }
                if(delete_index >= 0 && logical == (uint16_t)delete_index) {
                    ++logical;
                    continue;
                }
                if(delete_index >= 0) {
                    if(parsed.container_index == delete_index)
                        parsed.container_index = -1;
                    else if(parsed.container_index > delete_index)
                        --parsed.container_index;
                }
                if(replacement && logical >= replace_start &&
                   logical < (uint16_t)(replace_start + replacement->item_count)) {
                    uint8_t local = (uint8_t)(logical - replace_start);
                    success = dnd_storage_write_item_record(output, &replacement->items[local]);
                } else {
                    success = dnd_storage_write_item_record(output, &parsed);
                }
                ++logical;
            }
            if(storage_file_get_error(input) != FSE_OK) success = false;
        }
    }
    if(input) {
        storage_file_close(input);
        storage_file_free(input);
    }
    free(line);
    if(success && replacement) {
        uint16_t replacement_end = (uint16_t)replace_start + replacement->item_count;
        uint16_t append_from = logical > replace_start ? logical : replace_start;
        for(uint16_t logical_index = append_from; success && logical_index < replacement_end;
            ++logical_index) {
            uint8_t local = (uint8_t)(logical_index - replace_start);
            success = dnd_storage_write_item_record(output, &replacement->items[local]);
        }
    }
    if(success && append_item) success = dnd_storage_write_item_record(output, append_item);
    return dnd_storage_publish_collection_snapshot(storage, output, snapshot, path, success);
}

bool dnd_storage_save_items_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    const DndCharacter* character) {
    if(!storage || !character || (character->item_count && !character->items)) return false;
    return dnd_storage_rewrite_items(
        storage, profile, "Main", character, start, character, -1, NULL);
}

bool dnd_storage_save_items_window_bag(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    uint16_t start,
    const DndCharacter* character) {
    if(!storage || !character || (character->item_count && !character->items)) return false;
    return dnd_storage_rewrite_items(storage, profile, bag, character, start, character, -1, NULL);
}

bool dnd_storage_save_inventory_currency(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const int32_t currency[5]) {
    if(!storage || !owner || !currency) return false;
    char path[DND_FS_PATH_LEN], snapshot[DND_FS_PATH_LEN];
    File* output = dnd_storage_open_collection_snapshot(
        storage, profile, owner, "items", snapshot, sizeof(snapshot), path, sizeof(path));
    if(!output) return false;
    bool success = dnd_storage_write_raw(output, "DNDItems=1\n") &&
                   dnd_storage_write_inventory_currency(output, currency);
    File* input = NULL;
    char* line = NULL;
    if(success && storage_file_exists(storage, path)) {
        input = storage_file_alloc(storage);
        if(!input || !storage_file_open(input, path, FSAM_READ, FSOM_OPEN_EXISTING))
            success = false;
        if(success) {
            line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
            if(!line) success = false;
        }
        if(success) {
            DndDolphinsReader reader;
            dnd_storage_reader_init(&reader, input);
            while(success &&
                  dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) {
                if(!strncmp(line, "DNDItems=", 9U) || !strncmp(line, "Currency=", 9U)) continue;
                success = dnd_storage_write_raw(output, line) &&
                          dnd_storage_write_raw(output, "\n");
            }
            if(storage_file_get_error(input) != FSE_OK) success = false;
        }
    }
    if(input) {
        storage_file_close(input);
        storage_file_free(input);
    }
    free(line);
    return dnd_storage_publish_collection_snapshot(storage, output, snapshot, path, success);
}

static bool dnd_storage_append_items_bag_internal(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    const DndCharacter* owner,
    const DndItem* item,
    uint8_t count) {
    if(!storage || !owner || !item || !count) return false;
    if(!dnd_storage_ensure_items_bag_sidecar(storage, profile, bag)) return false;

    /* Append-only writers do not own Inventory metadata. Copy the sidecar byte
       for byte and add only the new item record; DNDInventory is solely
       responsible for creating Currency= when it later opens an item-only file. */
    char live[DND_FS_PATH_LEN], snapshot[DND_FS_PATH_LEN];
    File* output = dnd_storage_open_items_bag_snapshot(
        storage, profile, owner, bag, snapshot, sizeof(snapshot), live, sizeof(live));
    if(!output) return false;

    uint64_t copied_size = 0U;
    bool needs_separator = false;
    bool success = dnd_storage_copy_live_collection_to_snapshot(
        storage, live, output, &copied_size, &needs_separator);
    if(success && !copied_size) {
        success = dnd_storage_write_raw(output, "DNDItems=1\n");
        if(success && !dnd_storage_inventory_bag_is_main(bag))
            success = dnd_storage_writef(output, "BagName=%s\n", bag ? bag : "");
    }
    if(success && needs_separator) success = dnd_storage_write_raw(output, "\n");
    for(uint8_t i = 0U; success && i < count; ++i)
        success = dnd_storage_write_item_record(output, &item[i]);
    return dnd_storage_publish_collection_snapshot(storage, output, snapshot, live, success);
}

bool dnd_storage_append_items(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const DndItem* item,
    uint8_t count) {
    return dnd_storage_append_items_bag_internal(storage, profile, "Main", owner, item, count);
}

bool dnd_storage_append_item_bag(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    const DndCharacter* owner,
    const DndItem* item) {
    return dnd_storage_append_items_bag_internal(storage, profile, bag, owner, item, 1U);
}

bool dnd_storage_append_item(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const DndItem* item) {
    return dnd_storage_append_items(storage, profile, owner, item, 1U);
}

bool dnd_storage_delete_item_bag(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    const DndCharacter* owner,
    uint16_t index) {
    if(!storage || !owner) return false;
    return dnd_storage_rewrite_items(storage, profile, bag, owner, 0U, NULL, index, NULL);
}

static bool
    dnd_storage_item_selected(const uint8_t* selected_bits, uint16_t source_total, uint16_t index) {
    return selected_bits && index < source_total &&
           (selected_bits[index >> 3U] & (uint8_t)(1U << (index & 7U)));
}

static uint16_t dnd_storage_selected_rank_before(
    const uint8_t* selected_bits,
    uint16_t source_total,
    uint16_t index) {
    if(!selected_bits || !source_total || !index) return 0U;
    if(index > source_total) index = source_total;
    uint16_t count = 0U;
    for(uint16_t i = 0U; i < index; ++i)
        if(dnd_storage_item_selected(selected_bits, source_total, i)) ++count;
    return count;
}

static bool dnd_storage_publish_two_item_snapshots(
    Storage* storage,
    File* source_output,
    const char* source_snapshot,
    const char* source_live,
    File* destination_output,
    const char* destination_snapshot,
    const char* destination_live,
    bool success) {
    success = dnd_storage_close_synced_file(source_output, success);
    success = dnd_storage_close_synced_file(destination_output, success);
    if(!success) return false;

    char source_temp[DND_FS_LONG_PATH_LEN], destination_temp[DND_FS_LONG_PATH_LEN];
    char source_backup[DND_FS_LONG_PATH_LEN], destination_backup[DND_FS_LONG_PATH_LEN];
    snprintf(source_temp, sizeof(source_temp), "%s.move.tmp", source_live);
    snprintf(destination_temp, sizeof(destination_temp), "%s.move.tmp", destination_live);
    snprintf(source_backup, sizeof(source_backup), "%s.move.bak", source_live);
    snprintf(destination_backup, sizeof(destination_backup), "%s.move.bak", destination_live);

    (void)storage_common_remove(storage, source_temp);
    (void)storage_common_remove(storage, destination_temp);
    if(!dnd_storage_copy_file_direct(storage, source_snapshot, source_temp) ||
       !dnd_storage_copy_file_direct(storage, destination_snapshot, destination_temp)) {
        (void)storage_common_remove(storage, source_temp);
        (void)storage_common_remove(storage, destination_temp);
        return false;
    }

    /* Recover stale backups before beginning a new two-file transaction. */
    if(storage_file_exists(storage, source_backup)) {
        if(!storage_file_exists(storage, source_live))
            (void)storage_common_rename(storage, source_backup, source_live);
        else
            (void)storage_common_remove(storage, source_backup);
    }
    if(storage_file_exists(storage, destination_backup)) {
        if(!storage_file_exists(storage, destination_live))
            (void)storage_common_rename(storage, destination_backup, destination_live);
        else
            (void)storage_common_remove(storage, destination_backup);
    }

    bool source_had_live = storage_file_exists(storage, source_live);
    bool destination_had_live = storage_file_exists(storage, destination_live);
    if(source_had_live && storage_common_rename(storage, source_live, source_backup) != FSE_OK)
        return false;
    if(destination_had_live &&
       storage_common_rename(storage, destination_live, destination_backup) != FSE_OK) {
        if(source_had_live) (void)storage_common_rename(storage, source_backup, source_live);
        return false;
    }

    bool source_published = storage_common_rename(storage, source_temp, source_live) == FSE_OK;
    bool destination_published = false;
    if(source_published)
        destination_published =
            storage_common_rename(storage, destination_temp, destination_live) == FSE_OK;

    if(!source_published || !destination_published) {
        if(storage_file_exists(storage, source_live))
            (void)storage_common_remove(storage, source_live);
        if(storage_file_exists(storage, destination_live))
            (void)storage_common_remove(storage, destination_live);
        if(source_had_live) (void)storage_common_rename(storage, source_backup, source_live);
        if(destination_had_live)
            (void)storage_common_rename(storage, destination_backup, destination_live);
        (void)storage_common_remove(storage, source_temp);
        (void)storage_common_remove(storage, destination_temp);
        return false;
    }

    if(source_had_live) (void)storage_common_remove(storage, source_backup);
    if(destination_had_live) (void)storage_common_remove(storage, destination_backup);
    return true;
}

bool dnd_storage_move_items_bag_selected(
    Storage* storage,
    uint32_t profile,
    const char* source_bag,
    const char* destination_bag,
    const DndCharacter* owner,
    const uint8_t* selected_bits,
    uint16_t source_total,
    uint16_t* moved_count) {
    if(moved_count) *moved_count = 0U;
    if(!storage || !owner || !selected_bits || !source_total || !source_bag || !destination_bag ||
       !strcmp(source_bag, destination_bag))
        return false;

    uint16_t selected_count =
        dnd_storage_selected_rank_before(selected_bits, source_total, source_total);
    if(!selected_count) return false;
    if(!dnd_storage_ensure_items_bag_sidecar(storage, profile, source_bag) ||
       !dnd_storage_ensure_items_bag_sidecar(storage, profile, destination_bag))
        return false;

    uint16_t destination_total = 0U;
    if(!dnd_storage_visit_items_bag(
           storage, profile, destination_bag, NULL, NULL, &destination_total))
        return false;
    if((uint32_t)destination_total + selected_count > UINT16_MAX) return false;

    char source_live[DND_FS_PATH_LEN], source_snapshot[DND_FS_PATH_LEN];
    char destination_live[DND_FS_PATH_LEN], destination_snapshot[DND_FS_PATH_LEN];
    File* source_output = dnd_storage_open_items_bag_snapshot(
        storage,
        profile,
        owner,
        source_bag,
        source_snapshot,
        sizeof(source_snapshot),
        source_live,
        sizeof(source_live));
    if(!source_output) return false;
    File* destination_output = dnd_storage_open_items_bag_snapshot(
        storage,
        profile,
        owner,
        destination_bag,
        destination_snapshot,
        sizeof(destination_snapshot),
        destination_live,
        sizeof(destination_live));
    if(!destination_output) {
        dnd_storage_close_synced_file(source_output, false);
        return false;
    }

    uint64_t copied_size = 0U;
    bool needs_separator = false;
    bool success = dnd_storage_copy_live_collection_to_snapshot(
        storage, destination_live, destination_output, &copied_size, &needs_separator);
    if(success && !copied_size) {
        success = dnd_storage_write_raw(destination_output, "DNDItems=1\n");
        if(success && !dnd_storage_inventory_bag_is_main(destination_bag))
            success = dnd_storage_writef(
                destination_output, "BagName=%s\n", destination_bag ? destination_bag : "");
    }
    if(success && needs_separator) success = dnd_storage_write_raw(destination_output, "\n");

    File* input = NULL;
    char* line = NULL;
    uint16_t logical = 0U;
    if(success) {
        input = storage_file_alloc(storage);
        if(!input || !storage_file_open(input, source_live, FSAM_READ, FSOM_OPEN_EXISTING))
            success = false;
    }
    if(success) {
        line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
        if(!line) success = false;
    }
    if(success) {
        DndDolphinsReader reader;
        dnd_storage_reader_init(&reader, input);
        while(success && dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) {
            if(strncmp(line, "I|", 2U)) continue;
            DndItem item;
            if(!dnd_storage_parse_item_record(line, &item)) continue;
            if(dnd_storage_item_selected(selected_bits, source_total, logical)) {
                if(item.container_index >= 0) {
                    uint16_t container = (uint16_t)item.container_index;
                    if(container < source_total &&
                       dnd_storage_item_selected(selected_bits, source_total, container)) {
                        item.container_index = (int32_t)destination_total +
                                               dnd_storage_selected_rank_before(
                                                   selected_bits, source_total, container);
                    } else {
                        item.container_index = -1;
                    }
                }
                success = dnd_storage_write_item_record(destination_output, &item);
            }
            if(logical < UINT16_MAX) ++logical;
        }
        if(storage_file_get_error(input) != FSE_OK) success = false;
        if(logical != source_total) success = false;
    }
    if(input) {
        storage_file_close(input);
        storage_file_free(input);
        input = NULL;
    }
    free(line);
    line = NULL;

    /* Rewrite the source, preserving non-item metadata and malformed/manual lines. */
    logical = 0U;
    if(success) {
        input = storage_file_alloc(storage);
        if(!input || !storage_file_open(input, source_live, FSAM_READ, FSOM_OPEN_EXISTING))
            success = false;
    }
    if(success) {
        line = malloc(DND_STORAGE_COLLECTION_LINE_LEN);
        if(!line) success = false;
    }
    if(success) {
        DndDolphinsReader reader;
        dnd_storage_reader_init(&reader, input);
        while(success && dnd_storage_read_line(&reader, line, DND_STORAGE_COLLECTION_LINE_LEN)) {
            if(strncmp(line, "I|", 2U)) {
                success = dnd_storage_write_raw(source_output, line) &&
                          dnd_storage_write_raw(source_output, "\n");
                continue;
            }
            DndItem item;
            if(!dnd_storage_parse_item_record(line, &item)) {
                success = dnd_storage_write_raw(source_output, line) &&
                          dnd_storage_write_raw(source_output, "\n");
                continue;
            }
            bool selected = dnd_storage_item_selected(selected_bits, source_total, logical);
            if(!selected) {
                if(item.container_index >= 0) {
                    uint16_t container = (uint16_t)item.container_index;
                    if(container < source_total &&
                       dnd_storage_item_selected(selected_bits, source_total, container)) {
                        item.container_index = -1;
                    } else if(container < source_total) {
                        item.container_index =
                            (int32_t)container - dnd_storage_selected_rank_before(
                                                     selected_bits, source_total, container);
                    }
                }
                success = dnd_storage_write_item_record(source_output, &item);
            }
            if(logical < UINT16_MAX) ++logical;
        }
        if(storage_file_get_error(input) != FSE_OK) success = false;
        if(logical != source_total) success = false;
    }
    if(input) {
        storage_file_close(input);
        storage_file_free(input);
    }
    free(line);

    success = dnd_storage_publish_two_item_snapshots(
        storage,
        source_output,
        source_snapshot,
        source_live,
        destination_output,
        destination_snapshot,
        destination_live,
        success);
    if(success && moved_count) *moved_count = selected_count;
    return success;
}

bool dnd_storage_delete_item(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    uint16_t index) {
    return dnd_storage_delete_item_bag(storage, profile, "Main", owner, index);
}

static bool dnd_storage_inventory_nonmain_bag_filename(
    const char* filename,
    uint32_t profile,
    char* safe,
    size_t safe_size) {
    if(!filename || strncmp(filename, "inv", 3U) || !strncmp(filename, "inventory_", 10U))
        return false;
    char suffix[32];
    snprintf(suffix, sizeof(suffix), "_%lu.txt", (unsigned long)profile);
    size_t length = strlen(filename), suffix_len = strlen(suffix);
    if(length <= 3U + suffix_len || strcmp(filename + length - suffix_len, suffix)) return false;
    size_t name_len = length - 3U - suffix_len;
    if(!name_len || name_len >= safe_size) return false;
    memcpy(safe, filename + 3U, name_len);
    safe[name_len] = '\0';
    return true;
}

static bool dnd_storage_inventory_custom_bag_filename(
    const char* filename,
    uint32_t profile,
    char* safe,
    size_t safe_size) {
    return dnd_storage_inventory_nonmain_bag_filename(filename, profile, safe, safe_size) &&
           strcmp(safe, "Group") != 0;
}

static bool dnd_storage_inventory_read_bag_name(
    Storage* storage,
    const char* path,
    const char* fallback,
    char* name,
    size_t size) {
    dnd_storage_copy(name, size, fallback);
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    char line[64];
    DndDolphinsReader reader;
    dnd_storage_reader_init(&reader, file);
    while(dnd_storage_read_line(&reader, line, sizeof(line))) {
        if(strncmp(line, "BagName=", 8U)) continue;
        if(line[8]) dnd_storage_copy(name, size, line + 8U);
        break;
    }
    bool ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

uint8_t dnd_storage_inventory_bag_count(Storage* storage, uint32_t profile) {
    if(!storage) return 2U;
    uint8_t count = 2U;
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return count;
    }
    FileInfo info;
    char filename[128], safe[DND_INVENTORY_BAG_NAME_LEN];
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info) ||
           !dnd_storage_inventory_custom_bag_filename(filename, profile, safe, sizeof(safe)))
            continue;
        if(count < UINT8_MAX) ++count;
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return count;
}

bool dnd_storage_inventory_bag_at(
    Storage* storage,
    uint32_t profile,
    uint8_t index,
    char* name,
    size_t size) {
    if(!name || !size) return false;
    if(index == 0U) {
        dnd_storage_copy(name, size, "Main");
        return true;
    }
    if(index == 1U) {
        dnd_storage_copy(name, size, "Group");
        return true;
    }
    if(!storage) return false;
    uint8_t logical = 2U;
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    bool found = false;
    FileInfo info;
    char filename[128], safe[DND_INVENTORY_BAG_NAME_LEN], path[DND_FS_PATH_LEN];
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info) ||
           !dnd_storage_inventory_custom_bag_filename(filename, profile, safe, sizeof(safe)))
            continue;
        if(logical++ != index) continue;
        if(dnd_fs_child_path(path, sizeof(path), DND_STORAGE_DATA_DIR, NULL, filename))
            (void)dnd_storage_inventory_read_bag_name(storage, path, safe, name, size);
        found = true;
        break;
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return found;
}

bool dnd_storage_inventory_bag_create(Storage* storage, uint32_t profile, const char* name) {
    if(!storage || !name || !name[0] || !strcmp(name, "Main") || !strcmp(name, "Group"))
        return false;
    for(const char* p = name; *p; ++p)
        if(*p == '\n' || *p == '\r' || *p == '=') return false;
    char path[DND_FS_PATH_LEN];
    dnd_storage_items_bag_path(path, sizeof(path), profile, name);
    if(storage_file_exists(storage, path)) return false;
    return dnd_storage_ensure_items_bag_sidecar(storage, profile, name);
}

bool dnd_storage_inventory_bag_delete(Storage* storage, uint32_t profile, const char* name) {
    if(!storage || !name || dnd_storage_inventory_bag_is_main(name) ||
       dnd_storage_inventory_bag_is_group(name))
        return false;
    char path[DND_FS_PATH_LEN];
    dnd_storage_items_bag_path(path, sizeof(path), profile, name);
    return storage_file_exists(storage, path) && storage_common_remove(storage, path) == FSE_OK;
}

static bool dnd_storage_parse_profile_filename(const char* filename, DndProfileEntry* entry);

static void dnd_storage_prepare_character_load(DndSaveData* data) {
    dnd_data_clear(data);
    dnd_data_set_defaults(data);
    DndCharacter* c = &data->character;
    /* Spells, items, Features, and applied-grant history live outside the core
       character. Legacy embedded Feature/Grant rows are ignored by the parser
       and never allocated during profile load. */
    dnd_data_clear_spells(c);
    dnd_data_reserve_features_exact(c, 0U);
    dnd_data_clear_items(c);
    dnd_data_reserve_grants_exact(c, 0U);
    c->feature_count = 0U;
    c->grant_count = 0U;
}

static bool dnd_storage_write_character(File* file, const DndSaveData* data) {
    const DndCharacter* c = &data->character;
    /* Features and applied grants are lazy sidecars. Never serialize a resident
       feature page or transient grant-review batch into the core character. */
    const uint8_t feature_count = 0U;
    const uint8_t grant_count = 0U;
    char key[48];
    if(!dnd_storage_writef(file, "DNDolphinsCharacter=%u\n", DND_STORAGE_TEXT_VERSION) ||
       !dnd_storage_write_string(file, "Name", c->name) ||
       !dnd_storage_write_string(file, "Player", c->player) ||
       !dnd_storage_write_string(file, "Species", c->species) ||
       !dnd_storage_write_string(file, "Background", c->background) ||
       !dnd_storage_write_string(file, "Alignment", c->alignment) ||
       !dnd_storage_write_string(file, "OriginFeat", c->origin_feat) ||
       !dnd_storage_write_string(file, "Senses", c->senses) ||
       !dnd_storage_writef(file, "IdentityRules=%u\n", c->size) ||
       !dnd_storage_writef(
           file,
           "Progress=%u,%lu,%u,%u\n",
           c->class_count,
           (unsigned long)c->experience,
           c->milestone_leveling,
           c->inspiration))
        return false;

    for(uint8_t i = 0U; i < c->class_count; ++i) {
        snprintf(key, sizeof(key), "Class%uName", i);
        if(!dnd_storage_write_string(file, key, c->classes[i].name)) return false;
        snprintf(key, sizeof(key), "Class%uSubclass", i);
        if(!dnd_storage_write_string(file, key, c->classes[i].subclass) ||
           !dnd_storage_writef(
               file,
               "Class%uData=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
               i,
               c->classes[i].level,
               c->classes[i].hit_die,
               c->classes[i].hit_dice_current,
               c->classes[i].hit_dice_max,
               c->classes[i].spellcasting_mode,
               c->classes[i].spellcasting_ability,
               c->classes[i].cantrip_limit,
               c->classes[i].prepared_limit,
               c->classes[i].spellbook_size,
               c->classes[i].pact_slot_level,
               c->classes[i].pact_slots_current,
               c->classes[i].pact_slots_max,
               c->classes[i].mystic_arcanum_mask,
               c->classes[i].spell_points_current,
               c->classes[i].spell_points_max,
               0U))
            return false;
    }

    if(!dnd_storage_write_i8_array(file, "AbilityScores", c->ability_scores, DND_ABILITY_COUNT) ||
       !dnd_storage_write_u8_array(
           file, "SaveProficiency", c->saving_throw_proficiency, DND_ABILITY_COUNT) ||
       !dnd_storage_write_i8_array(file, "SaveMisc", c->saving_throw_misc, DND_ABILITY_COUNT) ||
       !dnd_storage_write_u8_array(
           file, "SkillProficiency", c->skill_proficiency, DND_SKILL_COUNT) ||
       !dnd_storage_write_i8_array(file, "SkillMisc", c->skill_misc, DND_SKILL_COUNT) ||
       !dnd_storage_writef(
           file,
           "Vitals=%d,%d,%d,%d,%d,%d,%u,%u,%u,%u,%u,%u\n",
           c->hp_current,
           c->hp_max,
           c->hp_temporary,
           c->armor_class,
           c->speed,
           c->initiative_misc,
           c->exhaustion,
           c->death_successes,
           c->death_failures,
           c->hit_die,
           c->hit_dice_current,
           c->hit_dice_max) ||
       !dnd_storage_writef(
           file,
           "Spellcasting=%u,%d,%d,%u\n",
           c->spellcasting_ability,
           c->spell_attack_misc,
           c->spell_save_misc,
           c->arcane_recovery_used) ||
       !dnd_storage_write_u8_array(
           file, "SpellSlotsCurrent", c->spell_slots_current, DND_SLOT_COUNT) ||
       !dnd_storage_write_u8_array(file, "SpellSlotsMax", c->spell_slots_max, DND_SLOT_COUNT))
        return false;

    if(!dnd_storage_writef(file, "FeatureCount=%u\n", feature_count)) return false;
    for(uint8_t i = 0U; i < feature_count; ++i) {
        snprintf(key, sizeof(key), "Feature%uName", i);
        if(!dnd_storage_write_string(file, key, c->features[i].name)) return false;
        snprintf(key, sizeof(key), "Feature%uDetail", i);
        if(!dnd_storage_write_string(file, key, c->features[i].detail) ||
           !dnd_storage_writef(
               file,
               "Feature%uData=%d,%d,%u,%u,%u,%u,%u\n",
               i,
               c->features[i].uses_current,
               c->features[i].uses_max,
               c->features[i].class_index,
               c->features[i].class_level_gained,
               c->features[i].recharge,
               c->features[i].resource_formula,
               c->features[i].resource_ability))
            return false;
    }

    if(!dnd_storage_write_string(file, "Conditions", c->conditions) ||
       !dnd_storage_write_string(file, "Concentration", c->concentration) ||
       !dnd_storage_write_string(file, "TemporaryEffects", c->temporary_effects) ||
       !dnd_storage_write_string(file, "Resistances", c->resistances) ||
       !dnd_storage_write_string(file, "Immunities", c->immunities) ||
       !dnd_storage_write_string(file, "Vulnerabilities", c->vulnerabilities) ||
       !dnd_storage_write_string(file, "MovementModes", c->movement_modes) ||
       !dnd_storage_writef(
           file,
           "CombatFlags=%u,%u,%d\n",
           c->reaction_available,
           c->encumbrance_mode,
           c->carrying_capacity_override) ||
       !dnd_storage_writef(file, "GrantCount=%u\n", grant_count))
        return false;

    for(uint8_t i = 0U; i < grant_count; ++i) {
        const DndGrant* grant = &c->grants[i];
        snprintf(key, sizeof(key), "Grant%uStableId", i);
        if(!dnd_storage_write_string(file, key, grant->stable_id)) return false;
        snprintf(key, sizeof(key), "Grant%uSource", i);
        if(!dnd_storage_write_string(file, key, grant->source)) return false;
        snprintf(key, sizeof(key), "Grant%uOption", i);
        if(!dnd_storage_write_string(file, key, grant->option_name)) return false;
        snprintf(key, sizeof(key), "Grant%uPrerequisites", i);
        if(!dnd_storage_write_string(file, key, grant->prerequisites)) return false;
        snprintf(key, sizeof(key), "Grant%uValue", i);
        if(!dnd_storage_write_string(file, key, grant->grant_value) ||
           !dnd_storage_writef(
               file,
               "Grant%uData=%u,%u,%u,%u\n",
               i,
               grant->source_type,
               grant->class_index,
               grant->level_gained,
               grant->status))
            return false;
    }

    if(!dnd_storage_writef(file, "AttackTemplateCount=%u\n", c->attack_template_count))
        return false;
    for(uint8_t i = 0U; i < c->attack_template_count; ++i) {
        const DndAttackTemplate* attack = &c->attack_templates[i];
        snprintf(key, sizeof(key), "AttackTemplate%uName", i);
        if(!dnd_storage_write_string(file, key, attack->name)) return false;
        snprintf(key, sizeof(key), "AttackTemplate%uMastery", i);
        if(!dnd_storage_write_string(file, key, attack->mastery)) return false;
        snprintf(key, sizeof(key), "AttackTemplate%uDamageType", i);
        if(!dnd_storage_write_string(file, key, attack->damage_type)) return false;
        snprintf(key, sizeof(key), "AttackTemplate%uRiderType", i);
        if(!dnd_storage_write_string(file, key, attack->rider_type) ||
           !dnd_storage_writef(
               file,
               "AttackTemplate%uData=%u,%u,%u,%d,%u,%u,%u,%u,%u\n",
               i,
               attack->type | (attack->recharge << 3U),
               attack->ability,
               attack->save_ability,
               attack->attack_misc,
               attack->save_dc,
               attack->damage_dice,
               attack->damage_die,
               attack->rider_dice,
               attack->rider_die))
            return false;
    }

    return dnd_storage_writef(file, "End=OK\n");
}

static bool dnd_storage_read_character(
    File* file,
    DndSaveData* data,
    const DndProfileEntry* fallback_entry) {
    if(!file || !data) return false;
    dnd_storage_prepare_character_load(data);
    DndCharacter* c = &data->character;
    /* Filename name metadata is a safe fallback for partial/older character files.
       A missing/malformed Name must never turn an existing character into the
       synthetic New Hero default on the next save. The filename level is total
       character level, so it is applied only later when no class-level records
       were recovered and the character remains single-class. */
    if(fallback_entry) dnd_storage_copy(c->name, sizeof(c->name), fallback_entry->name);
    DndDolphinsReader reader;
    dnd_storage_reader_init(&reader, file);
    char line[DND_STORAGE_ENCODED_LINE_LEN];
    int32_t n[32];
    /* A valid primary filename is itself usable recovery metadata. Even if the
       body contains only unknown/future fields, keep the filename name/level
       fallback instead of treating the readable character as a failed load. */
    bool recognized_data = fallback_entry != NULL;
    bool class_level_seen = false;

    while(dnd_storage_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        const char* key = line;
        uint8_t index = 0U;
        uint32_t number = 0U;

        /* Version is intentionally informational. A mismatched or malformed version
           never rejects fields that this build still recognizes. */
        if(!strcmp(key, "DNDolphinsCharacter") || !strcmp(key, "PocketD20Character") ||
           !strcmp(key, "End"))
            continue;

#define LOAD_STRING(field, name)                                  \
    if(!strcmp(key, name)) {                                      \
        dnd_storage_decode_string((field), sizeof(field), value); \
        recognized_data = true;                                   \
        continue;                                                 \
    }
        LOAD_STRING(c->name, "Name")
        LOAD_STRING(c->player, "Player")
        LOAD_STRING(c->species, "Species")
        LOAD_STRING(c->background, "Background")
        LOAD_STRING(c->alignment, "Alignment")
        LOAD_STRING(c->origin_feat, "OriginFeat")
        LOAD_STRING(c->senses, "Senses")
        LOAD_STRING(c->conditions, "Conditions")
        LOAD_STRING(c->concentration, "Concentration")
        LOAD_STRING(c->temporary_effects, "TemporaryEffects")
        LOAD_STRING(c->resistances, "Resistances")
        LOAD_STRING(c->immunities, "Immunities")
        LOAD_STRING(c->vulnerabilities, "Vulnerabilities")
        LOAD_STRING(c->movement_modes, "MovementModes")
#undef LOAD_STRING

        if(!strcmp(key, "IdentityRules")) {
            if(dnd_storage_parse_numbers(value, n, 1U) == 1U) {
                c->size = (uint8_t)n[0];
                recognized_data = true;
            }
            continue;
        }
        if(!strcmp(key, "Progress")) {
            size_t count = dnd_storage_parse_numbers(value, n, 4U);
            if(count >= 1U && n[0] > 0 && n[0] <= (int32_t)DND_MAX_CLASSES)
                c->class_count = (uint8_t)n[0];
            if(count >= 2U && n[1] >= 0) c->experience = (uint32_t)n[1];
            if(count >= 3U) c->milestone_leveling = n[2] ? 1U : 0U;
            if(count >= 4U) c->inspiration = n[3] ? 1U : 0U;
            if(count) recognized_data = true;
            continue;
        }
        if(dnd_storage_indexed_key(key, "Class", "Name", DND_MAX_CLASSES, &index)) {
            dnd_storage_decode_string(
                c->classes[index].name, sizeof(c->classes[index].name), value);
            if(c->class_count <= index) c->class_count = (uint8_t)(index + 1U);
            recognized_data = true;
            continue;
        }
        if(dnd_storage_indexed_key(key, "Class", "Subclass", DND_MAX_CLASSES, &index)) {
            dnd_storage_decode_string(
                c->classes[index].subclass, sizeof(c->classes[index].subclass), value);
            if(c->class_count <= index) c->class_count = (uint8_t)(index + 1U);
            recognized_data = true;
            continue;
        }
        if(dnd_storage_indexed_key(key, "Class", "Data", DND_MAX_CLASSES, &index)) {
            size_t count = dnd_storage_parse_numbers(value, n, 16U);
            DndClassLevel* cl = &c->classes[index];
            if(count >= 1U) {
                cl->level = (uint8_t)n[0];
                class_level_seen = true;
            }
            if(count >= 2U) cl->hit_die = (uint8_t)n[1];
            if(count >= 3U) cl->hit_dice_current = (uint8_t)n[2];
            if(count >= 4U) cl->hit_dice_max = (uint8_t)n[3];
            if(count >= 5U) cl->spellcasting_mode = (uint8_t)n[4];
            if(count >= 6U) cl->spellcasting_ability = (uint8_t)n[5];
            if(count >= 7U) cl->cantrip_limit = (uint8_t)n[6];
            if(count >= 8U) cl->prepared_limit = (uint8_t)n[7];
            if(count >= 9U) cl->spellbook_size = (uint16_t)n[8];
            if(count >= 10U) cl->pact_slot_level = (uint8_t)n[9];
            if(count >= 11U) cl->pact_slots_current = (uint8_t)n[10];
            if(count >= 12U) cl->pact_slots_max = (uint8_t)n[11];
            if(count >= 13U) cl->mystic_arcanum_mask = (uint16_t)n[12];
            if(count >= 14U) cl->spell_points_current = (uint16_t)n[13];
            if(count >= 15U) cl->spell_points_max = (uint16_t)n[14];
            if(count) {
                if(c->class_count <= index) c->class_count = (uint8_t)(index + 1U);
                recognized_data = true;
            }
            continue;
        }

#define LOAD_ARRAY(key_name, target, expected, cast_type)             \
    if(!strcmp(key, key_name)) {                                      \
        size_t count = dnd_storage_parse_numbers(value, n, expected); \
        if(count) {                                                   \
            for(size_t i = 0U; i < count && i < expected; ++i)        \
                (target)[i] = (cast_type)n[i];                        \
            recognized_data = true;                                   \
        }                                                             \
        continue;                                                     \
    }
        LOAD_ARRAY("AbilityScores", c->ability_scores, DND_ABILITY_COUNT, int8_t)
        LOAD_ARRAY("SaveProficiency", c->saving_throw_proficiency, DND_ABILITY_COUNT, uint8_t)
        LOAD_ARRAY("SaveMisc", c->saving_throw_misc, DND_ABILITY_COUNT, int8_t)
        LOAD_ARRAY("SkillProficiency", c->skill_proficiency, DND_SKILL_COUNT, uint8_t)
        LOAD_ARRAY("SkillMisc", c->skill_misc, DND_SKILL_COUNT, int8_t)
        LOAD_ARRAY("SpellSlotsCurrent", c->spell_slots_current, DND_SLOT_COUNT, uint8_t)
        LOAD_ARRAY("SpellSlotsMax", c->spell_slots_max, DND_SLOT_COUNT, uint8_t)
#undef LOAD_ARRAY

        if(!strcmp(key, "Vitals")) {
            size_t count = dnd_storage_parse_numbers(value, n, 12U);
            if(count >= 1U) c->hp_current = (int16_t)n[0];
            if(count >= 2U) c->hp_max = (int16_t)n[1];
            if(count >= 3U) c->hp_temporary = (int16_t)n[2];
            if(count >= 4U) c->armor_class = (int16_t)n[3];
            if(count >= 5U) c->speed = (int16_t)n[4];
            if(count >= 6U) c->initiative_misc = (int8_t)n[5];
            if(count >= 7U) c->exhaustion = (uint8_t)n[6];
            if(count >= 8U) c->death_successes = (uint8_t)n[7];
            if(count >= 9U) c->death_failures = (uint8_t)n[8];
            if(count >= 10U) c->hit_die = (uint8_t)n[9];
            if(count >= 11U) c->hit_dice_current = (uint8_t)n[10];
            if(count >= 12U) c->hit_dice_max = (uint8_t)n[11];
            if(count) recognized_data = true;
            continue;
        }
        if(!strcmp(key, "Spellcasting")) {
            size_t count = dnd_storage_parse_numbers(value, n, 4U);
            if(count >= 1U) c->spellcasting_ability = (uint8_t)n[0];
            if(count >= 2U) c->spell_attack_misc = (int8_t)n[1];
            if(count >= 3U) c->spell_save_misc = (int8_t)n[2];
            if(count >= 4U) c->arcane_recovery_used = (uint8_t)n[3];
            if(count) recognized_data = true;
            continue;
        }

        /* Features and applied grants moved to lazy sidecars. Legacy embedded
           Feature/Grant fields are recognized only so tolerant old-profile
           loading can continue; they are deliberately not allocated or migrated.
           This prevents old profiles from recreating the large startup working
           set that the sidecars were introduced to remove. */
        if(!strcmp(key, "FeatureCount") || !strcmp(key, "GrantCount")) {
            recognized_data = true;
            continue;
        }
        if(dnd_storage_indexed_key(key, "Feature", "Name", DND_RESIDENT_RECORD_LIMIT, &index) ||
           dnd_storage_indexed_key(key, "Feature", "Detail", DND_RESIDENT_RECORD_LIMIT, &index) ||
           dnd_storage_indexed_key(key, "Feature", "Data", DND_RESIDENT_RECORD_LIMIT, &index)) {
            recognized_data = true;
            continue;
        }

        /* Legacy inline Language/Training fields are tolerated but intentionally
           not migrated. The scalable character sidecar format makes these files authoritative. */
        if(!strcmp(key, "LanguageCount") || !strncmp(key, "Language", 8U) ||
           !strcmp(key, "OtherProficiencies") || !strcmp(key, "ToolProficiencies") ||
           !strcmp(key, "ArmorTraining") || !strcmp(key, "WeaponTraining")) {
            recognized_data = true;
            continue;
        }
        if(!strcmp(key, "CombatFlags")) {
            size_t count = dnd_storage_parse_numbers(value, n, 3U);
            if(count >= 1U) c->reaction_available = (uint8_t)n[0];
            if(count >= 2U) c->encumbrance_mode = (uint8_t)n[1];
            if(count >= 3U) c->carrying_capacity_override = (int16_t)n[2];
            if(count) recognized_data = true;
            continue;
        }

        if(dnd_storage_indexed_key(key, "Grant", "StableId", DND_MAX_GRANTS, &index) ||
           dnd_storage_indexed_key(key, "Grant", "Source", DND_MAX_GRANTS, &index) ||
           dnd_storage_indexed_key(key, "Grant", "Option", DND_MAX_GRANTS, &index) ||
           dnd_storage_indexed_key(key, "Grant", "Prerequisites", DND_MAX_GRANTS, &index) ||
           dnd_storage_indexed_key(key, "Grant", "Value", DND_MAX_GRANTS, &index) ||
           dnd_storage_indexed_key(key, "Grant", "Data", DND_MAX_GRANTS, &index)) {
            recognized_data = true;
            continue;
        }

        if(!strcmp(key, "AttackTemplateCount")) {
            if(dnd_storage_parse_u32_range(value, DND_MAX_ATTACK_TEMPLATES, &number)) {
                c->attack_template_count = (uint8_t)number;
                recognized_data = true;
            }
            continue;
        }
        if(dnd_storage_indexed_key(
               key, "AttackTemplate", "Name", DND_MAX_ATTACK_TEMPLATES, &index) ||
           dnd_storage_indexed_key(
               key, "AttackTemplate", "Mastery", DND_MAX_ATTACK_TEMPLATES, &index) ||
           dnd_storage_indexed_key(
               key, "AttackTemplate", "DamageType", DND_MAX_ATTACK_TEMPLATES, &index) ||
           dnd_storage_indexed_key(
               key, "AttackTemplate", "RiderType", DND_MAX_ATTACK_TEMPLATES, &index) ||
           dnd_storage_indexed_key(
               key, "AttackTemplate", "Data", DND_MAX_ATTACK_TEMPLATES, &index)) {
            if(c->attack_template_count <= index) c->attack_template_count = (uint8_t)(index + 1U);
            DndAttackTemplate* attack = &c->attack_templates[index];
            if(dnd_storage_indexed_key(
                   key, "AttackTemplate", "Name", DND_MAX_ATTACK_TEMPLATES, &index))
                dnd_storage_decode_string(attack->name, sizeof(attack->name), value);
            else if(dnd_storage_indexed_key(
                        key, "AttackTemplate", "Mastery", DND_MAX_ATTACK_TEMPLATES, &index))
                dnd_storage_decode_string(attack->mastery, sizeof(attack->mastery), value);
            else if(dnd_storage_indexed_key(
                        key, "AttackTemplate", "DamageType", DND_MAX_ATTACK_TEMPLATES, &index))
                dnd_storage_decode_string(attack->damage_type, sizeof(attack->damage_type), value);
            else if(dnd_storage_indexed_key(
                        key, "AttackTemplate", "RiderType", DND_MAX_ATTACK_TEMPLATES, &index))
                dnd_storage_decode_string(attack->rider_type, sizeof(attack->rider_type), value);
            else {
                size_t count = dnd_storage_parse_numbers(value, n, 9U);
                if(count >= 1U) {
                    attack->type = (uint8_t)n[0] & 0x07U;
                    attack->recharge = (uint8_t)n[0] >> 3U;
                }
                if(count >= 2U) attack->ability = (uint8_t)n[1];
                if(count >= 3U) attack->save_ability = (uint8_t)n[2];
                if(count >= 4U) attack->attack_misc = (int8_t)n[3];
                if(count >= 5U) attack->save_dc = (uint8_t)n[4];
                if(count >= 6U) attack->damage_dice = (uint8_t)n[5];
                if(count >= 7U) attack->damage_die = (uint8_t)n[6];
                if(count >= 8U) attack->rider_dice = (uint8_t)n[7];
                if(count >= 9U) attack->rider_die = (uint8_t)n[8];
            }
            recognized_data = true;
            continue;
        }
        /* Party, initiative, encounter-history and unknown fields are intentionally
           ignored. Their state belongs to companion apps and cannot invalidate a character. */
    }

    if(!class_level_seen && fallback_entry && c->class_count == 1U &&
       fallback_entry->level >= 1U && fallback_entry->level <= 20U)
        c->classes[0].level = fallback_entry->level;

    bool io_ok = storage_file_get_error(file) == FSE_OK;
    dnd_data_sanitize(data);
    return io_ok && recognized_data;
}

static bool dnd_storage_load_text_path(Storage* storage, const char* path, DndSaveData* data) {
    DndProfileEntry fallback;
    const DndProfileEntry* fallback_entry = NULL;
    const char* filename = strrchr(path, '/');
    filename = filename ? filename + 1U : path;
    if(dnd_storage_parse_profile_filename(filename, &fallback)) fallback_entry = &fallback;

    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool success = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                   dnd_storage_read_character(file, data, fallback_entry);
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

static void dnd_storage_work_path(char* output, size_t size, uint32_t profile, const char* kind) {
    snprintf(
        output, size, "%s/custom_%s_%lu.tmp", DND_STORAGE_DATA_DIR, kind, (unsigned long)profile);
}

static bool dnd_storage_parse_profile_filename(const char* filename, DndProfileEntry* entry) {
    if(!filename || !entry) return false;
    size_t length = strlen(filename);
    if(length < 11U || strncmp(filename, "ch_", 3U) != 0 ||
       strcmp(filename + length - 4U, ".txt") != 0)
        return false;

    const char* id_begin = filename + 3U;
    const char* id_end = strchr(id_begin, '_');
    if(!id_end) return false;
    uint32_t id = 0U;
    if(!dnd_storage_parse_u32_span(id_begin, id_end, UINT32_MAX, &id)) return false;

    const char* level_separator = filename + length - 5U;
    while(level_separator > id_end && *level_separator != '_')
        --level_separator;
    if(*level_separator != '_' || level_separator <= id_end + 1U) return false;
    uint32_t level = 0U;
    if(!dnd_storage_parse_u32_span(level_separator + 1U, filename + length - 4U, UINT8_MAX, &level))
        return false;

    entry->id = id;
    entry->level = (uint8_t)level;
    size_t name_length = (size_t)(level_separator - (id_end + 1U));
    if(name_length >= sizeof(entry->name)) name_length = sizeof(entry->name) - 1U;
    memcpy(entry->name, id_end + 1U, name_length);
    entry->name[name_length] = '\0';
    for(size_t i = 0U; entry->name[i]; ++i)
        if(entry->name[i] == '_') entry->name[i] = ' ';
    return true;
}

bool dnd_storage_find_profile_path(Storage* storage, uint32_t profile, char* output, size_t size) {
    File* directory = storage_file_alloc(storage);
    if(!directory) return false;
    if(!storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    bool found = false;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        DndProfileEntry entry;
        if(!file_info_is_dir(&info) && dnd_storage_parse_profile_filename(filename, &entry) &&
           entry.id == profile) {
            size_t prefix_length = strlen(DND_STORAGE_DATA_DIR);
            size_t filename_length = strlen(filename);
            if(prefix_length + filename_length + 2U > size) continue;
            memcpy(output, DND_STORAGE_DATA_DIR, prefix_length);
            output[prefix_length] = '/';
            memcpy(output + prefix_length + 1U, filename, filename_length + 1U);
            found = true;
            break;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return found;
}

static bool dnd_storage_character_shd_path(char* output, size_t size, const char* character_path) {
    if(!output || !size || !character_path) return false;
    size_t length = strlen(character_path);
    if(length < 5U || strcmp(character_path + length - 4U, ".txt") != 0 || length + 1U > size)
        return false;
    memcpy(output, character_path, length - 4U);
    memcpy(output + length - 4U, ".shd", 5U);
    return true;
}

static bool dnd_storage_parse_core_shd_filename(const char* filename, DndProfileEntry* entry) {
    if(!filename || !entry) return false;
    size_t length = strlen(filename);
    if(length < 11U || strcmp(filename + length - 4U, ".shd") != 0) return false;
    char normalized[128];
    if(length + 1U > sizeof(normalized)) return false;
    memcpy(normalized, filename, length + 1U);
    memcpy(normalized + length - 4U, ".txt", 5U);
    /* Collection snapshots have a suffix after the numeric level, so the normal
       profile parser rejects them. Only ch_ID_Name_LEVEL.shd reaches true here. */
    return dnd_storage_parse_profile_filename(normalized, entry);
}

static bool dnd_storage_find_shd_path(
    Storage* storage,
    uint32_t profile,
    uint8_t level,
    char* output,
    size_t size) {
    if(!storage || !output || !size) return false;
    File* directory = storage_file_alloc(storage);
    if(!directory) return false;
    if(!storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    bool found = false;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        DndProfileEntry entry;
        if(file_info_is_dir(&info) || !dnd_storage_parse_core_shd_filename(filename, &entry) ||
           entry.id != profile || entry.level != level)
            continue;
        if(dnd_fs_child_path(output, size, DND_STORAGE_DATA_DIR, NULL, filename)) found = true;
        break;
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return found;
}

static void dnd_storage_remove_duplicate_core_shd(
    Storage* storage,
    uint32_t profile,
    uint8_t level,
    const char* keep_path) {
    if(!storage || !keep_path) return;
    /* There should normally be only one core SHD per level. A same-level rename
       can leave an older filename, so remove stale duplicates after the new one
       is safely written. Restart the directory scan after each removal to avoid
       mutating an open iterator. */
    while(true) {
        File* directory = storage_file_alloc(storage);
        if(!directory) return;
        if(!storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
            storage_file_free(directory);
            return;
        }
        FileInfo info;
        char filename[128];
        char stale[DND_FS_PATH_LEN] = {0};
        while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
            DndProfileEntry entry;
            if(file_info_is_dir(&info) || !dnd_storage_parse_core_shd_filename(filename, &entry) ||
               entry.id != profile || entry.level != level)
                continue;
            if(!dnd_fs_child_path(stale, sizeof(stale), DND_STORAGE_DATA_DIR, NULL, filename)) {
                stale[0] = '\0';
                continue;
            }
            if(strcmp(stale, keep_path) != 0) break;
            stale[0] = '\0';
        }
        storage_dir_close(directory);
        storage_file_free(directory);
        if(!stale[0]) return;
        if(storage_common_remove(storage, stale) != FSE_OK) return;
    }
}

static bool dnd_storage_refresh_character_shd(
    Storage* storage,
    uint32_t profile,
    const char* character_path) {
    char shd_path[DND_FS_PATH_LEN];
    if(!storage || !dnd_storage_character_shd_path(shd_path, sizeof(shd_path), character_path))
        return false;
    if(!dnd_storage_copy_file_direct(storage, character_path, shd_path)) return false;
    DndProfileEntry entry;
    const char* filename = strrchr(shd_path, '/');
    filename = filename ? filename + 1U : shd_path;
    if(dnd_storage_parse_core_shd_filename(filename, &entry))
        dnd_storage_remove_duplicate_core_shd(storage, profile, entry.level, shd_path);
    return true;
}

static void dnd_storage_level_sidecar_snapshot_path_fields(
    char* output,
    size_t size,
    uint32_t profile,
    const char* character_name,
    uint8_t level,
    const char* suffix) {
    char safe_name[DND_CHARACTER_NAME_LEN];
    dnd_storage_filename_name(
        safe_name,
        sizeof(safe_name),
        character_name && character_name[0] ? character_name : "Unnamed");
    snprintf(
        output,
        size,
        "%s/ch_%lu_%s_%u_%s.shd",
        DND_STORAGE_DATA_DIR,
        (unsigned long)profile,
        safe_name,
        level ? level : 1U,
        suffix);
}

static void dnd_storage_level_sidecar_snapshot_path(
    char* output,
    size_t size,
    uint32_t profile,
    const DndCharacter* character,
    const char* suffix) {
    dnd_storage_level_sidecar_snapshot_path_fields(
        output,
        size,
        profile,
        character ? character->name : NULL,
        character ? dnd_storage_character_level(character) : 1U,
        suffix);
}

static bool dnd_storage_level_bundle_marker_path_fields(
    char* output,
    size_t size,
    uint32_t profile,
    const char* character_name,
    uint8_t level) {
    if(!output || !size) return false;
    dnd_storage_level_sidecar_snapshot_path_fields(
        output, size, profile, character_name, level, "bundle");
    return output[0] != '\0';
}

static void dnd_storage_clear_level_bundle_marker(
    Storage* storage,
    uint32_t profile,
    const char* character_name,
    uint8_t level) {
    if(!storage) return;
    char marker[DND_FS_PATH_LEN];
    if(!dnd_storage_level_bundle_marker_path_fields(
           marker, sizeof(marker), profile, character_name, level))
        return;
    if(storage_file_exists(storage, marker)) (void)storage_common_remove(storage, marker);
}

static bool dnd_storage_write_level_bundle_marker(
    Storage* storage,
    uint32_t profile,
    const char* character_name,
    uint8_t level) {
    if(!storage) return false;
    char marker[DND_FS_PATH_LEN];
    if(!dnd_storage_level_bundle_marker_path_fields(
           marker, sizeof(marker), profile, character_name, level))
        return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool success = storage_file_open(file, marker, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                   dnd_storage_write_raw(file, "DNDHistoryBundle=3\n") && storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    return success && storage_file_exists(storage, marker);
}

static uint8_t dnd_storage_level_bundle_marker_version(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character) {
    if(!storage || !character) return false;
    char marker[DND_FS_PATH_LEN];
    if(!dnd_storage_level_bundle_marker_path_fields(
           marker,
           sizeof(marker),
           profile,
           character->name,
           dnd_storage_character_level(character)))
        return false;
    File* file = storage_file_alloc(storage);
    if(!file) return 0U;
    char value[32] = {0};
    bool ok = storage_file_open(file, marker, FSAM_READ, FSOM_OPEN_EXISTING);
    if(ok) storage_file_read(file, value, sizeof(value) - 1U);
    if(storage_file_get_error(file) != FSE_OK) ok = false;
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) return 0U;
    return !strncmp(value, "DNDHistoryBundle=3", 18U) ? 3U :
           !strncmp(value, "DNDHistoryBundle=2", 18U) ? 2U :
           !strncmp(value, "DNDHistoryBundle=1", 18U) ? 1U :
                                                        0U;
}

static bool dnd_storage_remove_level_bag_snapshots(
    Storage* storage,
    uint32_t profile,
    const char* character_name,
    uint8_t level) {
    char safe_name[DND_CHARACTER_NAME_LEN];
    char prefix[96];
    dnd_storage_filename_name(
        safe_name,
        sizeof(safe_name),
        character_name && character_name[0] ? character_name : "Unnamed");
    snprintf(
        prefix,
        sizeof(prefix),
        "ch_%lu_%s_%u_itemsbag_",
        (unsigned long)profile,
        safe_name,
        level ? level : 1U);
    const size_t prefix_len = strlen(prefix);

    while(true) {
        File* directory = storage_file_alloc(storage);
        if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
            if(directory) storage_file_free(directory);
            return false;
        }
        FileInfo info;
        char filename[128];
        char path[DND_FS_PATH_LEN] = {0};
        while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
            size_t length = strlen(filename);
            if(file_info_is_dir(&info) || strncmp(filename, prefix, prefix_len) != 0 ||
               length < 4U || strcmp(filename + length - 4U, ".shd") != 0)
                continue;
            if(dnd_fs_child_path(path, sizeof(path), DND_STORAGE_DATA_DIR, NULL, filename)) break;
            path[0] = '\0';
        }
        storage_dir_close(directory);
        storage_file_free(directory);
        if(!path[0]) return true;
        if(storage_common_remove(storage, path) != FSE_OK) return false;
    }
}

static bool dnd_storage_refresh_level_inventory_bags_fields(
    Storage* storage,
    uint32_t profile,
    const char* character_name,
    uint8_t level) {
    if(!dnd_storage_remove_level_bag_snapshots(storage, profile, character_name, level))
        return false;

    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    bool success = true;
    FileInfo info;
    char filename[128];
    char safe[DND_INVENTORY_BAG_NAME_LEN];
    while(success && storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info) ||
           !dnd_storage_inventory_nonmain_bag_filename(filename, profile, safe, sizeof(safe)))
            continue;
        char live[DND_FS_PATH_LEN];
        char snapshot[DND_FS_PATH_LEN];
        char suffix[48];
        if(!dnd_fs_child_path(live, sizeof(live), DND_STORAGE_DATA_DIR, NULL, filename)) {
            success = false;
            break;
        }
        snprintf(suffix, sizeof(suffix), "itemsbag_%s", safe);
        dnd_storage_level_sidecar_snapshot_path_fields(
            snapshot, sizeof(snapshot), profile, character_name, level, suffix);
        success = dnd_storage_copy_file_direct(storage, live, snapshot);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return success;
}

static bool dnd_storage_refresh_level_sidecars_fields(
    Storage* storage,
    uint32_t profile,
    const char* character_name,
    uint8_t level) {
    if(!storage) return false;
    const char* live_kinds[] = {
        "spellbook", "items", "feats", "appliedgrants", "languages", "proficiencies"};
    const char* shd_suffixes[] = {
        "spellbook", "items", "features", "appliedgrants", "languages", "proficiencies"};
    char live[DND_FS_PATH_LEN];
    char snapshot[DND_FS_PATH_LEN];
    bool success = true;
    for(uint8_t i = 0U; i < 6U; ++i) {
        dnd_storage_collection_path(live, sizeof(live), profile, live_kinds[i]);
        dnd_storage_level_sidecar_snapshot_path_fields(
            snapshot, sizeof(snapshot), profile, character_name, level, shd_suffixes[i]);
        if(storage_file_exists(storage, live)) {
            if(!dnd_storage_copy_file_direct(storage, live, snapshot)) success = false;
        } else if(
            storage_file_exists(storage, snapshot) &&
            storage_common_remove(storage, snapshot) != FSE_OK) {
            success = false;
        }
    }
    if(!dnd_storage_refresh_level_inventory_bags_fields(storage, profile, character_name, level))
        success = false;
    return success;
}

static bool dnd_storage_refresh_level_sidecars(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character) {
    if(!character) return false;
    return dnd_storage_refresh_level_sidecars_fields(
        storage, profile, character->name, dnd_storage_character_level(character));
}

static bool dnd_storage_save_profile_internal(
    Storage* storage,
    uint32_t profile,
    const DndSaveData* data,
    const char* known_old_path,
    bool update_history) {
    furi_assert(storage);
    furi_assert(data);
    storage_common_mkdir(storage, DND_STORAGE_DATA_DIR);
    char old_path[DND_FS_PATH_LEN] = {0};
    char new_path[DND_FS_PATH_LEN];
    char temp_path[DND_FS_PATH_LEN];
    char backup_path[DND_FS_PATH_LEN];
    bool had_save = false;
    if(known_old_path && storage_file_exists(storage, known_old_path)) {
        dnd_storage_copy(old_path, sizeof(old_path), known_old_path);
        had_save = true;
    } else {
        had_save = dnd_storage_find_profile_path(storage, profile, old_path, sizeof(old_path));
    }
    dnd_storage_profile_path(new_path, sizeof(new_path), profile, &data->character);
    /* Keep one level-tagged core SHD beside the canonical character save. The
       live .txt remains authoritative; SHD history is best-effort and must never
       block a primary save. Refresh the old level before a level/name transition. */
    if(update_history && had_save) {
        bool core_history_ok = dnd_storage_refresh_character_shd(storage, profile, old_path);
        const char* old_filename = strrchr(old_path, '/');
        old_filename = old_filename ? old_filename + 1U : old_path;
        DndProfileEntry old_entry;
        if(dnd_storage_parse_profile_filename(old_filename, &old_entry)) {
            dnd_storage_clear_level_bundle_marker(
                storage, profile, old_entry.name, old_entry.level);
            bool sidecar_history_ok = dnd_storage_refresh_level_sidecars_fields(
                storage, profile, old_entry.name, old_entry.level);
            if(core_history_ok && sidecar_history_ok)
                (void)dnd_storage_write_level_bundle_marker(
                    storage, profile, old_entry.name, old_entry.level);
        }
    }
    dnd_storage_work_path(temp_path, sizeof(temp_path), profile, "write");
    dnd_storage_work_path(backup_path, sizeof(backup_path), profile, "backup");
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool written = storage_file_open(file, temp_path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                   dnd_storage_write_character(file, data) && storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!written) {
        storage_common_remove(storage, temp_path);
        return false;
    }
    if(had_save) {
        storage_common_remove(storage, backup_path);
        if(storage_common_rename(storage, old_path, backup_path) != FSE_OK) {
            storage_common_remove(storage, temp_path);
            return false;
        }
    }
    if(storage_common_rename(storage, temp_path, new_path) != FSE_OK) {
        if(had_save) storage_common_rename(storage, backup_path, old_path);
        return false;
    }
    /* Refresh the complete current-level history set after the primary is safely
       published. Core/sidecar SHD failure is intentionally non-fatal. */
    if(update_history) {
        dnd_storage_clear_level_bundle_marker(
            storage, profile, data->character.name, dnd_storage_character_level(&data->character));
        bool core_history_ok = dnd_storage_refresh_character_shd(storage, profile, new_path);
        bool sidecar_history_ok =
            dnd_storage_refresh_level_sidecars(storage, profile, &data->character);
        if(core_history_ok && sidecar_history_ok)
            (void)dnd_storage_write_level_bundle_marker(
                storage,
                profile,
                data->character.name,
                dnd_storage_character_level(&data->character));
    }
    return true;
}

bool dnd_storage_save_profile(Storage* storage, uint32_t profile, const DndSaveData* data) {
    return dnd_storage_save_profile_internal(storage, profile, data, NULL, false);
}

bool dnd_storage_save_profile_updated(Storage* storage, uint32_t profile, const DndSaveData* data) {
    return dnd_storage_save_profile_internal(storage, profile, data, NULL, true);
}

bool dnd_storage_save_profile_known_updated(
    Storage* storage,
    const DndProfileEntry* current_entry,
    const DndSaveData* data) {
    furi_assert(current_entry);
    char safe_name[DND_CHARACTER_NAME_LEN];
    char current_path[DND_FS_PATH_LEN];
    dnd_storage_filename_name(safe_name, sizeof(safe_name), current_entry->name);
    snprintf(
        current_path,
        sizeof(current_path),
        "%s/ch_%lu_%s_%u.txt",
        DND_STORAGE_DATA_DIR,
        (unsigned long)current_entry->id,
        safe_name,
        current_entry->level);
    return dnd_storage_save_profile_internal(storage, current_entry->id, data, current_path, true);
}

bool dnd_storage_validate_character_path(Storage* storage, const char* path) {
    if(!storage || !path || !path[0]) return false;
    DndSaveData* data = calloc(1U, sizeof(DndSaveData));
    if(!data) return false;
    bool ok = dnd_storage_load_text_path(storage, path, data);
    dnd_data_clear(data);
    free(data);
    return ok;
}

bool dnd_storage_load_profile(
    Storage* storage,
    uint32_t profile,
    DndSaveData* data,
    bool* recovered_backup) {
    furi_assert(storage);
    furi_assert(data);
    if(recovered_backup) *recovered_backup = false;
    char path[DND_FS_PATH_LEN];
    char backup_path[DND_FS_PATH_LEN];
    bool primary_found = dnd_storage_find_profile_path(storage, profile, path, sizeof(path));
    if(primary_found && dnd_storage_load_text_path(storage, path, data)) return true;

    /* A readable retained backup is a fallback for an unreadable primary. The
       character parser itself remains field-name best-effort; backup recovery is
       only reached when the primary cannot be read into a usable character. */
    dnd_storage_work_path(backup_path, sizeof(backup_path), profile, "backup");
    if(dnd_storage_load_text_path(storage, backup_path, data)) {
        if(recovered_backup) *recovered_backup = true;
        return true;
    }
    dnd_data_clear(data);
    dnd_data_set_defaults(data);
    return false;
}

static bool dnd_storage_remove_if_present(Storage* storage, const char* path) {
    return !storage_file_exists(storage, path) || storage_common_remove(storage, path) == FSE_OK;
}

static bool dnd_storage_remove_nonmain_inventory_bags(Storage* storage, uint32_t profile) {
    if(!storage) return false;
    while(true) {
        File* directory = storage_file_alloc(storage);
        if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
            if(directory) storage_file_free(directory);
            return false;
        }
        FileInfo info;
        char filename[128], safe[DND_INVENTORY_BAG_NAME_LEN], path[DND_FS_PATH_LEN] = {0};
        while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
            if(file_info_is_dir(&info) ||
               !dnd_storage_inventory_nonmain_bag_filename(filename, profile, safe, sizeof(safe)))
                continue;
            if(dnd_fs_child_path(path, sizeof(path), DND_STORAGE_DATA_DIR, NULL, filename)) break;
            path[0] = '\0';
        }
        storage_dir_close(directory);
        storage_file_free(directory);
        if(!path[0]) return true;
        if(storage_common_remove(storage, path) != FSE_OK) return false;
    }
}

static bool dnd_storage_copy_nonmain_inventory_bags(
    Storage* storage,
    uint32_t source_profile,
    uint32_t destination_profile) {
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    bool success = true;
    FileInfo info;
    char filename[128], safe[DND_INVENTORY_BAG_NAME_LEN];
    while(success && storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info) || !dnd_storage_inventory_nonmain_bag_filename(
                                          filename, source_profile, safe, sizeof(safe)))
            continue;
        char source[DND_FS_PATH_LEN], destination[DND_FS_PATH_LEN];
        if(!dnd_fs_child_path(source, sizeof(source), DND_STORAGE_DATA_DIR, NULL, filename)) {
            success = false;
            break;
        }
        snprintf(
            destination,
            sizeof(destination),
            "%s/inv%s_%lu.txt",
            DND_STORAGE_DATA_DIR,
            safe,
            (unsigned long)destination_profile);
        success = dnd_storage_copy_file_direct(storage, source, destination);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return success;
}

static bool dnd_storage_archive_inventory_bag_path(
    char* output,
    size_t size,
    uint32_t profile,
    const char* safe) {
    if(!output || !size || !safe || !safe[0]) return false;
    int written = snprintf(
        output,
        size,
        "%s/ch_%lu_invbag_%s.txt",
        DND_STORAGE_ARCHIVE_DIR,
        (unsigned long)profile,
        safe);
    return written > 0 && (size_t)written < size;
}

static bool dnd_storage_remove_archived_inventory_bags(Storage* storage, uint32_t profile) {
    if(!storage) return false;
    char prefix[48];
    snprintf(prefix, sizeof(prefix), "ch_%lu_invbag_", (unsigned long)profile);
    while(true) {
        File* directory = storage_file_alloc(storage);
        if(!directory || !storage_dir_open(directory, DND_STORAGE_ARCHIVE_DIR)) {
            if(directory) storage_file_free(directory);
            return false;
        }
        FileInfo info;
        char filename[128];
        char path[DND_FS_LONG_PATH_LEN] = {0};
        while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
            size_t length = strlen(filename);
            if(file_info_is_dir(&info) || strncmp(filename, prefix, strlen(prefix)) != 0 ||
               length < 5U || strcmp(filename + length - 4U, ".txt") != 0)
                continue;
            if(dnd_fs_child_path(path, sizeof(path), DND_STORAGE_ARCHIVE_DIR, NULL, filename))
                break;
            path[0] = '\0';
        }
        storage_dir_close(directory);
        storage_file_free(directory);
        if(!path[0]) return true;
        if(storage_common_remove(storage, path) != FSE_OK) return false;
    }
}

static bool dnd_storage_archive_nonmain_inventory_bags(Storage* storage, uint32_t profile) {
    if(!storage) return false;
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    bool success = true;
    FileInfo info;
    char filename[128], safe[DND_INVENTORY_BAG_NAME_LEN];
    /* Preflight all destinations before copying anything. */
    while(success && storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info) ||
           !dnd_storage_inventory_nonmain_bag_filename(filename, profile, safe, sizeof(safe)))
            continue;
        char destination[DND_FS_LONG_PATH_LEN];
        if(!dnd_storage_archive_inventory_bag_path(
               destination, sizeof(destination), profile, safe) ||
           storage_file_exists(storage, destination))
            success = false;
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(!success) return false;

    directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    while(success && storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info) ||
           !dnd_storage_inventory_nonmain_bag_filename(filename, profile, safe, sizeof(safe)))
            continue;
        char source[DND_FS_PATH_LEN], destination[DND_FS_LONG_PATH_LEN];
        if(!dnd_fs_child_path(source, sizeof(source), DND_STORAGE_DATA_DIR, NULL, filename) ||
           !dnd_storage_archive_inventory_bag_path(
               destination, sizeof(destination), profile, safe)) {
            success = false;
            break;
        }
        success = dnd_storage_copy_file_direct(storage, source, destination);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(!success) (void)dnd_storage_remove_archived_inventory_bags(storage, profile);
    return success;
}

static bool dnd_storage_restore_archived_inventory_bags(Storage* storage, uint32_t profile) {
    if(!storage) return false;
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_ARCHIVE_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    char prefix[48];
    snprintf(prefix, sizeof(prefix), "ch_%lu_invbag_", (unsigned long)profile);
    const size_t prefix_len = strlen(prefix);
    bool success = true;
    FileInfo info;
    char filename[128];
    while(success && storage_dir_read(directory, &info, filename, sizeof(filename))) {
        size_t length = strlen(filename);
        if(file_info_is_dir(&info) || strncmp(filename, prefix, prefix_len) != 0 ||
           length <= prefix_len + 4U || strcmp(filename + length - 4U, ".txt") != 0)
            continue;
        size_t safe_len = length - prefix_len - 4U;
        if(!safe_len || safe_len >= DND_INVENTORY_BAG_NAME_LEN) {
            success = false;
            break;
        }
        char safe[DND_INVENTORY_BAG_NAME_LEN];
        memcpy(safe, filename + prefix_len, safe_len);
        safe[safe_len] = '\0';
        char source[DND_FS_LONG_PATH_LEN], destination[DND_FS_PATH_LEN];
        if(!dnd_fs_child_path(source, sizeof(source), DND_STORAGE_ARCHIVE_DIR, NULL, filename)) {
            success = false;
            break;
        }
        snprintf(
            destination,
            sizeof(destination),
            "%s/inv%s_%lu.txt",
            DND_STORAGE_DATA_DIR,
            safe,
            (unsigned long)profile);
        success = dnd_storage_copy_file_direct(storage, source, destination);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return success;
}

bool dnd_storage_delete_profile(Storage* storage, uint32_t profile) {
    char path[DND_FS_PATH_LEN];
    char temp_path[DND_FS_PATH_LEN];
    char backup_path[DND_FS_PATH_LEN];
    bool found = dnd_storage_find_profile_path(storage, profile, path, sizeof(path));
    dnd_storage_work_path(temp_path, sizeof(temp_path), profile, "write");
    dnd_storage_work_path(backup_path, sizeof(backup_path), profile, "backup");
    bool success = (!found || dnd_storage_remove_if_present(storage, path)) &&
                   dnd_storage_remove_if_present(storage, temp_path) &&
                   dnd_storage_remove_if_present(storage, backup_path);
    dnd_storage_spellbook_path(path, sizeof(path), profile);
    success = dnd_storage_remove_if_present(storage, path) && success;
    dnd_storage_items_path(path, sizeof(path), profile);
    success = dnd_storage_remove_if_present(storage, path) && success;
    snprintf(path, sizeof(path), "%s/feats_%lu.txt", DND_STORAGE_DATA_DIR, (unsigned long)profile);
    success = dnd_storage_remove_if_present(storage, path) && success;
    snprintf(
        path,
        sizeof(path),
        "%s/appliedgrants_%lu.txt",
        DND_STORAGE_DATA_DIR,
        (unsigned long)profile);
    success = dnd_storage_remove_if_present(storage, path) && success;
    const char* added[] = {"languages", "proficiencies"};
    for(uint8_t i = 0U; i < 2U; ++i) {
        dnd_storage_collection_path(path, sizeof(path), profile, added[i]);
        success = dnd_storage_remove_if_present(storage, path) && success;
    }
    success = dnd_storage_remove_nonmain_inventory_bags(storage, profile) && success;
    /* Level-tagged .shd files are historical records. They are intentionally
       never deleted when the live profile or sidecars are removed. */
    return success;
}

static bool dnd_storage_copy_file(
    Storage* storage,
    const char* source,
    const char* destination,
    const char* temporary) {
    if(!storage || !source || !destination || !temporary) return false;
    storage_common_remove(storage, temporary);
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    if(!input || !output) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        return false;
    }
    bool success = storage_file_open(input, source, FSAM_READ, FSOM_OPEN_EXISTING) &&
                   storage_file_open(output, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    uint8_t buffer[256];
    while(success) {
        size_t count = storage_file_read(input, buffer, sizeof(buffer));
        if(!count) break;
        success = storage_file_write(output, buffer, count) == count;
    }
    if(success) success = storage_file_get_error(input) == FSE_OK && storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(!success) {
        storage_common_remove(storage, temporary);
        return false;
    }
    char backup[DND_FS_LONG_PATH_LEN];
    int backup_length = snprintf(backup, sizeof(backup), "%s.publish.bak", destination);
    if(backup_length < 0 || (size_t)backup_length >= sizeof(backup)) {
        storage_common_remove(storage, temporary);
        return false;
    }
    return dnd_storage_publish_temp(storage, temporary, destination, backup);
}

bool dnd_storage_duplicate_profile(Storage* storage, uint32_t source, uint32_t destination) {
    char source_path[DND_FS_PATH_LEN];
    if(!dnd_storage_find_profile_path(storage, source, source_path, sizeof(source_path)))
        return false;
    const char* filename = strrchr(source_path, '/');
    filename = filename ? filename + 1U : source_path;
    DndProfileEntry entry;
    if(!dnd_storage_parse_profile_filename(filename, &entry)) return false;
    char safe_name[DND_CHARACTER_NAME_LEN];
    dnd_storage_filename_name(safe_name, sizeof(safe_name), entry.name);
    char destination_path[DND_FS_PATH_LEN];
    char temporary[DND_FS_PATH_LEN];
    snprintf(
        destination_path,
        sizeof(destination_path),
        "%s/ch_%lu_%s_%u.txt",
        DND_STORAGE_DATA_DIR,
        (unsigned long)destination,
        safe_name,
        entry.level);
    dnd_storage_work_path(temporary, sizeof(temporary), destination, "duplicate");
    if(!dnd_storage_copy_file(storage, source_path, destination_path, temporary)) return false;

    const char* collections[] = {
        "spellbook", "items", "feats", "appliedgrants", "languages", "proficiencies"};
    for(uint8_t i = 0U; i < 6U; ++i) {
        dnd_storage_collection_path(source_path, sizeof(source_path), source, collections[i]);
        dnd_storage_collection_path(
            destination_path, sizeof(destination_path), destination, collections[i]);
        if(!storage_file_exists(storage, source_path)) continue;
        if(!dnd_storage_copy_file_direct(storage, source_path, destination_path)) {
            dnd_storage_delete_profile(storage, destination);
            return false;
        }
    }
    if(!dnd_storage_copy_nonmain_inventory_bags(storage, source, destination)) {
        dnd_storage_delete_profile(storage, destination);
        return false;
    }
    return true;
}

bool dnd_storage_archive_profile(Storage* storage, uint32_t profile) {
    char source_path[DND_FS_PATH_LEN];
    if(!dnd_storage_find_profile_path(storage, profile, source_path, sizeof(source_path)))
        return false;
    storage_common_mkdir(storage, DND_STORAGE_ARCHIVE_DIR);
    const char* filename = strrchr(source_path, '/');
    filename = filename ? filename + 1U : source_path;
    char destination[DND_FS_LONG_PATH_LEN];
    if(!dnd_fs_child_path(
           destination, sizeof(destination), DND_STORAGE_ARCHIVE_DIR, NULL, filename))
        return false;
    if(storage_file_exists(storage, destination)) return false;

    char collection_source[6][DND_FS_PATH_LEN];
    char collection_destination[6][DND_FS_LONG_PATH_LEN];
    bool collection_present[6] = {false};
    const char* collections[] = {
        "spellbook", "items", "feats", "appliedgrants", "languages", "proficiencies"};
    for(uint8_t i = 0U; i < 6U; ++i) {
        dnd_storage_collection_path(
            collection_source[i], sizeof(collection_source[i]), profile, collections[i]);
        collection_present[i] = storage_file_exists(storage, collection_source[i]);
        snprintf(
            collection_destination[i],
            sizeof(collection_destination[i]),
            "%s/ch_%lu_%s.txt",
            DND_STORAGE_ARCHIVE_DIR,
            (unsigned long)profile,
            collections[i]);
        if(collection_present[i] && storage_file_exists(storage, collection_destination[i]))
            return false;
    }

    if(!dnd_storage_archive_nonmain_inventory_bags(storage, profile)) return false;
    if(storage_common_rename(storage, source_path, destination) != FSE_OK) {
        (void)dnd_storage_remove_archived_inventory_bags(storage, profile);
        return false;
    }
    for(uint8_t i = 0U; i < 6U; ++i) {
        if(!collection_present[i]) continue;
        if(storage_common_rename(storage, collection_source[i], collection_destination[i]) ==
           FSE_OK) {
            continue;
        }
        /* Keep archive as an all-or-nothing character set when a sidecar move fails. */
        for(uint8_t rollback = 0U; rollback < i; ++rollback) {
            if(collection_present[rollback])
                storage_common_rename(
                    storage, collection_destination[rollback], collection_source[rollback]);
        }
        storage_common_rename(storage, destination, source_path);
        (void)dnd_storage_remove_archived_inventory_bags(storage, profile);
        return false;
    }
    if(!dnd_storage_remove_nonmain_inventory_bags(storage, profile)) {
        /* Restore the live set from archive copies, then roll the fixed files back. */
        (void)dnd_storage_restore_archived_inventory_bags(storage, profile);
        for(uint8_t rollback = 0U; rollback < 6U; ++rollback) {
            if(collection_present[rollback])
                (void)storage_common_rename(
                    storage, collection_destination[rollback], collection_source[rollback]);
        }
        (void)storage_common_rename(storage, destination, source_path);
        (void)dnd_storage_remove_archived_inventory_bags(storage, profile);
        return false;
    }
    return true;
}

bool dnd_storage_verify_profile(Storage* storage, uint32_t profile) {
    char path[DND_FS_PATH_LEN];
    if(!dnd_storage_find_profile_path(storage, profile, path, sizeof(path))) return false;

    /* Verification uses the normal best-effort parser, but its full character
       state is transient and must not consume nearly 4 KiB of the UI thread
       stack. Keep the save format and parser behavior unchanged while owning
       the temporary state on checked heap storage. */
    DndSaveData* parsed = calloc(1U, sizeof(DndSaveData));
    if(!parsed) return false;
    bool ok = dnd_storage_load_text_path(storage, path, parsed);
    dnd_data_clear(parsed);
    free(parsed);
    return ok;
}

bool dnd_storage_validate_profile_semantics(Storage* storage, uint32_t profile) {
    if(!storage) return false;
    char path[DND_FS_PATH_LEN];
    if(!dnd_storage_find_profile_path(storage, profile, path, sizeof(path))) return false;

    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    DndDolphinsReader reader;
    dnd_storage_reader_init(&reader, file);
    char line[DND_STORAGE_ENCODED_LINE_LEN];
    int32_t values[16];
    while(ok && dnd_storage_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        uint8_t index = 0U;
        if(!strcmp(line, "Progress")) {
            size_t count = dnd_storage_parse_numbers(value, values, 4U);
            ok = count >= 1U && values[0] >= 1 && values[0] <= (int32_t)DND_MAX_CLASSES;
        } else if(dnd_storage_indexed_key(line, "Class", "Data", DND_MAX_CLASSES, &index)) {
            size_t count = dnd_storage_parse_numbers(value, values, 16U);
            ok = count >= 1U && values[0] >= 1 && values[0] <= 20;
        } else if(!strcmp(line, "AbilityScores")) {
            size_t count = dnd_storage_parse_numbers(value, values, DND_ABILITY_COUNT);
            ok = count == DND_ABILITY_COUNT;
            for(size_t i = 0U; ok && i < count; ++i)
                ok = values[i] >= 1 && values[i] <= 30;
        } else if(!strcmp(line, "Vitals")) {
            size_t count = dnd_storage_parse_numbers(value, values, 12U);
            ok = count >= 2U && values[0] >= 0 && values[1] >= 1 && values[1] <= 999 &&
                 values[0] <= values[1];
            if(ok && count >= 3U) ok = values[2] >= 0 && values[2] <= 999;
        }
    }
    if(ok) ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

bool dnd_storage_recover_profile_backup(Storage* storage, uint32_t profile, DndSaveData* data) {
    char backup_path[DND_FS_PATH_LEN];
    char primary_path[DND_FS_PATH_LEN];
    char rejected_path[DND_FS_PATH_LEN];
    dnd_storage_work_path(backup_path, sizeof(backup_path), profile, "backup");
    if(!dnd_storage_load_text_path(storage, backup_path, data)) return false;
    bool had_primary =
        dnd_storage_find_profile_path(storage, profile, primary_path, sizeof(primary_path));
    dnd_storage_work_path(rejected_path, sizeof(rejected_path), profile, "rejected");
    storage_common_remove(storage, rejected_path);
    if(had_primary && storage_common_rename(storage, primary_path, rejected_path) != FSE_OK)
        return false;
    bool restored = dnd_storage_save_profile_updated(storage, profile, data);
    if(restored) {
        storage_common_remove(storage, rejected_path);
        return true;
    }
    if(had_primary) storage_common_rename(storage, rejected_path, primary_path);
    return false;
}

uint8_t dnd_storage_list_shd_levels(
    Storage* storage,
    uint32_t profile,
    uint8_t* levels,
    uint8_t capacity) {
    if(!storage || !levels || !capacity) return 0U;
    File* directory = storage_file_alloc(storage);
    if(!directory) return 0U;
    if(!storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        storage_file_free(directory);
        return 0U;
    }
    FileInfo info;
    char filename[128];
    uint8_t count = 0U;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        DndProfileEntry entry;
        if(file_info_is_dir(&info) || !dnd_storage_parse_core_shd_filename(filename, &entry) ||
           entry.id != profile)
            continue;
        bool duplicate = false;
        for(uint8_t i = 0U; i < count; ++i)
            if(levels[i] == entry.level) duplicate = true;
        if(!duplicate && count < capacity) levels[count++] = entry.level;
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    /* Highest level first makes the newest progression state easiest to reach. */
    for(uint8_t i = 0U; i < count; ++i) {
        for(uint8_t j = (uint8_t)(i + 1U); j < count; ++j) {
            if(levels[j] <= levels[i]) continue;
            uint8_t swap = levels[i];
            levels[i] = levels[j];
            levels[j] = swap;
        }
    }
    return count;
}

static void dnd_storage_restore_sidecar_live_path(
    char* output,
    size_t size,
    uint32_t profile,
    uint8_t index) {
    const char* kinds[] = {
        "spellbook", "items", "feats", "appliedgrants", "languages", "proficiencies"};
    dnd_storage_collection_path(output, size, profile, kinds[index]);
}

static void dnd_storage_restore_sidecar_snapshot_path(
    char* output,
    size_t size,
    uint32_t profile,
    const DndCharacter* character,
    uint8_t index) {
    const char* suffixes[] = {
        "spellbook", "items", "features", "appliedgrants", "languages", "proficiencies"};
    dnd_storage_level_sidecar_snapshot_path(output, size, profile, character, suffixes[index]);
}

static bool dnd_storage_inventory_bag_backup_filename(
    const char* filename,
    uint32_t profile,
    char* original,
    size_t original_size) {
    if(!filename) return false;
    const char* suffix = ".shdbak";
    size_t length = strlen(filename);
    size_t suffix_len = strlen(suffix);
    if(length <= suffix_len || strcmp(filename + length - suffix_len, suffix) != 0) return false;
    size_t base_len = length - suffix_len;
    if(base_len >= original_size) return false;
    memcpy(original, filename, base_len);
    original[base_len] = '\0';
    char safe[DND_INVENTORY_BAG_NAME_LEN];
    return dnd_storage_inventory_nonmain_bag_filename(original, profile, safe, sizeof(safe));
}

static bool dnd_storage_cleanup_bag_backups(Storage* storage, uint32_t profile, bool restore) {
    while(true) {
        File* directory = storage_file_alloc(storage);
        if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
            if(directory) storage_file_free(directory);
            return false;
        }
        FileInfo info;
        char filename[128];
        char original[128];
        char backup[DND_FS_PATH_LEN] = {0};
        char live[DND_FS_PATH_LEN] = {0};
        while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
            if(file_info_is_dir(&info) || !dnd_storage_inventory_bag_backup_filename(
                                              filename, profile, original, sizeof(original)))
                continue;
            if(!dnd_fs_child_path(backup, sizeof(backup), DND_STORAGE_DATA_DIR, NULL, filename) ||
               !dnd_fs_child_path(live, sizeof(live), DND_STORAGE_DATA_DIR, NULL, original)) {
                backup[0] = '\0';
                continue;
            }
            break;
        }
        storage_dir_close(directory);
        storage_file_free(directory);
        if(!backup[0]) return true;
        bool ok = restore ? dnd_storage_copy_file_direct(storage, backup, live) : true;
        if(storage_common_remove(storage, backup) != FSE_OK) return false;
        if(!ok) return false;
    }
}

static bool dnd_storage_backup_nonmain_inventory_bags(Storage* storage, uint32_t profile) {
    (void)dnd_storage_cleanup_bag_backups(storage, profile, false);
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    bool success = true;
    FileInfo info;
    char filename[128];
    char safe[DND_INVENTORY_BAG_NAME_LEN];
    while(success && storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info) ||
           !dnd_storage_inventory_nonmain_bag_filename(filename, profile, safe, sizeof(safe)))
            continue;
        char live[DND_FS_PATH_LEN];
        char backup[DND_FS_LONG_PATH_LEN];
        if(!dnd_fs_child_path(live, sizeof(live), DND_STORAGE_DATA_DIR, NULL, filename)) {
            success = false;
            break;
        }
        snprintf(backup, sizeof(backup), "%s.shdbak", live);
        success = dnd_storage_copy_file_direct(storage, live, backup);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return success;
}

static bool dnd_storage_restore_inventory_bags(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character) {
    if(!dnd_storage_backup_nonmain_inventory_bags(storage, profile)) return false;
    if(!dnd_storage_remove_nonmain_inventory_bags(storage, profile)) {
        (void)dnd_storage_cleanup_bag_backups(storage, profile, true);
        return false;
    }

    char safe_character[DND_CHARACTER_NAME_LEN];
    char prefix[96];
    dnd_storage_filename_name(
        safe_character,
        sizeof(safe_character),
        character && character->name[0] ? character->name : "Unnamed");
    snprintf(
        prefix,
        sizeof(prefix),
        "ch_%lu_%s_%u_itemsbag_",
        (unsigned long)profile,
        safe_character,
        character ? dnd_storage_character_level(character) : 1U);
    const size_t prefix_len = strlen(prefix);

    File* directory = storage_file_alloc(storage);
    bool success = directory && storage_dir_open(directory, DND_STORAGE_DATA_DIR);
    if(success) {
        FileInfo info;
        char filename[128];
        while(success && storage_dir_read(directory, &info, filename, sizeof(filename))) {
            size_t length = strlen(filename);
            if(file_info_is_dir(&info) || strncmp(filename, prefix, prefix_len) != 0 ||
               length <= prefix_len + 4U || strcmp(filename + length - 4U, ".shd") != 0)
                continue;
            size_t safe_len = length - prefix_len - 4U;
            if(!safe_len || safe_len >= DND_INVENTORY_BAG_NAME_LEN) {
                success = false;
                break;
            }
            char safe[DND_INVENTORY_BAG_NAME_LEN];
            char snapshot[DND_FS_PATH_LEN];
            char live[DND_FS_PATH_LEN];
            memcpy(safe, filename + prefix_len, safe_len);
            safe[safe_len] = '\0';
            if(!dnd_fs_child_path(
                   snapshot, sizeof(snapshot), DND_STORAGE_DATA_DIR, NULL, filename)) {
                success = false;
                break;
            }
            snprintf(
                live,
                sizeof(live),
                "%s/inv%s_%lu.txt",
                DND_STORAGE_DATA_DIR,
                safe,
                (unsigned long)profile);
            success = dnd_storage_copy_file_direct(storage, snapshot, live);
        }
    }
    if(directory) {
        if(success || storage_file_get_error(directory) == FSE_OK) storage_dir_close(directory);
        storage_file_free(directory);
    }
    if(!success) {
        (void)dnd_storage_remove_nonmain_inventory_bags(storage, profile);
        (void)dnd_storage_cleanup_bag_backups(storage, profile, true);
        return false;
    }
    return dnd_storage_cleanup_bag_backups(storage, profile, false);
}

typedef struct {
    char core_snapshot[DND_FS_PATH_LEN];
    char old_core[DND_FS_PATH_LEN];
    char rollback_core[DND_FS_PATH_LEN];
    char live[6][DND_FS_PATH_LEN];
    char snapshot[6][DND_FS_PATH_LEN];
    char rollback[6][DND_FS_PATH_LEN];
    bool live_present[6];
    bool snapshot_present[6];
    bool old_core_present;
} DndShdRestoreContext;

static bool dnd_storage_restore_shd_internal(
    Storage* storage,
    uint32_t profile,
    uint8_t level,
    const char* core_snapshot,
    DndSaveData* data) {
    if(!storage || !data || !core_snapshot || level < 1U || level > 20U) return false;
    DndShdRestoreContext* context = calloc(1U, sizeof(DndShdRestoreContext));
    if(!context) return false;
    bool success = false;

    dnd_storage_copy(context->core_snapshot, sizeof(context->core_snapshot), core_snapshot);
    if(strcmp(context->core_snapshot, core_snapshot) ||
       !dnd_storage_load_text_path(storage, context->core_snapshot, data))
        goto cleanup;

    uint8_t bundle_version =
        dnd_storage_level_bundle_marker_version(storage, profile, &data->character);

    context->old_core_present = dnd_storage_find_profile_path(
        storage, profile, context->old_core, sizeof(context->old_core));
    dnd_storage_work_path(
        context->rollback_core, sizeof(context->rollback_core), profile, "shd_core");
    storage_common_remove(storage, context->rollback_core);
    if(context->old_core_present &&
       !dnd_storage_copy_file_direct(storage, context->old_core, context->rollback_core))
        goto cleanup;

    const char* rollback_kind[] = {
        "shd_spell",
        "shd_items",
        "shd_features",
        "shd_grants",
        "shd_languages",
        "shd_proficiencies"};
    for(uint8_t i = 0U; i < 6U; ++i) {
        dnd_storage_restore_sidecar_live_path(
            context->live[i], sizeof(context->live[i]), profile, i);
        dnd_storage_restore_sidecar_snapshot_path(
            context->snapshot[i], sizeof(context->snapshot[i]), profile, &data->character, i);
        dnd_storage_work_path(
            context->rollback[i], sizeof(context->rollback[i]), profile, rollback_kind[i]);
        storage_common_remove(storage, context->rollback[i]);
        context->live_present[i] = storage_file_exists(storage, context->live[i]);
        context->snapshot_present[i] = storage_file_exists(storage, context->snapshot[i]);
        if(context->live_present[i] &&
           !dnd_storage_copy_file_direct(storage, context->live[i], context->rollback[i]))
            goto cleanup;
    }

    success = dnd_storage_save_profile(storage, profile, data);
    for(uint8_t i = 0U; success && i < 6U; ++i) {
        if(context->snapshot_present[i])
            success =
                dnd_storage_copy_file_direct(storage, context->snapshot[i], context->live[i]);
        else if(bundle_version >= (i < 4U ? 1U : 2U) && storage_file_exists(storage, context->live[i]))
            success = storage_common_remove(storage, context->live[i]) == FSE_OK;
    }
    if(success && bundle_version >= 3U)
        success = dnd_storage_restore_inventory_bags(storage, profile, &data->character);

    if(!success) {
        char restored_core[DND_FS_PATH_LEN];
        if(dnd_storage_find_profile_path(storage, profile, restored_core, sizeof(restored_core)))
            (void)storage_common_remove(storage, restored_core);
        if(context->old_core_present)
            (void)dnd_storage_copy_file_direct(storage, context->rollback_core, context->old_core);
        for(uint8_t i = 0U; i < 6U; ++i) {
            if(context->live_present[i])
                (void)dnd_storage_copy_file_direct(
                    storage, context->rollback[i], context->live[i]);
            else if(storage_file_exists(storage, context->live[i]))
                (void)storage_common_remove(storage, context->live[i]);
        }
        goto cleanup;
    }

    /* The restored core must not retain stale resident collection pointers. Live
       sidecars are authoritative and will be paged in again by their owner FAPs. */
    dnd_data_clear_spells(&data->character);
    dnd_data_clear_items(&data->character);
    dnd_data_reserve_features_exact(&data->character, 0U);
    dnd_data_reserve_grants_exact(&data->character, 0U);
    data->character.feature_count = 0U;
    data->character.grant_count = 0U;

cleanup:
    storage_common_remove(storage, context->rollback_core);
    for(uint8_t i = 0U; i < 6U; ++i)
        storage_common_remove(storage, context->rollback[i]);
    free(context);
    return success;
}

bool dnd_storage_restore_shd(Storage* storage, uint32_t profile, uint8_t level, DndSaveData* data) {
    char core_snapshot[DND_FS_PATH_LEN];
    if(!storage || !data || level < 1U || level > 20U ||
       !dnd_storage_find_shd_path(storage, profile, level, core_snapshot, sizeof(core_snapshot)))
        return false;
    return dnd_storage_restore_shd_internal(storage, profile, level, core_snapshot, data);
}

bool dnd_storage_restore_shd_path(
    Storage* storage,
    uint32_t profile,
    const char* core_snapshot,
    DndSaveData* data) {
    if(!storage || !core_snapshot || !data) return false;
    const char* filename = strrchr(core_snapshot, '/');
    filename = filename ? filename + 1U : core_snapshot;
    DndProfileEntry entry;
    if(!dnd_storage_parse_core_shd_filename(filename, &entry) || entry.id != profile) return false;
    char expected[DND_FS_PATH_LEN];
    if(!dnd_fs_child_path(expected, sizeof(expected), DND_STORAGE_DATA_DIR, NULL, filename) ||
       strcmp(expected, core_snapshot))
        return false;
    return dnd_storage_restore_shd_internal(storage, profile, entry.level, core_snapshot, data);
}

bool dnd_storage_move_legacy_profiles(Storage* storage) {
    if(!storage) return false;
    storage_common_mkdir(storage, DND_STORAGE_DATA_DIR);
    FileInfo legacy_info;
    if(storage_common_stat(storage, DND_STORAGE_LEGACY_PROFILE_DIR, &legacy_info) != FSE_OK ||
       !file_info_is_dir(&legacy_info))
        return true;

    bool all_ok = true;
    while(true) {
        File* directory = storage_file_alloc(storage);
        if(!directory) return false;
        if(!storage_dir_open(directory, DND_STORAGE_LEGACY_PROFILE_DIR)) {
            storage_file_free(directory);
            return false;
        }
        FileInfo info;
        char filename[128];
        bool moved_one = false;
        while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
            size_t length = strlen(filename);
            if(file_info_is_dir(&info) || strncmp(filename, "ch_", 3U) != 0 || length < 8U ||
               strcmp(filename + length - 4U, ".txt") != 0)
                continue;
            DndProfileEntry legacy_entry;
            if(!dnd_storage_parse_profile_filename(filename, &legacy_entry)) continue;

            char source[DND_FS_LONG_PATH_LEN];
            char destination[DND_FS_LONG_PATH_LEN];
            if(!dnd_fs_child_path(
                   source, sizeof(source), DND_STORAGE_LEGACY_PROFILE_DIR, NULL, filename) ||
               !dnd_fs_child_path(
                   destination, sizeof(destination), DND_STORAGE_DATA_DIR, NULL, filename)) {
                all_ok = false;
                continue;
            }
            /* The current root wins by character ID, not only by exact filename.
               This prevents two primary ch*.txt files with the same ID when the
               character name/level changed after the old copy was created. */
            char current_path[DND_FS_LONG_PATH_LEN];
            if(dnd_storage_find_profile_path(
                   storage, legacy_entry.id, current_path, sizeof(current_path)))
                continue;
            if(storage_file_exists(storage, destination)) continue;
            if(storage_common_rename(storage, source, destination) == FSE_OK) {
                moved_one = true;
                break;
            }
            all_ok = false;
        }
        storage_dir_close(directory);
        storage_file_free(directory);
        if(!moved_one) break;
    }
    return all_ok;
}

void dnd_storage_profiles_set_defaults(DndProfileState* profiles) {
    memset(profiles, 0, sizeof(*profiles));
}

void dnd_storage_profiles_free(DndProfileState* profiles) {
    if(!profiles) return;
    dnd_storage_profiles_set_defaults(profiles);
}

static void
    dnd_storage_profiles_insert_smallest(DndProfileState* profiles, const DndProfileEntry* entry) {
    if(profiles->cache_count < DND_STORAGE_PROFILE_CACHE_SIZE) {
        profiles->entries[profiles->cache_count++] = *entry;
    } else {
        if(entry->id >= profiles->entries[profiles->cache_count - 1U].id) return;
        profiles->entries[profiles->cache_count - 1U] = *entry;
    }
    uint8_t position = (uint8_t)(profiles->cache_count - 1U);
    while(position > 0U && profiles->entries[position - 1U].id > profiles->entries[position].id) {
        DndProfileEntry swap = profiles->entries[position - 1U];
        profiles->entries[position - 1U] = profiles->entries[position];
        profiles->entries[position] = swap;
        --position;
    }
}

static void
    dnd_storage_profiles_insert_largest(DndProfileState* profiles, const DndProfileEntry* entry) {
    if(profiles->cache_count < DND_STORAGE_PROFILE_CACHE_SIZE) {
        profiles->entries[profiles->cache_count++] = *entry;
        uint8_t position = (uint8_t)(profiles->cache_count - 1U);
        while(position > 0U &&
              profiles->entries[position - 1U].id > profiles->entries[position].id) {
            DndProfileEntry swap = profiles->entries[position - 1U];
            profiles->entries[position - 1U] = profiles->entries[position];
            profiles->entries[position] = swap;
            --position;
        }
        return;
    }
    if(entry->id <= profiles->entries[0].id) return;
    profiles->entries[0] = *entry;
    uint8_t position = 0U;
    while(position + 1U < profiles->cache_count &&
          profiles->entries[position].id > profiles->entries[position + 1U].id) {
        DndProfileEntry swap = profiles->entries[position + 1U];
        profiles->entries[position + 1U] = profiles->entries[position];
        profiles->entries[position] = swap;
        ++position;
    }
}

static bool dnd_storage_profiles_scan_cache(
    Storage* storage,
    DndProfileState* profiles,
    uint32_t boundary,
    bool after,
    uint16_t cache_start) {
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    profiles->cache_count = 0U;
    FileInfo info;
    char filename[128];
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info)) continue;
        DndProfileEntry entry;
        if(!dnd_storage_parse_profile_filename(filename, &entry)) continue;
        if(after) {
            if(entry.id <= boundary) continue;
            dnd_storage_profiles_insert_smallest(profiles, &entry);
        } else {
            if(entry.id >= boundary) continue;
            dnd_storage_profiles_insert_largest(profiles, &entry);
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(!profiles->cache_count) return false;
    profiles->cache_start = after ? cache_start :
                                    (cache_start >= profiles->cache_count ?
                                         (uint16_t)(cache_start - profiles->cache_count) :
                                         0U);
    return true;
}

bool dnd_storage_profiles_find(Storage* storage, uint32_t profile, DndProfileEntry* output) {
    if(!storage) return false;
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    bool found = false;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info)) continue;
        DndProfileEntry entry;
        if(dnd_storage_parse_profile_filename(filename, &entry) && entry.id == profile) {
            if(output) *output = entry;
            found = true;
            break;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return found;
}

bool dnd_storage_profiles_refresh(Storage* storage, DndProfileState* profiles) {
    furi_assert(storage);
    furi_assert(profiles);
    storage_common_mkdir(storage, DND_STORAGE_DATA_DIR);
    profiles->count = 0U;
    profiles->cache_start = 0U;
    profiles->cache_count = 0U;
    profiles->active_entry_valid = 0U;
    profiles->highest_reserved_id = 0U;
    profiles->reserved_id_seen = 0U;
    profiles->character_file_seen = 0U;
    profiles->scan_succeeded = 0U;
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    bool success = true;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info)) continue;

        size_t length = strlen(filename);
        bool collection_sidecar =
            !strncmp(filename, "spellbook_", 10U) || !strncmp(filename, "inventory_", 10U) ||
            !strncmp(filename, "feats_", 6U) || !strncmp(filename, "appliedgrants_", 14U) ||
            (length >= 14U && !strcmp(filename + length - 14U, "_spellbook.txt")) ||
            (length >= 10U && !strcmp(filename + length - 10U, "_items.txt"));
        bool character_related =
            (length >= 8U && !collection_sidecar && !strncmp(filename, "ch_", 3U) &&
             !strcmp(filename + length - 4U, ".txt")) ||
            !strncmp(filename, "custom_backup_", 14U) ||
            !strncmp(filename, "custom_rejected_", 16U) ||
            !strncmp(filename, "custom_write_", 13U);
        if(character_related) profiles->character_file_seen = 1U;

        DndProfileEntry entry;
        bool primary = dnd_storage_parse_profile_filename(filename, &entry);
        if(!primary) continue;
        if(!profiles->reserved_id_seen || entry.id > profiles->highest_reserved_id)
            profiles->highest_reserved_id = entry.id;
        profiles->reserved_id_seen = 1U;
        profiles->character_file_seen = 1U;
        if(entry.id == profiles->active_profile) {
            profiles->active_entry = entry;
            profiles->active_entry_valid = 1U;
        }
        if(profiles->count == UINT16_MAX) {
            success = false;
            break;
        }
        ++profiles->count;
        dnd_storage_profiles_insert_smallest(profiles, &entry);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    profiles->scan_succeeded = success ? 1U : 0U;
    return success;
}

const DndProfileEntry*
    dnd_storage_profiles_entry_at(Storage* storage, DndProfileState* profiles, uint16_t index) {
    if(!storage || !profiles || index >= profiles->count) return NULL;
    if(!profiles->cache_count && !dnd_storage_profiles_refresh(storage, profiles)) return NULL;

    uint16_t guard = 0U;
    while(index < profiles->cache_start && profiles->cache_count && guard++ < UINT16_MAX) {
        uint32_t before = profiles->entries[0].id;
        uint16_t old_start = profiles->cache_start;
        if(!dnd_storage_profiles_scan_cache(storage, profiles, before, false, old_start) ||
           profiles->cache_start >= old_start)
            return NULL;
    }
    guard = 0U;
    while(index >= (uint16_t)(profiles->cache_start + profiles->cache_count) &&
          profiles->cache_count && guard++ < UINT16_MAX) {
        uint32_t after = profiles->entries[profiles->cache_count - 1U].id;
        uint16_t next_start = (uint16_t)(profiles->cache_start + profiles->cache_count);
        if(!dnd_storage_profiles_scan_cache(storage, profiles, after, true, next_start))
            return NULL;
    }
    if(index < profiles->cache_start ||
       index >= (uint16_t)(profiles->cache_start + profiles->cache_count))
        return NULL;
    return &profiles->entries[index - profiles->cache_start];
}

bool dnd_storage_profiles_window(Storage* storage, DndProfileState* profiles, uint16_t start) {
    if(!storage || !profiles || start >= profiles->count) return false;
    if(profiles->cache_count && profiles->cache_start == start) return true;
    if(start == 0U) {
        uint32_t active = profiles->active_profile;
        bool scanned = dnd_storage_profiles_refresh(storage, profiles);
        profiles->active_profile = active;
        return scanned && profiles->cache_count;
    }
    const DndProfileEntry* previous = dnd_storage_profiles_entry_at(storage, profiles, start - 1U);
    if(!previous) return false;
    uint32_t boundary = previous->id;
    return dnd_storage_profiles_scan_cache(storage, profiles, boundary, true, start);
}

bool dnd_storage_profiles_next_after(
    Storage* storage,
    uint32_t after_profile,
    DndProfileEntry* output) {
    if(!storage || !output) return false;
    File* directory = storage_file_alloc(storage);
    if(!directory) return false;
    if(!storage_dir_open(directory, DND_STORAGE_DATA_DIR)) {
        storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    DndProfileEntry next = {0};
    DndProfileEntry first = {0};
    bool have_next = false;
    bool have_first = false;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info)) continue;
        DndProfileEntry entry;
        if(!dnd_storage_parse_profile_filename(filename, &entry)) continue;
        if(!have_first || entry.id < first.id) {
            first = entry;
            have_first = true;
        }
        if(entry.id > after_profile && (!have_next || entry.id < next.id)) {
            next = entry;
            have_next = true;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(have_next) {
        *output = next;
        return true;
    }
    if(have_first && first.id != after_profile) {
        *output = first;
        return true;
    }
    return false;
}

uint32_t dnd_storage_profiles_next_id(const DndProfileState* profiles) {
    if(!profiles->reserved_id_seen) return 0U;
    return profiles->highest_reserved_id < UINT32_MAX ? profiles->highest_reserved_id + 1U :
                                                        UINT32_MAX;
}

bool dnd_storage_profiles_load(Storage* storage, DndProfileState* profiles) {
    furi_assert(storage);
    furi_assert(profiles);
    dnd_storage_profiles_set_defaults(profiles);

    /* DNDolphins may still recover its own profile list if metadata is absent.
       All FAPs use dnd_profile_handoff for Active= metadata; this full storage
       module owns profile scanning, fallback selection and profile-file persistence. */
    uint32_t active_profile = 0U;
    if(dnd_profile_ref_active_id(storage, &active_profile))
        profiles->active_profile = active_profile;

    bool scanned = dnd_storage_profiles_refresh(storage, profiles);
    bool active_found = dnd_storage_profiles_find(storage, profiles->active_profile, NULL);
    if(!active_found && profiles->count) {
        DndProfileEntry next;
        if(dnd_storage_profiles_next_after(storage, profiles->active_profile, &next)) {
            profiles->active_profile = next.id;
            profiles->active_entry = next;
            profiles->active_entry_valid = 1U;
        }
    }
    return scanned;
}

bool dnd_storage_profiles_save(Storage* storage, const DndProfileState* profiles) {
    furi_assert(storage);
    furi_assert(profiles);
    storage_common_mkdir(storage, DND_STORAGE_DATA_DIR);
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool written =
        storage_file_open(
            file, DND_STORAGE_ACTIVE_PROFILE_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        dnd_storage_writef(file, "Active=%lu\n", (unsigned long)profiles->active_profile) &&
        storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!written) {
        storage_common_remove(storage, DND_STORAGE_ACTIVE_PROFILE_TEMP_PATH);
        return false;
    }
    return dnd_storage_publish_temp(
        storage,
        DND_STORAGE_ACTIVE_PROFILE_TEMP_PATH,
        DND_STORAGE_ACTIVE_PROFILE_PATH,
        DND_STORAGE_ACTIVE_PROFILE_BACKUP_PATH);
}
