#include <furi.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <lib/flipper_format/flipper_format.h>
#include <storage/storage.h>

struct fgp_storage {
    Storage* storage;
    File* file;
    FuriString* base_path;
    FuriString* file_name;
    FuriString* date;
    DateTime saved_date;
    uint32_t count;
};

static void fgp_build_path_and_dir(struct fgp_storage* storage) {
    /* Set up base file path */
    furi_string_set(storage->base_path, APP_DATA_PATH(""));
    storage_common_resolve_path_and_ensure_app_directory(storage->storage, storage->base_path);

    /* Get today's date */
    furi_hal_rtc_get_datetime(&storage->saved_date);

    /* Build directory name based on date */
    furi_string_printf(
        storage->date,
        "%d-%02d-%02d",
        storage->saved_date.year,
        storage->saved_date.month,
        storage->saved_date.day);
    furi_string_cat(storage->base_path, storage->date);
    /* TODO: Error checking ? */

    /* Make directory if it doesn't exist */
    /* TODO: Is there any way to speed this up? */
    /* Consider only making the dir on startup, and just using this func to
	 * rebuild the date used in the name?
	 */
    storage_simply_mkdir(storage->storage, furi_string_get_cstr(storage->base_path));
}

void fgp_storage_next_count(void* fgp_storage) {
    struct fgp_storage* storage = fgp_storage;
    DateTime cur_date = {0};
    FlipperFormat* format = NULL;
    FuriString* fs_tmp = furi_string_alloc();

    /* Save the current count to disk before incrementing.
	 * This is because at application load, the alloc() function here
	 * will get the count on disk, then add 1 to it.
	 */
    /* XXX: TODO: One issue with this is basically it thrashing the disk,
	 * rewriting this file all the time. Its _probably_ not an issue, but
	 * if it is, some future ideas would be to do something like:
	 * - At application start, check for file marker of application safely
	 *   shut down. If it exists, load count in to it, move on.
	 * - If it does not exist, then consider either looking for a folder of
	 *   today's date and checking the files names of it, or, walking the
	 *   data folder, find the latest date folder, and the highest count within
	 *   that. There really is not an issue with resetting the count if the
	 *   folder dated today does not exist.
	 * - At app shutdown, write the count back to disk alongside a safely exited
	 *   marker.
	 */
    storage->file = storage_file_alloc(storage->storage);

    furi_string_set(fs_tmp, APP_DATA_PATH(""));
    storage_common_resolve_path_and_ensure_app_directory(storage->storage, fs_tmp);
    furi_string_cat(fs_tmp, ".settings");

    format = flipper_format_file_alloc(storage->storage);
    if(!flipper_format_file_open_always(format, furi_string_get_cstr(fs_tmp))) {
        FURI_LOG_E("f ops", "failed to open settings file");
    }
    flipper_format_write_uint32(format, "Count", &storage->count, 1);
    flipper_format_file_close(format);

    /* Now it is safe to bump the count, roll over if necessary, and then
	 * check the date. If we somehow entered a new day, update the save file
	 * path.
	 *
	 * We do this specifically after a counter change because we don't want
	 * there to be issues with multi-image prints. The count should only change
	 * after a full image is received.
	 */
    storage->count++;
    if(storage->count > 9999) storage->count = 0;
    furi_hal_rtc_get_datetime(&cur_date);
    /* XXX: If this does rollover, it can take a lot of time to create the new
	 * directory. Is this worth doing? Maybe update the file string but not
	 * make the new dir?
	 */
    if(cur_date.day != storage->saved_date.day) fgp_build_path_and_dir(storage);
}

/* TODO: Add a tell() function... Why? */

/* True if file opened successfully */
bool fgp_storage_open(void* fgp_storage, const char* extension) {
    struct fgp_storage* storage = fgp_storage;
    bool ret = false;

    FuriString* fs_tmp = furi_string_alloc_printf(
        "%s/%s%s_%04d%s",
        furi_string_get_cstr(storage->base_path),
        furi_string_get_cstr(storage->file_name),
        furi_string_get_cstr(storage->date),
        (uint16_t)storage->count,
        extension);

    ret = storage_file_open(
        storage->file, furi_string_get_cstr(fs_tmp), FSAM_WRITE, FSOM_OPEN_APPEND);

    furi_string_free(fs_tmp);

    return ret;
}

size_t fgp_storage_write(void* fgp_storage, const void* buf, size_t len) {
    struct fgp_storage* storage = fgp_storage;
    return storage_file_write(storage->file, buf, len);
}

bool fgp_storage_close(void* fgp_storage) {
    struct fgp_storage* storage = fgp_storage;
    return storage_file_close(storage->file);
}

bool fgp_storage_seek(void* fgp_storage, off_t offs, bool from_start) {
    struct fgp_storage* storage = fgp_storage;

    if(from_start)
        return storage_file_seek(storage->file, (uint32_t)offs, true);
    else
        return storage_file_seek(
            storage->file, (uint32_t)(storage_file_tell(storage->file) + offs), true);
}

void fgp_storage_free(void* fgp_storage) {
    struct fgp_storage* storage = fgp_storage;

    /* Before closing all of our file handles, try and delete the directory
	 * we created earlier, if its empty it will remove, it if files were added
	 * to it, it will remain.
	 */
    storage_simply_remove(storage->storage, furi_string_get_cstr(storage->base_path));

    /* Close storage and file records */
    storage_file_free(storage->file);
    furi_record_close(RECORD_STORAGE);

    /* Close strings */
    furi_string_free(storage->base_path);
    furi_string_free(storage->file_name);
    furi_string_free(storage->date);

    free(storage);
}

/* Extension is used for making a full file */
void* fgp_storage_alloc(char* file_prefix, char* extension) {
    struct fgp_storage* storage = malloc(sizeof(struct fgp_storage));
    FuriString* fs_tmp = furi_string_alloc();
    FlipperFormat* format = NULL;
    UNUSED(extension);

    storage->storage = furi_record_open(RECORD_STORAGE);
    storage->file = storage_file_alloc(storage->storage);

    storage->file_name = furi_string_alloc_set(file_prefix);
    storage->base_path = furi_string_alloc();
    storage->date = furi_string_alloc();

    /* Get settings */
    furi_string_set(fs_tmp, APP_DATA_PATH(""));
    storage_common_resolve_path_and_ensure_app_directory(storage->storage, fs_tmp);
    furi_string_cat(fs_tmp, ".settings");

    format = flipper_format_file_alloc(storage->storage);
    if(!flipper_format_file_open_existing(format, furi_string_get_cstr(fs_tmp))) {
        FURI_LOG_I("f ops", "no settings file found, setting count to 0000");
        storage->count = 0;
    }
    flipper_format_read_uint32(format, "Count", &storage->count, 1);
    FURI_LOG_I("f ops", "read count: %ld", storage->count);
    storage->count++;
    flipper_format_file_close(format);

    fgp_build_path_and_dir(storage);

    furi_string_free(fs_tmp);

    return storage;
}
