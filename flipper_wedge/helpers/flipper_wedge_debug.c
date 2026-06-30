#include "flipper_wedge_debug.h"
#include <storage/storage.h>
#include <stdarg.h>

#define DEBUG_LOG_PATH APP_DATA_PATH("debug.log")
#define DEBUG_LOG_MAX_SIZE (50 * 1024)  // 50KB max log size
#define DEBUG_LOG_KEEP_SIZE (25 * 1024)  // Keep most recent 25KB when rotating

static Storage* debug_storage = NULL;
static File* debug_file = NULL;
static FuriMutex* debug_mutex = NULL;

// Rotate log file by keeping only the most recent portion
static void debug_rotate_log(void) {
    FileInfo file_info;
    if(storage_common_stat(debug_storage, DEBUG_LOG_PATH, &file_info) != FSE_OK) {
        return;  // File doesn't exist, nothing to rotate
    }

    if(file_info.size <= DEBUG_LOG_MAX_SIZE) {
        return;  // File not too large yet
    }

    // Read the last DEBUG_LOG_KEEP_SIZE bytes
    File* read_file = storage_file_alloc(debug_storage);
    if(!storage_file_open(read_file, DEBUG_LOG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(read_file);
        return;
    }

    // Calculate offset to start reading from
    uint64_t offset = file_info.size - DEBUG_LOG_KEEP_SIZE;
    storage_file_seek(read_file, offset, true);

    // Read the tail
    uint8_t* buffer = malloc(DEBUG_LOG_KEEP_SIZE);
    uint16_t bytes_read = storage_file_read(read_file, buffer, DEBUG_LOG_KEEP_SIZE);
    storage_file_close(read_file);
    storage_file_free(read_file);

    // Rewrite file with just the tail
    File* write_file = storage_file_alloc(debug_storage);
    if(storage_file_open(write_file, DEBUG_LOG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Add rotation marker
        const char* marker = "=== LOG ROTATED ===\n";
        storage_file_write(write_file, marker, strlen(marker));

        // Write the tail
        storage_file_write(write_file, buffer, bytes_read);
        storage_file_sync(write_file);
    }
    storage_file_close(write_file);
    storage_file_free(write_file);

    free(buffer);
}

void flipper_wedge_debug_init(void) {
    // Allocate mutex for thread-safe logging
    if(!debug_mutex) {
        debug_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    }

    furi_mutex_acquire(debug_mutex, FuriWaitForever);

    // Open storage
    debug_storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_common_mkdir(debug_storage, APP_DATA_PATH(""));

    // Rotate log if too large
    debug_rotate_log();

    // Open log file for append
    debug_file = storage_file_alloc(debug_storage);
    if(!storage_file_open(debug_file, DEBUG_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        // Failed to open, try creating new file
        storage_file_close(debug_file);
        storage_file_open(debug_file, DEBUG_LOG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    }

    // Write session start marker
    const char* marker = "\n=== DEBUG SESSION START ===\n";
    storage_file_write(debug_file, marker, strlen(marker));
    storage_file_sync(debug_file);

    furi_mutex_release(debug_mutex);
}

void flipper_wedge_debug_log(const char* tag, const char* format, ...) {
    if(!debug_file || !debug_storage || !debug_mutex) {
        return;  // Not initialized
    }

    furi_mutex_acquire(debug_mutex, FuriWaitForever);

    // Get tick count for timestamp (milliseconds since boot)
    uint32_t ticks = furi_get_tick();
    uint32_t ms = ticks * 1000 / furi_kernel_get_tick_frequency();

    // Format timestamp [MM:SS.mmm]
    uint32_t seconds = ms / 1000;
    uint32_t millis = ms % 1000;
    uint32_t minutes = seconds / 60;
    seconds = seconds % 60;

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "[%02lu:%02lu.%03lu]", minutes, seconds, millis);

    // Write timestamp and tag
    storage_file_write(debug_file, timestamp, strlen(timestamp));
    storage_file_write(debug_file, " ", 1);
    storage_file_write(debug_file, tag, strlen(tag));
    storage_file_write(debug_file, ": ", 2);

    // Format and write message
    va_list args;
    va_start(args, format);
    char message[256];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    storage_file_write(debug_file, message, strlen(message));
    storage_file_write(debug_file, "\n", 1);

    // Sync to ensure data is written
    storage_file_sync(debug_file);

    furi_mutex_release(debug_mutex);
}

void flipper_wedge_debug_close(void) {
    if(!debug_mutex) return;

    furi_mutex_acquire(debug_mutex, FuriWaitForever);

    if(debug_file) {
        const char* marker = "=== DEBUG SESSION END ===\n\n";
        storage_file_write(debug_file, marker, strlen(marker));
        storage_file_sync(debug_file);
        storage_file_close(debug_file);
        storage_file_free(debug_file);
        debug_file = NULL;
    }

    if(debug_storage) {
        furi_record_close(RECORD_STORAGE);
        debug_storage = NULL;
    }

    furi_mutex_release(debug_mutex);
    furi_mutex_free(debug_mutex);
    debug_mutex = NULL;
}
