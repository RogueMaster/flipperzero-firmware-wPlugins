#pragma once

#include <furi.h>
#include "baud_table.h"

/* Written straight through to the SD card, so the log survives a battery pull
 * or a target that browns out the moment you get the interesting line. */
#define SESSION_LOG_DIR "/ext/apps_data/hermes"
#define SESSION_LOG_PATH_MAX (96u)

typedef struct SessionLog SessionLog;

SessionLog* session_log_alloc(void);
void session_log_free(SessionLog* log);

/** Open a new timestamped capture and write the header block.
 *
 * The header records the link settings, so a file found months later still
 * says what it was captured from. Returns false if the SD card is missing or
 * the directory cannot be made.
 */
bool session_log_open(SessionLog* log, uint32_t baud, HermesFraming framing, HermesPort port);

/** Close the capture. Safe when not open. */
void session_log_close(SessionLog* log);

bool session_log_is_open(const SessionLog* log);

/** Append received bytes verbatim - no cooking, no escape stripping.
 *
 * The on-screen terminal drops ANSI sequences to stay readable; the file keeps
 * them, because a log you later grep or replay should be what the wire said.
 */
void session_log_write(SessionLog* log, const uint8_t* data, size_t len);

/** Note something the user did, as a bracketed line between the target's
 *  output - so a log shows what was typed, not just what came back. */
void session_log_note(SessionLog* log, const char* what);

uint32_t session_log_bytes(const SessionLog* log);

/** Filename only (no directory), for showing on screen. Empty when closed. */
const char* session_log_name(const SessionLog* log);
