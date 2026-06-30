#pragma once

#include <flipper_format/flipper_format.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APDULogModeOpenExisting,
    APDULogModeOpenAlways,
} APDULogMode;

typedef struct APDULog APDULog;

/** Check if the file list exists
 *
 * @param path      - list path
 *
 * @return true if list exists, false otherwise
*/
bool apdu_log_check_presence(const char* path);

/** Open or create list
 * Depending on mode, list will be opened or created.
 *
 * @param path      - Path of the file that contain the list
 * @param mode      - ListKeysMode value
 *
 * @return Returns APDULog list instance
*/
APDULog* apdu_log_alloc(const char* path, APDULogMode mode);

/** Close list
 *
 * @param instance  - APDULog list instance
*/
void apdu_log_free(APDULog* instance);

/** Get total number of lines in list
 *
 * @param instance  - APDULog list instance
 *
 * @return Returns total number of lines in list
*/
size_t apdu_log_get_total_lines(APDULog* instance);

/** Rewind list
 *
 * @param instance  - APDULog list instance
 *
 * @return Returns true if rewind was successful, false otherwise
*/
bool apdu_log_rewind(APDULog* instance);

bool apdu_log_get_next_log_str(APDULog* instance, FuriString* log);

#ifdef __cplusplus
}
#endif
