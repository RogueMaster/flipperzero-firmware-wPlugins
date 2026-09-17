#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

#include "sr_io.h"
#include "sr_model.h"

typedef struct SrWorker SrWorker;

typedef struct {
    uint32_t lines_ready; /* Lines delivered by sr_line (truncated ones included) */
    uint32_t lines_truncated;
    uint32_t lines_applied; /* Actual calls to sr_model_apply */
    uint32_t stack_min; /* Lowest remaining stack seen; 0 if never sampled */
} SrWorkerStats;

/* io / model / mtx are all borrowed, not owned. Returns NULL if any of them is NULL. */
SrWorker* sr_worker_alloc(SrIo* io, SrModel* model, FuriMutex* mtx);
/* If thread is still non-NULL -> furi_check fails (the caller must join first). NULL safe. */
void sr_worker_free(SrWorker* w);

/* Starts only when sr_io_is_open(io) and it has not started yet. Returns false on failure, which is not fatal. */
bool sr_worker_start(SrWorker* w);
void sr_worker_request_stop(SrWorker* w); /* Atomically sets the exit flag; NULL safe */
/* join + furi_thread_free, then thread is set to NULL. No-op if never started. */
void sr_worker_join(SrWorker* w);

enum { SR_WORKER_CMD_MAX = 32 };

/* Queue one command, to be sent by the worker thread while it does not hold the app mutex.
 * cmd must carry its own line terminator (for example "info\n").
 * Returns false on an invalid argument, on exceeding SR_WORKER_CMD_MAX-1, or when the slot
 * still holds an unsent command.
 *
 * Single writer: only the GUI thread may call this (from scene callbacks). Calling it from the
 * worker thread or a second thread is forbidden. */
bool sr_worker_send_cmd(SrWorker* w, const char* cmd);
void sr_worker_get_stats(const SrWorker* w, SrWorkerStats* out);

/* Read the cumulative acknowledgement count for one command class (L2 of ADR-022 decision 1;
 * T4.9 wired SrCmdAckObs through to the UI).
 *
 * NOTE: reads a single count[cls] and **does not return the whole SrCmdAckObs** -- relaxed
 *    field-by-field reads of rev and count[] can tear (observing an intermediate state where rev
 *    has grown but count has not). A single-field atomic read has no such problem,
 *    and this is exactly the usage described in the type comment at src/sr_types.h:262-264:
 *    snapshot count[cls] when sending and watch for it to change.
 * NOTE: the return value is a **wrapping counter**: consumers must compare with != and must not
 *    use > (src/sr_types.h:258-259; this has been stepped on three times).
 *
 * w == NULL, w->parser == NULL, or cls out of range -> returns 0. Callable from any thread (read only). */
uint32_t sr_worker_cmdack_count(const SrWorker* w, SrCmdAckClass cls);

/* Last VBUS-present sample taken on the worker thread (threshold 4.5 V, must
 * match sr_io.c SR_IO_VBUS_PRESENT_V). Readable from any thread. w == NULL → false. */
bool sr_worker_vbus_present(const SrWorker* w);
