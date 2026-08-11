/*
 * A small in-memory log shown by scene_log.
 *
 * The link layer writes to it from a worker thread while the GUI thread reads
 * it, so every entry point takes a mutex. Oldest text is dropped once the log
 * exceeds MESHCORE_LOG_CAP.
 */
#pragma once

#include <furi.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MESHCORE_LOG_CAP 2048u

/* Every line also goes to the system log under this tag, so a session can be
 * watched from a laptop with `log` on the Flipper CLI instead of by holding
 * the device and scrolling. Filter with `log MeshCore`. */
#define MESHCORE_LOG_TAG "MeshCore"

typedef struct MeshCoreLog MeshCoreLog;

MeshCoreLog* meshcore_log_alloc(void);
void meshcore_log_free(MeshCoreLog* log);

/** Append one line. A trailing newline is added automatically. */
void meshcore_log_printf(MeshCoreLog* log, const char* format, ...)
    _ATTRIBUTE((__format__(__printf__, 2, 3)));

/** Append a hex dump of one companion payload. `tx` picks the direction mark. */
void meshcore_log_frame(MeshCoreLog* log, bool tx, const uint8_t* payload, size_t len);

/** Copy the current contents into `out` for display. */
void meshcore_log_snapshot(MeshCoreLog* log, FuriString* out);

/** Bumped on every append. A view can compare it against what it last drew and
 *  skip the redraw otherwise — redrawing a scrollable view resets the user's
 *  scroll position, so doing it on a timer makes the view feel broken. */
uint32_t meshcore_log_revision(MeshCoreLog* log);

void meshcore_log_clear(MeshCoreLog* log);

/* Where the log is left after the app closes. */
#define MESHCORE_LOG_FILE EXT_PATH("apps_data/meshcore_cfg/last_run.log")

/** Write the log to the card, overwriting whatever the previous run left.
 *
 *  Called once on the way out rather than line by line: a synced SD write costs
 *  tens of milliseconds and this is called from the session worker, which is
 *  also the thread draining the UART.
 *
 *  It exists because the on-screen log needs the device in hand, and a scene
 *  that only misbehaves when nobody is holding the Flipper is a scene that
 *  cannot be debugged. After the app exits:
 *
 *      storage read /ext/apps_data/meshcore_cfg/last_run.log
 *
 *  (After. Reading a file the app still holds open wedges the storage service.) */
void meshcore_log_dump(MeshCoreLog* log);
