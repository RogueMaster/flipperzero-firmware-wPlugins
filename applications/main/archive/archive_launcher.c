#include "helpers/archive_helpers_ext.h"
#include <loader/loader.h>
#include <storage/storage.h>

/* This bridge keeps the desktop's Archive shortcut and FuriString context. */
int32_t archive_launcher_app(void* context) {
    const FuriString* path = context;
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_start_with_gui_error(
        loader,
        ARCHIVE_BROWSER_FAP_PATH,
        path && !furi_string_empty(path) ? furi_string_get_cstr(path) : NULL);
    furi_record_close(RECORD_LOADER);
    return 0;
}

/* The resident bridge owns the mounted File until the launch queue finishes.
 * No FAP callbacks or code pointers survive the browser's unload. */
static File* archive_handoff_image;

void archive_handoff_disk_image(File* disk_image) {
    furi_check(disk_image);
    /* A chained browser can replace a previous image after unmounting it. */
    if(archive_handoff_image && archive_handoff_image != disk_image) {
        storage_file_free(archive_handoff_image);
    }
    archive_handoff_image = disk_image;
}

void archive_handoff_cleanup(void) {
    if(!archive_handoff_image) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    // Do not free the image if FatFs failed to detach its backing pointer.
    FS_Error error = storage_virtual_unmount(storage);
    if(error != FSE_OK && error != FSE_NOT_READY) {
        FURI_LOG_W("Archive", "Image unmount failed; deferring cleanup");
        furi_record_close(RECORD_STORAGE);
        return;
    }
    storage_virtual_quit(storage);
    storage_file_free(archive_handoff_image);
    archive_handoff_image = NULL;
    furi_record_close(RECORD_STORAGE);
}
