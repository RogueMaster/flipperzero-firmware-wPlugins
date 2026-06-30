#include "flipper_wedge_log.h"
#include <storage/storage.h>
#include <furi_hal_rtc.h>

#define SCAN_LOG_PATH APP_DATA_PATH("scan_log.txt")
#define SCAN_LOG_MAX_SIZE (200 * 1024)  // 200KB max log size
#define SCAN_LOG_KEEP_SIZE (100 * 1024)  // Keep most recent 100KB when rotating

static FuriMutex* log_mutex = NULL;

// Rotate log file by keeping only the most recent portion
static void log_rotate(Storage* storage) {
    FileInfo file_info;
    if(storage_common_stat(storage, SCAN_LOG_PATH, &file_info) != FSE_OK) {
        return;  // File doesn't exist, nothing to rotate
    }

    if(file_info.size <= SCAN_LOG_MAX_SIZE) {
        return;  // File not too large yet
    }

    // Read the last SCAN_LOG_KEEP_SIZE bytes
    File* read_file = storage_file_alloc(storage);
    if(!storage_file_open(read_file, SCAN_LOG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(read_file);
        return;
    }

    // Calculate offset to start reading from
    uint64_t offset = file_info.size - SCAN_LOG_KEEP_SIZE;
    storage_file_seek(read_file, offset, true);

    // Read the tail
    uint8_t* buffer = malloc(SCAN_LOG_KEEP_SIZE);
    uint16_t bytes_read = storage_file_read(read_file, buffer, SCAN_LOG_KEEP_SIZE);
    storage_file_close(read_file);
    storage_file_free(read_file);

    // Rewrite file with just the tail
    File* write_file = storage_file_alloc(storage);
    if(storage_file_open(write_file, SCAN_LOG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
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

void flipper_wedge_log_scan(const char* data) {
    if(!data) return;

    // Allocate mutex on first use
    if(!log_mutex) {
        log_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    }

    furi_mutex_acquire(log_mutex, FuriWaitForever);

    // Open storage
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    // Rotate log if too large
    log_rotate(storage);

    // Open log file for append
    File* log_file = storage_file_alloc(storage);
    bool opened = storage_file_open(log_file, SCAN_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(!opened) {
        // Failed to open, try creating new file
        storage_file_close(log_file);
        opened = storage_file_open(log_file, SCAN_LOG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    }

    if(opened) {
        // Get current date/time
        DateTime datetime;
        furi_hal_rtc_get_datetime(&datetime);

        // Format timestamp: YYYY-MM-DD HH:MM:SS
        char timestamp[32];
        snprintf(timestamp, sizeof(timestamp),
                 "%04d-%02d-%02d %02d:%02d:%02d",
                 datetime.year, datetime.month, datetime.day,
                 datetime.hour, datetime.minute, datetime.second);

        // Write log entry: [timestamp] data\n
        storage_file_write(log_file, "[", 1);
        storage_file_write(log_file, timestamp, strlen(timestamp));
        storage_file_write(log_file, "] ", 2);
        storage_file_write(log_file, data, strlen(data));
        storage_file_write(log_file, "\n", 1);

        // Sync to ensure data is written
        storage_file_sync(log_file);
        storage_file_close(log_file);
    }

    storage_file_free(log_file);
    furi_record_close(RECORD_STORAGE);

    furi_mutex_release(log_mutex);
}

void flipper_wedge_log_close(void) {
    if(!log_mutex) return;

    furi_mutex_acquire(log_mutex, FuriWaitForever);
    furi_mutex_release(log_mutex);
    furi_mutex_free(log_mutex);
    log_mutex = NULL;
}
