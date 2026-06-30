#include "uart_utils.h"
#include "log_manager.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>
#include "sequential_file.h"

#define COMMAND_BUFFER_SIZE   128
#define PCAP_WRITE_CHUNK_SIZE 1024u

// Define directories in an array for loop-based creation
static const char* GHOST_DIRECTORIES[] = {
    GHOST_ESP_APP_FOLDER,
    GHOST_ESP_APP_FOLDER_PCAPS,
    GHOST_ESP_APP_FOLDER_LOGS,
    GHOST_ESP_APP_FOLDER_WARDRIVE};

// Helper macro for error handling and cleanup
#define CLEANUP_AND_RETURN(ctx, msg, retval) \
    do {                                     \
        FURI_LOG_E("Storage", msg);          \
        if(ctx) {                            \
            uart_storage_free(ctx);          \
        }                                    \
        return retval;                       \
    } while(0)

void uart_storage_safe_cleanup(UartStorageContext* ctx) {
    if(!ctx) return;

    // Safely close current file if open
    if(ctx->current_file) {
        if(storage_file_is_open(ctx->current_file)) {
            storage_file_sync(ctx->current_file);
            storage_file_close(ctx->current_file);
        }
        ctx->HasOpenedFile = false;
    }

    // Safely close log file if open
    if(ctx->log_file) {
        if(storage_file_is_open(ctx->log_file)) {
            storage_file_sync(ctx->log_file);
            storage_file_close(ctx->log_file);
        }
    }
}

UartStorageContext* uart_storage_init(UartContext* parentContext) {
    uint32_t func_start_time = furi_get_tick();
    FURI_LOG_D("Storage", "Starting storage initialization");

    uint32_t step_start = furi_get_tick();
    // Allocate and initialize context
    UartStorageContext* ctx = malloc(sizeof(UartStorageContext));
    uint32_t elapsed_step = furi_get_tick() - step_start;
    if(!ctx) {
        FURI_LOG_E(
            "Storage", "Failed to allocate UartStorageContext (Time taken: %lu ms)", elapsed_step);
        return NULL;
    }
    FURI_LOG_I("Storage", "Allocated UartStorageContext (Time taken: %lu ms)", elapsed_step);

    // Initialize context fields
    step_start = furi_get_tick();
    memset(ctx, 0, sizeof(UartStorageContext));
    ctx->parentContext = parentContext;
    elapsed_step = furi_get_tick() - step_start;
    FURI_LOG_I(
        "Storage", "Initialized UartStorageContext fields (Time taken: %lu ms)", elapsed_step);

    // Open storage API
    step_start = furi_get_tick();
    ctx->storage_api = furi_record_open(RECORD_STORAGE);
    elapsed_step = furi_get_tick() - step_start;
    if(!ctx->storage_api) {
        FURI_LOG_E("Storage", "Failed to open RECORD_STORAGE (Time taken: %lu ms)", elapsed_step);
        free(ctx);
        return NULL;
    }
    FURI_LOG_I("Storage", "Opened RECORD_STORAGE (Time taken: %lu ms)", elapsed_step);

    // Allocate file handles
    step_start = furi_get_tick();
    ctx->current_file = storage_file_alloc(ctx->storage_api);
    ctx->log_file = storage_file_alloc(ctx->storage_api);
    elapsed_step = furi_get_tick() - step_start;
    if(!ctx->current_file || !ctx->log_file) {
        FURI_LOG_E(
            "Storage", "Failed to allocate file handles (Time taken: %lu ms)", elapsed_step);
        uart_storage_free(ctx);
        return NULL;
    }
    FURI_LOG_I(
        "Storage", "Allocated current_file and log_file (Time taken: %lu ms)", elapsed_step);

    // Create directories
    step_start = furi_get_tick();
    size_t num_directories = sizeof(GHOST_DIRECTORIES) / sizeof(GHOST_DIRECTORIES[0]);
    for(size_t i = 0; i < num_directories; i++) {
        uint32_t dir_step_start = furi_get_tick();
        if(!storage_simply_mkdir(ctx->storage_api, GHOST_DIRECTORIES[i])) {
            FURI_LOG_W(
                "Storage",
                "Failed to create or confirm directory: %s (Time taken: %lu ms)",
                GHOST_DIRECTORIES[i],
                furi_get_tick() - dir_step_start);
        } else {
            FURI_LOG_I(
                "Storage",
                "Created/Confirmed directory: %s (Time taken: %lu ms)",
                GHOST_DIRECTORIES[i],
                furi_get_tick() - dir_step_start);
        }
    }
    elapsed_step = furi_get_tick() - step_start;
    FURI_LOG_I(
        "Storage", "Directory creation/confirmation completed (Time taken: %lu ms)", elapsed_step);

    // Ensure all directories are accessible
    step_start = furi_get_tick();
    bool dirs_ok = true;
    if(!storage_dir_exists(ctx->storage_api, GHOST_ESP_APP_FOLDER_PCAPS)) {
        FURI_LOG_E("Storage", "PCAP directory not accessible");
        dirs_ok = false;
    }
    if(!storage_dir_exists(ctx->storage_api, GHOST_ESP_APP_FOLDER_LOGS)) {
        FURI_LOG_E("Storage", "Logs directory not accessible");
        dirs_ok = false;
    }
    if(!storage_dir_exists(ctx->storage_api, GHOST_ESP_APP_FOLDER_WARDRIVE)) {
        FURI_LOG_E("Storage", "Wardrive directory not accessible");
        dirs_ok = false;
    }
    elapsed_step = furi_get_tick() - step_start;
    FURI_LOG_I("Storage", "Checked directory accessibility (Time taken: %lu ms)", elapsed_step);

    if(!dirs_ok) {
        step_start = furi_get_tick();
        FURI_LOG_E("Storage", "Critical directories missing, attempting cleanup");
        uart_storage_safe_cleanup(ctx);
        // Retry directory creation
        for(size_t i = 0; i < num_directories; i++) {
            uint32_t retry_dir_start = furi_get_tick();
            storage_simply_mkdir(ctx->storage_api, GHOST_DIRECTORIES[i]);
            FURI_LOG_I(
                "Storage",
                "Retried creating directory: %s (Time taken: %lu ms)",
                GHOST_DIRECTORIES[i],
                furi_get_tick() - retry_dir_start);
        }
        elapsed_step = furi_get_tick() - step_start;
        FURI_LOG_I(
            "Storage", "Retry directory creation completed (Time taken: %lu ms)", elapsed_step);
    }

    // Initialize log file
    step_start = furi_get_tick();
    ctx->HasOpenedFile = sequential_file_open(
        ctx->storage_api, ctx->log_file, GHOST_ESP_APP_FOLDER_LOGS, "ghost_logs", "txt");
    elapsed_step = furi_get_tick() - step_start;
    if(!ctx->HasOpenedFile) {
        FURI_LOG_W(
            "Storage",
            "Failed to open log file, attempting cleanup (Time taken: %lu ms)",
            elapsed_step);
        uart_storage_safe_cleanup(ctx);
        // Retry log file creation
        step_start = furi_get_tick();
        ctx->HasOpenedFile = sequential_file_open(
            ctx->storage_api, ctx->log_file, GHOST_ESP_APP_FOLDER_LOGS, "ghost_logs", "txt");
        elapsed_step = furi_get_tick() - step_start;
        if(!ctx->HasOpenedFile) {
            FURI_LOG_W(
                "Storage",
                "Log init failed after cleanup, continuing without logging (Time taken: %lu ms)",
                elapsed_step);
        } else {
            FURI_LOG_I(
                "Storage",
                "Successfully opened log file after retry (Time taken: %lu ms)",
                elapsed_step);
        }
    } else {
        FURI_LOG_I("Storage", "Opened log file successfully (Time taken: %lu ms)", elapsed_step);
    }

    step_start = furi_get_tick();
    ctx->IsWritingToFile = false;
    elapsed_step = furi_get_tick() - step_start;
    FURI_LOG_I("Storage", "Initialized IsWritingToFile flag (Time taken: %lu ms)", elapsed_step);

    uint32_t func_elapsed = furi_get_tick() - func_start_time;
    FURI_LOG_I("Storage", "Storage initialization completed in %lu ms", func_elapsed);
    return ctx;
}

void uart_storage_rx_callback(uint8_t* buf, size_t len, void* context) {
    UartContext* app = (UartContext*)context;

    // Basic sanity checks with detailed logging
    if(!app || !app->storageContext || !buf || len == 0) {
        FURI_LOG_E(
            "Storage",
            "Invalid parameters in storage callback: app=%p, storageContext=%p, buf=%p, len=%zu",
            (void*)app,
            app ? (void*)app->storageContext : NULL,
            (void*)buf,
            len);
        return;
    }

    // **Ensure PCAP File is Open**
    if(!app->storageContext->current_file || !app->storageContext->HasOpenedFile) {
        FURI_LOG_E("Storage", "PCAP file is not open. Data cannot be written.");
        return;
    }

    FURI_LOG_D("Storage", "Received %zu bytes for PCAP write", len);

    // Log the first few bytes for debugging
    size_t bytes_to_log = (len < 16) ? len : 16;
    FURI_LOG_D("Storage", "First %zu bytes of buffer:", bytes_to_log);
    for(size_t i = 0; i < bytes_to_log; i++) {
        FURI_LOG_D("Storage", "Byte %zu: 0x%02X", i, buf[i]);
    }
    if(len > bytes_to_log) {
        FURI_LOG_D("Storage", "... (%zu more bytes)", len - bytes_to_log);
    }

    // Write data and verify with detailed logging
    size_t written = storage_file_write(app->storageContext->current_file, buf, len);
    if(written != len) {
        FURI_LOG_E(
            "Storage", "Failed to write PCAP data: Expected %zu, Written %zu", len, written);
        app->pcap = false; // Reset PCAP state on write failure
        return;
    }

    FURI_LOG_D("Storage", "Successfully wrote %zu bytes to PCAP file", written);

    // Optionally, calculate and log a checksum for data integrity
    uint8_t checksum = 0;
    for(size_t i = 0; i < len; i++) {
        checksum ^= buf[i];
    }
    FURI_LOG_D("Storage", "Data Checksum (XOR): 0x%02X", checksum);

    // Periodic sync every ~8KB to balance between safety and performance
    static size_t bytes_since_sync = 0;

    // Reset counter if this is the first write to a new file session
    if(bytes_since_sync > 8192 && app->storageContext->HasOpenedFile) {
        bytes_since_sync = 0;
    }

    bytes_since_sync += len;
    FURI_LOG_D("Storage", "Accumulated %zu bytes since last sync", bytes_since_sync);
    if(bytes_since_sync >= 8192) {
        storage_file_sync(app->storageContext->current_file);
        FURI_LOG_D("Storage", "PCAP file synced to storage");
        bytes_since_sync = 0;
    }
}

void uart_storage_reset_logs(UartStorageContext* ctx) {
    if(!ctx || !ctx->storage_api) return;

    if(ctx->log_file) {
        storage_file_close(ctx->log_file);
        storage_file_free(ctx->log_file);
        ctx->log_file = storage_file_alloc(ctx->storage_api);
    }

    ctx->HasOpenedFile = sequential_file_open(
        ctx->storage_api, ctx->log_file, GHOST_ESP_APP_FOLDER_LOGS, "ghost_logs", "txt");

    if(!ctx->HasOpenedFile) {
        FURI_LOG_E("Storage", "Failed to reset log file");
    }
}

void uart_storage_free(UartStorageContext* ctx) {
    if(!ctx) return;

    // Do safe cleanup first
    FURI_LOG_I("Storage", "Safe cleanup: closing open files");
    uart_storage_safe_cleanup(ctx);

    // Free file handles
    FURI_LOG_I("Storage", "Freeing file handles");
    if(ctx->current_file) {
        storage_file_free(ctx->current_file);
        ctx->current_file = NULL;
    }

    if(ctx->log_file) {
        storage_file_free(ctx->log_file);
        ctx->log_file = NULL;
    }

    // Close storage record at the very end
    if(ctx->storage_api) {
        FURI_LOG_I("Storage", "Closing RECORD_STORAGE");
        furi_record_close(RECORD_STORAGE);
        ctx->storage_api = NULL;
    }

    FURI_LOG_I("Storage", "Freeing storage context memory");
    free(ctx);
    FURI_LOG_I("Storage", "Storage context freed");
}
