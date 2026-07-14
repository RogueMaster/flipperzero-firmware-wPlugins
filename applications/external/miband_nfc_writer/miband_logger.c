/**
 * @file miband_logger.c
 * @brief Logging implementation
 */

#include "miband_logger.h"
#include <furi_hal_rtc.h>
#include <stdarg.h>
#include <stdio.h>

#define TAG "MiBandLogger"

MiBandLogger* miband_logger_alloc(Storage* storage) {
    if(!storage) {
        FURI_LOG_E(TAG, "Storage is NULL!");
        return NULL;
    }

    MiBandLogger* logger = malloc(sizeof(MiBandLogger));
    if(!logger) {
        FURI_LOG_E(TAG, "Failed to allocate logger");
        return NULL;
    }

    logger->capacity = MAX_LOG_ENTRIES;
    logger->entries = malloc(sizeof(LogEntry) * logger->capacity);
    if(!logger->entries) {
        FURI_LOG_E(TAG, "Failed to allocate log entries");
        free(logger);
        return NULL;
    }

    logger->count = 0;
    logger->storage = storage;
    logger->enabled = true;

    // SAFE directory creation con check
    FS_Error err;

    err = storage_common_mkdir(storage, EXT_PATH("apps_data"));
    if(err != FSE_OK && err != FSE_EXIST) {
        FURI_LOG_W(TAG, "Failed to create apps_data: %d", err);
    }

    err = storage_common_mkdir(storage, EXT_PATH("apps_data/miband_nfc"));
    if(err != FSE_OK && err != FSE_EXIST) {
        FURI_LOG_W(TAG, "Failed to create miband_nfc dir: %d", err);
    }

    err = storage_common_mkdir(storage, LOG_PATH);
    if(err != FSE_OK && err != FSE_EXIST) {
        FURI_LOG_W(TAG, "Failed to create log dir: %d", err);
    }

    FURI_LOG_I(TAG, "Logger initialized");
    return logger;
}

void miband_logger_free(MiBandLogger* logger) {
    if(logger) {
        if(logger->entries) {
            free(logger->entries);
        }
        free(logger);
    }
}

void miband_logger_log(MiBandLogger* logger, LogLevel level, const char* format, ...) {
    if(!logger || !logger->enabled || !logger->entries || !logger->storage) return;

    // Rotate if full
    if(logger->count >= logger->capacity) {
        // Remove oldest 25% entries
        size_t keep = logger->capacity * 3 / 4;
        memmove(
            logger->entries, logger->entries + (logger->count - keep), sizeof(LogEntry) * keep);
        logger->count = keep;
    }

    LogEntry* entry = &logger->entries[logger->count++];

    // Get timestamp
    furi_hal_rtc_get_datetime(&entry->timestamp);
    entry->level = level;

    // Format message
    va_list args;
    va_start(args, format);
    vsnprintf(entry->message, sizeof(entry->message), format, args);
    va_end(args);

    // Also log to FURI
    switch(level) {
    case LogLevelDebug:
        FURI_LOG_D(TAG, "%s", entry->message);
        break;
    case LogLevelInfo:
        FURI_LOG_I(TAG, "%s", entry->message);
        break;
    case LogLevelWarning:
        FURI_LOG_W(TAG, "%s", entry->message);
        break;
    case LogLevelError:
        FURI_LOG_E(TAG, "%s", entry->message);
        break;
    }
}

bool miband_logger_export(MiBandLogger* logger, const char* filename) {
    if(!logger || logger->count == 0) return false;

    FuriString* filepath = furi_string_alloc_printf("%s/%s", LOG_PATH, filename);
    File* file = storage_file_alloc(logger->storage);
    bool success = false;

    if(storage_file_open(file, furi_string_get_cstr(filepath), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Write header
        const char* header = "Mi Band NFC Writer - Log Export\n"
                             "================================\n\n";
        storage_file_write(file, header, strlen(header));

        // FIX: Reuse a single FuriString instead of alloc/free per entry
        FuriString* line = furi_string_alloc();
        for(size_t i = 0; i < logger->count; i++) {
            LogEntry* entry = &logger->entries[i];

            const char* level_str;
            switch(entry->level) {
            case LogLevelDebug:
                level_str = "DEBUG";
                break;
            case LogLevelInfo:
                level_str = "INFO ";
                break;
            case LogLevelWarning:
                level_str = "WARN ";
                break;
            case LogLevelError:
                level_str = "ERROR";
                break;
            default:
                level_str = "?????";
                break;
            }

            furi_string_printf(
                line,
                "%04d-%02d-%02d %02d:%02d:%02d [%s] %s\n",
                entry->timestamp.year,
                entry->timestamp.month,
                entry->timestamp.day,
                entry->timestamp.hour,
                entry->timestamp.minute,
                entry->timestamp.second,
                level_str,
                entry->message);

            storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
        }
        furi_string_free(line);

        storage_file_close(file);
        success = true;
        FURI_LOG_I(TAG, "Exported %zu log entries to %s", logger->count, filename);
    }

    storage_file_free(file);
    furi_string_free(filepath);
    return success;
}

void miband_logger_clear(MiBandLogger* logger) {
    if(logger) {
        logger->count = 0;
    }
}

size_t miband_logger_get_count(MiBandLogger* logger) {
    return logger ? logger->count : 0;
}

void miband_logger_set_enabled(MiBandLogger* logger, bool enabled) {
    if(logger) {
        logger->enabled = enabled;
    }
}
