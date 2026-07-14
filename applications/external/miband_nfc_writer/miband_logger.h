/**
 * @file miband_logger.h
 * @brief Logging system with file export
 */

#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <datetime/datetime.h>

#define LOG_PATH        EXT_PATH("apps_data/miband_nfc/logs")
#define MAX_LOG_ENTRIES 50
#define MAX_LOG_SIZE    50000 // 50KB max per file

typedef enum {
    LogLevelDebug,
    LogLevelInfo,
    LogLevelWarning,
    LogLevelError,
} LogLevel;

typedef struct {
    DateTime timestamp;
    LogLevel level;
    char message[64];
} LogEntry;

typedef struct {
    LogEntry* entries;
    size_t count;
    size_t capacity;
    Storage* storage;
    bool enabled;
} MiBandLogger;

/**
 * @brief Create logger instance
 */
MiBandLogger* miband_logger_alloc(Storage* storage);

/**
 * @brief Free logger instance
 */
void miband_logger_free(MiBandLogger* logger);

/**
 * @brief Add log entry
 */
void miband_logger_log(MiBandLogger* logger, LogLevel level, const char* format, ...);

/**
 * @brief Export logs to file
 */
bool miband_logger_export(MiBandLogger* logger, const char* filename);

/**
 * @brief Clear all logs
 */
void miband_logger_clear(MiBandLogger* logger);

/**
 * @brief Get log count
 */
size_t miband_logger_get_count(MiBandLogger* logger);

/**
 * @brief Enable/disable logging
 */
void miband_logger_set_enabled(MiBandLogger* logger, bool enabled);
