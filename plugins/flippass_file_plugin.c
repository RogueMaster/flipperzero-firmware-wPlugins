#include "flippass_file_plugin.h"
#include "../kdbx/kdbx_header_scan.h"

#include <storage/storage.h>
#include <toolbox/path.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define FLIPPASS_FILE_NAME_SIZE  96U
#define FLIPPASS_FILE_LABEL_SIZE 64U

static bool flippass_file_name_has_kdbx_extension(const char* name) {
    const char* extension = NULL;

    if(name == NULL) {
        return false;
    }

    extension = strrchr(name, '.');
    if(extension == NULL || strlen(extension) != 5U) {
        return false;
    }

    return (tolower((unsigned char)extension[1]) == 'k') &&
           (tolower((unsigned char)extension[2]) == 'd') &&
           (tolower((unsigned char)extension[3]) == 'b') &&
           (tolower((unsigned char)extension[4]) == 'x') && extension[5] == '\0';
}

static bool flippass_file_validate_component(
    const char* text,
    FuriString* error,
    const char* required_message) {
    if(text == NULL || text[0] == '\0') {
        furi_string_set_str(error, required_message);
        return false;
    }

    for(const char* cursor = text; *cursor != '\0'; cursor++) {
        if(strchr("<>:\"/\\|?*", *cursor) != NULL) {
            furi_string_set_str(error, "Use a valid file name.");
            return false;
        }
    }

    return true;
}

static void flippass_file_compose_database_name(FuriString* out, const char* name) {
    furi_string_reset(out);
    if(name == NULL || name[0] == '\0') {
        return;
    }

    furi_string_set_str(out, name);
    if(!flippass_file_name_has_kdbx_extension(name)) {
        furi_string_cat_str(out, ".kdbx");
    }
}

static void flippass_file_copy_file_label(const char* name, char* out, size_t out_size) {
    const char* source = name != NULL ? name : "";
    size_t copy_size = strlen(source);

    furi_assert(out);
    furi_assert(out_size > 0U);

    if(copy_size > 5U && flippass_file_name_has_kdbx_extension(source)) {
        copy_size -= 5U;
    }
    if(copy_size >= out_size) {
        copy_size = out_size - 1U;
    }
    memcpy(out, source, copy_size);
    out[copy_size] = '\0';
}

static bool flippass_file_has_parent_directory(const char* directory) {
    return directory != NULL && directory[0] != '\0' &&
           strcmp(directory, STORAGE_EXT_PATH_PREFIX) != 0;
}

static void flippass_file_resolve_directory(
    const FlipPassFileListRequestV1* request,
    FuriString* out_directory) {
    furi_string_reset(out_directory);

    if(request->requested_directory != NULL && request->requested_directory[0] != '\0') {
        furi_string_set_str(out_directory, request->requested_directory);
        return;
    }

    if(request->fallback_file_path != NULL && request->fallback_file_path[0] != '\0') {
        path_extract_dirname(request->fallback_file_path, out_directory);
    }

    if(furi_string_empty(out_directory)) {
        furi_string_set_str(out_directory, request->root_path);
    }
}

static bool flippass_file_validate_list_request(
    const FlipPassFileListRequestV1* request,
    const FlipPassFileHostApiV1* host_api,
    FuriString* error) {
    if(request == NULL || host_api == NULL || error == NULL ||
       request->api_version != FLIPPASS_FILE_PLUGIN_API_VERSION ||
       host_api->api_version != FLIPPASS_FILE_HOST_API_VERSION || request->root_path == NULL ||
       request->resolved_directory == NULL || request->has_parent == NULL ||
       request->max_items < 2U || host_api->add_item == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass received an invalid file-plugin request.");
        }
        return false;
    }

    return true;
}

static void flippass_file_add_item(
    const FlipPassFileHostApiV1* host_api,
    FlipPassFilePluginItemType type,
    const char* label,
    const char* name) {
    host_api->add_item(
        host_api->context, type, label != NULL ? label : "", name != NULL ? name : "");
}

static bool flippass_file_list_directory(
    const FlipPassFileListRequestV1* request,
    const FlipPassFileHostApiV1* host_api,
    FuriString* error) {
    Storage* storage = NULL;
    File* directory = NULL;
    FileInfo info = {0};
    char name[FLIPPASS_FILE_NAME_SIZE];
    char label[FLIPPASS_FILE_LABEL_SIZE];
    size_t item_count = 0U;
    size_t content_count = 0U;
    bool ok = false;

    if(!flippass_file_validate_list_request(request, host_api, error)) {
        return false;
    }

    storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, request->root_path);
    flippass_file_resolve_directory(request, request->resolved_directory);

    directory = storage_file_alloc(storage);
    if(directory == NULL) {
        furi_string_set_str(error, "FlipPass could not allocate a directory handle.");
        goto cleanup;
    }

    if(!storage_dir_open(directory, furi_string_get_cstr(request->resolved_directory))) {
        furi_string_set_str(request->resolved_directory, request->root_path);
        if(!storage_dir_open(directory, furi_string_get_cstr(request->resolved_directory))) {
            furi_string_set_str(error, "FlipPass could not open the database directory.");
            goto cleanup;
        }
    }

    *request->has_parent =
        flippass_file_has_parent_directory(furi_string_get_cstr(request->resolved_directory));
    if(*request->has_parent && item_count < request->max_items) {
        flippass_file_add_item(host_api, FlipPassFilePluginItemUp, "..", "");
        item_count++;
    }

    memset(name, 0, sizeof(name));
    while(item_count < (request->max_items - 1U) &&
          storage_dir_read(directory, &info, name, sizeof(name))) {
        if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        if(file_info_is_dir(&info)) {
            flippass_file_add_item(host_api, FlipPassFilePluginItemDirectory, name, name);
            item_count++;
            content_count++;
        } else if(flippass_file_name_has_kdbx_extension(name)) {
            flippass_file_copy_file_label(name, label, sizeof(label));
            flippass_file_add_item(host_api, FlipPassFilePluginItemFile, label, name);
            item_count++;
            content_count++;
        }
    }

    if(content_count == 0U && item_count < (request->max_items - 1U)) {
        flippass_file_add_item(host_api, FlipPassFilePluginItemInfo, "No databases", "");
        item_count++;
    }

    flippass_file_add_item(host_api, FlipPassFilePluginItemNewObject, "New Object", "");
    ok = true;

cleanup:
    if(directory != NULL) {
        storage_dir_close(directory);
        storage_file_free(directory);
    }
    if(storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }
    return ok;
}

static bool flippass_file_delete_path(const char* path, FuriString* error) {
    Storage* storage = NULL;
    bool deleted = false;

    if(path == NULL || path[0] == '\0' || error == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "No database path was selected.");
        }
        return false;
    }

    storage = furi_record_open(RECORD_STORAGE);
    deleted = storage_simply_remove(storage, path);
    furi_record_close(RECORD_STORAGE);

    if(!deleted) {
        furi_string_set_str(error, "The selected database could not be deleted.");
    }

    return deleted;
}

static bool flippass_file_make_directory(
    const char* parent_directory,
    const char* directory_name,
    FuriString* out_path,
    FuriString* error) {
    Storage* storage = NULL;
    bool ok = false;

    if(parent_directory == NULL || parent_directory[0] == '\0' || out_path == NULL ||
       error == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "No parent directory was selected.");
        }
        return false;
    }

    if(!flippass_file_validate_component(directory_name, error, "Directory name is required.")) {
        return false;
    }

    path_concat(parent_directory, directory_name, out_path);
    storage = furi_record_open(RECORD_STORAGE);
    if(storage_common_stat(storage, furi_string_get_cstr(out_path), NULL) == FSE_OK) {
        furi_string_set_str(error, "A file or directory with that name already exists.");
        goto cleanup;
    }

    if(!storage_simply_mkdir(storage, furi_string_get_cstr(out_path))) {
        furi_string_set_str(error, "The directory could not be created.");
        goto cleanup;
    }

    ok = true;

cleanup:
    if(storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }
    if(!ok) {
        furi_string_reset(out_path);
    }
    return ok;
}

static bool flippass_file_rename_database(
    const char* source_path,
    const char* requested_name,
    FuriString* out_path,
    FuriString* error) {
    FuriString* file_name = furi_string_alloc();
    FuriString* dirname = furi_string_alloc();
    Storage* storage = NULL;
    bool ok = false;

    if(source_path == NULL || source_path[0] == '\0' || out_path == NULL || error == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "No database path was selected.");
        }
        goto cleanup;
    }

    if(!flippass_file_validate_component(
           requested_name, error, "A target file name is required.")) {
        goto cleanup;
    }

    flippass_file_compose_database_name(file_name, requested_name);
    path_extract_dirname(source_path, dirname);
    path_concat(furi_string_get_cstr(dirname), furi_string_get_cstr(file_name), out_path);

    if(strcmp(source_path, furi_string_get_cstr(out_path)) == 0) {
        ok = true;
        goto cleanup;
    }

    storage = furi_record_open(RECORD_STORAGE);
    if(storage_file_exists(storage, furi_string_get_cstr(out_path))) {
        furi_string_set_str(error, "A database with that name already exists.");
        goto cleanup;
    }

    if(storage_common_rename(storage, source_path, furi_string_get_cstr(out_path)) != FSE_OK) {
        furi_string_set_str(error, "The selected database could not be renamed.");
        goto cleanup;
    }

    ok = true;

cleanup:
    if(storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }
    if(!ok && out_path != NULL) {
        furi_string_reset(out_path);
    }
    furi_string_free(file_name);
    furi_string_free(dirname);
    return ok;
}

/* Streams the header through the Storage API: fields the check does not need are
 * skipped with seek, bounds are checked against the file size, nothing is buffered
 * beyond what kdbx_header_scan keeps on the stack. */
typedef struct {
    File* file;
    uint64_t size;
    uint64_t position;
} FlipPassFileHeaderSource;

static bool flippass_file_header_read(void* context, uint8_t* out, size_t size) {
    FlipPassFileHeaderSource* source = context;

    if(size > source->size - source->position) {
        return false;
    }
    const size_t read = storage_file_read(source->file, out, size);
    source->position += read;
    return read == size;
}

static bool flippass_file_header_skip(void* context, uint32_t size) {
    FlipPassFileHeaderSource* source = context;

    if(size > source->size - source->position ||
       !storage_file_seek(source->file, (uint32_t)(source->position + size), true)) {
        return false;
    }
    source->position += size;
    return true;
}

static bool flippass_file_header_rewind(void* context) {
    FlipPassFileHeaderSource* source = context;

    if(!storage_file_seek(source->file, 0U, true)) {
        return false;
    }
    source->position = 0U;
    return true;
}

static bool
    flippass_file_check_database(const char* path, uint32_t* compression, FuriString* error) {
    char message[160];
    FlipPassFileHeaderSource source = {0};
    const KDBXHeaderScanSource scan_source = {
        .context = &source,
        .read = flippass_file_header_read,
        .skip = flippass_file_header_skip,
        .rewind = flippass_file_header_rewind,
    };
    bool ok = false;

    if(path == NULL || path[0] == '\0' || error == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "No database path was selected.");
        }
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    source.file = storage_file_alloc(storage);
    if(!storage_file_open(source.file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        furi_string_set_str(error, "The database file could not be read.");
    } else {
        source.size = storage_file_size(source.file);
        ok =
            kdbx_header_scan_with_compression(&scan_source, compression, message, sizeof(message));
        if(!ok) {
            furi_string_set_str(error, message);
        }
        storage_file_close(source.file);
    }
    storage_file_free(source.file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static const FlipPassFilePluginV1 flippass_file_plugin = {
    .api_version = FLIPPASS_FILE_PLUGIN_API_VERSION,
    .list_directory = flippass_file_list_directory,
    .delete_path = flippass_file_delete_path,
    .make_directory = flippass_file_make_directory,
    .rename_database = flippass_file_rename_database,
    .check_database = flippass_file_check_database,
};

static const FlipperAppPluginDescriptor flippass_file_descriptor = {
    .appid = FLIPPASS_FILE_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_FILE_PLUGIN_API_VERSION,
    .entry_point = &flippass_file_plugin,
};

const FlipperAppPluginDescriptor* flippass_file_plugin_ep(void) {
    return &flippass_file_descriptor;
}
