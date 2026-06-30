#include <mp_flipper_runtime.h>
#include <mp_flipper_halport.h>

#include <furi.h>
#include <storage/storage.h>

#include <py/mperrno.h>

#include "mp_flipper_context.h"

mp_flipper_import_stat_t mp_flipper_try_resolve_filesystem_path(FuriString* path) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    const char* path_str = furi_string_get_cstr(path);
    FuriString* _path = furi_string_alloc_printf("%s", path_str);

    mp_flipper_import_stat_t stat = MP_FLIPPER_IMPORT_STAT_FILE;

    FS_Error error;
    FileInfo info;

    do {
        // make path absolute
        if(!furi_string_start_with_str(_path, "/")) {
            furi_string_printf(_path, "%s/%s", mp_flipper_root_module_path, path_str);
        }

        // check if file or folder exists
        error = storage_common_stat(ctx->storage, furi_string_get_cstr(_path), &info);
        if(error == FSE_OK) {
            break;
        }

        // check for existing python file
        furi_string_cat_str(_path, ".py");

        error = storage_common_stat(ctx->storage, furi_string_get_cstr(_path), &info);
        if(error == FSE_OK) {
            break;
        }
    } while(false);

    // file or folder missing
    if(error == FSE_NOT_EXIST) {
        stat = MP_FLIPPER_IMPORT_STAT_NO_EXIST;
    }
    // abort on error
    else if(error != FSE_OK) {
        mp_flipper_raise_os_error_with_filename(MP_ENOENT, furi_string_get_cstr(path));
    }
    // path points to directory
    else if((info.flags & FSF_DIRECTORY) == FSF_DIRECTORY) {
        stat = MP_FLIPPER_IMPORT_STAT_DIR;
    }

    furi_string_move(path, _path);

    return stat;
}
