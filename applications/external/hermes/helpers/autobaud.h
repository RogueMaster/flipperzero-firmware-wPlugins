#pragma once

#include <furi.h>
#include <furi_hal.h>
#include "baud_table.h"

/* 512 edges is roughly 60 bytes of 8N1 traffic - plenty for a stable fit, and
 * small enough that the whole capture stays in a 2 KB buffer. */
#define AUTOBAUD_MAX_EDGES (512u)
#define AUTOBAUD_CANDIDATES (4u)

/* Widest square wave the scope view can show at once. */
#define AUTOBAUD_TRACE_EDGES (48u)

typedef struct {
    uint32_t baud; // a standard rate from hermes_baud_table
    uint8_t confidence; // 0..100
    int32_t error_ppm; // how far the raw fit sits from this rate, signed
} AutobaudCandidate;

/** One measured stretch of line at a constant level. */
typedef struct {
    uint32_t delta; // duration, in CPU cycles
    bool high;
} AutobaudSegment;

typedef enum {
    AutobaudVerdictOk, // a bit time was fitted
    AutobaudVerdictSilent, // the line never moved
    AutobaudVerdictTooFewEdges, // it moved, but not enough to fit anything
    AutobaudVerdictNoFit, // edges exist but no consistent bit time explains them
} AutobaudVerdict;

typedef struct {
    AutobaudVerdict verdict;
    uint32_t baud_raw; // straight from the fit, before snapping
    uint32_t bit_time_cycles; // fitted bit time, in 64 MHz CPU cycles
    uint32_t edges_used; // edges that survived filtering
    uint8_t fit_percent; // share of those edges the bit time explains
    uint8_t candidate_count;
    AutobaudCandidate candidates[AUTOBAUD_CANDIDATES]; // best first
} AutobaudResult;

typedef struct Autobaud Autobaud;

Autobaud* autobaud_alloc(void);
void autobaud_free(Autobaud* ab);

/** Take the RX pin as an interrupt input and start timing edges.
 *
 * Acquires the serial handle first so the console service cannot fight us for
 * the pin. Returns false if the port is already busy.
 */
bool autobaud_start(Autobaud* ab, HermesPort port);

/** Release the pin and hand the port back. Safe to call when not started. */
void autobaud_stop(Autobaud* ab);

uint32_t autobaud_edge_count(const Autobaud* ab);
bool autobaud_is_full(const Autobaud* ab);

/** Whether the line is currently sitting high (idle) - drives the "no signal" hint. */
bool autobaud_line_idle_high(const Autobaud* ab);

/** Copy the most recent segments for the scope, newest last.
 *
 * Each carries its own level rather than assuming edges alternate, so a missed
 * edge shows up in the trace as the glitch it is instead of inverting the rest
 * of the waveform.
 */
size_t autobaud_trace(const Autobaud* ab, AutobaudSegment* out, size_t max);

/** Fit a bit time over everything captured so far. Never blocks. */
void autobaud_analyze(const Autobaud* ab, AutobaudResult* out);
