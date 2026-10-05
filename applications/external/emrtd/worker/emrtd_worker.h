/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The reading state machine.
 *
 * Everything below the user interface happens on the NFC stack's own thread,
 * inside the poller callback: selecting the application, running an access
 * driver, and then reading file after file through Secure Messaging. The
 * worker owns that sequence and reports back through a callback that the
 * scene turns into custom events.
 *
 * Two rules keep this safe and are worth stating in the header, because
 * breaking either produces a hang rather than an error:
 *
 *  - the callback is invoked on the NFC thread, so it may not touch the GUI
 *    directly; it posts a custom event and returns;
 *  - the poller callback only ever returns NfcCommandStop or
 *    NfcCommandContinue. Stopping the poller from inside its own callback
 *    deadlocks, so the scene stops it after the event arrives.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../access/emrtd_access.h"
#include "../protocol/emrtd_files.h"
#include "../protocol/emrtd_lds.h"
#include "../protocol/emrtd_security_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EmrtdWorker EmrtdWorker;

/** What the worker is doing, which is what the progress view shows. */
typedef enum {
    EmrtdWorkerStageIdle,
    EmrtdWorkerStageWaitingForCard,
    EmrtdWorkerStageSelectingApplication,
    EmrtdWorkerStageReadingCardAccess,
    EmrtdWorkerStageAuthenticating,
    EmrtdWorkerStageReadingFile,
    EmrtdWorkerStageVerifying,
    EmrtdWorkerStageExporting,
    EmrtdWorkerStageDone,
    EmrtdWorkerStageError,
} EmrtdWorkerStage;

const char* emrtd_worker_stage_text(EmrtdWorkerStage stage);

/** Outcome of one file. */
typedef enum {
    EmrtdFileStateAbsent, /**< Not announced by EF.COM. */
    EmrtdFileStatePending,
    EmrtdFileStateReading,
    EmrtdFileStateRead,
    EmrtdFileStateSkipped, /**< EAC protected, or deselected by the user. */
    EmrtdFileStateFailed,
} EmrtdFileState;

/** Whether a file's content matched the hash EF.SOD lists for it. */
typedef enum {
    EmrtdHashStateUnknown,
    EmrtdHashStateMatch,
    EmrtdHashStateMismatch,
    EmrtdHashStateNotListed,
    EmrtdHashStateUnsupportedDigest,
} EmrtdHashState;

typedef struct {
    EmrtdFileState state;
    EmrtdHashState hash_state;
    size_t size;
    EmrtdError error;
} EmrtdFileResult;

/**
 * The Secure Messaging keys a read derived, kept so that the result can show
 * them.
 *
 * They are copied out of the EmrtdSm the moment the session opens, before the
 * first protected command moves the counter on, so the SSC here is the one the
 * session started from and the three values together describe the session as
 * it began.
 *
 * @warning This lives in EmrtdReadResult, which emrtd_export_write_report()
 *          also reads. Nothing here may be written to the card: an APDU trace
 *          next to these keys is the whole session decrypted by whoever picks
 *          the card up. See docs/security.md.
 */
typedef struct {
    bool present;
    EmrtdCipher cipher;
    uint8_t ks_enc[EMRTD_KEY_MAX_SIZE];
    uint8_t ks_mac[EMRTD_KEY_MAX_SIZE];
    uint8_t ssc[EMRTD_BLOCK_MAX_SIZE]; /**< As the session opened. */
} EmrtdSessionKeys;

/** Everything a completed read produced. */
typedef struct {
    bool application_selected;
    bool card_access_read;
    EmrtdSecurityInfos security_infos;

    bool authenticated;
    EmrtdAccessOutcome access;
    EmrtdSessionKeys keys;

    bool has_com;
    EmrtdEfCom com;
    bool has_mrz;
    EmrtdMrz mrz;
    bool has_dg11;
    EmrtdDg11 dg11;
    bool has_dg12;
    EmrtdDg12 dg12;
    bool has_dg15;
    EmrtdDg15 dg15;
    bool has_sod;
    EmrtdEfSod sod;
    bool has_face;
    EmrtdFaceImage face;
    size_t face_size;

    EmrtdFileResult files[EmrtdFileCount];

    uint8_t hashes_checked;
    uint8_t hashes_matched;

    bool exported;
    char export_path[64];

    EmrtdError error;
    EmrtdFileId error_file; /**< Which file the error belongs to, if any. */
    /**
     * How far the read had got when it stopped: the last stage it reported
     * before the error. Meaningful only while @c error is set, and what
     * emrtd_worker_stopped_text() turns into words.
     */
    EmrtdWorkerStage error_stage;
} EmrtdReadResult;

/**
 * Where a read that failed had got to, in one sentence, or NULL.
 *
 * The sentence is broken into lines with '\n' by the same rule as
 * emrtd_error_hint(), because it is shown under the hint on the error screen.
 *
 * The error says what went wrong and this says when, and the two together are
 * what tells a chip that went quiet while it computed PACE from one that went
 * quiet half way through DG2. Static; safe from any thread.
 */
const char* emrtd_worker_stopped_text(const EmrtdReadResult* result);

/** Progress, delivered on the NFC thread. */
typedef struct {
    EmrtdWorkerStage stage;
    EmrtdFileId file; /**< Meaningful while reading a file. */
    size_t bytes_done;
    size_t bytes_total;
    uint8_t percent; /**< 0..100 over the whole read. */
    const char* detail; /**< Static string, may be NULL. */
} EmrtdWorkerProgress;

/**
 * Progress callback.
 *
 * Runs on the NFC thread. Post an event and return; do not block.
 *
 * @return false to abort the read.
 */
typedef bool (*EmrtdWorkerCallback)(const EmrtdWorkerProgress* progress, void* context);

/** What the user asked for. */
typedef struct {
    EmrtdCredentials credentials;
    EmrtdAccessMethod method;
    EmrtdFileMask files; /**< Which data groups to attempt. */
    bool export_to_sd;
    bool write_trace; /**< An APDU log next to the export, for diagnosis. */
} EmrtdWorkerConfig;

/*
 * Declared rather than included: the worker is handed an Nfc instance but
 * never looks inside one, and a header that pulls the whole NFC stack in
 * cannot be included from a view. Without this the pointer type below would
 * first come into being inside a parameter list, which the firmware's warning
 * set treats as an error.
 */
struct Nfc;

/**
 * @param[out] result  filled in place as the read proceeds, and owned by the
 *                     caller: it has to outlive the worker, because every
 *                     screen after the read is drawn from it.
 */
EmrtdWorker* emrtd_worker_alloc(EmrtdReadResult* result);
void emrtd_worker_free(EmrtdWorker* worker);

void emrtd_worker_set_config(EmrtdWorker* worker, const EmrtdWorkerConfig* config);
void emrtd_worker_set_callback(EmrtdWorker* worker, EmrtdWorkerCallback callback, void* context);

/** Start polling. The worker owns the Nfc instance it is given for the run. */
void emrtd_worker_start(EmrtdWorker* worker, struct Nfc* nfc);

/** Ask the read to stop. Safe from the GUI thread. */
void emrtd_worker_stop(EmrtdWorker* worker);

/** The result, valid once the worker has reported Done or Error. */
const EmrtdReadResult* emrtd_worker_result(const EmrtdWorker* worker);

#ifdef __cplusplus
}
#endif
