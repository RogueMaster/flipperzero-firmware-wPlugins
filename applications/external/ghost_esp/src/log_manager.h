#pragma once

#include <storage/storage.h>
#include <stdbool.h>

#define MAX_LOGS_TO_KEEP 5
#define MAX_FILENAME_LEN 256

/**
 * @brief Get the path to the latest log file
 * 
 * @param storage Storage instance
 * @param dir Directory to search in
 * @param prefix Prefix of log files to search for
 * @param out_path Buffer to store the resulting path
 * @return true if a log file was found, false otherwise
 */
bool get_latest_log_file(Storage* storage, const char* dir, const char* prefix, char* out_path);
