#pragma once

#include <furi.h>

// Debug logging to SD card with automatic log rotation
// Logs are written to /ext/apps_data/flipper_wedge/debug.log
// When log reaches max size, oldest 50% is pruned to keep recent data

/** Initialize debug logging
 * Creates log file if needed, prunes if too large
 */
void flipper_wedge_debug_init(void);

/** Log a debug message to SD card
 * Thread-safe, includes timestamp and tag
 *
 * @param tag Tag/module name
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void flipper_wedge_debug_log(const char* tag, const char* format, ...);

/** Close debug logging and flush buffers
 */
void flipper_wedge_debug_close(void);
