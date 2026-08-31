#include "dndolphins_storage.h"
#include "dnd_fs.h"
#include "dnd_handoff.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POCKET_D20_TEXT_VERSION                  5U
#define POCKET_D20_VALUE_LINE_LEN                320U
#define POCKET_D20_ENCODED_LINE_LEN              ((POCKET_D20_DETAIL_LEN * 3U) + 64U)
#define POCKET_D20_FORMAT_LINE_LEN               256U
#define POCKET_D20_READ_BUFFER                   256U
#define POCKET_D20_COLLECTION_LINE_LEN           1280U
#define POCKET_D20_ITEM_SEED_LINE_LEN              256U
#define POCKET_D20_DATA_DIR                      POCKET_D20_CHARACTER_DATA_ROOT
#define POCKET_D20_EXPORT_DIR                    POCKET_D20_CHARACTER_DATA_ROOT "/exports"
#define POCKET_D20_ARCHIVE_DIR                   POCKET_D20_CHARACTER_DATA_ROOT "/archive"

#define POCKET_D20_LEGACY_PROFILE_DIR "/ext/apps_data/dungeons_and_dolphins/profiles"

#define POCKET_D20_ACTIVE_PROFILE_PATH      POCKET_D20_CHARACTER_DATA_ROOT "/custom_active_profile.txt"
#define POCKET_D20_ACTIVE_PROFILE_TEMP_PATH POCKET_D20_CHARACTER_DATA_ROOT "/custom_active_profile.tmp"
#define POCKET_D20_ACTIVE_PROFILE_BACKUP_PATH POCKET_D20_CHARACTER_DATA_ROOT "/custom_active_profile.bak"

static void pocket_d20_copy(char* destination, size_t size, const char* source) {
    if(size == 0U) return;
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

static bool pocket_d20_parse_u32_span(
    const char* begin, const char* end, uint32_t maximum, uint32_t* output) {
    if(!begin || !end || !output || begin >= end) return false;
    uint32_t value = 0U;
    for(const char* cursor = begin; cursor < end; ++cursor) {
        if(*cursor < '0' || *cursor > '9') return false;
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(value > maximum / 10U ||
           (value == maximum / 10U && digit > maximum % 10U))
            return false;
        value = value * 10U + digit;
    }
    *output = value;
    return true;
}

static bool pocket_d20_parse_u32_range(const char* text, uint32_t maximum, uint32_t* output) {
    return text && pocket_d20_parse_u32_span(text, text + strlen(text), maximum, output);
}

static bool pocket_d20_publish_temp(
    Storage* storage,
    const char* temporary,
    const char* destination,
    const char* backup) {
    if(!storage || !temporary || !destination || !backup) return false;
    bool had_destination = storage_file_exists(storage, destination);
    if(had_destination) {
        if(storage_file_exists(storage, backup) && storage_common_remove(storage, backup) != FSE_OK)
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

static bool pocket_d20_copy_file(
    Storage* storage,
    const char* source,
    const char* destination,
    const char* temporary);

static uint8_t pocket_d20_character_level(const PocketCharacter* character) {
    uint16_t total = 0U;
    for(uint8_t i = 0U; i < character->class_count; ++i)
        total += character->classes[i].level;
    if(total < 1U) total = 1U;
    return total > 255U ? 255U : (uint8_t)total;
}

static void pocket_d20_filename_name(char* output, size_t size, const char* name) {
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
        pocket_d20_copy(output, size, "Unnamed");
        return;
    }
    output[position] = '\0';
}

static void pocket_d20_profile_path(
    char* output,
    size_t size,
    uint32_t profile,
    const PocketCharacter* character) {
    char safe_name[POCKET_D20_CHARACTER_NAME_LEN];
    pocket_d20_filename_name(safe_name, sizeof(safe_name), character->name);
    snprintf(
        output,
        size,
        "%s/ch_%lu_%s_%u.txt",
        POCKET_D20_DATA_DIR,
        (unsigned long)profile,
        safe_name,
        pocket_d20_character_level(character));
}

static bool pocket_d20_writef(File* file, const char* format, ...) {
    char line[POCKET_D20_FORMAT_LINE_LEN];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if(length < 0 || (size_t)length >= sizeof(line)) return false;
    return storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static uint8_t pocket_d20_hex_value(char value) {
    if(value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if(value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
    if(value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    return 0xFFU;
}

static bool pocket_d20_write_string(File* file, const char* key, const char* value) {
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

static void pocket_d20_decode_string(char* destination, size_t size, const char* value) {
    size_t output = 0U;
    for(size_t input = 0U; value[input] != '\0' && output + 1U < size; ++input) {
        if(value[input] == '%' && value[input + 1U] && value[input + 2U]) {
            uint8_t high = pocket_d20_hex_value(value[input + 1U]);
            uint8_t low = pocket_d20_hex_value(value[input + 2U]);
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
    pocket_d20_write_i8_array(File* file, const char* key, const int8_t* values, size_t count) {
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
    pocket_d20_write_u8_array(File* file, const char* key, const uint8_t* values, size_t count) {
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
    uint8_t buffer[POCKET_D20_READ_BUFFER];
    uint16_t position;
    uint16_t count;
    bool eof;
} PocketD20Reader;

static void pocket_d20_reader_init(PocketD20Reader* reader, File* file) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool pocket_d20_reader_next(PocketD20Reader* reader, char* value) {
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
    return true;
}

static bool pocket_d20_read_line(PocketD20Reader* reader, char* line, size_t size) {
    size_t position = 0U;
    char character = '\0';
    while(position + 1U < size) {
        if(!pocket_d20_reader_next(reader, &character)) break;
        if(character == '\n') break;
        if(character != '\r') line[position++] = character;
    }
    line[position] = '\0';
    return position > 0U || character == '\n';
}


static bool pocket_d20_parse_i32_span(const char* begin, const char* end, int32_t* output) {
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
        if(value > maximum / 10U ||
           (value == maximum / 10U && digit > maximum % 10U))
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

static size_t pocket_d20_parse_numbers(const char* value, int32_t* numbers, size_t maximum) {
    if(!value || !numbers || !maximum || !value[0]) return 0U;
    size_t count = 0U;
    const char* cursor = value;
    while(count < maximum) {
        const char* separator = strchr(cursor, ',');
        const char* end = separator ? separator : cursor + strlen(cursor);
        if(!pocket_d20_parse_i32_span(cursor, end, &numbers[count])) return 0U;
        ++count;
        if(!separator) return count;
        cursor = separator + 1U;
        if(!cursor[0]) return 0U;
    }
    /* More fields than expected are malformed rather than silently ignored. */
    return cursor[0] ? 0U : count;
}

static bool pocket_d20_indexed_key(
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


static void pocket_d20_spellbook_path(char* output, size_t size, uint32_t profile) {
    snprintf(
        output,
        size,
        "%s/spellbook_%lu.txt",
        POCKET_D20_DATA_DIR,
        (unsigned long)profile);
}

static void pocket_d20_items_path(char* output, size_t size, uint32_t profile) {
    snprintf(
        output,
        size,
        "%s/inventory_%lu.txt",
        POCKET_D20_DATA_DIR,
        (unsigned long)profile);
}

/* Collection files are established when their screen is opened.  Appends then
   operate only on an existing file, so Add New never depends on OPEN_ALWAYS
   semantics or a post-append reread to create its backing store. */
static bool pocket_d20_ensure_collection_sidecar(
    Storage* storage, const char* path, const char* header) {
    if(!storage || !path || !header) return false;

    /* Match the proven character-save path used before collection sidecars:
       ensure the known app-data directory, then let storage_file_open() be the
       authority. Do not gate writes through recursive stat()/mkdir checks on
       /ext; those checks can reject a valid mounted path before the file open is
       ever attempted on some firmware builds. */
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);

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
    bool success = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                   storage_file_write(file, header, strlen(header)) == strlen(header) &&
                   storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    return success && storage_file_exists(storage, path);
}

bool pocket_d20_storage_ensure_spellbook_sidecar(Storage* storage, uint32_t profile) {
    char path[POCKET_D20_PATH_LEN];
    pocket_d20_spellbook_path(path, sizeof(path), profile);
    return pocket_d20_ensure_collection_sidecar(storage, path, "DNDSpellbook=1\n");
}

bool pocket_d20_storage_ensure_items_sidecar(Storage* storage, uint32_t profile) {
    char path[POCKET_D20_PATH_LEN];
    pocket_d20_items_path(path, sizeof(path), profile);
    return pocket_d20_ensure_collection_sidecar(storage, path, "DNDItems=1\n");
}

static void pocket_d20_collection_snapshot_path(
    char* output,
    size_t size,
    uint32_t profile,
    const PocketCharacter* character,
    const char* collection) {
    char safe_name[POCKET_D20_CHARACTER_NAME_LEN];
    pocket_d20_filename_name(
        safe_name,
        sizeof(safe_name),
        character && character->name[0] ? character->name : "Unnamed");
    snprintf(
        output,
        size,
        "%s/ch_%lu_%s_%u_%s.swd",
        POCKET_D20_DATA_DIR,
        (unsigned long)profile,
        safe_name,
        character ? pocket_d20_character_level(character) : 1U,
        collection);
}

static bool pocket_d20_write_raw(File* file, const char* value) {
    if(!file || !value) return false;
    size_t length = strlen(value);
    return storage_file_write(file, value, length) == length;
}

static bool pocket_d20_write_collection_field(File* file, const char* value) {
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

static uint8_t pocket_d20_split_collection_line(char* line, char** fields, uint8_t capacity) {
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

static bool pocket_d20_close_synced_file(File* file, bool success) {
    if(file) {
        if(success) success = storage_file_sync(file);
        storage_file_close(file);
        storage_file_free(file);
    }
    return success;
}

static bool pocket_d20_copy_file_direct(
    Storage* storage, const char* source, const char* destination) {
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

static File* pocket_d20_open_collection_snapshot(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    const char* collection,
    char* snapshot,
    size_t snapshot_size,
    char* live,
    size_t live_size) {
    if(!storage || !owner || !collection || !snapshot || !live) return NULL;
    if(!strcmp(collection, "spellbook"))
        pocket_d20_spellbook_path(live, live_size, profile);
    else
        pocket_d20_items_path(live, live_size, profile);
    pocket_d20_collection_snapshot_path(
        snapshot, snapshot_size, profile, owner, collection);
    /* The collection and its SWD snapshot both live directly in the canonical
       DNDolphins data directory. Use the same best-effort mkdir pattern as the
       working character save path instead of the recursive parent validator. */
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);
    File* file = storage_file_alloc(storage);
    if(!file) return NULL;
    if(!storage_file_open(file, snapshot, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        return NULL;
    }
    return file;
}

static bool pocket_d20_publish_collection_snapshot(
    Storage* storage,
    File* file,
    const char* snapshot,
    const char* live,
    bool success) {
    success = pocket_d20_close_synced_file(file, success);
    if(!success) return false;
    return pocket_d20_copy_file_direct(storage, snapshot, live);
}


static bool pocket_d20_copy_live_collection_to_snapshot(
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

static bool pocket_d20_write_spell_record(
    File* file,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max) {
    if(!file || !spell) return false;
    return pocket_d20_write_raw(file, "S|") &&
           pocket_d20_write_collection_field(file, spell->name) &&
           pocket_d20_write_raw(file, "|") &&
           pocket_d20_write_collection_field(file, spell->detail) &&
           pocket_d20_write_raw(file, "|") &&
           pocket_d20_write_collection_field(file, spell->stable_id) &&
           pocket_d20_write_raw(file, "|") &&
           pocket_d20_write_collection_field(file, spell->source) &&
           pocket_d20_write_raw(file, "|") &&
           pocket_d20_write_collection_field(file, spell->school) &&
           pocket_d20_write_raw(file, "|") &&
           pocket_d20_write_collection_field(file, spell->grant_name) &&
           pocket_d20_writef(
               file,
               "|%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
               spell->level,
               spell->class_index,
               spell->prepared,
               spell->ritual,
               known,
               always_prepared,
               free_casts_current,
               free_casts_max,
               spell->grant_source);
}

static bool pocket_d20_parse_spell_record(
    char* line,
    PocketSpell* spell,
    uint8_t* known,
    uint8_t* always_prepared,
    uint8_t* free_casts_current,
    uint8_t* free_casts_max) {
    if(!line || !spell || !known || !always_prepared || !free_casts_current || !free_casts_max)
        return false;
    char* fields[8];
    uint8_t field_count = pocket_d20_split_collection_line(line, fields, 8U);
    if(field_count != 8U || strcmp(fields[0], "S")) return false;
    int32_t n[9];
    if(pocket_d20_parse_numbers(fields[7], n, 9U) != 9U) return false;
    memset(spell, 0, sizeof(*spell));
    pocket_d20_decode_string(spell->name, sizeof(spell->name), fields[1]);
    pocket_d20_decode_string(spell->detail, sizeof(spell->detail), fields[2]);
    pocket_d20_decode_string(spell->stable_id, sizeof(spell->stable_id), fields[3]);
    pocket_d20_decode_string(spell->source, sizeof(spell->source), fields[4]);
    pocket_d20_decode_string(spell->school, sizeof(spell->school), fields[5]);
    pocket_d20_decode_string(spell->grant_name, sizeof(spell->grant_name), fields[6]);
    spell->level = (uint8_t)n[0];
    spell->class_index = (uint8_t)n[1];
    spell->prepared = n[2] ? 1U : 0U;
    spell->ritual = n[3] ? 1U : 0U;
    *known = n[4] ? 1U : 0U;
    *always_prepared = n[5] ? 1U : 0U;
    *free_casts_current = (uint8_t)n[6];
    *free_casts_max = (uint8_t)n[7];
    spell->grant_source = (uint8_t)n[8];
    if(spell->level > 9U) spell->level = 9U;
    if(spell->class_index >= POCKET_D20_MAX_CLASSES) spell->class_index = 0U;
    if(*free_casts_max > 20U) *free_casts_max = 20U;
    if(*free_casts_current > *free_casts_max) *free_casts_current = *free_casts_max;
    if(spell->grant_source >= PocketGrantSourceCount) spell->grant_source = PocketGrantSpecies;
    return true;
}

static bool pocket_d20_write_item_record(File* file, const PocketItem* item) {
    if(!file || !item) return false;
    return pocket_d20_write_raw(file, "I|") &&
           pocket_d20_write_collection_field(file, item->name) &&
           pocket_d20_write_raw(file, "|") &&
           pocket_d20_write_collection_field(file, item->detail) &&
           pocket_d20_write_raw(file, "|") &&
           pocket_d20_write_collection_field(file, item->ammunition_group) &&
           pocket_d20_writef(
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
               item->container_index,
               item->charges_current,
               item->charges_max,
               item->armor_base,
               item->armor_dex_cap,
               item->shield_bonus);
}

static bool pocket_d20_parse_item_record(char* line, PocketItem* item) {
    if(!line || !item) return false;
    char* fields[5];
    uint8_t field_count = pocket_d20_split_collection_line(line, fields, 5U);
    if(field_count != 5U || strcmp(fields[0], "I")) return false;
    int32_t n[25];
    if(pocket_d20_parse_numbers(fields[4], n, 25U) != 25U) return false;
    memset(item, 0, sizeof(*item));
    pocket_d20_decode_string(item->name, sizeof(item->name), fields[1]);
    pocket_d20_decode_string(item->detail, sizeof(item->detail), fields[2]);
    pocket_d20_decode_string(item->ammunition_group, sizeof(item->ammunition_group), fields[3]);
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
    item->container_index = (int8_t)n[19];
    item->charges_current = (int16_t)n[20];
    item->charges_max = (int16_t)n[21];
    item->armor_base = (uint8_t)n[22];
    item->armor_dex_cap = (int8_t)n[23];
    item->shield_bonus = (uint8_t)n[24];
    if(item->attack_ability > PocketAttackAbilityBest)
        item->attack_ability = PocketAttackAbilityAuto;
    if(item->damage_type >= PocketDamageTypeCount) item->damage_type = PocketDamageBludgeoning;
    if(item->damage_dice > 20U) item->damage_dice = 20U;
    if(item->extra_dice > 20U) item->extra_dice = 20U;
    if(item->container_index < -1 || item->container_index >= (int8_t)POCKET_D20_MAX_ITEMS)
        item->container_index = -1;
    if(item->armor_dex_cap < -1 || item->armor_dex_cap > 9) item->armor_dex_cap = -1;
    return true;
}

bool pocket_d20_storage_visit_spells(
    Storage* storage,
    uint32_t profile,
    PocketD20SpellRecordVisitor visitor,
    void* context,
    uint8_t* total_count) {
    if(!storage) return false;
    if(total_count) *total_count = 0U;
    char path[POCKET_D20_PATH_LEN];
    pocket_d20_spellbook_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    char* line = malloc(POCKET_D20_COLLECTION_LINE_LEN);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    PocketD20Reader reader;
    pocket_d20_reader_init(&reader, file);
    bool success = true;
    uint8_t logical = 0U;
    while(pocket_d20_read_line(&reader, line, POCKET_D20_COLLECTION_LINE_LEN)) {
        if(strncmp(line, "S|", 2U)) continue;
        PocketSpell parsed;
        uint8_t known = 0U, always = 0U, free_current = 0U, free_max = 0U;
        if(!pocket_d20_parse_spell_record(
               line, &parsed, &known, &always, &free_current, &free_max))
            continue;
        bool keep_scanning = true;
        if(visitor)
            keep_scanning = visitor(
                logical, &parsed, known, always, free_current, free_max, context);
        if(logical < POCKET_D20_MAX_SPELLS) ++logical;
        if(!keep_scanning) break;
    }
    if(storage_file_get_error(file) != FSE_OK) success = false;
    if(total_count) *total_count = logical;
    free(line);
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

typedef struct {
    PocketD20SpellClassCounts* counts;
} PocketD20SpellCountContext;

static bool pocket_d20_count_spell_record(
    uint8_t logical_index,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    (void)logical_index;
    (void)free_casts_current;
    (void)free_casts_max;
    PocketD20SpellCountContext* count_context = context;
    if(!count_context || !count_context->counts || !spell ||
       spell->class_index >= POCKET_D20_MAX_CLASSES)
        return true;
    uint8_t class_index = spell->class_index;
    if(known && count_context->counts->known[class_index] < UINT8_MAX)
        ++count_context->counts->known[class_index];
    if((spell->prepared || always_prepared) &&
       count_context->counts->prepared[class_index] < UINT8_MAX)
        ++count_context->counts->prepared[class_index];
    return true;
}

bool pocket_d20_storage_spell_class_counts(
    Storage* storage,
    uint32_t profile,
    PocketD20SpellClassCounts* counts,
    uint8_t* total_count) {
    if(!storage || !counts) return false;
    memset(counts, 0, sizeof(*counts));
    PocketD20SpellCountContext context = {.counts = counts};
    return pocket_d20_storage_visit_spells(
        storage, profile, pocket_d20_count_spell_record, &context, total_count);
}

bool pocket_d20_storage_load_spellbook_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    PocketCharacter* character,
    uint8_t* total_count) {
    if(!storage || !character || !total_count) return false;
    pocket_d20_data_clear_spells(character);
    *total_count = 0U;
    char path[POCKET_D20_PATH_LEN];
    pocket_d20_spellbook_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    char* line = malloc(POCKET_D20_COLLECTION_LINE_LEN);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    PocketD20Reader reader;
    pocket_d20_reader_init(&reader, file);
    bool success = true;
    uint8_t logical = 0U;
    while(pocket_d20_read_line(&reader, line, POCKET_D20_COLLECTION_LINE_LEN)) {
        if(strncmp(line, "S|", 2U)) continue;
        PocketSpell parsed;
        uint8_t known = 0U, always = 0U, free_current = 0U, free_max = 0U;
        if(!pocket_d20_parse_spell_record(
               line, &parsed, &known, &always, &free_current, &free_max))
            continue;
        if(logical >= start &&
           logical < (uint8_t)(start + POCKET_D20_COLLECTION_CACHE_SIZE)) {
            /* Grow only as records are actually retained. An empty or one-record
               sidecar must not pay the heap cost of an eight-record page. The
               normal reserve helper grows 1 -> 2 -> 4 -> 8 and never exceeds the
               eight-record logical window because this branch retains at most eight. */
            if(character->spell_count >= character->spell_capacity &&
               !pocket_d20_data_reserve_spells(
                   character, (uint8_t)(character->spell_count + 1U))) {
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
        if(logical < POCKET_D20_MAX_SPELLS) ++logical;
    }
    if(storage_file_get_error(file) != FSE_OK) success = false;
    *total_count = logical;
    free(line);
    storage_file_close(file);
    storage_file_free(file);
    if(!success) pocket_d20_data_clear_spells(character);
    return success;
}

static bool pocket_d20_rewrite_spellbook(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    uint8_t replace_start,
    const PocketCharacter* replacement,
    int16_t delete_index,
    const PocketSpell* append_spell,
    uint8_t append_known,
    uint8_t append_always,
    uint8_t append_free_current,
    uint8_t append_free_max) {
    if(!owner) return false;
    char path[POCKET_D20_PATH_LEN], snapshot[POCKET_D20_PATH_LEN];
    File* output = pocket_d20_open_collection_snapshot(
        storage,
        profile,
        owner,
        "spellbook",
        snapshot,
        sizeof(snapshot),
        path,
        sizeof(path));
    if(!output) return false;
    bool success = pocket_d20_write_raw(output, "DNDSpellbook=1\n");
    File* input = NULL;
    char* line = NULL;
    uint8_t logical = 0U;
    if(success && storage_file_exists(storage, path)) {
        input = storage_file_alloc(storage);
        if(!input || !storage_file_open(input, path, FSAM_READ, FSOM_OPEN_EXISTING)) success = false;
        if(success) {
            line = malloc(POCKET_D20_COLLECTION_LINE_LEN);
            if(!line) success = false;
        }
        if(success) {
            PocketD20Reader reader;
            pocket_d20_reader_init(&reader, input);
            while(success && pocket_d20_read_line(&reader, line, POCKET_D20_COLLECTION_LINE_LEN)) {
                if(!strncmp(line, "DNDSpellbook=", 13U)) continue;
                if(strncmp(line, "S|", 2U)) {
                    success = pocket_d20_write_raw(output, line) && pocket_d20_write_raw(output, "\n");
                    continue;
                }
                /* Logical indexes are based only on valid records, exactly like the
                   page loader. Preserve malformed/manual lines without allowing them
                   to shift which later valid spell is edited or deleted. */
                PocketSpell parsed;
                uint8_t parsed_known = 0U, parsed_always = 0U;
                uint8_t parsed_free_current = 0U, parsed_free_max = 0U;
                if(!pocket_d20_parse_spell_record(
                       line,
                       &parsed,
                       &parsed_known,
                       &parsed_always,
                       &parsed_free_current,
                       &parsed_free_max)) {
                    success = pocket_d20_write_raw(output, line) && pocket_d20_write_raw(output, "\n");
                    continue;
                }
                if(delete_index >= 0 && logical == (uint8_t)delete_index) {
                    ++logical;
                    continue;
                }
                if(replacement && logical >= replace_start &&
                   logical < (uint8_t)(replace_start + replacement->spell_count)) {
                    uint8_t local = (uint8_t)(logical - replace_start);
                    success = pocket_d20_write_spell_record(
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
                    success = pocket_d20_write_spell_record(
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
        for(uint16_t logical_index = append_from;
            success && logical_index < replacement_end;
            ++logical_index) {
            uint8_t local = (uint8_t)(logical_index - replace_start);
            success = pocket_d20_write_spell_record(
                output,
                &replacement->spells[local],
                replacement->spell_known[local],
                replacement->spell_always_prepared[local],
                replacement->spell_free_casts_current[local],
                replacement->spell_free_casts_max[local]);
        }
    }
    if(success && append_spell)
        success = pocket_d20_write_spell_record(
            output,
            append_spell,
            append_known,
            append_always,
            append_free_current,
            append_free_max);
    return pocket_d20_publish_collection_snapshot(storage, output, snapshot, path, success);
}

bool pocket_d20_storage_save_spellbook_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    const PocketCharacter* character) {
    if(!storage || !character) return false;
    if(character->spell_count &&
       (!character->spells || !character->spell_known || !character->spell_always_prepared ||
        !character->spell_free_casts_current || !character->spell_free_casts_max))
        return false;
    return pocket_d20_rewrite_spellbook(
        storage, profile, character, start, character, -1, NULL, 0U, 0U, 0U, 0U);
}

bool pocket_d20_storage_append_spell(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max) {
    if(!storage || !owner || !spell) return false;
    if(!pocket_d20_storage_ensure_spellbook_sidecar(storage, profile)) return false;

    char live[POCKET_D20_PATH_LEN], snapshot[POCKET_D20_PATH_LEN];
    File* output = pocket_d20_open_collection_snapshot(
        storage, profile, owner, "spellbook", snapshot, sizeof(snapshot), live, sizeof(live));
    if(!output) return false;

    uint64_t copied_size = 0U;
    bool needs_separator = false;
    bool success = pocket_d20_copy_live_collection_to_snapshot(
        storage, live, output, &copied_size, &needs_separator);
    if(success && !copied_size) success = pocket_d20_write_raw(output, "DNDSpellbook=1\n");
    if(success && needs_separator) success = pocket_d20_write_raw(output, "\n");
    if(success)
        success = pocket_d20_write_spell_record(
            output, spell, known, always_prepared, free_casts_current, free_casts_max);
    return pocket_d20_publish_collection_snapshot(storage, output, snapshot, live, success);
}

bool pocket_d20_storage_delete_spell(
    Storage* storage, uint32_t profile, const PocketCharacter* owner, uint8_t index) {
    if(!storage || !owner) return false;
    return pocket_d20_rewrite_spellbook(
        storage, profile, owner, 0U, NULL, index, NULL, 0U, 0U, 0U, 0U);
}

bool pocket_d20_storage_visit_items(
    Storage* storage,
    uint32_t profile,
    PocketD20ItemRecordVisitor visitor,
    void* context,
    uint8_t* total_count) {
    if(!storage) return false;
    if(total_count) *total_count = 0U;
    char path[POCKET_D20_PATH_LEN];
    pocket_d20_items_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    char* line = malloc(POCKET_D20_COLLECTION_LINE_LEN);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    PocketD20Reader reader;
    pocket_d20_reader_init(&reader, file);
    bool success = true;
    uint8_t logical = 0U;
    while(pocket_d20_read_line(&reader, line, POCKET_D20_COLLECTION_LINE_LEN)) {
        if(strncmp(line, "I|", 2U)) continue;
        PocketItem parsed;
        if(!pocket_d20_parse_item_record(line, &parsed)) continue;
        bool keep_scanning = true;
        if(visitor) keep_scanning = visitor(logical, &parsed, context);
        if(logical < POCKET_D20_MAX_ITEMS) ++logical;
        if(!keep_scanning) break;
    }
    if(storage_file_get_error(file) != FSE_OK) success = false;
    if(total_count) *total_count = logical;
    free(line);
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

typedef struct {
    PocketD20ItemAggregate* aggregate;
} PocketD20ItemAggregateContext;

static bool pocket_d20_aggregate_item_record(
    uint8_t logical_index, const PocketItem* item, void* context) {
    (void)logical_index;
    PocketD20ItemAggregateContext* aggregate_context = context;
    if(!aggregate_context || !aggregate_context->aggregate || !item) return true;
    PocketD20ItemAggregate* aggregate = aggregate_context->aggregate;
    if(item->quantity > 0) {
        int32_t weight = (int32_t)item->weight_tenths * item->quantity;
        int32_t carried = (int32_t)aggregate->carried_weight_tenths + weight;
        aggregate->carried_weight_tenths = carried > INT16_MAX ? INT16_MAX :
                                             carried < INT16_MIN ? INT16_MIN : (int16_t)carried;
        if(item->equipped) {
            int32_t equipped = (int32_t)aggregate->equipped_weight_tenths + weight;
            aggregate->equipped_weight_tenths = equipped > INT16_MAX ? INT16_MAX :
                                                   equipped < INT16_MIN ? INT16_MIN :
                                                                          (int16_t)equipped;
        }
    }
    if(item->attuned && aggregate->attuned_count < UINT8_MAX) ++aggregate->attuned_count;
    if(item->equipped) {
        if(item->shield_bonus) {
            uint16_t shield = (uint16_t)aggregate->shield_bonus + item->shield_bonus;
            aggregate->shield_bonus = shield > UINT8_MAX ? UINT8_MAX : (uint8_t)shield;
        }
        if(item->armor_base && !aggregate->armor_base) {
            aggregate->armor_base = item->armor_base;
            aggregate->armor_dex_cap = item->armor_dex_cap;
        }
    }
    return true;
}

bool pocket_d20_storage_item_aggregate(
    Storage* storage,
    uint32_t profile,
    PocketD20ItemAggregate* aggregate,
    uint8_t* total_count) {
    if(!storage || !aggregate) return false;
    memset(aggregate, 0, sizeof(*aggregate));
    aggregate->armor_dex_cap = -1;
    PocketD20ItemAggregateContext context = {.aggregate = aggregate};
    return pocket_d20_storage_visit_items(
        storage, profile, pocket_d20_aggregate_item_record, &context, total_count);
}

bool pocket_d20_storage_items_exist(Storage* storage, uint32_t profile) {
    if(!storage) return false;
    char path[POCKET_D20_PATH_LEN];
    pocket_d20_items_path(path, sizeof(path), profile);
    return storage_file_exists(storage, path);
}

bool pocket_d20_storage_remove_live_items(Storage* storage, uint32_t profile) {
    if(!storage) return false;
    char path[POCKET_D20_PATH_LEN];
    pocket_d20_items_path(path, sizeof(path), profile);
    return !storage_file_exists(storage, path) || storage_common_remove(storage, path) == FSE_OK;
}

static int32_t pocket_d20_add_currency_saturated(int32_t current, int32_t addition) {
    int64_t total = (int64_t)current + addition;
    if(total > INT32_MAX) return INT32_MAX;
    if(total < INT32_MIN) return INT32_MIN;
    return (int32_t)total;
}

static bool pocket_d20_compose_item_asset(
    Storage* storage,
    const char* path,
    const char* match,
    File* output,
    uint8_t* item_count,
    int32_t currency[5]) {
    if(!storage || !path || !match || !output || !item_count || !currency) return false;
    if(!match[0] || !storage_file_exists(storage, path)) return true;
    File* input = storage_file_alloc(storage);
    if(!input) return false;
    if(!storage_file_open(input, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(input);
        return false;
    }
    char* line = malloc(POCKET_D20_ITEM_SEED_LINE_LEN);
    if(!line) {
        storage_file_close(input);
        storage_file_free(input);
        return false;
    }
    PocketD20Reader reader;
    pocket_d20_reader_init(&reader, input);
    bool success = true;
    while(success && pocket_d20_read_line(&reader, line, POCKET_D20_ITEM_SEED_LINE_LEN)) {
        if(!line[0] || line[0] == '#') continue;
        char* separator = strchr(line, '|');
        if(!separator) continue;
        *separator = '\0';
        if(strcmp(line, match)) continue;
        char* payload = separator + 1U;
        if(!strncmp(payload, "C|", 2U)) {
            int32_t values[5];
            if(pocket_d20_parse_numbers(payload + 2U, values, 5U) != 5U) continue;
            for(uint8_t i = 0U; i < 5U; ++i)
                currency[i] = pocket_d20_add_currency_saturated(currency[i], values[i]);
        } else if(!strncmp(payload, "I|", 2U) && *item_count < POCKET_D20_MAX_ITEMS) {
            PocketItem item;
            if(!pocket_d20_parse_item_record(payload, &item)) continue;
            success = pocket_d20_write_item_record(output, &item);
            if(success) ++(*item_count);
        }
    }
    if(storage_file_get_error(input) != FSE_OK) success = false;
    free(line);
    storage_file_close(input);
    storage_file_free(input);
    return success;
}

bool pocket_d20_storage_create_items_from_assets(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    const PocketD20ItemSeedAsset* assets,
    uint8_t asset_count,
    int32_t currency[5],
    bool* created) {
    if(created) *created = false;
    if(currency) memset(currency, 0, 5U * sizeof(currency[0]));
    if(!storage || !owner || (!assets && asset_count) || !currency) return false;
    if(pocket_d20_storage_items_exist(storage, profile)) return true;

    char live[POCKET_D20_PATH_LEN];
    pocket_d20_items_path(live, sizeof(live), profile);
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);
    File* output = storage_file_alloc(storage);
    if(!output) return false;
    /* items_exist() above is the ownership guard. Use the same proven create
       mode as the rest of the app rather than CREATE_NEW, which can fail on
       first use before inventory_<id>.txt ever appears on the SD card. */
    if(!storage_file_open(output, live, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(output);
        return false;
    }

    bool success = pocket_d20_write_raw(output, "DNDItems=1\n");
    uint8_t item_count = 0U;
    for(uint8_t index = 0U; success && index < asset_count; ++index) {
        if(!assets[index].path || !assets[index].match) {
            success = false;
            break;
        }
        success = pocket_d20_compose_item_asset(
            storage,
            assets[index].path,
            assets[index].match,
            output,
            &item_count,
            currency);
    }
    if(success) success = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    if(!success) {
        storage_common_remove(storage, live);
        memset(currency, 0, 5U * sizeof(currency[0]));
        return false;
    }
    if(created) *created = true;
    return true;
}

bool pocket_d20_storage_load_items_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    PocketCharacter* character,
    uint8_t* total_count) {
    if(!storage || !character || !total_count) return false;
    pocket_d20_data_clear_items(character);
    *total_count = 0U;
    char path[POCKET_D20_PATH_LEN];
    pocket_d20_items_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    char* line = malloc(POCKET_D20_COLLECTION_LINE_LEN);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    PocketD20Reader reader;
    pocket_d20_reader_init(&reader, file);
    bool success = true;
    uint8_t logical = 0U;
    while(pocket_d20_read_line(&reader, line, POCKET_D20_COLLECTION_LINE_LEN)) {
        if(strncmp(line, "I|", 2U)) continue;
        PocketItem parsed;
        if(!pocket_d20_parse_item_record(line, &parsed)) continue;
        if(logical >= start &&
           logical < (uint8_t)(start + POCKET_D20_COLLECTION_CACHE_SIZE)) {
            /* Match spellbook paging: allocate for the records that are present,
               growing 1 -> 2 -> 4 -> 8 instead of reserving all eight up front. */
            if(character->item_count >= character->item_capacity &&
               !pocket_d20_data_reserve_items(
                   character, (uint8_t)(character->item_count + 1U))) {
                success = false;
                break;
            }
            character->items[character->item_count++] = parsed;
        }
        if(logical < POCKET_D20_MAX_ITEMS) ++logical;
    }
    if(storage_file_get_error(file) != FSE_OK) success = false;
    *total_count = logical;
    free(line);
    storage_file_close(file);
    storage_file_free(file);
    if(!success) pocket_d20_data_clear_items(character);
    return success;
}

static bool pocket_d20_rewrite_items(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    uint8_t replace_start,
    const PocketCharacter* replacement,
    int16_t delete_index,
    const PocketItem* append_item) {
    if(!owner) return false;
    char path[POCKET_D20_PATH_LEN], snapshot[POCKET_D20_PATH_LEN];
    File* output = pocket_d20_open_collection_snapshot(
        storage,
        profile,
        owner,
        "items",
        snapshot,
        sizeof(snapshot),
        path,
        sizeof(path));
    if(!output) return false;
    bool success = pocket_d20_write_raw(output, "DNDItems=1\n");
    File* input = NULL;
    char* line = NULL;
    uint8_t logical = 0U;
    if(success && storage_file_exists(storage, path)) {
        input = storage_file_alloc(storage);
        if(!input || !storage_file_open(input, path, FSAM_READ, FSOM_OPEN_EXISTING)) success = false;
        if(success) {
            line = malloc(POCKET_D20_COLLECTION_LINE_LEN);
            if(!line) success = false;
        }
        if(success) {
            PocketD20Reader reader;
            pocket_d20_reader_init(&reader, input);
            while(success && pocket_d20_read_line(&reader, line, POCKET_D20_COLLECTION_LINE_LEN)) {
                if(!strncmp(line, "DNDItems=", 9U)) continue;
                if(strncmp(line, "I|", 2U)) {
                    success = pocket_d20_write_raw(output, line) && pocket_d20_write_raw(output, "\n");
                    continue;
                }
                /* Keep malformed/manual item lines, but do not count them as logical
                   records when locating a page replacement or delete target. */
                PocketItem parsed;
                if(!pocket_d20_parse_item_record(line, &parsed)) {
                    success = pocket_d20_write_raw(output, line) && pocket_d20_write_raw(output, "\n");
                    continue;
                }
                if(delete_index >= 0 && logical == (uint8_t)delete_index) {
                    ++logical;
                    continue;
                }
                if(replacement && logical >= replace_start &&
                   logical < (uint8_t)(replace_start + replacement->item_count)) {
                    uint8_t local = (uint8_t)(logical - replace_start);
                    success = pocket_d20_write_item_record(output, &replacement->items[local]);
                } else {
                    success = pocket_d20_write_item_record(output, &parsed);
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
        for(uint16_t logical_index = append_from;
            success && logical_index < replacement_end;
            ++logical_index) {
            uint8_t local = (uint8_t)(logical_index - replace_start);
            success = pocket_d20_write_item_record(output, &replacement->items[local]);
        }
    }
    if(success && append_item) success = pocket_d20_write_item_record(output, append_item);
    return pocket_d20_publish_collection_snapshot(storage, output, snapshot, path, success);
}

bool pocket_d20_storage_save_items_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    const PocketCharacter* character) {
    if(!storage || !character || (character->item_count && !character->items)) return false;
    return pocket_d20_rewrite_items(storage, profile, character, start, character, -1, NULL);
}

bool pocket_d20_storage_append_item(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    const PocketItem* item) {
    if(!storage || !owner || !item) return false;
    if(!pocket_d20_storage_ensure_items_sidecar(storage, profile)) return false;

    char live[POCKET_D20_PATH_LEN], snapshot[POCKET_D20_PATH_LEN];
    File* output = pocket_d20_open_collection_snapshot(
        storage, profile, owner, "items", snapshot, sizeof(snapshot), live, sizeof(live));
    if(!output) return false;

    uint64_t copied_size = 0U;
    bool needs_separator = false;
    bool success = pocket_d20_copy_live_collection_to_snapshot(
        storage, live, output, &copied_size, &needs_separator);
    if(success && !copied_size) success = pocket_d20_write_raw(output, "DNDItems=1\n");
    if(success && needs_separator) success = pocket_d20_write_raw(output, "\n");
    if(success) success = pocket_d20_write_item_record(output, item);
    return pocket_d20_publish_collection_snapshot(storage, output, snapshot, live, success);
}

bool pocket_d20_storage_delete_item(
    Storage* storage, uint32_t profile, const PocketCharacter* owner, uint8_t index) {
    if(!storage || !owner) return false;
    return pocket_d20_rewrite_items(storage, profile, owner, 0U, NULL, index, NULL);
}

static bool pocket_d20_parse_profile_filename(const char* filename, PocketProfileEntry* entry);

static void pocket_d20_prepare_character_load(PocketSaveData* data) {
    pocket_d20_data_clear(data);
    pocket_d20_data_set_defaults(data);
    PocketCharacter* c = &data->character;
    /* Owned spells and items live in their own per-character files and are not
       loaded as part of the core character. Other dynamic collections still come
       from the character file. */
    pocket_d20_data_clear_spells(c);
    pocket_d20_data_reserve_features_exact(c, 0U);
    pocket_d20_data_clear_items(c);
    pocket_d20_data_reserve_grants_exact(c, 0U);
    c->feature_count = 0U;
    c->grant_count = 0U;
}



static bool pocket_d20_write_character(File* file, const PocketSaveData* data) {
    const PocketCharacter* c = &data->character;
    const uint8_t feature_count = c->features ? c->feature_count : 0U;
    const uint8_t grant_count = c->grants ? c->grant_count : 0U;
    char key[48];
    if(!pocket_d20_writef(file, "PocketD20Character=%u\n", POCKET_D20_TEXT_VERSION) ||
       !pocket_d20_write_string(file, "Name", c->name) ||
       !pocket_d20_write_string(file, "Player", c->player) ||
       !pocket_d20_write_string(file, "Species", c->species) ||
       !pocket_d20_write_string(file, "Background", c->background) ||
       !pocket_d20_write_string(file, "Alignment", c->alignment) ||
       !pocket_d20_write_string(file, "OtherProficiencies", c->other_proficiencies) ||
       !pocket_d20_write_string(file, "OriginFeat", c->origin_feat) ||
       !pocket_d20_write_string(file, "ToolProficiencies", c->tool_proficiencies) ||
       !pocket_d20_write_string(file, "ArmorTraining", c->armor_training) ||
       !pocket_d20_write_string(file, "WeaponTraining", c->weapon_training) ||
       !pocket_d20_write_string(file, "Senses", c->senses) ||
       !pocket_d20_writef(file, "IdentityRules=%u\n", c->size) ||
       !pocket_d20_writef(
           file,
           "Progress=%u,%lu,%u,%u\n",
           c->class_count,
           (unsigned long)c->experience,
           c->milestone_leveling,
           c->inspiration))
        return false;

    for(uint8_t i = 0U; i < c->class_count; ++i) {
        snprintf(key, sizeof(key), "Class%uName", i);
        if(!pocket_d20_write_string(file, key, c->classes[i].name)) return false;
        snprintf(key, sizeof(key), "Class%uSubclass", i);
        if(!pocket_d20_write_string(file, key, c->classes[i].subclass) ||
           !pocket_d20_writef(
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

    if(!pocket_d20_write_i8_array(
           file, "AbilityScores", c->ability_scores, POCKET_D20_ABILITY_COUNT) ||
       !pocket_d20_write_u8_array(
           file, "SaveProficiency", c->saving_throw_proficiency, POCKET_D20_ABILITY_COUNT) ||
       !pocket_d20_write_i8_array(
           file, "SaveMisc", c->saving_throw_misc, POCKET_D20_ABILITY_COUNT) ||
       !pocket_d20_write_u8_array(
           file, "SkillProficiency", c->skill_proficiency, POCKET_D20_SKILL_COUNT) ||
       !pocket_d20_write_i8_array(file, "SkillMisc", c->skill_misc, POCKET_D20_SKILL_COUNT) ||
       !pocket_d20_writef(
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
       !pocket_d20_writef(
           file,
           "Spellcasting=%u,%d,%d,%u\n",
           c->spellcasting_ability,
           c->spell_attack_misc,
           c->spell_save_misc,
           c->arcane_recovery_used) ||
       !pocket_d20_write_u8_array(
           file, "SpellSlotsCurrent", c->spell_slots_current, POCKET_D20_SLOT_COUNT) ||
       !pocket_d20_write_u8_array(
           file, "SpellSlotsMax", c->spell_slots_max, POCKET_D20_SLOT_COUNT) ||
       !pocket_d20_writef(
           file,
           "Currency=%ld,%ld,%ld,%ld,%ld\n",
           (long)c->currency_cp,
           (long)c->currency_sp,
           (long)c->currency_ep,
           (long)c->currency_gp,
           (long)c->currency_pp))
        return false;

    if(!pocket_d20_writef(file, "FeatureCount=%u\n", feature_count)) return false;
    for(uint8_t i = 0U; i < feature_count; ++i) {
        snprintf(key, sizeof(key), "Feature%uName", i);
        if(!pocket_d20_write_string(file, key, c->features[i].name)) return false;
        snprintf(key, sizeof(key), "Feature%uDetail", i);
        if(!pocket_d20_write_string(file, key, c->features[i].detail) ||
           !pocket_d20_writef(
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

    if(!pocket_d20_writef(file, "LanguageCount=%u\n", c->language_count)) return false;
    for(uint8_t i = 0U; i < c->language_count; ++i) {
        snprintf(key, sizeof(key), "Language%u", i);
        if(!pocket_d20_write_string(file, key, c->languages[i])) return false;
    }

    if(!pocket_d20_write_string(file, "Conditions", c->conditions) ||
       !pocket_d20_write_string(file, "Concentration", c->concentration) ||
       !pocket_d20_write_string(file, "TemporaryEffects", c->temporary_effects) ||
       !pocket_d20_write_string(file, "Resistances", c->resistances) ||
       !pocket_d20_write_string(file, "Immunities", c->immunities) ||
       !pocket_d20_write_string(file, "Vulnerabilities", c->vulnerabilities) ||
       !pocket_d20_write_string(file, "MovementModes", c->movement_modes) ||
       !pocket_d20_writef(
           file,
           "CombatFlags=%u,%u,%d\n",
           c->reaction_available,
           c->encumbrance_mode,
           c->carrying_capacity_override) ||
       !pocket_d20_writef(file, "GrantCount=%u\n", grant_count))
        return false;

    for(uint8_t i = 0U; i < grant_count; ++i) {
        const PocketGrant* grant = &c->grants[i];
        snprintf(key, sizeof(key), "Grant%uStableId", i);
        if(!pocket_d20_write_string(file, key, grant->stable_id)) return false;
        snprintf(key, sizeof(key), "Grant%uSource", i);
        if(!pocket_d20_write_string(file, key, grant->source)) return false;
        snprintf(key, sizeof(key), "Grant%uOption", i);
        if(!pocket_d20_write_string(file, key, grant->option_name)) return false;
        snprintf(key, sizeof(key), "Grant%uPrerequisites", i);
        if(!pocket_d20_write_string(file, key, grant->prerequisites)) return false;
        snprintf(key, sizeof(key), "Grant%uValue", i);
        if(!pocket_d20_write_string(file, key, grant->grant_value) ||
           !pocket_d20_writef(
               file,
               "Grant%uData=%u,%u,%u,%u\n",
               i,
               grant->source_type,
               grant->class_index,
               grant->level_gained,
               grant->status))
            return false;
    }

    if(!pocket_d20_writef(file, "AttackTemplateCount=%u\n", c->attack_template_count))
        return false;
    for(uint8_t i = 0U; i < c->attack_template_count; ++i) {
        const PocketAttackTemplate* attack = &c->attack_templates[i];
        snprintf(key, sizeof(key), "AttackTemplate%uName", i);
        if(!pocket_d20_write_string(file, key, attack->name)) return false;
        snprintf(key, sizeof(key), "AttackTemplate%uMastery", i);
        if(!pocket_d20_write_string(file, key, attack->mastery)) return false;
        snprintf(key, sizeof(key), "AttackTemplate%uDamageType", i);
        if(!pocket_d20_write_string(file, key, attack->damage_type)) return false;
        snprintf(key, sizeof(key), "AttackTemplate%uRiderType", i);
        if(!pocket_d20_write_string(file, key, attack->rider_type) ||
           !pocket_d20_writef(
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

    return pocket_d20_writef(file, "End=OK\n");
}

static bool pocket_d20_read_character(
    File* file,
    PocketSaveData* data,
    const PocketProfileEntry* fallback_entry) {
    if(!file || !data) return false;
    pocket_d20_prepare_character_load(data);
    PocketCharacter* c = &data->character;
    /* Filename name metadata is a safe fallback for partial/older character files.
       A missing/malformed Name must never turn an existing character into the
       synthetic New Hero default on the next save. The filename level is total
       character level, so it is applied only later when no class-level records
       were recovered and the character remains single-class. */
    if(fallback_entry)
        pocket_d20_copy(c->name, sizeof(c->name), fallback_entry->name);
    PocketD20Reader reader;
    pocket_d20_reader_init(&reader, file);
    char line[POCKET_D20_ENCODED_LINE_LEN];
    int32_t n[32];
    /* A valid primary filename is itself usable recovery metadata. Even if the
       body contains only unknown/future fields, keep the filename name/level
       fallback instead of treating the readable character as a failed load. */
    bool recognized_data = fallback_entry != NULL;
    bool class_level_seen = false;

    while(pocket_d20_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        const char* key = line;
        uint8_t index = 0U;
        uint32_t number = 0U;

        /* Version is intentionally informational. A mismatched or malformed version
           never rejects fields that this build still recognizes. */
        if(!strcmp(key, "PocketD20Character") || !strcmp(key, "End")) continue;

#define LOAD_STRING(field, name) \
        if(!strcmp(key, name)) { pocket_d20_decode_string((field), sizeof(field), value); recognized_data = true; continue; }
        LOAD_STRING(c->name, "Name")
        LOAD_STRING(c->player, "Player")
        LOAD_STRING(c->species, "Species")
        LOAD_STRING(c->background, "Background")
        LOAD_STRING(c->alignment, "Alignment")
        LOAD_STRING(c->other_proficiencies, "OtherProficiencies")
        LOAD_STRING(c->origin_feat, "OriginFeat")
        LOAD_STRING(c->tool_proficiencies, "ToolProficiencies")
        LOAD_STRING(c->armor_training, "ArmorTraining")
        LOAD_STRING(c->weapon_training, "WeaponTraining")
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
            if(pocket_d20_parse_numbers(value, n, 1U) == 1U) { c->size = (uint8_t)n[0]; recognized_data = true; }
            continue;
        }
        if(!strcmp(key, "Progress")) {
            size_t count = pocket_d20_parse_numbers(value, n, 4U);
            if(count >= 1U && n[0] > 0 && n[0] <= (int32_t)POCKET_D20_MAX_CLASSES) c->class_count = (uint8_t)n[0];
            if(count >= 2U && n[1] >= 0) c->experience = (uint32_t)n[1];
            if(count >= 3U) c->milestone_leveling = n[2] ? 1U : 0U;
            if(count >= 4U) c->inspiration = n[3] ? 1U : 0U;
            if(count) recognized_data = true;
            continue;
        }
        if(pocket_d20_indexed_key(key, "Class", "Name", POCKET_D20_MAX_CLASSES, &index)) {
            pocket_d20_decode_string(c->classes[index].name, sizeof(c->classes[index].name), value);
            if(c->class_count <= index) c->class_count = (uint8_t)(index + 1U);
            recognized_data = true; continue;
        }
        if(pocket_d20_indexed_key(key, "Class", "Subclass", POCKET_D20_MAX_CLASSES, &index)) {
            pocket_d20_decode_string(c->classes[index].subclass, sizeof(c->classes[index].subclass), value);
            if(c->class_count <= index) c->class_count = (uint8_t)(index + 1U);
            recognized_data = true; continue;
        }
        if(pocket_d20_indexed_key(key, "Class", "Data", POCKET_D20_MAX_CLASSES, &index)) {
            size_t count = pocket_d20_parse_numbers(value, n, 16U);
            PocketClassLevel* cl = &c->classes[index];
            if(count >= 1U) { cl->level = (uint8_t)n[0]; class_level_seen = true; }
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
            if(count) { if(c->class_count <= index) c->class_count = (uint8_t)(index + 1U); recognized_data = true; }
            continue;
        }

#define LOAD_ARRAY(key_name, target, expected, cast_type) \
        if(!strcmp(key, key_name)) { \
            size_t count = pocket_d20_parse_numbers(value, n, expected); \
            if(count) { for(size_t i = 0U; i < count && i < expected; ++i) (target)[i] = (cast_type)n[i]; recognized_data = true; } \
            continue; \
        }
        LOAD_ARRAY("AbilityScores", c->ability_scores, POCKET_D20_ABILITY_COUNT, int8_t)
        LOAD_ARRAY("SaveProficiency", c->saving_throw_proficiency, POCKET_D20_ABILITY_COUNT, uint8_t)
        LOAD_ARRAY("SaveMisc", c->saving_throw_misc, POCKET_D20_ABILITY_COUNT, int8_t)
        LOAD_ARRAY("SkillProficiency", c->skill_proficiency, POCKET_D20_SKILL_COUNT, uint8_t)
        LOAD_ARRAY("SkillMisc", c->skill_misc, POCKET_D20_SKILL_COUNT, int8_t)
        LOAD_ARRAY("SpellSlotsCurrent", c->spell_slots_current, POCKET_D20_SLOT_COUNT, uint8_t)
        LOAD_ARRAY("SpellSlotsMax", c->spell_slots_max, POCKET_D20_SLOT_COUNT, uint8_t)
#undef LOAD_ARRAY

        if(!strcmp(key, "Vitals")) {
            size_t count = pocket_d20_parse_numbers(value, n, 12U);
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
            size_t count = pocket_d20_parse_numbers(value, n, 4U);
            if(count >= 1U) c->spellcasting_ability = (uint8_t)n[0];
            if(count >= 2U) c->spell_attack_misc = (int8_t)n[1];
            if(count >= 3U) c->spell_save_misc = (int8_t)n[2];
            if(count >= 4U) c->arcane_recovery_used = (uint8_t)n[3];
            if(count) recognized_data = true;
            continue;
        }
        if(!strcmp(key, "Currency")) {
            size_t count = pocket_d20_parse_numbers(value, n, 5U);
            if(count >= 1U) c->currency_cp = n[0];
            if(count >= 2U) c->currency_sp = n[1];
            if(count >= 3U) c->currency_ep = n[2];
            if(count >= 4U) c->currency_gp = n[3];
            if(count >= 5U) c->currency_pp = n[4];
            if(count) recognized_data = true;
            continue;
        }

#define LOAD_DYNAMIC_COUNT(name, maximum, reserve_fn, count_field) \
        if(!strcmp(key, name)) { \
            if(pocket_d20_parse_u32_range(value, maximum, &number)) { \
                if(number == 0U || reserve_fn(c, (uint8_t)number)) { count_field = (uint8_t)number; recognized_data = true; } \
            } \
            continue; \
        }
        LOAD_DYNAMIC_COUNT("FeatureCount", POCKET_D20_MAX_FEATURES, pocket_d20_data_reserve_features, c->feature_count)
        LOAD_DYNAMIC_COUNT("GrantCount", POCKET_D20_MAX_GRANTS, pocket_d20_data_reserve_grants, c->grant_count)
#undef LOAD_DYNAMIC_COUNT

        if(pocket_d20_indexed_key(key, "Feature", "Name", POCKET_D20_MAX_FEATURES, &index) ||
           pocket_d20_indexed_key(key, "Feature", "Detail", POCKET_D20_MAX_FEATURES, &index) ||
           pocket_d20_indexed_key(key, "Feature", "Data", POCKET_D20_MAX_FEATURES, &index)) {
            if(!pocket_d20_data_reserve_features(c, (uint8_t)(index + 1U))) continue;
            if(c->feature_count <= index) c->feature_count = (uint8_t)(index + 1U);
            PocketFeature* feature = &c->features[index];
            if(pocket_d20_indexed_key(key, "Feature", "Name", POCKET_D20_MAX_FEATURES, &index))
                pocket_d20_decode_string(feature->name, sizeof(feature->name), value);
            else if(pocket_d20_indexed_key(key, "Feature", "Detail", POCKET_D20_MAX_FEATURES, &index))
                pocket_d20_decode_string(feature->detail, sizeof(feature->detail), value);
            else {
                size_t count = pocket_d20_parse_numbers(value, n, 7U);
                if(count >= 1U) feature->uses_current = (int16_t)n[0];
                if(count >= 2U) feature->uses_max = (int16_t)n[1];
                if(count >= 3U) feature->class_index = (uint8_t)n[2];
                if(count >= 4U) feature->class_level_gained = (uint8_t)n[3];
                if(count >= 5U) feature->recharge = (uint8_t)n[4];
                if(count >= 6U) feature->resource_formula = (uint8_t)n[5];
                if(count >= 7U) feature->resource_ability = (uint8_t)n[6];
            }
            recognized_data = true; continue;
        }

        if(!strcmp(key, "LanguageCount")) {
            if(pocket_d20_parse_u32_range(value, POCKET_D20_MAX_LANGUAGES, &number)) { c->language_count = (uint8_t)number; recognized_data = true; }
            continue;
        }
        if(pocket_d20_indexed_key(key, "Language", "", POCKET_D20_MAX_LANGUAGES, &index)) {
            pocket_d20_decode_string(c->languages[index], sizeof(c->languages[index]), value);
            if(c->language_count <= index) c->language_count = (uint8_t)(index + 1U);
            recognized_data = true; continue;
        }
        if(!strcmp(key, "CombatFlags")) {
            size_t count = pocket_d20_parse_numbers(value, n, 3U);
            if(count >= 1U) c->reaction_available = (uint8_t)n[0];
            if(count >= 2U) c->encumbrance_mode = (uint8_t)n[1];
            if(count >= 3U) c->carrying_capacity_override = (int16_t)n[2];
            if(count) recognized_data = true;
            continue;
        }

        if(pocket_d20_indexed_key(key, "Grant", "StableId", POCKET_D20_MAX_GRANTS, &index) ||
           pocket_d20_indexed_key(key, "Grant", "Source", POCKET_D20_MAX_GRANTS, &index) ||
           pocket_d20_indexed_key(key, "Grant", "Option", POCKET_D20_MAX_GRANTS, &index) ||
           pocket_d20_indexed_key(key, "Grant", "Prerequisites", POCKET_D20_MAX_GRANTS, &index) ||
           pocket_d20_indexed_key(key, "Grant", "Value", POCKET_D20_MAX_GRANTS, &index) ||
           pocket_d20_indexed_key(key, "Grant", "Data", POCKET_D20_MAX_GRANTS, &index)) {
            if(!pocket_d20_data_reserve_grants(c, (uint8_t)(index + 1U))) continue;
            if(c->grant_count <= index) c->grant_count = (uint8_t)(index + 1U);
            PocketGrant* grant = &c->grants[index];
            if(pocket_d20_indexed_key(key, "Grant", "StableId", POCKET_D20_MAX_GRANTS, &index))
                pocket_d20_decode_string(grant->stable_id, sizeof(grant->stable_id), value);
            else if(pocket_d20_indexed_key(key, "Grant", "Source", POCKET_D20_MAX_GRANTS, &index))
                pocket_d20_decode_string(grant->source, sizeof(grant->source), value);
            else if(pocket_d20_indexed_key(key, "Grant", "Option", POCKET_D20_MAX_GRANTS, &index))
                pocket_d20_decode_string(grant->option_name, sizeof(grant->option_name), value);
            else if(pocket_d20_indexed_key(key, "Grant", "Prerequisites", POCKET_D20_MAX_GRANTS, &index))
                pocket_d20_decode_string(grant->prerequisites, sizeof(grant->prerequisites), value);
            else if(pocket_d20_indexed_key(key, "Grant", "Value", POCKET_D20_MAX_GRANTS, &index))
                pocket_d20_decode_string(grant->grant_value, sizeof(grant->grant_value), value);
            else {
                size_t count = pocket_d20_parse_numbers(value, n, 4U);
                if(count >= 1U) grant->source_type = (uint8_t)n[0];
                if(count >= 2U) grant->class_index = (uint8_t)n[1];
                if(count >= 3U) grant->level_gained = (uint8_t)n[2];
                if(count >= 4U) grant->status = (uint8_t)n[3];
            }
            recognized_data = true; continue;
        }

        if(!strcmp(key, "AttackTemplateCount")) {
            if(pocket_d20_parse_u32_range(value, POCKET_D20_MAX_ATTACK_TEMPLATES, &number)) { c->attack_template_count = (uint8_t)number; recognized_data = true; }
            continue;
        }
        if(pocket_d20_indexed_key(key, "AttackTemplate", "Name", POCKET_D20_MAX_ATTACK_TEMPLATES, &index) ||
           pocket_d20_indexed_key(key, "AttackTemplate", "Mastery", POCKET_D20_MAX_ATTACK_TEMPLATES, &index) ||
           pocket_d20_indexed_key(key, "AttackTemplate", "DamageType", POCKET_D20_MAX_ATTACK_TEMPLATES, &index) ||
           pocket_d20_indexed_key(key, "AttackTemplate", "RiderType", POCKET_D20_MAX_ATTACK_TEMPLATES, &index) ||
           pocket_d20_indexed_key(key, "AttackTemplate", "Data", POCKET_D20_MAX_ATTACK_TEMPLATES, &index)) {
            if(c->attack_template_count <= index) c->attack_template_count = (uint8_t)(index + 1U);
            PocketAttackTemplate* attack = &c->attack_templates[index];
            if(pocket_d20_indexed_key(key, "AttackTemplate", "Name", POCKET_D20_MAX_ATTACK_TEMPLATES, &index))
                pocket_d20_decode_string(attack->name, sizeof(attack->name), value);
            else if(pocket_d20_indexed_key(key, "AttackTemplate", "Mastery", POCKET_D20_MAX_ATTACK_TEMPLATES, &index))
                pocket_d20_decode_string(attack->mastery, sizeof(attack->mastery), value);
            else if(pocket_d20_indexed_key(key, "AttackTemplate", "DamageType", POCKET_D20_MAX_ATTACK_TEMPLATES, &index))
                pocket_d20_decode_string(attack->damage_type, sizeof(attack->damage_type), value);
            else if(pocket_d20_indexed_key(key, "AttackTemplate", "RiderType", POCKET_D20_MAX_ATTACK_TEMPLATES, &index))
                pocket_d20_decode_string(attack->rider_type, sizeof(attack->rider_type), value);
            else {
                size_t count = pocket_d20_parse_numbers(value, n, 9U);
                if(count >= 1U) { attack->type = (uint8_t)n[0] & 0x07U; attack->recharge = (uint8_t)n[0] >> 3U; }
                if(count >= 2U) attack->ability = (uint8_t)n[1];
                if(count >= 3U) attack->save_ability = (uint8_t)n[2];
                if(count >= 4U) attack->attack_misc = (int8_t)n[3];
                if(count >= 5U) attack->save_dc = (uint8_t)n[4];
                if(count >= 6U) attack->damage_dice = (uint8_t)n[5];
                if(count >= 7U) attack->damage_die = (uint8_t)n[6];
                if(count >= 8U) attack->rider_dice = (uint8_t)n[7];
                if(count >= 9U) attack->rider_die = (uint8_t)n[8];
            }
            recognized_data = true; continue;
        }
        /* Party, initiative, encounter-history and unknown fields are intentionally
           ignored. Their state belongs to companion apps and cannot invalidate a character. */
    }

    if(!class_level_seen && fallback_entry && c->class_count == 1U &&
       fallback_entry->level >= 1U && fallback_entry->level <= 20U)
        c->classes[0].level = fallback_entry->level;

    bool io_ok = storage_file_get_error(file) == FSE_OK;
    pocket_d20_data_sanitize(data);
    return io_ok && recognized_data;
}

static bool pocket_d20_load_text_path(
    Storage* storage,
    const char* path,
    PocketSaveData* data) {
    PocketProfileEntry fallback;
    const PocketProfileEntry* fallback_entry = NULL;
    const char* filename = strrchr(path, '/');
    filename = filename ? filename + 1U : path;
    if(pocket_d20_parse_profile_filename(filename, &fallback)) fallback_entry = &fallback;

    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool success = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                   pocket_d20_read_character(file, data, fallback_entry);
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

static void pocket_d20_work_path(char* output, size_t size, uint32_t profile, const char* kind) {
    snprintf(
        output, size, "%s/custom_%s_%lu.tmp", POCKET_D20_DATA_DIR, kind, (unsigned long)profile);
}

static bool pocket_d20_parse_profile_filename(const char* filename, PocketProfileEntry* entry) {
    if(!filename || !entry) return false;
    size_t length = strlen(filename);
    if(length < 11U || strncmp(filename, "ch_", 3U) != 0 ||
       strcmp(filename + length - 4U, ".txt") != 0)
        return false;

    const char* id_begin = filename + 3U;
    const char* id_end = strchr(id_begin, '_');
    if(!id_end) return false;
    uint32_t id = 0U;
    if(!pocket_d20_parse_u32_span(id_begin, id_end, UINT32_MAX, &id)) return false;

    const char* level_separator = filename + length - 5U;
    while(level_separator > id_end && *level_separator != '_')
        --level_separator;
    if(*level_separator != '_' || level_separator <= id_end + 1U) return false;
    uint32_t level = 0U;
    if(!pocket_d20_parse_u32_span(
           level_separator + 1U, filename + length - 4U, UINT8_MAX, &level))
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

static bool
    pocket_d20_find_profile_path(Storage* storage, uint32_t profile, char* output, size_t size) {
    File* directory = storage_file_alloc(storage);
    if(!directory) return false;
    if(!storage_dir_open(directory, POCKET_D20_DATA_DIR)) {
        storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    bool found = false;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        PocketProfileEntry entry;
        if(!file_info_is_dir(&info) && pocket_d20_parse_profile_filename(filename, &entry) &&
           entry.id == profile) {
            size_t prefix_length = strlen(POCKET_D20_DATA_DIR);
            size_t filename_length = strlen(filename);
            if(prefix_length + filename_length + 2U > size) continue;
            memcpy(output, POCKET_D20_DATA_DIR, prefix_length);
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

static bool pocket_d20_shadow_path(
    char* output, size_t size, const char* character_path) {
    if(!output || !size || !character_path) return false;
    size_t length = strlen(character_path);
    if(length < 5U || strcmp(character_path + length - 4U, ".txt") != 0 || length + 1U > size)
        return false;
    memcpy(output, character_path, length - 4U);
    memcpy(output + length - 4U, ".shd", 5U);
    return true;
}

static bool pocket_d20_refresh_shadow(
    Storage* storage, const char* character_path) {
    char shadow_path[POCKET_D20_PATH_LEN];
    if(!storage || !pocket_d20_shadow_path(shadow_path, sizeof(shadow_path), character_path))
        return false;

    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    if(!input || !output) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        return false;
    }

    bool success = storage_file_open(input, character_path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                   storage_file_open(output, shadow_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
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
    return success;
}

static bool pocket_d20_storage_save_profile_internal(
    Storage* storage,
    uint32_t profile,
    const PocketSaveData* data,
    const char* known_old_path,
    bool update_shadow) {
    furi_assert(storage);
    furi_assert(data);
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);
    char old_path[POCKET_D20_PATH_LEN] = {0};
    char new_path[POCKET_D20_PATH_LEN];
    char temp_path[POCKET_D20_PATH_LEN];
    char backup_path[POCKET_D20_PATH_LEN];
    bool had_save = false;
    if(known_old_path && storage_file_exists(storage, known_old_path)) {
        pocket_d20_copy(old_path, sizeof(old_path), known_old_path);
        had_save = true;
    } else {
        had_save = pocket_d20_find_profile_path(storage, profile, old_path, sizeof(old_path));
    }
    pocket_d20_profile_path(new_path, sizeof(new_path), profile, &data->character);
    /* Keep one immutable-by-level shadow name beside each character save. While the
       character remains at the same level, this file is refreshed in place to the
       newest successful state. When the filename changes on level-up, the prior
       level's .shd is never removed and remains the last pre-level-up state. */
    /* Shadow maintenance is best-effort. A .shd write failure must never block
       the primary character save. The existing primary remains the authoritative
       source until the new .txt is published successfully. */
    if(update_shadow && had_save) pocket_d20_refresh_shadow(storage, old_path);
    pocket_d20_work_path(temp_path, sizeof(temp_path), profile, "write");
    pocket_d20_work_path(backup_path, sizeof(backup_path), profile, "backup");
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool written = storage_file_open(file, temp_path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                   pocket_d20_write_character(file, data) && storage_file_sync(file);
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
    /* Refresh the current-level shadow after the primary is safely published.
       Shadow failure is intentionally non-fatal: the .txt save already succeeded. */
    if(update_shadow) pocket_d20_refresh_shadow(storage, new_path);
    return true;
}

bool pocket_d20_storage_save_profile(
    Storage* storage,
    uint32_t profile,
    const PocketSaveData* data) {
    return pocket_d20_storage_save_profile_internal(storage, profile, data, NULL, false);
}

bool pocket_d20_storage_save_profile_updated(
    Storage* storage,
    uint32_t profile,
    const PocketSaveData* data) {
    return pocket_d20_storage_save_profile_internal(storage, profile, data, NULL, true);
}

bool pocket_d20_storage_save_profile_known_updated(
    Storage* storage,
    const PocketProfileEntry* current_entry,
    const PocketSaveData* data) {
    furi_assert(current_entry);
    char safe_name[POCKET_D20_CHARACTER_NAME_LEN];
    char current_path[POCKET_D20_PATH_LEN];
    pocket_d20_filename_name(safe_name, sizeof(safe_name), current_entry->name);
    snprintf(
        current_path,
        sizeof(current_path),
        "%s/ch_%lu_%s_%u.txt",
        POCKET_D20_DATA_DIR,
        (unsigned long)current_entry->id,
        safe_name,
        current_entry->level);
    return pocket_d20_storage_save_profile_internal(
        storage, current_entry->id, data, current_path, true);
}

bool pocket_d20_storage_load_profile(
    Storage* storage,
    uint32_t profile,
    PocketSaveData* data,
    bool* recovered_backup) {
    furi_assert(storage);
    furi_assert(data);
    if(recovered_backup) *recovered_backup = false;
    char path[POCKET_D20_PATH_LEN];
    char backup_path[POCKET_D20_PATH_LEN];
    bool primary_found = pocket_d20_find_profile_path(storage, profile, path, sizeof(path));
    if(primary_found && pocket_d20_load_text_path(storage, path, data)) return true;

    /* A readable retained backup is a fallback for an unreadable primary. The
       character parser itself remains field-name best-effort; backup recovery is
       only reached when the primary cannot be read into a usable character. */
    pocket_d20_work_path(backup_path, sizeof(backup_path), profile, "backup");
    if(pocket_d20_load_text_path(storage, backup_path, data)) {
        if(recovered_backup) *recovered_backup = true;
        return true;
    }
    pocket_d20_data_clear(data);
    pocket_d20_data_set_defaults(data);
    return false;
}

static bool pocket_d20_remove_if_present(Storage* storage, const char* path) {
    return !storage_file_exists(storage, path) || storage_common_remove(storage, path) == FSE_OK;
}

bool pocket_d20_storage_delete_profile(Storage* storage, uint32_t profile) {
    char path[POCKET_D20_PATH_LEN];
    char temp_path[POCKET_D20_PATH_LEN];
    char backup_path[POCKET_D20_PATH_LEN];
    bool found = pocket_d20_find_profile_path(storage, profile, path, sizeof(path));
    pocket_d20_work_path(temp_path, sizeof(temp_path), profile, "write");
    pocket_d20_work_path(backup_path, sizeof(backup_path), profile, "backup");
    bool success = (!found || pocket_d20_remove_if_present(storage, path)) &&
                   pocket_d20_remove_if_present(storage, temp_path) &&
                   pocket_d20_remove_if_present(storage, backup_path);
    pocket_d20_spellbook_path(path, sizeof(path), profile);
    success = pocket_d20_remove_if_present(storage, path) && success;
    pocket_d20_items_path(path, sizeof(path), profile);
    success = pocket_d20_remove_if_present(storage, path) && success;
    /* Level-tagged .swd files are historical records. They are intentionally
       never deleted when the live profile or sidecars are removed. */
    return success;
}

static bool pocket_d20_copy_file(
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
    char backup[POCKET_D20_LONG_PATH_LEN];
    int backup_length = snprintf(backup, sizeof(backup), "%s.publish.bak", destination);
    if(backup_length < 0 || (size_t)backup_length >= sizeof(backup)) {
        storage_common_remove(storage, temporary);
        return false;
    }
    return pocket_d20_publish_temp(storage, temporary, destination, backup);
}

bool pocket_d20_storage_duplicate_profile(Storage* storage, uint32_t source, uint32_t destination) {
    char source_path[POCKET_D20_PATH_LEN];
    if(!pocket_d20_find_profile_path(storage, source, source_path, sizeof(source_path)))
        return false;
    const char* filename = strrchr(source_path, '/');
    filename = filename ? filename + 1U : source_path;
    PocketProfileEntry entry;
    if(!pocket_d20_parse_profile_filename(filename, &entry)) return false;
    char safe_name[POCKET_D20_CHARACTER_NAME_LEN];
    pocket_d20_filename_name(safe_name, sizeof(safe_name), entry.name);
    char destination_path[POCKET_D20_PATH_LEN];
    char temporary[POCKET_D20_PATH_LEN];
    snprintf(
        destination_path,
        sizeof(destination_path),
        "%s/ch_%lu_%s_%u.txt",
        POCKET_D20_DATA_DIR,
        (unsigned long)destination,
        safe_name,
        entry.level);
    pocket_d20_work_path(temporary, sizeof(temporary), destination, "duplicate");
    if(!pocket_d20_copy_file(storage, source_path, destination_path, temporary)) return false;

    const char* collections[] = {"spellbook", "items"};
    for(uint8_t i = 0U; i < 2U; ++i) {
        if(!strcmp(collections[i], "spellbook")) {
            pocket_d20_spellbook_path(source_path, sizeof(source_path), source);
            pocket_d20_spellbook_path(destination_path, sizeof(destination_path), destination);
        } else {
            pocket_d20_items_path(source_path, sizeof(source_path), source);
            pocket_d20_items_path(destination_path, sizeof(destination_path), destination);
        }
        if(!storage_file_exists(storage, source_path)) continue;
        if(!pocket_d20_copy_file_direct(storage, source_path, destination_path)) {
            pocket_d20_storage_delete_profile(storage, destination);
            return false;
        }
    }
    return true;
}

bool pocket_d20_storage_export_profile(Storage* storage, uint32_t profile) {
    char source_path[POCKET_D20_PATH_LEN];
    if(!pocket_d20_find_profile_path(storage, profile, source_path, sizeof(source_path)))
        return false;
    storage_common_mkdir(storage, POCKET_D20_EXPORT_DIR);
    const char* filename = strrchr(source_path, '/');
    filename = filename ? filename + 1U : source_path;
    char destination[POCKET_D20_LONG_PATH_LEN];
    char temporary[POCKET_D20_LONG_PATH_LEN];
    if(!pocket_d20_child_path(
           destination, sizeof(destination), POCKET_D20_EXPORT_DIR, "export_", filename))
        return false;
    snprintf(
        temporary,
        sizeof(temporary),
        "%s/export_%lu.tmp",
        POCKET_D20_EXPORT_DIR,
        (unsigned long)profile);
    if(!pocket_d20_copy_file(storage, source_path, destination, temporary)) return false;

    const char* collections[] = {"spellbook", "items"};
    for(uint8_t i = 0U; i < 2U; ++i) {
        if(!strcmp(collections[i], "spellbook"))
            pocket_d20_spellbook_path(source_path, sizeof(source_path), profile);
        else
            pocket_d20_items_path(source_path, sizeof(source_path), profile);
        if(!storage_file_exists(storage, source_path)) continue;
        snprintf(
            destination,
            sizeof(destination),
            "%s/export_ch_%lu_%s.txt",
            POCKET_D20_EXPORT_DIR,
            (unsigned long)profile,
            collections[i]);
        if(!pocket_d20_copy_file_direct(storage, source_path, destination)) return false;
    }
    return true;
}

bool pocket_d20_storage_archive_profile(Storage* storage, uint32_t profile) {
    char source_path[POCKET_D20_PATH_LEN];
    if(!pocket_d20_find_profile_path(storage, profile, source_path, sizeof(source_path)))
        return false;
    storage_common_mkdir(storage, POCKET_D20_ARCHIVE_DIR);
    const char* filename = strrchr(source_path, '/');
    filename = filename ? filename + 1U : source_path;
    char destination[POCKET_D20_LONG_PATH_LEN];
    if(!pocket_d20_child_path(
           destination, sizeof(destination), POCKET_D20_ARCHIVE_DIR, NULL, filename))
        return false;
    if(storage_file_exists(storage, destination)) return false;

    char collection_source[2][POCKET_D20_PATH_LEN];
    char collection_destination[2][POCKET_D20_LONG_PATH_LEN];
    bool collection_present[2] = {false, false};
    const char* collections[] = {"spellbook", "items"};
    for(uint8_t i = 0U; i < 2U; ++i) {
        if(!strcmp(collections[i], "spellbook"))
            pocket_d20_spellbook_path(collection_source[i], sizeof(collection_source[i]), profile);
        else
            pocket_d20_items_path(collection_source[i], sizeof(collection_source[i]), profile);
        collection_present[i] = storage_file_exists(storage, collection_source[i]);
        snprintf(
            collection_destination[i],
            sizeof(collection_destination[i]),
            "%s/ch_%lu_%s.txt",
            POCKET_D20_ARCHIVE_DIR,
            (unsigned long)profile,
            collections[i]);
        if(collection_present[i] && storage_file_exists(storage, collection_destination[i]))
            return false;
    }

    if(storage_common_rename(storage, source_path, destination) != FSE_OK) return false;
    for(uint8_t i = 0U; i < 2U; ++i) {
        if(!collection_present[i]) continue;
        if(storage_common_rename(storage, collection_source[i], collection_destination[i]) == FSE_OK) {
            continue;
        }
        /* Keep archive as an all-or-nothing character set when a sidecar move fails. */
        for(uint8_t rollback = 0U; rollback < i; ++rollback) {
            if(collection_present[rollback])
                storage_common_rename(
                    storage, collection_destination[rollback], collection_source[rollback]);
        }
        storage_common_rename(storage, destination, source_path);
        return false;
    }
    return true;
}

bool pocket_d20_storage_verify_profile(Storage* storage, uint32_t profile) {
    char path[POCKET_D20_PATH_LEN];
    if(!pocket_d20_find_profile_path(storage, profile, path, sizeof(path))) return false;

    /* Verification uses the normal best-effort parser, but its full character
       state is transient and must not consume nearly 4 KiB of the UI thread
       stack. Keep the save format and parser behavior unchanged while owning
       the temporary state on checked heap storage. */
    PocketSaveData* parsed = calloc(1U, sizeof(PocketSaveData));
    if(!parsed) return false;
    bool ok = pocket_d20_load_text_path(storage, path, parsed);
    pocket_d20_data_clear(parsed);
    free(parsed);
    return ok;
}

bool pocket_d20_storage_restore_backup(Storage* storage, uint32_t profile, PocketSaveData* data) {
    char backup_path[POCKET_D20_PATH_LEN];
    char primary_path[POCKET_D20_PATH_LEN];
    char rejected_path[POCKET_D20_PATH_LEN];
    pocket_d20_work_path(backup_path, sizeof(backup_path), profile, "backup");
    if(!pocket_d20_load_text_path(storage, backup_path, data)) return false;
    bool had_primary =
        pocket_d20_find_profile_path(storage, profile, primary_path, sizeof(primary_path));
    pocket_d20_work_path(rejected_path, sizeof(rejected_path), profile, "rejected");
    storage_common_remove(storage, rejected_path);
    if(had_primary && storage_common_rename(storage, primary_path, rejected_path) != FSE_OK)
        return false;
    bool restored = pocket_d20_storage_save_profile_updated(storage, profile, data);
    if(restored) {
        storage_common_remove(storage, rejected_path);
        return true;
    }
    if(had_primary) storage_common_rename(storage, rejected_path, primary_path);
    return false;
}

bool pocket_d20_storage_import_first(Storage* storage, uint32_t destination, PocketSaveData* data) {
    File* directory = storage_file_alloc(storage);
    if(!directory) return false;
    if(!storage_dir_open(directory, POCKET_D20_EXPORT_DIR)) {
        storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    bool imported = false;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        size_t length = strlen(filename);
        if(file_info_is_dir(&info) || length < 12U || strncmp(filename, "export_ch_", 10U) != 0 ||
           strcmp(filename + length - 4U, ".txt") != 0)
            continue;

        /* Only import a core character export here. Collection exports are
           companions and are copied after their matching core file succeeds. */
        PocketProfileEntry source_entry;
        if(!pocket_d20_parse_profile_filename(filename + 7U, &source_entry)) continue;

        char path[POCKET_D20_LONG_PATH_LEN];
        if(!pocket_d20_child_path(path, sizeof(path), POCKET_D20_EXPORT_DIR, NULL, filename))
            continue;
        if(!pocket_d20_load_text_path(storage, path, data) ||
           !pocket_d20_storage_save_profile(storage, destination, data))
            continue;

        /* The current storage model has no embedded spell/item migration.
           Destination sidecars are created only when matching current-format
           collection exports are present. */
        pocket_d20_data_clear_spells(&data->character);
        pocket_d20_data_clear_items(&data->character);

        const char* collections[] = {"spellbook", "items"};
        bool companions_ok = true;
        for(uint8_t i = 0U; i < 2U; ++i) {
            char source_collection[POCKET_D20_LONG_PATH_LEN];
            char destination_collection[POCKET_D20_PATH_LEN];
            snprintf(
                source_collection,
                sizeof(source_collection),
                "%s/export_ch_%lu_%s.txt",
                POCKET_D20_EXPORT_DIR,
                (unsigned long)source_entry.id,
                collections[i]);
            if(!storage_file_exists(storage, source_collection)) continue;
            if(!strcmp(collections[i], "spellbook"))
                pocket_d20_spellbook_path(
                    destination_collection, sizeof(destination_collection), destination);
            else
                pocket_d20_items_path(
                    destination_collection, sizeof(destination_collection), destination);
            if(!pocket_d20_copy_file_direct(
                   storage, source_collection, destination_collection)) {
                companions_ok = false;
                break;
            }
        }
        if(!companions_ok) {
            pocket_d20_storage_delete_profile(storage, destination);
            break;
        }
        imported = true;
        break;
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return imported;
}

bool pocket_d20_storage_move_legacy_profiles(Storage* storage) {
    if(!storage) return false;
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);
    FileInfo legacy_info;
    if(storage_common_stat(storage, POCKET_D20_LEGACY_PROFILE_DIR, &legacy_info) != FSE_OK ||
       !file_info_is_dir(&legacy_info))
        return true;

    bool all_ok = true;
    while(true) {
        File* directory = storage_file_alloc(storage);
        if(!directory) return false;
        if(!storage_dir_open(directory, POCKET_D20_LEGACY_PROFILE_DIR)) {
            storage_file_free(directory);
            return false;
        }
        FileInfo info;
        char filename[128];
        bool moved_one = false;
        while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
            size_t length = strlen(filename);
            if(file_info_is_dir(&info) || strncmp(filename, "ch_", 3U) != 0 ||
               length < 8U || strcmp(filename + length - 4U, ".txt") != 0)
                continue;
            PocketProfileEntry legacy_entry;
            if(!pocket_d20_parse_profile_filename(filename, &legacy_entry)) continue;

            char source[POCKET_D20_LONG_PATH_LEN];
            char destination[POCKET_D20_LONG_PATH_LEN];
            if(!pocket_d20_child_path(
                   source, sizeof(source), POCKET_D20_LEGACY_PROFILE_DIR, NULL, filename) ||
               !pocket_d20_child_path(
                   destination, sizeof(destination), POCKET_D20_DATA_DIR, NULL, filename)) {
                all_ok = false;
                continue;
            }
            /* The current root wins by character ID, not only by exact filename.
               This prevents two primary ch*.txt files with the same ID when the
               character name/level changed after the old copy was created. */
            char current_path[POCKET_D20_LONG_PATH_LEN];
            if(pocket_d20_find_profile_path(
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

void pocket_d20_profiles_set_defaults(PocketProfileState* profiles) {
    memset(profiles, 0, sizeof(*profiles));
}

void pocket_d20_profiles_free(PocketProfileState* profiles) {
    if(!profiles) return;
    pocket_d20_profiles_set_defaults(profiles);
}

static void pocket_d20_profiles_insert_smallest(
    PocketProfileState* profiles,
    const PocketProfileEntry* entry) {
    if(profiles->cache_count < POCKET_D20_PROFILE_CACHE_SIZE) {
        profiles->entries[profiles->cache_count++] = *entry;
    } else {
        if(entry->id >= profiles->entries[profiles->cache_count - 1U].id) return;
        profiles->entries[profiles->cache_count - 1U] = *entry;
    }
    uint8_t position = (uint8_t)(profiles->cache_count - 1U);
    while(position > 0U && profiles->entries[position - 1U].id > profiles->entries[position].id) {
        PocketProfileEntry swap = profiles->entries[position - 1U];
        profiles->entries[position - 1U] = profiles->entries[position];
        profiles->entries[position] = swap;
        --position;
    }
}

static void pocket_d20_profiles_insert_largest(
    PocketProfileState* profiles,
    const PocketProfileEntry* entry) {
    if(profiles->cache_count < POCKET_D20_PROFILE_CACHE_SIZE) {
        profiles->entries[profiles->cache_count++] = *entry;
        uint8_t position = (uint8_t)(profiles->cache_count - 1U);
        while(position > 0U && profiles->entries[position - 1U].id > profiles->entries[position].id) {
            PocketProfileEntry swap = profiles->entries[position - 1U];
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
        PocketProfileEntry swap = profiles->entries[position + 1U];
        profiles->entries[position + 1U] = profiles->entries[position];
        profiles->entries[position] = swap;
        ++position;
    }
}

static bool pocket_d20_profiles_scan_cache(
    Storage* storage,
    PocketProfileState* profiles,
    uint32_t boundary,
    bool after,
    uint16_t cache_start) {
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, POCKET_D20_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    profiles->cache_count = 0U;
    FileInfo info;
    char filename[128];
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info)) continue;
        PocketProfileEntry entry;
        if(!pocket_d20_parse_profile_filename(filename, &entry)) continue;
        if(after) {
            if(entry.id <= boundary) continue;
            pocket_d20_profiles_insert_smallest(profiles, &entry);
        } else {
            if(entry.id >= boundary) continue;
            pocket_d20_profiles_insert_largest(profiles, &entry);
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(!profiles->cache_count) return false;
    profiles->cache_start = after ? cache_start :
        (cache_start >= profiles->cache_count ? (uint16_t)(cache_start - profiles->cache_count) : 0U);
    return true;
}

bool pocket_d20_profiles_find(
    Storage* storage,
    uint32_t profile,
    PocketProfileEntry* output) {
    if(!storage) return false;
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, POCKET_D20_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    bool found = false;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info)) continue;
        PocketProfileEntry entry;
        if(pocket_d20_parse_profile_filename(filename, &entry) && entry.id == profile) {
            if(output) *output = entry;
            found = true;
            break;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return found;
}

bool pocket_d20_profiles_refresh(Storage* storage, PocketProfileState* profiles) {
    furi_assert(storage);
    furi_assert(profiles);
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);
    profiles->count = 0U;
    profiles->cache_start = 0U;
    profiles->cache_count = 0U;
    profiles->active_entry_valid = 0U;
    profiles->highest_reserved_id = 0U;
    profiles->reserved_id_seen = 0U;
    profiles->character_file_seen = 0U;
    profiles->scan_succeeded = 0U;
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, POCKET_D20_DATA_DIR)) {
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
            (length >= 14U && !strcmp(filename + length - 14U, "_spellbook.txt")) ||
            (length >= 10U && !strcmp(filename + length - 10U, "_items.txt"));
        bool character_related =
            (length >= 8U && !collection_sidecar && !strncmp(filename, "ch_", 3U) &&
             !strcmp(filename + length - 4U, ".txt")) ||
            !strncmp(filename, "custom_backup_", 14U) ||
            !strncmp(filename, "custom_rejected_", 16U) ||
            !strncmp(filename, "custom_write_", 13U);
        if(character_related) profiles->character_file_seen = 1U;

        PocketProfileEntry entry;
        bool primary = pocket_d20_parse_profile_filename(filename, &entry);
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
        pocket_d20_profiles_insert_smallest(profiles, &entry);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    profiles->scan_succeeded = success ? 1U : 0U;
    return success;
}

const PocketProfileEntry* pocket_d20_profiles_entry_at(
    Storage* storage,
    PocketProfileState* profiles,
    uint16_t index) {
    if(!storage || !profiles || index >= profiles->count) return NULL;
    if(!profiles->cache_count && !pocket_d20_profiles_refresh(storage, profiles)) return NULL;

    uint16_t guard = 0U;
    while(index < profiles->cache_start && profiles->cache_count && guard++ < UINT16_MAX) {
        uint32_t before = profiles->entries[0].id;
        uint16_t old_start = profiles->cache_start;
        if(!pocket_d20_profiles_scan_cache(storage, profiles, before, false, old_start) ||
           profiles->cache_start >= old_start)
            return NULL;
    }
    guard = 0U;
    while(index >= (uint16_t)(profiles->cache_start + profiles->cache_count) &&
          profiles->cache_count && guard++ < UINT16_MAX) {
        uint32_t after = profiles->entries[profiles->cache_count - 1U].id;
        uint16_t next_start = (uint16_t)(profiles->cache_start + profiles->cache_count);
        if(!pocket_d20_profiles_scan_cache(storage, profiles, after, true, next_start)) return NULL;
    }
    if(index < profiles->cache_start ||
       index >= (uint16_t)(profiles->cache_start + profiles->cache_count))
        return NULL;
    return &profiles->entries[index - profiles->cache_start];
}

bool pocket_d20_profiles_window(
    Storage* storage,
    PocketProfileState* profiles,
    uint16_t start) {
    if(!storage || !profiles || start >= profiles->count) return false;
    if(profiles->cache_count && profiles->cache_start == start) return true;
    if(start == 0U) {
        uint32_t active = profiles->active_profile;
        bool scanned = pocket_d20_profiles_refresh(storage, profiles);
        profiles->active_profile = active;
        return scanned && profiles->cache_count;
    }
    const PocketProfileEntry* previous = pocket_d20_profiles_entry_at(storage, profiles, start - 1U);
    if(!previous) return false;
    uint32_t boundary = previous->id;
    return pocket_d20_profiles_scan_cache(storage, profiles, boundary, true, start);
}

bool pocket_d20_profiles_next_after(
    Storage* storage,
    uint32_t after_profile,
    PocketProfileEntry* output) {
    if(!storage || !output) return false;
    File* directory = storage_file_alloc(storage);
    if(!directory) return false;
    if(!storage_dir_open(directory, POCKET_D20_DATA_DIR)) {
        storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    PocketProfileEntry next = {0};
    PocketProfileEntry first = {0};
    bool have_next = false;
    bool have_first = false;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info)) continue;
        PocketProfileEntry entry;
        if(!pocket_d20_parse_profile_filename(filename, &entry)) continue;
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

uint32_t pocket_d20_profiles_next_id(const PocketProfileState* profiles) {
    if(!profiles->reserved_id_seen) return 0U;
    return profiles->highest_reserved_id < UINT32_MAX ? profiles->highest_reserved_id + 1U :
                                                        UINT32_MAX;
}

bool pocket_d20_profiles_load(Storage* storage, PocketProfileState* profiles) {
    furi_assert(storage);
    furi_assert(profiles);
    pocket_d20_profiles_set_defaults(profiles);

    /* Active-profile metadata is best-effort by field name. Missing, reordered,
       malformed, or future fields cannot prevent character discovery. */
    File* file = storage_file_alloc(storage);
    if(file && storage_file_open(file, POCKET_D20_ACTIVE_PROFILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        PocketD20Reader reader;
        pocket_d20_reader_init(&reader, file);
        char line[POCKET_D20_VALUE_LINE_LEN];
        while(pocket_d20_read_line(&reader, line, sizeof(line))) {
            char* value = strchr(line, '=');
            if(!value) continue;
            *value++ = '\0';
            if(strcmp(line, "Active") != 0) continue;
            uint32_t active_profile = 0U;
            if(pocket_d20_parse_u32_range(value, UINT32_MAX, &active_profile))
                profiles->active_profile = active_profile;
        }
    }
    if(file) {
        storage_file_close(file);
        storage_file_free(file);
    }

    bool scanned = pocket_d20_profiles_refresh(storage, profiles);
    bool active_found = pocket_d20_profiles_find(storage, profiles->active_profile, NULL);
    if(!active_found && profiles->count) {
        PocketProfileEntry next;
        if(pocket_d20_profiles_next_after(storage, profiles->active_profile, &next)) {
            profiles->active_profile = next.id;
            profiles->active_entry = next;
            profiles->active_entry_valid = 1U;
        }
    }
    return scanned;
}

bool pocket_d20_profiles_save(Storage* storage, const PocketProfileState* profiles) {
    furi_assert(storage);
    furi_assert(profiles);
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool written =
        storage_file_open(
            file, POCKET_D20_ACTIVE_PROFILE_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        pocket_d20_writef(file, "Active=%lu\n", (unsigned long)profiles->active_profile) &&
        storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!written) {
        storage_common_remove(storage, POCKET_D20_ACTIVE_PROFILE_TEMP_PATH);
        return false;
    }
    return pocket_d20_publish_temp(
        storage,
        POCKET_D20_ACTIVE_PROFILE_TEMP_PATH,
        POCKET_D20_ACTIVE_PROFILE_PATH,
        POCKET_D20_ACTIVE_PROFILE_BACKUP_PATH);
}
