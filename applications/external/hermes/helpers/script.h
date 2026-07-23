#pragma once

#include <furi.h>

/* Held in RAM rather than streamed from a file handle: a script is a handful of
 * commands, and keeping the card out of the send path means a slow read can
 * never stall the console. */
#define SCRIPT_MAX_BYTES (2048u)
#define SCRIPT_MAX_LINES (64u)

/* Gap between lines. A target has to echo, parse and act on each one, and a
 * shell that is still printing its prompt will drop whatever arrives early. */
#define SCRIPT_LINE_GAP_MS (250u)

typedef struct Script Script;

Script* script_alloc(void);
void script_free(Script* sc);

/** Load a text file, splitting it into lines.
 *
 * Blank lines and those starting with '#' are dropped, so a script can be
 * commented. Returns false if the file is missing, empty or too big.
 */
bool script_load(Script* sc, const char* path);

void script_clear(Script* sc);

bool script_is_loaded(const Script* sc);
uint16_t script_line_count(const Script* sc);
const char* script_name(const Script* sc);

/* ----------------------------------------------------------- playback ----- */

/** Begin sending from the first line. */
void script_start(Script* sc);
void script_stop(Script* sc);
bool script_is_running(const Script* sc);

uint16_t script_position(const Script* sc); // lines sent so far

/** The next line to send, or NULL if it is not yet time (or we are done).
 *
 * Call once per UI tick; it paces itself, so the caller never blocks and the
 * console keeps rendering the target's replies as they arrive.
 */
const char* script_next_line(Script* sc);
