#pragma once

#include <furi.h>
#include "baud_table.h"

/* One window per rate. Long enough for a console to say something, short enough
 * that sweeping the whole table stays under a few seconds. */
#define VERIFIER_WINDOW_MS (150u)

typedef struct {
    uint32_t baud;
    HermesFraming framing;
    uint16_t bytes; // bytes the UART handed us in the window
    uint16_t errors; // frame + noise + parity: the line disagreeing with us
    uint16_t overruns; // our own fault, never scored against the candidate
    uint8_t printable_pct;
    uint8_t score; // 0..100
} VerifierEntry;

typedef struct {
    bool complete; // the sweep ran to the end rather than being cancelled
    bool saw_traffic; // anything at all arrived, at any rate
    VerifierEntry best;
    uint8_t entry_count;
    VerifierEntry entries[HERMES_BAUD_COUNT]; // phase A, ordered as tested
} VerifierReport;

typedef struct Verifier Verifier;

/** Fires on the worker thread as each window closes. */
typedef void (*VerifierProgressCallback)(void* context);

Verifier* verifier_alloc(void);
void verifier_free(Verifier* v);

void verifier_set_callback(Verifier* v, VerifierProgressCallback cb, void* context);

/** Sweep `bauds` on the real UART, then refine the framing on the winner.
 *
 * Returns immediately; the work runs on a private thread. Pass a short list
 * from the timing fit, or the whole table when the fit came up empty.
 */
bool verifier_start(Verifier* v, HermesPort port, const uint32_t* bauds, size_t count);

/** Ask the worker to wind up, then wait for it. Safe if never started. */
void verifier_stop(Verifier* v);

bool verifier_is_running(const Verifier* v);
uint8_t verifier_progress(const Verifier* v); // 0..100
uint32_t verifier_current_baud(const Verifier* v);

/** Valid once verifier_is_running() goes false. */
const VerifierReport* verifier_report(const Verifier* v);
