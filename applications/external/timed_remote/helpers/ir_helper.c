#include <flipper_format/flipper_format.h>
#include <furi.h>
#include <infrared/worker/infrared_worker.h>
#include "../signal/infrared_error_code.h"
#include <stdlib.h>
#include <storage/storage.h>
#include <string.h>

#include "ir_helper.h"

static bool ir_add_signal(IrSignalList*, InfraredSignal*, const char*);
static bool ir_is_file(const FileInfo*, const char*);

IrSignalList* ir_list_alloc(void) {
    IrSignalList* list;

    list = malloc(sizeof(IrSignalList));
    if(list == NULL) return NULL;

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;

    return list;
}

void ir_list_free(IrSignalList* list) {
    size_t i;

    if(list == NULL) return;

    for(i = 0; i < list->count; i++) {
        if(list->items[i].signal != NULL) infrared_signal_free(list->items[i].signal);
        if(list->items[i].name != NULL) furi_string_free(list->items[i].name);
    }
    free(list->items);
    free(list);
}

bool ir_load(const char* path, IrSignalList* list) {
    InfraredErrorCode err;
    InfraredSignal* signal;
    FlipperFormat* ff;
    FuriString* filetype;
    FuriString* sig;
    Storage* storage;
    uint32_t version;
    bool header_ok;
    bool success;

    if(path == NULL || list == NULL) return false;

    storage = furi_record_open(RECORD_STORAGE);
    if(storage == NULL) return false;

    ff = flipper_format_file_alloc(storage);
    if(ff == NULL) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    success = false;
    filetype = NULL;
    sig = NULL;

    do {
        if(!flipper_format_file_open_existing(ff, path)) break;

        filetype = furi_string_alloc();
        if(filetype == NULL) break;

        header_ok = flipper_format_read_header(ff, filetype, &version);
        if(!header_ok) break;

        sig = furi_string_alloc();
        if(sig == NULL) break;

        for(;;) {
            signal = infrared_signal_alloc();
            if(signal == NULL) break;

            err = infrared_signal_read(signal, ff, sig);
            if(err == InfraredErrorCodeNone) {
                if(!ir_add_signal(list, signal, furi_string_get_cstr(sig))) {
                    infrared_signal_free(signal);
                    break;
                }
            } else {
                infrared_signal_free(signal);
                break;
            }
        }
        success = true;
    } while(false);

    if(filetype != NULL) furi_string_free(filetype);
    if(sig != NULL) furi_string_free(sig);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    return success;
}

void ir_tx(InfraredSignal* signal) {
    infrared_signal_transmit(signal);
}

bool ir_files(const char* dir_path, FuriString*** files, size_t* count) {
    FuriString** next_files;
    Storage* storage;
    FileInfo file_info;
    size_t capacity;
    char name_buf[256];
    File* dir;
    bool dir_open;
    bool ok;

    if(dir_path == NULL || files == NULL || count == NULL) return false;

    *files = NULL;
    *count = 0;
    capacity = 0;
    dir_open = false;
    ok = false;

    storage = furi_record_open(RECORD_STORAGE);
    if(storage == NULL) return false;

    dir = storage_file_alloc(storage);
    if(dir == NULL) goto done;

    if(!storage_dir_open(dir, dir_path)) goto done;
    dir_open = true;

    while(storage_dir_read(dir, &file_info, name_buf, sizeof(name_buf))) {
        if(!ir_is_file(&file_info, name_buf)) continue;

        if(*count >= capacity) {
            size_t new_capacity;

            new_capacity = capacity == 0 ? 8 : capacity * 2;
            next_files = realloc(*files, new_capacity * sizeof(FuriString*));
            if(next_files == NULL) {
                ir_files_free(*files, *count);
                *files = NULL;
                *count = 0;
                goto done;
            }
            *files = next_files;
            capacity = new_capacity;
        }

        (*files)[*count] = furi_string_alloc_set(name_buf);
        if((*files)[*count] == NULL) {
            ir_files_free(*files, *count);
            *files = NULL;
            *count = 0;
            goto done;
        }
        (*count)++;
    }

    ok = true;

done:
    if(dir != NULL) {
        if(dir_open) storage_dir_close(dir);
        storage_file_free(dir);
    }
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void ir_files_free(FuriString** files, size_t count) {
    size_t i;

    if(files == NULL) return;

    for(i = 0; i < count; i++) {
        if(files[i] != NULL) furi_string_free(files[i]);
    }
    free(files);
}

static bool ir_add_signal(IrSignalList* list, InfraredSignal* signal, const char* name) {
    IrSignalItem* items;
    size_t new_capacity;

    if(list->count >= list->capacity) {
        new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        items = realloc(list->items, new_capacity * sizeof(IrSignalItem));
        if(items == NULL) return false;
        list->items = items;
        list->capacity = new_capacity;
    }

    list->items[list->count].signal = signal;
    list->items[list->count].name = furi_string_alloc_set(name);
    if(list->items[list->count].name == NULL) return false;

    list->count++;
    return true;
}

static bool ir_is_file(const FileInfo* file_info, const char* name) {
    size_t len;

    if((file_info->flags & FSF_DIRECTORY) || name == NULL) return false;

    len = strlen(name);
    if(len <= 3) return false;

    return strcmp(name + len - 3, ".ir") == 0;
}
