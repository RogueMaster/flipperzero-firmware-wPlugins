#pragma once

#include "nfc_canary_i.h"

/* Record an event into the ring buffer and update session stats.
 * Caller must NOT hold state->mutex. */
void eventlog_add(AlerterState* state, const AlertEvent* ev);

/* Fetch event by display index (0 = newest). Returns false if out of range. */
bool eventlog_get(AlerterState* state, uint16_t index, AlertEvent* out);

/* Number of events currently retained (<= EVENT_LOG_MAX). */
uint16_t eventlog_count(AlerterState* state);

/* Lock-free variants, for callers that already hold state->mutex (the draw
 * path takes the lock once for the whole frame). */
bool eventlog_get_locked(const AlerterState* state, uint16_t index, AlertEvent* out);
uint16_t eventlog_count_locked(const AlerterState* state);

void eventlog_clear(AlerterState* state);

/* Append one event to CSV on the SD card. Best-effort; failure is non-fatal
 * (the in-memory log is the primary store). */
void eventlog_persist(const AlertEvent* ev);

/* Human-readable decode of a reader command byte -- the fingerprinting
 * payoff. Returns a static string; never NULL. */
const char* eventlog_cmd_name(uint8_t cmd);

/* Short protocol name, or "Unknown" for 0xFF. */
const char* eventlog_protocol_name(uint8_t protocol);
