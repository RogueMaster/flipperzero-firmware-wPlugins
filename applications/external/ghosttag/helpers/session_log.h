#pragma once

#include <furi.h>
#include "tracker_db.h"

/**
 * Append-only session log on the SD card.
 *
 * A hunt is something you do while walking, which is exactly when you are least
 * able to read a 128x64 screen. The log is what lets you sit down afterwards
 * and see what the app actually saw.
 *
 * Two rules it does not bend:
 *  - It is CAPPED. A log written by a long-running mode that appends forever
 *    fills the SD card and takes every other app's storage down with it. When
 *    the cap is hit the log says so, distinctly from having failed, because the
 *    two have different fixes.
 *  - Nothing from a DEMO session is ever written. A file full of invented
 *    trackers that reads exactly like a real one is the single worst thing this
 *    app could produce.
 */

#define GHOSTTAG_LOG_PATH     EXT_PATH("apps_data/ghosttag/sessions.csv")
#define GHOSTTAG_LOG_MAX_SIZE (64UL * 1024UL)

typedef enum {
    SessionLogOk,
    SessionLogFull, /* hit the cap - previous entries are intact */
    SessionLogFailed, /* no SD card, or the write errored */
} SessionLogStatus;

typedef struct SessionLog SessionLog;

SessionLog* session_log_alloc(void);
void session_log_free(SessionLog* log);

/** Open the file and write a session header. Safe to call when already open. */
SessionLogStatus session_log_begin(SessionLog* log);

/** Record one promoted follower. No-op unless begin() succeeded. */
SessionLogStatus session_log_follower(SessionLog* log, const TrackerRecord* rec);

/** Flush and close. Safe to call when not open. */
void session_log_end(SessionLog* log);

bool session_log_is_open(SessionLog* log);
SessionLogStatus session_log_status(SessionLog* log);

/** How many followers this session has written. */
uint32_t session_log_count(SessionLog* log);
