#pragma once

#include "hid_exfil.h"

/**
 * Allocate a new worker instance.
 * The receive buffer is allocated internally.
 */
HidExfilWorker* hid_exfil_worker_alloc(void);

/**
 * Free a worker instance and its receive buffer.
 */
void hid_exfil_worker_free(HidExfilWorker* worker);

/**
 * Set the progress callback for UI updates.
 */
void hid_exfil_worker_set_callback(
    HidExfilWorker* worker,
    HidExfilWorkerCallback callback,
    void* context);

/**
 * Configure the worker for a run.
 */
void hid_exfil_worker_configure(
    HidExfilWorker* worker,
    PayloadType payload_type,
    ExfilConfig* config);

/**
 * Allocate the receive buffer and start the worker thread.
 * Returns false (without starting) if the heap allocation fails.
 * The buffer is intentionally deferred here — not in worker_alloc —
 * so memory is only consumed when a run actually begins.
 */
bool hid_exfil_worker_start(HidExfilWorker* worker);

/**
 * Request the worker to stop (non-blocking).
 */
void hid_exfil_worker_stop(HidExfilWorker* worker);

/**
 * Toggle pause state.
 */
void hid_exfil_worker_toggle_pause(HidExfilWorker* worker);

/**
 * Request abort.
 */
void hid_exfil_worker_abort(HidExfilWorker* worker);

/**
 * Check if worker is still running.
 */
bool hid_exfil_worker_is_running(HidExfilWorker* worker);

/**
 * Get current state snapshot (thread-safe copy).
 */
ExfilState hid_exfil_worker_get_state(HidExfilWorker* worker);

/**
 * Get pointer to received data buffer and its length.
 */
uint8_t* hid_exfil_worker_get_data(HidExfilWorker* worker, uint32_t* out_len);
