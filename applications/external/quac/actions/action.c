
#include "quac.h"
#include "item.h"
#include "action_i.h"

void action_ql_resolve(
    void* context,
    const FuriString* action_path,
    FuriString* new_path,
    FuriString* error) {
    UNUSED(error);
    App* app = (App*)context;
    File* file_link = storage_file_alloc(app->storage);
    do {
        if(!storage_file_open(
               file_link, furi_string_get_cstr(action_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            ACTION_SET_ERROR(
                "Quac Link: Failed to open file %s", furi_string_get_cstr(action_path));
            break;
        }

        furi_string_reset(new_path);
        const size_t file_size = storage_file_size(file_link);
        size_t bytes_read = 0;
        while(bytes_read < file_size) {
            char buffer[65] = {0};
            const size_t chunk_size = MIN(file_size - bytes_read, sizeof(buffer) - 1);
            size_t chunk_read = storage_file_read(file_link, buffer, chunk_size);
            if(chunk_read != chunk_size) break;
            bytes_read += chunk_read;
            furi_string_cat_str(new_path, buffer);
        }
        furi_string_trim(new_path);

        if(bytes_read != file_size) {
            ACTION_SET_ERROR(
                "Quac Link: Error reading link file %s", furi_string_get_cstr(action_path));
            break;
        }
        if(!storage_file_exists(app->storage, furi_string_get_cstr(new_path))) {
            ACTION_SET_ERROR(
                "Quac Link: Linked file does not exist! %s", furi_string_get_cstr(new_path));
            break;
        }
    } while(false);
    storage_file_close(file_link);
    storage_file_free(file_link);
}

void action_tx(void* context, Item* item, FuriString* error) {
    // FURI_LOG_I(TAG, "action_run: %s : %s", furi_string_get_cstr(item->name), item->ext);

    FuriString* path = furi_string_alloc_set(item->path);
    if(item->is_link) {
        // This is a Quac link, open the file and pull the real filename
        action_ql_resolve(context, item->path, path, error);
        if(furi_string_size(error)) {
            furi_string_free(path);
            return;
        }
        FURI_LOG_I(TAG, "Resolved Quac link file to: %s", furi_string_get_cstr(path));
    }

    if(!strcmp(item->ext, ".sub")) {
        action_subghz_tx(context, path, error);
    } else if(!strcmp(item->ext, ".ir")) {
        action_ir_tx(context, path, error);
    } else if(!strcmp(item->ext, ".rfid")) {
        action_rfid_tx(context, path, error);
    } else if(!strcmp(item->ext, ".nfc")) {
        action_nfc_tx(context, path, error);
    } else if(!strcmp(item->ext, ".ibtn")) {
        action_ibutton_tx(context, path, error);
    } else if(!strcmp(item->ext, ".qpl")) {
        action_qpl_tx(context, path, error);
    } else {
        FURI_LOG_E(TAG, "Unknown item file type! %s", item->ext);
    }
    furi_string_free(path);
}
