#include "storage.h"

#include <flipper_format/flipper_format.h>

#include <stdio.h>
#include <string.h>

#define FILE_TYPE    "OpenShock Shocker"
#define FILE_VERSION 1

bool openshock_shocker_unique_stem(ShockerModel model, uint16_t id, char* stem_out, size_t stem_sz) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, OPENSHOCK_SAVE_DIR);

    const char* model_name = openshock_model_name(model);
    bool ok = false;

    for(unsigned suffix = 0; suffix < 1000; suffix++) {
        char candidate[OPENSHOCK_NAME_MAX_LEN];
        if(suffix == 0) {
            snprintf(candidate, sizeof(candidate), "%s_%u", model_name, id);
        } else {
            snprintf(candidate, sizeof(candidate), "%s_%u_%u", model_name, id, suffix);
        }

        char path[OPENSHOCK_PATH_MAX_LEN];
        snprintf(path, sizeof(path), "%s/%s.shk", OPENSHOCK_SAVE_DIR, candidate);

        FileInfo info;
        FS_Error err = storage_common_stat(storage, path, &info);
        if(err != FSE_OK) {
            snprintf(stem_out, stem_sz, "%s", candidate);
            ok = true;
            break;
        }
    }

    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool openshock_shocker_write(
    const char* stem,
    ShockerModel model,
    uint16_t id,
    uint8_t channel,
    uint8_t sync_group,
    const char* replace_path) {
    if(!stem || stem[0] == '\0') return false;

    if(sync_group > 9) sync_group = 9;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, OPENSHOCK_SAVE_DIR);

    char path[OPENSHOCK_PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%s.shk", OPENSHOCK_SAVE_DIR, stem);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_always(ff, path)) break;
        if(!flipper_format_write_header_cstr(ff, FILE_TYPE, FILE_VERSION)) break;
        if(!flipper_format_write_string_cstr(ff, "Name", stem)) break;
        if(!flipper_format_write_string_cstr(ff, "Model", openshock_model_name(model))) break;

        uint32_t val;
        val = id;
        if(!flipper_format_write_uint32(ff, "ID", &val, 1)) break;
        val = channel;
        if(!flipper_format_write_uint32(ff, "Channel", &val, 1)) break;
        val = sync_group;
        if(!flipper_format_write_uint32(ff, "Sync", &val, 1)) break;

        ok = true;
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);

    if(ok && replace_path && strcmp(replace_path, path) != 0) {
        storage_common_remove(storage, replace_path);
    }

    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool parse_model(const char* str, ShockerModel* out) {
    for(int i = 0; i < ShockerModelCount; i++) {
        if(strcmp(str, openshock_model_name((ShockerModel)i)) == 0) {
            *out = (ShockerModel)i;
            return true;
        }
    }
    return false;
}

static bool load_one(Storage* storage, const char* path, SavedShocker* out) {
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool load_ok = false;

    do {
        if(!flipper_format_file_open_existing(ff, path)) break;

        FuriString* str = furi_string_alloc();
        uint32_t version;
        if(!flipper_format_read_header(ff, str, &version)) {
            furi_string_free(str);
            break;
        }
        if(strcmp(furi_string_get_cstr(str), FILE_TYPE) != 0 || version != FILE_VERSION) {
            furi_string_free(str);
            break;
        }

        if(!flipper_format_read_string(ff, "Name", str)) {
            furi_string_free(str);
            break;
        }
        snprintf(out->name, sizeof(out->name), "%s", furi_string_get_cstr(str));

        if(!flipper_format_read_string(ff, "Model", str)) {
            furi_string_free(str);
            break;
        }
        if(!parse_model(furi_string_get_cstr(str), &out->model)) {
            furi_string_free(str);
            break;
        }
        furi_string_free(str);

        uint32_t val;
        if(!flipper_format_read_uint32(ff, "ID", &val, 1)) break;
        out->shocker_id = (uint16_t)val;

        if(!flipper_format_read_uint32(ff, "Channel", &val, 1)) break;
        out->channel = (uint8_t)val;

        out->sync_group = 0;
        if(flipper_format_read_uint32(ff, "Sync", &val, 1)) {
            if(val <= 9) out->sync_group = (uint8_t)val;
        }

        snprintf(out->filename, sizeof(out->filename), "%s", path);
        load_ok = true;
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    return load_ok;
}

static void saved_shocker_sort_by_name(SavedShocker* list, size_t count) {
    for(size_t i = 1; i < count; i++) {
        SavedShocker key = list[i];
        size_t j = i;
        while(j > 0) {
            int r = strcmp(list[j - 1].name, key.name);
            if(r == 0) r = strcmp(list[j - 1].filename, key.filename);
            if(r <= 0) break;
            list[j] = list[j - 1];
            j--;
        }
        list[j] = key;
    }
}

size_t openshock_shocker_list(SavedShocker* list, size_t max_count) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage_dir_exists(storage, OPENSHOCK_SAVE_DIR)) {
        furi_record_close(RECORD_STORAGE);
        return 0;
    }

    File* dir = storage_file_alloc(storage);
    size_t count = 0;

    if(storage_dir_open(dir, OPENSHOCK_SAVE_DIR)) {
        FileInfo info;
        char name[64];
        while(count < max_count && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;

            size_t len = strlen(name);
            if(len < 5 || strcmp(name + len - 4, ".shk") != 0) continue;

            char path[OPENSHOCK_PATH_MAX_LEN];
            snprintf(path, sizeof(path), "%s/%.60s", OPENSHOCK_SAVE_DIR, name);

            if(load_one(storage, path, &list[count])) {
                count++;
            }
        }
        storage_dir_close(dir);
    }

    if(count > 1) {
        saved_shocker_sort_by_name(list, count);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    return count;
}

bool openshock_shocker_delete(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FS_Error err = storage_common_remove(storage, path);
    furi_record_close(RECORD_STORAGE);
    return err == FSE_OK;
}

bool openshock_stem_exists(const char* stem) {
    if(!stem || !stem[0]) return false;
    char path[OPENSHOCK_PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%s.shk", OPENSHOCK_SAVE_DIR, stem);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo info;
    FS_Error err = storage_common_stat(storage, path, &info);
    furi_record_close(RECORD_STORAGE);
    return err == FSE_OK;
}
