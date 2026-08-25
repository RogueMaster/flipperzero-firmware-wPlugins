#pragma once

#include <furi.h>

#define LOGGER_FILENAME "debug.log"
#define LOGGER_MAX_SIZE (32 * 1024)
#define LOGGER_TAG      "ApiCaller"

/** Resolve the log path and make sure the app data dir exists (call once). */
void logger_init(void);

/** Append a line to the app debug log with an uptime timestamp. */
void logger_log(const char* message);
