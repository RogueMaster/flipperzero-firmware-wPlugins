#pragma once

#include "../rfs_types.h"

#define RFS_DIAG_ERROR_SLOTS 8
#define RFS_DIAG_ERROR_LEN   64

typedef struct RfsDiag RfsDiag;

RfsDiag* rfs_diag_alloc(Storage* storage);
void rfs_diag_free(RfsDiag* diag);

/** Enable or disable appending entries to the log file on the SD card. */
void rfs_diag_set_logging(RfsDiag* diag, bool enabled);

/** Record an error. Kept in memory for the Diagnostics screen and logged when enabled.
 *  Safe to call from any thread. */
void rfs_diag_error(RfsDiag* diag, const char* fmt, ...)
    _ATTRIBUTE((__format__(__printf__, 2, 3)));

/** Record an informational event (log file only). Safe to call from any thread. */
void rfs_diag_info(RfsDiag* diag, const char* fmt, ...) _ATTRIBUTE((__format__(__printf__, 2, 3)));

/** Append recent errors, newest first, to out. */
void rfs_diag_errors_to_string(RfsDiag* diag, FuriString* out);
