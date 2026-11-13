#include "keytool_dict.h"
#include <stream/stream.h>
#include <stream/file_stream.h>
#include <string.h>
#include <strings.h>

#define KEYTOOL_DICT_ASSET_USER "/ext/nfc/assets/mf_classic_dict_user.nfc"
#define KEYTOOL_DICT_ASSET_SYSTEM "/ext/nfc/assets/mf_classic_dict.nfc"
#define KEYTOOL_DICT_FOLDER_1 "/ext/nfc/dict"
#define KEYTOOL_DICT_FOLDER_2 "/ext/dict"

static bool keytool_has_dict_extension(const char* name) {
    static const char* exts[] = {".dict", ".txt", ".nfc"};
    size_t len = strlen(name);
    for(size_t i = 0; i < 3; i++) {
        size_t elen = strlen(exts[i]);
        if(len > elen && strcasecmp(name + (len - elen), exts[i]) == 0) return true;
    }
    return false;
}

// Non-recursive: lists dictionary-looking files directly inside folder_path.
static void keytool_dict_enumerate(
    Storage* storage,
    const char* folder_path,
    FuriString** names_out,
    size_t* count_out,
    size_t max_count) {
    *count_out = 0;

    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, folder_path)) {
        storage_file_free(dir);
        return;
    }

    FileInfo file_info;
    char name_buf[256];
    while(*count_out < max_count &&
          storage_dir_read(dir, &file_info, name_buf, sizeof(name_buf))) {
        if(file_info.flags & FSF_DIRECTORY) continue;
        if(!keytool_has_dict_extension(name_buf)) continue;
        furi_string_set_str(names_out[*count_out], name_buf);
        (*count_out)++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
}

static void keytool_dict_try_add_file(
    Storage* storage,
    const char* path,
    const char* label,
    FuriString** paths_out,
    FuriString** names_out,
    size_t* count,
    size_t max_count) {
    if(*count >= max_count) return;
    FileInfo info;
    if(storage_common_stat(storage, path, &info) == FSE_OK && !(info.flags & FSF_DIRECTORY)) {
        furi_string_set_str(paths_out[*count], path);
        furi_string_set_str(names_out[*count], label);
        (*count)++;
    }
}

void keytool_dict_autodetect(
    Storage* storage,
    FuriString** paths_out,
    FuriString** names_out,
    size_t* count_out,
    size_t max_count) {
    *count_out = 0;

    keytool_dict_try_add_file(
        storage,
        KEYTOOL_DICT_ASSET_USER,
        "mf_classic_dict_user.nfc",
        paths_out,
        names_out,
        count_out,
        max_count);
    keytool_dict_try_add_file(
        storage,
        KEYTOOL_DICT_ASSET_SYSTEM,
        "mf_classic_dict.nfc",
        paths_out,
        names_out,
        count_out,
        max_count);

    const char* folders[] = {KEYTOOL_DICT_FOLDER_1, KEYTOOL_DICT_FOLDER_2};
    for(size_t f = 0; f < 2 && *count_out < max_count; f++) {
        FileInfo dinfo;
        if(storage_common_stat(storage, folders[f], &dinfo) != FSE_OK) continue;
        if(!(dinfo.flags & FSF_DIRECTORY)) continue;

        size_t remaining = max_count - *count_out;
        if(remaining > KEYTOOL_MAX_DICTS) remaining = KEYTOOL_MAX_DICTS;

        FuriString* tmp_names[KEYTOOL_MAX_DICTS];
        for(size_t i = 0; i < remaining; i++) tmp_names[i] = furi_string_alloc();

        size_t tmp_count = 0;
        keytool_dict_enumerate(storage, folders[f], tmp_names, &tmp_count, remaining);

        for(size_t i = 0; i < tmp_count && *count_out < max_count; i++) {
            furi_string_printf(
                paths_out[*count_out],
                "%s/%s",
                folders[f],
                furi_string_get_cstr(tmp_names[i]));
            furi_string_set(names_out[*count_out], tmp_names[i]);
            (*count_out)++;
        }

        for(size_t i = 0; i < remaining; i++) furi_string_free(tmp_names[i]);
    }
}

// Reads up to 6 hex byte pairs out of a line, tolerating "FFFFFFFFFFFF",
// "FF FF FF FF FF FF", "FF:FF:FF:FF:FF:FF" and trailing comments.
// Lines starting with '#' or ';' are treated as comments.
static bool keytool_parse_key_line(const char* line, uint8_t* out_bytes) {
    int hex_count = 0;
    uint8_t byte = 0;
    int nibble_count = 0;

    for(const char* p = line; *p && hex_count < 6; p++) {
        char c = *p;
        int val;
        if(c >= '0' && c <= '9') {
            val = c - '0';
        } else if(c >= 'a' && c <= 'f') {
            val = c - 'a' + 10;
        } else if(c >= 'A' && c <= 'F') {
            val = c - 'A' + 10;
        } else {
            if(nibble_count != 0) return false; // malformed mid-byte
            if(hex_count == 0 && (c == '#' || c == ';')) return false; // comment line
            continue; // skip separators (space, colon, etc.)
        }
        byte = (uint8_t)((byte << 4) | val);
        nibble_count++;
        if(nibble_count == 2) {
            out_bytes[hex_count++] = byte;
            byte = 0;
            nibble_count = 0;
        }
    }
    return hex_count == 6;
}

void keytool_dict_scan_file(
    Storage* storage,
    const char* file_path,
    const KeyToolKeyEntry* keys,
    size_t key_count,
    bool* found_matrix,
    size_t dict_count,
    size_t dict_index) {
    Stream* stream = file_stream_alloc(storage);
    if(!file_stream_open(stream, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        stream_free(stream);
        return;
    }

    FuriString* line = furi_string_alloc();
    uint8_t key_bytes[6];

    while(stream_read_line(stream, line)) {
        if(furi_string_size(line) < 12) continue;
        if(!keytool_parse_key_line(furi_string_get_cstr(line), key_bytes)) continue;
        for(size_t k = 0; k < key_count; k++) {
            if(memcmp(keys[k].bytes, key_bytes, 6) == 0) {
                found_matrix[k * dict_count + dict_index] = true;
            }
        }
    }

    furi_string_free(line);
    file_stream_close(stream);
    stream_free(stream);
}
