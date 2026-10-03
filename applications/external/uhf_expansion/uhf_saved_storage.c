#include "uhf_saved_tag.h"
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UHF_SAVED_TAG_DIR STORAGE_APP_DATA_PATH_PREFIX "/tags"

static bool uhf_saved_path(uint16_t id, char* out, size_t size) {
    if(id == 0U || id > UHF_SAVED_TAG_LIMIT) return false;
    const int length = snprintf(out, size, UHF_SAVED_TAG_DIR "/tag_%03u.uhf", id);
    return length > 0 && (size_t)length < size;
}

bool uhf_saved_tags_list(uint16_t* ids, size_t capacity, size_t* count) {
    if(!ids || !count || !capacity) return false;
    *count = 0U;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    File* directory = storage_file_alloc(storage);
    bool ok = false;
    if(directory && storage_dir_open(directory, UHF_SAVED_TAG_DIR)) {
        char name[32];
        FileInfo info;
        while(storage_dir_read(directory, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY || strlen(name) != 11U || strncmp(name, "tag_", 4U) ||
               strcmp(name + 7U, ".uhf"))
                continue;
            if(name[4] < '0' || name[4] > '9' || name[5] < '0' || name[5] > '9' || name[6] < '0' ||
               name[6] > '9')
                continue;
            const uint16_t id = (name[4] - '0') * 100U + (name[5] - '0') * 10U + name[6] - '0';
            if(id && *count < capacity) ids[(*count)++] = id;
        }
        const FS_Error error = storage_file_get_error(directory);
        ok = error == FSE_OK || error == FSE_NOT_EXIST;
        /* qsort is not exported by the Flipper SDK. The bounded ID list is
           small enough for insertion sort without extra allocation. */
        for(size_t i = 1U; i < *count; i++) {
            const uint16_t id = ids[i];
            size_t position = i;
            while(position && ids[position - 1U] > id) {
                ids[position] = ids[position - 1U];
                position--;
            }
            ids[position] = id;
        }
    } else if(directory) {
        FileInfo info;
        /* An absent library is empty; storage/media failures remain errors. */
        ok = storage_common_stat(storage, UHF_SAVED_TAG_DIR, &info) == FSE_NOT_EXIST;
    }
    if(directory) {
        storage_dir_close(directory);
        storage_file_free(directory);
    }
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool uhf_saved_tag_load(uint16_t id, UhfSavedTag* out) {
    char path[128];
    if(!out || !uhf_saved_path(id, path, sizeof(path))) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(file && storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        const uint64_t size = storage_file_size(file);
        if(size > 0U && size < UHF_SAVED_TAG_TEXT_MAX) {
            char text[UHF_SAVED_TAG_TEXT_MAX];
            if(storage_file_read(file, text, (size_t)size) == size) {
                text[size] = '\0';
                ok = strlen(text) == size && uhf_saved_tag_parse(text, out);
            }
        }
    }
    if(file) {
        storage_file_close(file);
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool uhf_saved_tag_save(const UhfSavedTag* tag, uint16_t* id) {
    char text[UHF_SAVED_TAG_TEXT_MAX];
    if(!id || !uhf_saved_tag_format(tag, text, sizeof(text))) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    const FS_Error mkdir_result = storage_common_mkdir(storage, UHF_SAVED_TAG_DIR);
    bool ok = false;
    if(mkdir_result == FSE_OK || mkdir_result == FSE_EXIST) {
        for(uint16_t candidate = 1U; candidate <= UHF_SAVED_TAG_LIMIT; candidate++) {
            char path[128];
            uhf_saved_path(candidate, path, sizeof(path));
            if(storage_file_exists(storage, path)) continue;
            File* file = storage_file_alloc(storage);
            bool created = file && storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_NEW);
            if(created)
                ok = storage_file_write(file, text, strlen(text)) == strlen(text) &&
                     storage_file_sync(file);
            if(file) {
                storage_file_close(file);
                storage_file_free(file);
            }
            if(created && !ok) storage_common_remove(storage, path);
            if(ok) *id = candidate;
            break;
        }
    }
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool uhf_saved_tag_delete(uint16_t id) {
    char path[128];
    if(!uhf_saved_path(id, path, sizeof(path))) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    const bool ok = storage_common_remove(storage, path) == FSE_OK;
    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* Stage and sync before replacing the record; retain a backup on rollback failure. */
bool uhf_saved_tag_update(uint16_t id, const UhfSavedTag* tag) {
    char path[128], temp[136], backup[136], text[UHF_SAVED_TAG_TEXT_MAX];
    if(!uhf_saved_path(id, path, sizeof(path)) || !uhf_saved_tag_format(tag, text, sizeof(text)))
        return false;
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    bool ok = false;
    if(storage_file_exists(storage, path) && !storage_file_exists(storage, temp) &&
       !storage_file_exists(storage, backup)) {
        File* file = storage_file_alloc(storage);
        const bool created = file && storage_file_open(file, temp, FSAM_WRITE, FSOM_CREATE_NEW);
        const bool staged = created &&
                            storage_file_write(file, text, strlen(text)) == strlen(text) &&
                            storage_file_sync(file);
        if(file) {
            storage_file_close(file);
            storage_file_free(file);
        }
        if(staged && storage_common_rename(storage, path, backup) == FSE_OK) {
            ok = storage_common_rename(storage, temp, path) == FSE_OK;
            if(ok)
                storage_common_remove(storage, backup);
            else
                storage_common_rename(storage, backup, path);
        }
        if(created && !ok) storage_common_remove(storage, temp);
    }
    furi_record_close(RECORD_STORAGE);
    return ok;
}
