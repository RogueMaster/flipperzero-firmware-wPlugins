#pragma once

#include "nfc_fuzzer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback invoked on every test case completion from the worker thread.
 * @param result  Pointer to the result (only valid if anomaly != None).
 * @param current Current test case number (1-based).
 * @param total   Total number of test cases planned.
 * @param payload Current payload bytes.
 * @param payload_len Length of payload.
 * @param context User context (NfcFuzzerApp*).
 */
typedef void (*NfcFuzzerWorkerCallback)(
    const NfcFuzzerResult* result,
    uint32_t current,
    uint32_t total,
    const uint8_t* payload,
    uint8_t payload_len,
    void* context);

/** Callback invoked when the worker finishes (stopped or completed). */
typedef void (*NfcFuzzerWorkerDoneCallback)(void* context);

/** Allocate a fuzzer worker. */
NfcFuzzerWorker* nfc_fuzzer_worker_alloc(void);

/** Free a fuzzer worker. Must be stopped first. */
void nfc_fuzzer_worker_free(NfcFuzzerWorker* worker);

/** Set the progress / result callback. */
void nfc_fuzzer_worker_set_callback(
    NfcFuzzerWorker* worker,
    NfcFuzzerWorkerCallback callback,
    void* context);

/** Set the done callback. */
void nfc_fuzzer_worker_set_done_callback(
    NfcFuzzerWorker* worker,
    NfcFuzzerWorkerDoneCallback callback,
    void* context);

/** Start fuzzing with given profile, strategy, and settings. */
void nfc_fuzzer_worker_start(
    NfcFuzzerWorker* worker,
    NfcFuzzerProfile profile,
    NfcFuzzerStrategy strategy,
    const NfcFuzzerSettings* settings);

/** Request the worker to stop. Non-blocking; worker may take one cycle to halt. */
void nfc_fuzzer_worker_stop(NfcFuzzerWorker* worker);

/** Returns true if the worker thread is currently running. */
bool nfc_fuzzer_worker_is_running(NfcFuzzerWorker* worker);

#ifdef __cplusplus
}
#endif
