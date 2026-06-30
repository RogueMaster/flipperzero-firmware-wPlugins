#pragma once

#include <furi.h>

// User-facing scan logging to SD card
// Logs scanned UIDs and NDEF data when enabled via settings
// Logs are written to /ext/apps_data/flipper_wedge/scan_log.txt

/** Log a scanned tag to SD card
 * Thread-safe, includes timestamp and formatted data
 *
 * @param data The formatted scan data (UID or NDEF text)
 */
void flipper_wedge_log_scan(const char* data);

/** Clean up logging resources
 * Should be called on app exit to free the mutex
 */
void flipper_wedge_log_close(void);
