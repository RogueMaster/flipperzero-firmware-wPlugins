/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_worker.h"

#include "../emrtd_wipe.h"

#include <stdio.h>
#include <string.h>

#include <furi.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_4b/iso14443_4b_poller.h>

#include "../crypto/emrtd_crypto.h"
#include "../crypto/emrtd_sm.h"
#include "../emrtd.h"
#include "../protocol/emrtd_apdu.h"
#include "../protocol/emrtd_tlv.h"
#include "../transport/emrtd_iso14443_4.h"
#include "emrtd_export.h"

#define TAG "EmrtdWorker"

/** Room for one command and one response, envelope included. */
#define EMRTD_WORKER_APDU_BUFFER_SIZE 512

/**
 * How much of DG2 is held back while the image is located.
 *
 * The facial image sits behind the biometric information template and the
 * ISO/IEC 19794-5 facial record header, which together run to about a hundred
 * bytes. Half a kilobyte is comfortably more than that and is the only part of
 * the file that is ever in memory.
 */
#define EMRTD_WORKER_DG2_LOOKAHEAD 512

/** EF.CardAccess is a short SET OF SecurityInfo; a kilobyte is generous. */
#define EMRTD_WORKER_CARD_ACCESS_MAX 1024

/** Largest offset that fits the P1-P2 field of READ BINARY (ISO 7816-4). */
#define EMRTD_WORKER_SHORT_OFFSET_MAX 0x7FFF

/** Largest offset the odd instruction form encodes here, in two bytes. */
#define EMRTD_WORKER_LONG_OFFSET_MAX 0xFFFF

/**
 * What one heap block costs beyond the bytes asked for.
 *
 * FreeRTOS heap_4 puts an eight byte header in front of every block, and
 * memmgr_heap_get_max_free_block() reports the block including it. Asking
 * whether a request fits therefore means comparing against size plus this.
 */
#define EMRTD_WORKER_BLOCK_HEADER 8u

/** Sanity bound on a file, so a malformed length cannot spin the reader. */
#define EMRTD_WORKER_FILE_SIZE_MAX (64u * 1024u)

/**
 * Consecutive activation failures before the reader gives up.
 *
 * Every round costs about a tenth of a second whichever way it fails: the
 * ISO 14443-3 pollers delay that long after a failed activation, and a round
 * that fails at RATS answers NfcCommandReset, which cycles the field for the
 * same order of time. So this is roughly two seconds - long enough for a hand
 * to settle the document back onto the reader, short enough not to look like
 * a hang.
 *
 * The field has to be cycled rather than merely retried, and that is not
 * belt and braces: a chip that answered RATS has entered the ISO 14443-4
 * protocol state, where it ignores both WUPA and HLTA and answers only
 * I-, R- and S-blocks. Dropping the carrier is the only way back to idle, and
 * without it every remaining attempt is guaranteed to fail.
 */
#define EMRTD_WORKER_ACTIVATION_RETRIES 20

/** How long one turn of the wait for the read to finish lasts. */
#define EMRTD_WORKER_WAIT_SLICE_MS 100

/** Pause between two rounds of protocol detection. */
#define EMRTD_WORKER_DETECT_PAUSE_MS 20

/**
 * Stack for the control thread.
 *
 * Detection allocates and frees a poller on this thread, and that reaches the
 * NFC hardware abstraction layer; the firmware's own scanner gives the same
 * path four kilobytes.
 */
#define EMRTD_WORKER_STACK_SIZE (3u * 1024u)

/** Set from the poller callback once the read has finished, one way or another. */
#define EMRTD_WORKER_FLAG_FINISHED (1UL << 0)

/** Percentage of the whole read given to fetching the files. */
#define EMRTD_WORKER_FILES_PERCENT 65u
/** Percentage already spent by the time the first file is selected. */
#define EMRTD_WORKER_FILES_BASE    25u

/**
 * EF.CardAccess is not part of the eMRTD application's catalogue - it lives in
 * the master file and is readable without any authentication - so it has no
 * entry in emrtd_files.c. The export only ever looks at @c name, which is why
 * an entry made up here is enough to give it a file of its own.
 */
static const EmrtdFileInfo emrtd_worker_card_access_info = {
    .id = EmrtdFileCount,
    .fid = EMRTD_FID_CARD_ACCESS,
    .sfi = 0x1C,
    .tag = 0x00,
    .dg_number = -1,
    .eac_protected = false,
    .name = "EF.CardAccess",
    .label = "Card access",
    .description = "How the chip wants to be opened",
};

/** A running hash over one file, for the passive authentication check. */
typedef struct {
    bool active;
    bool sha256;
    mbedtls_sha1_context sha1;
    mbedtls_sha256_context sha2;
} EmrtdWorkerDigest;

struct EmrtdWorker {
    EmrtdWorkerConfig config;
    EmrtdWorkerCallback callback;
    void* callback_context;

    /*
     * Borrowed, not owned. The record is two kilobytes and every screen after
     * the read wants it, so the caller keeps it and the worker fills it in
     * place. Holding a second copy here cost that much heap for the length of
     * a read - the one stretch where there is none to spare - and it made the
     * result outlive nothing: the scene had to copy it out before on_exit
     * freed the worker, which is a step that can be forgotten.
     */
    EmrtdReadResult* result;

    Nfc* nfc;
    NfcPoller* poller;

    FuriThread* thread;
    FuriEventFlag* events;
    volatile bool stop_requested;
    bool running;

    EmrtdIso14443_4* transport;
    EmrtdTransceiver* transceiver;
    EmrtdSm sm;
    bool sm_active;

    EmrtdExport* export_ctx;

    uint8_t* command; /**< The serialised command APDU. */
    uint8_t* response; /**< The response, unprotected in place. */
    uint8_t* lookahead; /**< The head of DG2, until the image is located. */

    EmrtdWorkerDigest digest;
    uint8_t digest_value[64];

    const char* driver_name; /**< Static, owned by the access driver registry. */
    size_t files_total;
    size_t files_done;
    uint8_t activation_failures;
    /** Why the last activation round failed, for the screen when they run out. */
    EmrtdError activation_error;
    /** The last stage reported short of Done or Error, for a read that fails. */
    EmrtdWorkerStage stage;
};

/* --- Stage names -------------------------------------------------------- */

const char* emrtd_worker_stage_text(EmrtdWorkerStage stage) {
    switch(stage) {
    case EmrtdWorkerStageIdle:
        return "Idle";
    case EmrtdWorkerStageWaitingForCard:
        return "Hold the document";
    case EmrtdWorkerStageSelectingApplication:
        return "Opening document";
    case EmrtdWorkerStageReadingCardAccess:
        return "Reading card access";
    case EmrtdWorkerStageAuthenticating:
        return "Authenticating";
    case EmrtdWorkerStageReadingFile:
        return "Reading";
    case EmrtdWorkerStageVerifying:
        return "Verifying";
    case EmrtdWorkerStageExporting:
        return "Saving";
    case EmrtdWorkerStageDone:
        return "Done";
    case EmrtdWorkerStageError:
        return "Failed";
    default:
        return "";
    }
}

const char* emrtd_worker_stopped_text(const EmrtdReadResult* result) {
    if(result == NULL || result->error == EmrtdErrorNone) {
        return NULL;
    }
    switch(result->error_stage) {
    case EmrtdWorkerStageWaitingForCard:
        return "It stopped before the chip\nwould open a session.";
    case EmrtdWorkerStageSelectingApplication:
        return "It stopped while opening\nthe document, before\nanything was read.";
    case EmrtdWorkerStageReadingCardAccess:
        return "It stopped reading\nEF.CardAccess, before\nauthenticating.";
    case EmrtdWorkerStageAuthenticating:
        /*
         * The stage is reported once more after a driver succeeds, so the
         * flag is what says which side of the session the read stopped on.
         */
        return result->authenticated ?
                   "It stopped just after the\nsecure session opened." :
                   "It stopped while\nauthenticating, before\nany file was read.";
    case EmrtdWorkerStageReadingFile:
        return "It stopped while reading\nthe files.";
    default:
        return NULL;
    }
}

/* --- Progress ----------------------------------------------------------- */

/**
 * Tell the caller where the read has got to.
 *
 * Runs on the NFC thread. Once a stop has been asked for the callback is not
 * invoked again: the caller is on its way into emrtd_worker_stop(), and a
 * callback that posts to a queue nobody is draining would deadlock the two
 * threads against each other.
 *
 * @return false when the read should be abandoned
 */
static bool emrtd_worker_report(
    EmrtdWorker* worker,
    EmrtdWorkerStage stage,
    EmrtdFileId file,
    size_t bytes_done,
    size_t bytes_total,
    uint8_t percent,
    const char* detail) {
    if(stage < EmrtdWorkerStageDone) {
        worker->stage = stage;
    }
    if(worker->stop_requested) {
        return false;
    }
    if(worker->callback == NULL) {
        return true;
    }

    const EmrtdWorkerProgress progress = {
        .stage = stage,
        .file = file,
        .bytes_done = bytes_done,
        .bytes_total = bytes_total,
        .percent = percent > 100 ? 100 : percent,
        .detail = detail,
    };

    if(!worker->callback(&progress, worker->callback_context)) {
        FURI_LOG_I(TAG, "Aborted by the caller");
        worker->stop_requested = true;
        return false;
    }
    return true;
}

static uint8_t emrtd_worker_file_percent(const EmrtdWorker* worker, size_t done, size_t total) {
    const size_t files = worker->files_total > 0 ? worker->files_total : 1;
    const size_t span = EMRTD_WORKER_FILES_PERCENT / files;

    size_t value = (EMRTD_WORKER_FILES_PERCENT * worker->files_done) / files;
    if(total > 0 && done < total) {
        value += (span * done) / total;
    } else if(total > 0) {
        value += span;
    }

    value += EMRTD_WORKER_FILES_BASE;
    return value > 90 ? 90 : (uint8_t)value;
}

/* --- The diagnostic trace ----------------------------------------------- */

/*
 * What a trace is for: a document that reads on one reader and not on another
 * differs somewhere in this exchange, and the difference is never in the data
 * groups themselves - it is in which command the chip refuses, how much of a
 * file it will part with at a time, or which of the two READ BINARY forms it
 * implements. All of that is here, and docs/trace.md says how to read it and
 * how to turn one into a test the simulated chip can be held to.
 *
 * Every line the reader writes goes through this section, so the rule that
 * nothing secret reaches the file can be checked in one place: the notes
 * carry file names, offsets, lengths and status words, and never a key, a
 * nonce or a credential.
 */

/** True while there is an open export with tracing switched on. */
static bool emrtd_worker_tracing(const EmrtdWorker* worker) {
    return worker->export_ctx != NULL && worker->config.write_trace;
}

/**
 * The wire, as the radio saw it.
 *
 * Once a session exists this is the envelope and not the command: the body is
 * a cryptogram and the last two bytes are the tail of the MAC in DO'8E', not
 * a status word. Reading them as one would put a plausible, invented status
 * word in the trace for every protected exchange, which is worse than none -
 * so the status word is left to the layer that has the real one, and that
 * layer also writes the '>>' and '<<' lines the cryptogram hides.
 */
static void emrtd_worker_trace(void* context, bool outgoing, const uint8_t* data, size_t len) {
    EmrtdWorker* worker = context;
    if(worker->export_ctx == NULL) {
        return;
    }

    emrtd_export_trace(worker->export_ctx, outgoing ? "> " : "< ", data, len);

    /*
     * The answer above arrived, but not necessarily first time. Written here
     * rather than by the caller because the access protocols talk to the port
     * directly, and the PACE steps are where an identity card draws the most
     * from the field.
     */
    char recovery[48];
    if(!outgoing &&
       emrtd_iso14443_4_recovery_detail(worker->transport, recovery, sizeof(recovery))) {
        FURI_LOG_W(TAG, "Answer recovered: %s", recovery);
        emrtd_export_trace_note(
            worker->export_ctx, "# recover %s the answer was asked for again", recovery);
    }

    if(!outgoing && !worker->sm_active && len >= 2) {
        const uint16_t sw = (uint16_t)((data[len - 2] << 8) | data[len - 1]);
        emrtd_export_trace_note(worker->export_ctx, "# sw code=%04X %s", sw, emrtd_sw_text(sw));
    }
}

/*
 * The outcome of a file in one word each, for the note that closes it.
 *
 * The report says the same thing in prose; a trace is read by tools as well
 * as by people, so here the value is a single token and stays one.
 */
static const char* emrtd_worker_state_word(EmrtdFileState state) {
    switch(state) {
    case EmrtdFileStateAbsent:
        return "absent";
    case EmrtdFileStatePending:
        return "pending";
    case EmrtdFileStateReading:
        return "reading";
    case EmrtdFileStateRead:
        return "read";
    case EmrtdFileStateSkipped:
        return "skipped";
    default:
        return "failed";
    }
}

static const char* emrtd_worker_hash_word(EmrtdHashState state) {
    switch(state) {
    case EmrtdHashStateMatch:
        return "match";
    case EmrtdHashStateMismatch:
        return "mismatch";
    case EmrtdHashStateNotListed:
        return "not-listed";
    case EmrtdHashStateUnsupportedDigest:
        return "unsupported-digest";
    default:
        return "unchecked";
    }
}

/* --- The export ---------------------------------------------------------- */

/*
 * Exporting is optional, and a card that has been pulled out of the reader
 * matters while a full SD card does not: a storage failure is logged once and
 * then ignored, and with no export open every one of these does nothing.
 */

static void emrtd_worker_export_begin(EmrtdWorker* worker, const EmrtdFileInfo* info) {
    if(worker->export_ctx == NULL) {
        return;
    }
    if(emrtd_export_begin_file(worker->export_ctx, info) != EmrtdErrorNone) {
        FURI_LOG_W(TAG, "Export of %s could not be opened", info->name);
    }
}

static void emrtd_worker_export_write(EmrtdWorker* worker, const uint8_t* data, size_t len) {
    if(worker->export_ctx == NULL) {
        return;
    }
    if(emrtd_export_write(worker->export_ctx, data, len) != EmrtdErrorNone) {
        FURI_LOG_W(TAG, "Export write failed; the read continues");
    }
}

static void emrtd_worker_export_end(EmrtdWorker* worker) {
    if(worker->export_ctx != NULL) {
        emrtd_export_end_file(worker->export_ctx);
    }
}

static void emrtd_worker_export_arm_image(EmrtdWorker* worker, size_t offset, const char* suffix) {
    if(worker->export_ctx != NULL) {
        emrtd_export_arm_image(worker->export_ctx, offset, suffix);
    }
}

/* --- Talking to the chip ------------------------------------------------ */

/**
 * Largest response plaintext whose frame still fits.
 *
 * emrtd_transceiver_max_le() sizes the answer from FSD, which counts the whole
 * frame; this trims the ISO 14443-4 prologue and CRC off that budget as well,
 * because the firmware pollers hold PCB || INF || CRC in fixed buffers and
 * overflowing one of them aborts the application instead of failing.
 */
static size_t emrtd_worker_frame_limit(size_t block) {
    if(block == 0) {
        /* Without Secure Messaging the data and the status word share the field. */
        return EMRTD_ISO14443_4_MAX_INF - 2;
    }

    /* DO'87' tag, two length bytes and the padding indicator, DO'99', DO'8E', SW. */
    const size_t envelope = 4 + 4 + 10 + 2;
    if(EMRTD_ISO14443_4_MAX_INF <= envelope + block) {
        return 0;
    }

    size_t cipher = EMRTD_ISO14443_4_MAX_INF - envelope;
    cipher -= cipher % block;
    /* ISO 9797-1 method 2 always adds at least the 0x80 byte. */
    return cipher - 1;
}

static size_t emrtd_worker_chunk_size(const EmrtdWorker* worker) {
    const size_t block = worker->sm_active ? emrtd_cipher_block_size(worker->sm.cipher) : 0;

    size_t le = emrtd_transceiver_max_le(worker->transceiver, block);
    const size_t limit = emrtd_worker_frame_limit(block);

    if(limit > 0 && (le == 0 || le > limit)) {
        le = limit;
    }
    if(le == 0) {
        le = 16;
    }
    return le;
}

/**
 * Exchange one APDU, through Secure Messaging once a session exists.
 *
 * ICAO 9303-11 9.8: the Send Sequence Counter advances once for the command
 * and once for the response, so an exchange that is protected on the way out
 * must be unprotected on the way back or the two sides lose step.
 *
 * This is also the one place where both the command a caller meant and the
 * envelope that carried it exist at once, which is why the readable half of
 * the trace is written from here rather than from the transport.
 */
static EmrtdError emrtd_worker_transmit(
    EmrtdWorker* worker,
    const EmrtdCommandApdu* command,
    EmrtdResponseApdu* out) {
    size_t tx_len = 0;
    EmrtdError error;

    if(worker->sm_active) {
        /*
         * The plaintext is serialised into the send buffer and traced before
         * the envelope overwrites it. That costs nothing and needs no second
         * buffer on a thread that has little stack: emrtd_sm_protect() reads
         * the command structure, and the data it points at is always a
         * caller's own - never the buffer being written to.
         *
         * Only under a session, because without one the '>' line above is
         * already the command in the clear and a second copy of it would say
         * nothing.
         */
        if(emrtd_worker_tracing(worker)) {
            size_t plain_len = 0;
            if(emrtd_apdu_encode(
                   command, worker->command, EMRTD_WORKER_APDU_BUFFER_SIZE, &plain_len) ==
               EmrtdErrorNone) {
                emrtd_export_trace(worker->export_ctx, ">> ", worker->command, plain_len);
            }
        }

        error = emrtd_sm_protect(
            &worker->sm, command, worker->command, EMRTD_WORKER_APDU_BUFFER_SIZE, &tx_len);
    } else {
        error =
            emrtd_apdu_encode(command, worker->command, EMRTD_WORKER_APDU_BUFFER_SIZE, &tx_len);
    }
    if(error != EmrtdErrorNone) {
        return error;
    }

    size_t rx_len = 0;
    error = emrtd_transceiver_exchange(
        worker->transceiver,
        worker->command,
        tx_len,
        worker->response,
        EMRTD_WORKER_APDU_BUFFER_SIZE,
        &rx_len);
    if(error != EmrtdErrorNone) {
        /*
         * A command with no answer under it is the least informative thing a
         * trace can end with, so it is followed by what the radio said. One
         * error code covers a timeout, a checksum and an internal fault.
         */
        char detail[96];
        emrtd_iso14443_4_failure_detail(worker->transport, error, detail, sizeof(detail));
        FURI_LOG_W(TAG, "Exchange failed: %s", detail);
        if(emrtd_worker_tracing(worker)) {
            emrtd_export_trace_note(worker->export_ctx, "# error stage=exchange %s", detail);
        }
        return error;
    }

    if(!worker->sm_active) {
        return emrtd_apdu_decode(worker->response, rx_len, out);
    }

    /*
     * Once the session exists, every answer the chip gives is wrapped - the
     * status word included, in DO'99'. A bare two byte reply therefore is not
     * an answer to the command at all: it is the chip reporting that Secure
     * Messaging has ended (ICAO 9303-11 section 9.8.8 gives 69 87 and 69 88
     * for exactly this, and sends them unprotected because there is no longer
     * a key to protect them with).
     *
     * Nothing in such a reply is authenticated, so none of it may be believed.
     * Reading it as an ordinary status word would let anything in the field
     * answer a protected READ BINARY with 90 00 and have the file recorded as
     * read - empty, unverified, and indistinguishable from a real one. The
     * status word is kept for the trace, where it is useful, and the caller is
     * told the session is gone.
     */
    if(rx_len == 2) {
        const uint16_t bare_sw = (uint16_t)((worker->response[0] << 8) | worker->response[1]);
        FURI_LOG_W(TAG, "Session lost, chip answered %04X unprotected", bare_sw);
        if(emrtd_worker_tracing(worker)) {
            emrtd_export_trace_note(
                worker->export_ctx,
                "# error stage=secure-messaging sw=%04X unprotected, the session has ended: %s",
                bare_sw,
                emrtd_sw_text(bare_sw));
        }
        worker->sm_active = false;
        emrtd_sm_clear(&worker->sm);
        return EmrtdErrorSecureMessaging;
    }

    error = emrtd_sm_unprotect(
        &worker->sm,
        worker->response,
        rx_len,
        worker->response,
        EMRTD_WORKER_APDU_BUFFER_SIZE,
        out);

    if(emrtd_worker_tracing(worker)) {
        if(error == EmrtdErrorNone) {
            emrtd_export_trace_response(
                worker->export_ctx, "<< ", out->data, out->data_len, out->sw);
            emrtd_export_trace_note(
                worker->export_ctx, "# sw code=%04X %s", out->sw, emrtd_sw_text(out->sw));
        } else {
            /*
             * The envelope did not verify, so nothing inside it may be
             * written down as though it had: what the trace records is that
             * the check failed, and the '<' line above it is the evidence.
             */
            emrtd_export_trace_note(
                worker->export_ctx, "# error stage=secure-messaging %s", emrtd_error_text(error));
        }
    }

    return error;
}

static EmrtdError emrtd_worker_select_application(EmrtdWorker* worker, uint16_t* out_sw) {
    EmrtdCommandApdu command;
    emrtd_apdu_select_application(&command, EMRTD_AID, sizeof(EMRTD_AID));

    if(emrtd_worker_tracing(worker)) {
        emrtd_export_trace_note(worker->export_ctx, "# select target=application");
    }

    EmrtdResponseApdu response;
    const EmrtdError error = emrtd_worker_transmit(worker, &command, &response);
    if(error != EmrtdErrorNone) {
        return error;
    }

    *out_sw = response.sw;
    return EmrtdErrorNone;
}

static EmrtdError
    emrtd_worker_select_file(EmrtdWorker* worker, const EmrtdFileInfo* info, uint16_t* out_sw) {
    uint8_t fid[2];
    emrtd_file_fid_bytes(info, fid);

    EmrtdCommandApdu command;
    emrtd_apdu_select_file(&command, fid);

    /*
     * The file is named before its own SELECT rather than after it, so that
     * everything below the line - the reads, their offsets and whatever the
     * chip answered - belongs to the file above it and to no other.
     */
    if(emrtd_worker_tracing(worker)) {
        emrtd_export_trace_note(
            worker->export_ctx, "# select target=file name=%s fid=%04X", info->name, info->fid);
    }

    EmrtdResponseApdu response;
    const EmrtdError error = emrtd_worker_transmit(worker, &command, &response);
    if(error != EmrtdErrorNone) {
        return error;
    }

    *out_sw = response.sw;
    return EmrtdErrorNone;
}

/** 9000, and the warnings of ISO 7816-4 table 6, all come back with the data. */
static bool emrtd_worker_sw_has_data(uint16_t sw) {
    return sw == 0x9000 || (sw & 0xFF00) == 0x6200 || (sw & 0xFF00) == 0x6300;
}

/**
 * READ BINARY beyond the reach of P1-P2.
 *
 * ISO/IEC 7816-4 section 7.2.3: the odd instruction code carries the offset in
 * an offset data object '54' and answers inside a discretionary data object
 * '53'. Data groups above 32 kilobytes - which a high resolution DG2 can be -
 * cannot be read any other way. Not every chip implements it, and one that
 * does not simply refuses the command.
 */
static EmrtdError emrtd_worker_read_binary_long(
    EmrtdWorker* worker,
    size_t offset,
    size_t length,
    EmrtdResponseApdu* out) {
    if(offset > EMRTD_WORKER_LONG_OFFSET_MAX) {
        return EmrtdErrorUnsupported;
    }

    const uint8_t body[4] = {
        0x54,
        0x02,
        (uint8_t)(offset >> 8),
        (uint8_t)(offset & 0xFF),
    };

    const EmrtdCommandApdu command = {
        .cla = 0x00,
        .ins = 0xB1,
        .p1 = 0x00,
        .p2 = 0x00,
        .data = body,
        .data_len = sizeof(body),
        .le = (int)length,
    };

    if(emrtd_worker_tracing(worker)) {
        emrtd_export_trace_note(
            worker->export_ctx, "# read off=%zu le=%zu form=odd", offset, length);
    }

    const EmrtdError error = emrtd_worker_transmit(worker, &command, out);
    if(error != EmrtdErrorNone) {
        return error;
    }
    if(!emrtd_worker_sw_has_data(out->sw) || out->data_len == 0) {
        return EmrtdErrorNone;
    }

    EmrtdTlv node;
    if(!emrtd_tlv_parse_first(out->data, out->data_len, &node) || node.tag != 0x53) {
        FURI_LOG_W(TAG, "Odd INS response is not a '53' data object");
        if(emrtd_worker_tracing(worker)) {
            emrtd_export_trace_note(
                worker->export_ctx,
                "# error stage=read the answer to the odd instruction is not a '53' object");
        }
        return EmrtdErrorParse;
    }

    out->data = node.value;
    out->data_len = node.value_len;
    return EmrtdErrorNone;
}

/**
 * Read @p length bytes at @p offset of the file that is selected.
 *
 * A card that dislikes the expected length answers 6CXX with the length it is
 * willing to give, which ISO 7816-4 says to take and ask again.
 */
static EmrtdError emrtd_worker_read_binary(
    EmrtdWorker* worker,
    size_t offset,
    size_t length,
    EmrtdResponseApdu* out) {
    if(offset > EMRTD_WORKER_SHORT_OFFSET_MAX) {
        return emrtd_worker_read_binary_long(worker, offset, length, out);
    }

    EmrtdCommandApdu command;
    emrtd_apdu_read_binary(&command, (uint16_t)offset, length);

    if(emrtd_worker_tracing(worker)) {
        emrtd_export_trace_note(
            worker->export_ctx, "# read off=%zu le=%zu form=short", offset, length);
    }

    EmrtdError error = emrtd_worker_transmit(worker, &command, out);
    if(error != EmrtdErrorNone) {
        return error;
    }

    if((out->sw & 0xFF00) == 0x6C00) {
        const uint8_t sw2 = (uint8_t)(out->sw & 0xFF);
        size_t exact = sw2 == 0 ? EMRTD_LE_MAX : sw2;

        /*
         * The answer still has to fit one frame, whatever the card asks for.
         * Only one retry is made, so a card that insists on more than the
         * radio allows fails this file rather than looping.
         */
        const size_t safe = emrtd_worker_chunk_size(worker);
        if(exact > safe) {
            exact = safe;
        }
        FURI_LOG_D(TAG, "Card asked for Le %zu", exact);
        if(emrtd_worker_tracing(worker)) {
            emrtd_export_trace_note(
                worker->export_ctx,
                "# retry off=%zu le=%zu asked=%u the chip named its own length",
                offset,
                exact,
                (unsigned)(sw2 == 0 ? EMRTD_LE_MAX : sw2));
        }

        emrtd_apdu_read_binary(&command, (uint16_t)offset, exact);
        error = emrtd_worker_transmit(worker, &command, out);
    }

    return error;
}

/* --- The running hash --------------------------------------------------- */

static void emrtd_worker_digest_start(EmrtdWorker* worker) {
    EmrtdWorkerDigest* digest = &worker->digest;
    if(digest->active) {
        return;
    }

    /*
     * The algorithm is the one EF.SOD names, which is why EF.SOD is read
     * before any data group. Anything other than SHA-1 or SHA-256 is left
     * unhashed and reported as such rather than checked against the wrong sum.
     */
    if(!worker->result->has_sod || !worker->result->sod.digest_supported) {
        return;
    }

    if(worker->result->sod.digest_len == 20) {
        digest->sha256 = false;
        mbedtls_sha1_init(&digest->sha1);
        if(mbedtls_sha1_starts(&digest->sha1) != 0) {
            mbedtls_sha1_free(&digest->sha1);
            return;
        }
    } else if(worker->result->sod.digest_len == 32) {
        digest->sha256 = true;
        mbedtls_sha256_init(&digest->sha2);
        if(mbedtls_sha256_starts(&digest->sha2, 0) != 0) {
            mbedtls_sha256_free(&digest->sha2);
            return;
        }
    } else {
        return;
    }

    digest->active = true;
}

static void emrtd_worker_digest_update(EmrtdWorker* worker, const uint8_t* data, size_t len) {
    EmrtdWorkerDigest* digest = &worker->digest;
    if(!digest->active || len == 0) {
        return;
    }

    const int status = digest->sha256 ? mbedtls_sha256_update(&digest->sha2, data, len) :
                                        mbedtls_sha1_update(&digest->sha1, data, len);
    if(status != 0) {
        FURI_LOG_W(TAG, "Digest update failed");
        if(digest->sha256) {
            mbedtls_sha256_free(&digest->sha2);
        } else {
            mbedtls_sha1_free(&digest->sha1);
        }
        digest->active = false;
    }
}

/** Finish the hash and release the context. Returns its length, or zero. */
static size_t emrtd_worker_digest_finish(EmrtdWorker* worker) {
    EmrtdWorkerDigest* digest = &worker->digest;
    if(!digest->active) {
        return 0;
    }

    size_t len = 0;
    if(digest->sha256) {
        if(mbedtls_sha256_finish(&digest->sha2, worker->digest_value) == 0) {
            len = 32;
        }
        mbedtls_sha256_free(&digest->sha2);
    } else {
        if(mbedtls_sha1_finish(&digest->sha1, worker->digest_value) == 0) {
            len = 20;
        }
        mbedtls_sha1_free(&digest->sha1);
    }

    digest->active = false;
    return len;
}

static void emrtd_worker_digest_abort(EmrtdWorker* worker) {
    if(emrtd_worker_digest_finish(worker) > 0) {
        emrtd_secure_wipe(worker->digest_value, sizeof(worker->digest_value));
    }
}

/** Compare what was read against the sum EF.SOD lists for this data group. */
static void emrtd_worker_check_hash(
    EmrtdWorker* worker,
    const EmrtdFileInfo* info,
    EmrtdFileResult* entry,
    size_t digest_len) {
    if(info->dg_number < 0) {
        /*
         * EF.COM and EF.SOD carry no hash of their own - EF.SOD is the list -
         * so there is nothing to check rather than something missing.
         */
        entry->hash_state = EmrtdHashStateUnknown;
        return;
    }
    if(!worker->result->has_sod) {
        entry->hash_state = EmrtdHashStateUnknown;
        return;
    }
    if(!worker->result->sod.digest_supported || digest_len == 0) {
        entry->hash_state = EmrtdHashStateUnsupportedDigest;
        return;
    }

    const EmrtdSodHash* listed = emrtd_lds_sod_hash_for(&worker->result->sod, info->dg_number);
    if(listed == NULL) {
        entry->hash_state = EmrtdHashStateNotListed;
        return;
    }
    if(listed->hash_len != digest_len) {
        entry->hash_state = EmrtdHashStateUnsupportedDigest;
        return;
    }

    worker->result->hashes_checked++;
    if(memcmp(listed->hash, worker->digest_value, digest_len) == 0) {
        entry->hash_state = EmrtdHashStateMatch;
        worker->result->hashes_matched++;
    } else {
        FURI_LOG_W(TAG, "%s does not match the hash in EF.SOD", info->name);
        entry->hash_state = EmrtdHashStateMismatch;
    }
}

/* --- Reading one file --------------------------------------------------- */

/** How much of a file may be held in memory so that it can be decoded. */
static size_t emrtd_worker_parse_cap(EmrtdFileId id) {
    switch(id) {
    case EmrtdFileSod:
        /* The security object carries the hashes and the signer's certificate. */
        return 6 * 1024;
    case EmrtdFileCom:
        return 512;
    case EmrtdFileDg1:
        return 256;
    case EmrtdFileDg11:
    case EmrtdFileDg12:
    case EmrtdFileDg14:
    case EmrtdFileDg15:
        return 2 * 1024;
    default:
        /* Everything else, DG2 above all, is streamed and never assembled. */
        return 0;
    }
}

typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t len;
    bool overflowed;
} EmrtdWorkerParseBuffer;

/**
 * Stream the currently selected file to the export, the hash and, when it is
 * one of the small decodable ones, to a buffer.
 *
 * @param[in]  dg2   run the facial image look-ahead over the leading bytes
 * @param[out] entry updated with the size and the outcome
 */
static EmrtdError emrtd_worker_stream_file(
    EmrtdWorker* worker,
    const EmrtdFileInfo* info,
    bool dg2,
    EmrtdWorkerParseBuffer* parse,
    EmrtdFileResult* entry) {
    size_t offset = 0;
    size_t total = 0;
    bool sized = false;
    size_t look_len = 0;
    bool looking = dg2;
    EmrtdError error = EmrtdErrorNone;

    emrtd_worker_digest_start(worker);

    while(true) {
        if(worker->stop_requested) {
            error = EmrtdErrorCancelled;
            break;
        }

        size_t want = emrtd_worker_chunk_size(worker);
        if(sized) {
            if(offset >= total) {
                break;
            }
            if(total - offset < want) {
                want = total - offset;
            }
        }

        EmrtdResponseApdu response;
        error = emrtd_worker_read_binary(worker, offset, want, &response);
        if(error != EmrtdErrorNone) {
            break;
        }

        if(!emrtd_worker_sw_has_data(response.sw)) {
            /*
             * Past 32767 the read has had to switch to the odd instruction
             * form, which not every chip implements. A refusal there means the
             * rest of the file is out of reach, which is a different thing
             * from the file having ended, and the report should say so.
             */
            if(offset > EMRTD_WORKER_SHORT_OFFSET_MAX) {
                FURI_LOG_W(TAG, "%s: the chip refuses the long offset form", info->name);
                if(emrtd_worker_tracing(worker)) {
                    emrtd_export_trace_note(
                        worker->export_ctx,
                        "# error stage=read the chip refuses the odd instruction, so %s ends at %zu",
                        info->name,
                        offset);
                }
                error = EmrtdErrorUnsupported;
                break;
            }
            /*
             * 6B00 is what a chip says when the offset has run past the end of
             * the file, which after at least one good read just means the file
             * is finished rather than that anything went wrong.
             */
            if(response.sw == 0x6B00 && offset > 0) {
                if(emrtd_worker_tracing(worker)) {
                    emrtd_export_trace_note(
                        worker->export_ctx, "# eof reason=6B00 at=%zu", offset);
                }
                break;
            }
            error = emrtd_error_from_sw(response.sw);
            break;
        }
        if(response.data_len == 0) {
            if(emrtd_worker_tracing(worker)) {
                emrtd_export_trace_note(worker->export_ctx, "# eof reason=empty at=%zu", offset);
            }
            break;
        }

        size_t len = response.data_len;
        if(!sized) {
            /*
             * Every LDS file is one BER-TLV node, so its header states how
             * long the whole thing is and the reader knows after one exchange
             * how much is still to come.
             */
            total = emrtd_tlv_total_length(response.data, len);
            if(total == 0 || total > EMRTD_WORKER_FILE_SIZE_MAX) {
                FURI_LOG_W(TAG, "%s: cannot size from its header", info->name);
                if(emrtd_worker_tracing(worker)) {
                    emrtd_export_trace_note(
                        worker->export_ctx,
                        "# error stage=size the header of %s does not give a length",
                        info->name);
                }
                total = len;
            }
            sized = true;
            entry->size = total;

            /*
             * How long the file says it is, and how much of it the reader is
             * willing to ask for at a time. The second number is the one that
             * differs between chips, and a file that arrives short or in an
             * unexpected number of rounds is read from these two.
             */
            if(emrtd_worker_tracing(worker)) {
                emrtd_export_trace_note(
                    worker->export_ctx,
                    "# file name=%s size=%zu chunk=%zu",
                    info->name,
                    total,
                    emrtd_worker_chunk_size(worker));
            }
        }
        if(offset + len > total) {
            len = total - offset;
        }

        emrtd_worker_digest_update(worker, response.data, len);

        if(parse != NULL && !parse->overflowed) {
            if(parse->len + len > parse->capacity) {
                parse->overflowed = true;
            } else {
                memcpy(parse->data + parse->len, response.data, len);
                parse->len += len;
            }
        }

        if(looking) {
            /*
             * DG2 is the one file that will not fit in memory, so the image
             * has to be located before anything is written: the export can only
             * split out bytes it has not yet seen. Holding the head of the file
             * back for one or two exchanges is what buys that.
             */
            size_t room = EMRTD_WORKER_DG2_LOOKAHEAD - look_len;
            if(room > len) {
                room = len;
            }
            memcpy(worker->lookahead + look_len, response.data, room);
            look_len += room;

            EmrtdFaceImage face;
            const bool found = emrtd_lds_dg2_find_image(worker->lookahead, look_len, &face) &&
                               face.offset <= look_len;
            if(found) {
                worker->result->has_face = true;
                worker->result->face = face;
                emrtd_worker_export_arm_image(worker, face.offset, face.suffix);
            }

            if(found || look_len == EMRTD_WORKER_DG2_LOOKAHEAD || offset + len >= total) {
                looking = false;
                emrtd_worker_export_write(worker, worker->lookahead, look_len);
                look_len = 0;
                if(room < len) {
                    emrtd_worker_export_write(worker, response.data + room, len - room);
                }
            }
        } else {
            emrtd_worker_export_write(worker, response.data, len);
        }

        offset += len;

        if(!emrtd_worker_report(
               worker,
               EmrtdWorkerStageReadingFile,
               info->id,
               offset,
               total,
               emrtd_worker_file_percent(worker, offset, total),
               info->label)) {
            error = EmrtdErrorCancelled;
            break;
        }

        /* 6282 says the card reached the end of the file before filling Le. */
        if(response.sw == 0x6282) {
            if(emrtd_worker_tracing(worker)) {
                emrtd_export_trace_note(worker->export_ctx, "# eof reason=6282 at=%zu", offset);
            }
            break;
        }
    }

    /* A read that ended early leaves bytes still held back by the look-ahead. */
    if(look_len > 0) {
        emrtd_worker_export_write(worker, worker->lookahead, look_len);
    }

    if(error != EmrtdErrorNone) {
        emrtd_worker_digest_abort(worker);
        entry->size = offset;
        return error;
    }

    entry->size = offset;
    if(worker->result->has_face && dg2 && offset > worker->result->face.offset) {
        worker->result->face_size = offset - worker->result->face.offset;
    }

    const size_t digest_len = emrtd_worker_digest_finish(worker);
    emrtd_worker_check_hash(worker, info, entry, digest_len);
    emrtd_secure_wipe(worker->digest_value, sizeof(worker->digest_value));

    return EmrtdErrorNone;
}

/** Decode a file that was small enough to keep, and record what it held. */
static EmrtdError
    emrtd_worker_parse_file(EmrtdWorker* worker, EmrtdFileId id, const uint8_t* data, size_t len) {
    EmrtdReadResult* result = worker->result;

    switch(id) {
    case EmrtdFileCom: {
        const EmrtdError error = emrtd_lds_parse_com(data, len, &result->com);
        result->has_com = error == EmrtdErrorNone;
        return error;
    }
    case EmrtdFileSod: {
        const EmrtdError error = emrtd_lds_parse_sod(data, len, &result->sod);
        result->has_sod = error == EmrtdErrorNone;
        return error;
    }
    case EmrtdFileDg1: {
        const EmrtdError error = emrtd_lds_parse_dg1(data, len, &result->mrz);
        result->has_mrz = error == EmrtdErrorNone;
        return error;
    }
    case EmrtdFileDg11: {
        const EmrtdError error = emrtd_lds_parse_dg11(data, len, &result->dg11);
        result->has_dg11 = error == EmrtdErrorNone;
        return error;
    }
    case EmrtdFileDg12: {
        const EmrtdError error = emrtd_lds_parse_dg12(data, len, &result->dg12);
        result->has_dg12 = error == EmrtdErrorNone;
        return error;
    }
    case EmrtdFileDg14: {
        /*
         * DG14 repeats the SecurityInfos under the session. Where the chip let
         * EF.CardAccess be read first, the two agree and only the flags that
         * EF.CardAccess does not carry are taken from here.
         */
        EmrtdSecurityInfos infos;
        const EmrtdError error = emrtd_security_infos_parse(data, len, &infos);
        if(error != EmrtdErrorNone) {
            return error;
        }
        if(!result->card_access_read) {
            result->security_infos = infos;
        } else {
            result->security_infos.has_chip_auth |= infos.has_chip_auth;
            result->security_infos.has_terminal_auth |= infos.has_terminal_auth;
            result->security_infos.has_active_auth |= infos.has_active_auth;
        }
        return EmrtdErrorNone;
    }
    case EmrtdFileDg15: {
        const EmrtdError error = emrtd_lds_parse_dg15(data, len, &result->dg15);
        result->has_dg15 = error == EmrtdErrorNone;
        return error;
    }
    default:
        return EmrtdErrorNone;
    }
}

/** Select, stream, decode and score one file. */
static EmrtdError emrtd_worker_read_file(EmrtdWorker* worker, EmrtdFileId id) {
    const EmrtdFileInfo* info = emrtd_file_info(id);
    if(info == NULL) {
        return EmrtdErrorInternal;
    }

    EmrtdFileResult* entry = &worker->result->files[id];
    entry->state = EmrtdFileStateReading;
    entry->hash_state = EmrtdHashStateUnknown;
    entry->error = EmrtdErrorNone;
    entry->size = 0;

    if(!emrtd_worker_report(
           worker,
           EmrtdWorkerStageReadingFile,
           id,
           0,
           0,
           emrtd_worker_file_percent(worker, 0, 0),
           info->label)) {
        entry->state = EmrtdFileStateFailed;
        entry->error = EmrtdErrorCancelled;
        return EmrtdErrorCancelled;
    }

    uint16_t sw = 0;
    EmrtdError error = emrtd_worker_select_file(worker, info, &sw);
    if(error != EmrtdErrorNone) {
        entry->state = EmrtdFileStateFailed;
        entry->error = error;
        return error;
    }
    if(sw != 0x9000) {
        entry->error = emrtd_error_from_sw(sw);
        entry->state = entry->error == EmrtdErrorFileNotFound ? EmrtdFileStateAbsent :
                                                                EmrtdFileStateFailed;
        FURI_LOG_W(TAG, "SELECT %s: %04X", info->name, sw);
        if(emrtd_worker_tracing(worker)) {
            emrtd_export_trace_note(
                worker->export_ctx,
                "# done name=%s state=%s bytes=0 hash=unchecked",
                info->name,
                emrtd_worker_state_word(entry->state));
        }
        /* A file that is not there is not a reason to stop reading the rest. */
        worker->files_done++;
        return EmrtdErrorNone;
    }

    emrtd_worker_export_begin(worker, info);

    EmrtdWorkerParseBuffer parse = {0};
    EmrtdWorkerParseBuffer* parse_ptr = NULL;
    const size_t cap = emrtd_worker_parse_cap(id);
    /*
     * Asked before the allocation, not tested after it. pvPortMalloc does not
     * return NULL on this firmware: a request it cannot meet ends in
     * furi_crash("out of memory"), which reboots the device mid read. This is
     * the largest single allocation the reader makes after the radio thread -
     * six kilobytes for EF.SOD - and it is made deep into a read, when the
     * heap is at its most crowded. Going without it costs the decoded view of
     * the file; the bytes are still exported and still hashed.
     */
    if(cap > 0 && memmgr_heap_get_max_free_block() >= cap + EMRTD_WORKER_BLOCK_HEADER) {
        parse.data = malloc(cap);
        parse.capacity = cap;
        parse_ptr = &parse;
    } else if(cap > 0) {
        FURI_LOG_W(TAG, "No room to decode %s", info->name);
        if(emrtd_worker_tracing(worker)) {
            emrtd_export_trace_note(
                worker->export_ctx,
                "# error stage=decode no room for the %zu bytes %s needs; it is still exported",
                cap,
                info->name);
        }
    }

    error = emrtd_worker_stream_file(worker, info, id == EmrtdFileDg2, parse_ptr, entry);
    emrtd_worker_export_end(worker);

    if(error != EmrtdErrorNone) {
        entry->state = EmrtdFileStateFailed;
        entry->error = error;
    } else if(parse_ptr != NULL && parse.overflowed) {
        /* Read and exported in full, but too large to decode on this device. */
        FURI_LOG_W(TAG, "%s is larger than the decode buffer", info->name);
        entry->state = EmrtdFileStateFailed;
        entry->error = EmrtdErrorBufferTooSmall;
    } else {
        entry->state = EmrtdFileStateRead;
        if(parse_ptr != NULL && parse.len > 0) {
            const EmrtdError parse_error =
                emrtd_worker_parse_file(worker, id, parse.data, parse.len);
            if(parse_error != EmrtdErrorNone) {
                FURI_LOG_W(TAG, "%s did not decode", info->name);
                entry->error = parse_error;
            }
        }
    }

    if(parse.data != NULL) {
        /* The data groups hold personal details. free() clears the block on
         * every firmware this is built for (docs/platform.md, item 23); the
         * wipe repeats that in case a build does not. */
        emrtd_secure_wipe(parse.data, parse.capacity);
        free(parse.data);
    }

    /*
     * One line per file, whatever became of it. This is the line a reader of
     * the trace counts: as many of them as EF.COM announced, each with the
     * length the file turned out to be and what its hash did.
     */
    if(emrtd_worker_tracing(worker)) {
        emrtd_export_trace_note(
            worker->export_ctx,
            "# done name=%s state=%s bytes=%zu hash=%s",
            info->name,
            emrtd_worker_state_word(entry->state),
            entry->size,
            emrtd_worker_hash_word(entry->hash_state));
    }

    /* Counted whatever the outcome, so that the progress bar keeps moving. */
    worker->files_done++;
    return error;
}

/* --- EF.CardAccess ------------------------------------------------------ */

/**
 * Read EF.CardAccess, which lives in the master file and needs no session.
 *
 * ICAO 9303-11 9.2: this is the chip stating which access protocols it
 * supports, and it is the only thing a reader may look at before it has
 * authenticated. A chip that does not offer it is simply a BAC only document.
 */
static size_t
    emrtd_worker_read_card_access(EmrtdWorker* worker, uint8_t* buffer, size_t capacity) {
    const EmrtdFileInfo* info = &emrtd_worker_card_access_info;

    uint16_t sw = 0;
    const EmrtdError select = emrtd_worker_select_file(worker, info, &sw);
    if(select != EmrtdErrorNone) {
        /* The exchange failed, so there is no status word to report. */
        FURI_LOG_I(TAG, "EF.CardAccess could not be selected: %s", emrtd_error_text(select));
        return 0;
    }
    if(sw != 0x9000) {
        FURI_LOG_I(TAG, "EF.CardAccess is not available (%04X)", sw);
        return 0;
    }

    emrtd_worker_export_begin(worker, info);

    size_t offset = 0;
    size_t total = 0;
    bool sized = false;

    while(!worker->stop_requested) {
        size_t want = emrtd_worker_chunk_size(worker);
        if(sized) {
            if(offset >= total) {
                break;
            }
            if(total - offset < want) {
                want = total - offset;
            }
        }

        EmrtdResponseApdu response;
        if(emrtd_worker_read_binary(worker, offset, want, &response) != EmrtdErrorNone) {
            break;
        }
        if(!emrtd_worker_sw_has_data(response.sw) || response.data_len == 0) {
            break;
        }

        size_t len = response.data_len;
        if(!sized) {
            total = emrtd_tlv_total_length(response.data, len);
            if(total == 0 || total > capacity) {
                total = len < capacity ? len : capacity;
            }
            sized = true;
        }
        if(offset + len > total) {
            len = total - offset;
        }
        if(len == 0) {
            break;
        }

        memcpy(buffer + offset, response.data, len);
        emrtd_worker_export_write(worker, response.data, len);
        offset += len;

        if(response.sw == 0x6282) {
            break;
        }
    }

    emrtd_worker_export_end(worker);

    if(emrtd_worker_tracing(worker)) {
        emrtd_export_trace_note(
            worker->export_ctx,
            "# done name=%s state=%s bytes=%zu hash=unchecked",
            info->name,
            offset > 0 ? "read" : "failed",
            offset);
    }

    if(offset == 0) {
        return 0;
    }

    worker->result->card_access_read = true;
    if(emrtd_security_infos_parse(buffer, offset, &worker->result->security_infos) !=
       EmrtdErrorNone) {
        FURI_LOG_W(TAG, "EF.CardAccess did not parse");
    }
    FURI_LOG_I(TAG, "EF.CardAccess: %zu bytes", offset);
    return offset;
}

/* --- Access control ----------------------------------------------------- */

/** Try one driver. Leaves a session open in @c worker->sm when it succeeds. */
static EmrtdError emrtd_worker_try_driver(
    EmrtdWorker* worker,
    const EmrtdAccessDriver* driver,
    const uint8_t* card_access,
    size_t card_access_len) {
    FURI_LOG_I(TAG, "Trying %s", driver->name);

    EmrtdSm session;
    emrtd_secure_wipe(&session, sizeof(session));
    EmrtdAccessOutcome outcome;
    memset(&outcome, 0, sizeof(outcome));

    const EmrtdError error = driver->authenticate(
        worker->transceiver,
        card_access,
        card_access_len,
        &worker->config.credentials,
        &session,
        &outcome);
    if(error != EmrtdErrorNone) {
        emrtd_sm_clear(&session);
        FURI_LOG_W(TAG, "%s failed: %s", driver->name, emrtd_error_text(error));
        if(emrtd_worker_tracing(worker)) {
            emrtd_export_trace_note(
                worker->export_ctx,
                "# session protocol=%s result=failed %s",
                driver->name,
                emrtd_error_text(error));
        }
        return error;
    }

    /*
     * What opened the chip, never what opened it with: the summary names the
     * protocol, the cipher and the curve, and the keys derived from the
     * credentials stay where they are.
     */
    if(emrtd_worker_tracing(worker)) {
        emrtd_export_trace_note(
            worker->export_ctx,
            "# session protocol=%s result=open %s",
            driver->name,
            outcome.summary);
    }

    worker->sm = session;
    worker->sm_active = true;

    /*
     * Taken here and nowhere later. PACE re-selects the application inside the
     * session a few lines down, and that command moves the counter on; the SSC
     * the result is to show is the one the session started from. Only the
     * bytes the cipher actually uses are copied, so a 3DES session does not
     * appear to have thirty-two byte keys.
     *
     * Written straight into the result rather than staged in a local, because
     * this frame sits above the elliptic curve arithmetic on the NFC thread's
     * stack and eighty-odd bytes there are worth more than the symmetry with
     * the outcome above. `present` stays false until the session is proven, so
     * nothing reads them in the meantime.
     */
    EmrtdSessionKeys* keys = &worker->result->keys;
    keys->cipher = session.cipher;
    memcpy(keys->ks_enc, session.ks_enc, emrtd_cipher_key_size(session.cipher));
    memcpy(keys->ks_mac, session.ks_mac, emrtd_cipher_key_size(session.cipher));
    memcpy(keys->ssc, session.ssc, emrtd_cipher_block_size(session.cipher));

    emrtd_secure_wipe(&session, sizeof(session));

    if(driver->reselect_application) {
        /*
         * PACE leaves the master file selected, so the application has to be
         * chosen again - this time inside the session (9303-11, 4.4.4). A
         * session that cannot do even that is no use, so it is torn down and
         * the caller is free to try the next driver.
         */
        uint16_t sw = 0;
        const EmrtdError reselect = emrtd_worker_select_application(worker, &sw);
        const EmrtdError failure = reselect != EmrtdErrorNone ? reselect : emrtd_error_from_sw(sw);
        if(reselect != EmrtdErrorNone || sw != 0x9000) {
            FURI_LOG_E(TAG, "Re-select after %s failed (%04X)", driver->name, sw);
            worker->sm_active = false;
            emrtd_sm_clear(&worker->sm);
            emrtd_secure_wipe_object(keys);
            return failure;
        }
    }

    /*
     * Published only now that the session is known to work, and before the
     * progress report that follows, so that the scene has something true to
     * put on the screen the moment the event reaches it.
     */
    worker->result->access = outcome;
    worker->result->keys.present = true;
    worker->result->authenticated = true;
    worker->driver_name = driver->name;
    return EmrtdErrorNone;
}

/** The chip's own statement that the credentials do not open it. */
static bool emrtd_worker_is_key_verdict(EmrtdError error) {
    return error == EmrtdErrorWrongKey || error == EmrtdErrorPaceFailed;
}

/**
 * Open the chip.
 *
 * Every driver is asked what it makes of EF.CardAccess and the best answer is
 * tried first; a driver that fails is followed by the next, because a chip
 * that advertises PACE may still have to be opened with BAC. A method the user
 * pinned is the only one attempted, so that a deliberate choice is not quietly
 * overridden.
 */
static EmrtdError emrtd_worker_authenticate(
    EmrtdWorker* worker,
    const uint8_t* card_access,
    size_t card_access_len) {
    const size_t count = emrtd_access_driver_count();

    if(worker->config.method != EmrtdAccessMethodAuto) {
        const EmrtdAccessDriver* driver = emrtd_access_driver_for_method(worker->config.method);
        if(driver == NULL) {
            return EmrtdErrorNoAccessMethod;
        }

        EmrtdError reason = EmrtdErrorNone;
        if(driver->probe(card_access, card_access_len, &worker->config.credentials, &reason) ==
           EmrtdAccessScoreUnsupported) {
            return reason != EmrtdErrorNone ? reason : EmrtdErrorNoAccessMethod;
        }
        return emrtd_worker_try_driver(worker, driver, card_access, card_access_len);
    }

    EmrtdError best_error = EmrtdErrorNoAccessMethod;
    bool attempted = false;

    /* Highest score first; the registry order decides between equal scores. */
    for(int level = (int)EmrtdAccessScoreAnnounced; level >= (int)EmrtdAccessScorePossible;
        level--) {
        const EmrtdAccessScore score = (EmrtdAccessScore)level;
        for(size_t i = 0; i < count; i++) {
            if(worker->stop_requested) {
                return EmrtdErrorCancelled;
            }

            const EmrtdAccessDriver* driver = emrtd_access_driver_at(i);
            if(driver == NULL) {
                continue;
            }

            EmrtdError reason = EmrtdErrorNone;
            if(driver->probe(card_access, card_access_len, &worker->config.credentials, &reason) !=
               score) {
                if(reason != EmrtdErrorNone && best_error == EmrtdErrorNoAccessMethod) {
                    best_error = reason;
                }
                continue;
            }

            const EmrtdError error =
                emrtd_worker_try_driver(worker, driver, card_access, card_access_len);
            if(error == EmrtdErrorNone) {
                return EmrtdErrorNone;
            }

            if(error == EmrtdErrorCancelled) {
                return error;
            }

            /*
             * The first protocol to run is the one the chip asked for, and what
             * it said is the diagnosis; a fall back is a guess, often with a
             * different password, so nothing it says replaces that. Otherwise
             * a wrong CAN, followed by a BAC attempt the chip does not answer -
             * an identity card has no BAC to answer with - reaches the screen
             * as a document that moved away, and a CAN that has just opened
             * the chip can be blamed for the passport's MRZ that BAC tried
             * after it.
             *
             * When the protocol the chip announced could not be run at all,
             * the probe's reason is what is held - a curve out of reach, say -
             * and the fall back replaces it only with a verdict on the key it
             * used, which is the one thing it can add.
             */
            if(!attempted) {
                if(best_error == EmrtdErrorNoAccessMethod || emrtd_worker_is_key_verdict(error)) {
                    best_error = error;
                }
                attempted = true;
            }

            if(error == EmrtdErrorCardLost || error == EmrtdErrorTransport) {
                /* The document has gone; trying another protocol cannot help. */
                return best_error;
            }

            /*
             * A refused authentication can leave the chip in a state of its
             * own choosing, so the application is selected again before the
             * next driver is given a turn.
             */
            worker->sm_active = false;
            emrtd_sm_clear(&worker->sm);
            uint16_t sw = 0;
            if(emrtd_worker_select_application(worker, &sw) != EmrtdErrorNone) {
                return best_error;
            }
        }
    }

    return best_error;
}

/* --- The read ----------------------------------------------------------- */

/** Which data groups will be attempted, and therefore how progress is scaled. */
static size_t emrtd_worker_count_files(const EmrtdWorker* worker, EmrtdFileMask present) {
    size_t count = 2; /* EF.COM and EF.SOD are always attempted. */
    for(size_t i = (size_t)EmrtdFileDg1; i < (size_t)EmrtdFileCount; i++) {
        const EmrtdFileInfo* info = emrtd_file_info((EmrtdFileId)i);
        if(info == NULL || info->eac_protected) {
            continue;
        }
        if((present & EMRTD_FILE_BIT(i)) && (worker->config.files & EMRTD_FILE_BIT(i))) {
            count++;
        }
    }
    return count;
}

static void emrtd_worker_read_data_groups(EmrtdWorker* worker, EmrtdFileMask present) {
    for(size_t i = (size_t)EmrtdFileDg1; i < (size_t)EmrtdFileCount; i++) {
        if(worker->stop_requested) {
            return;
        }

        const EmrtdFileId id = (EmrtdFileId)i;
        const EmrtdFileInfo* info = emrtd_file_info(id);
        EmrtdFileResult* entry = &worker->result->files[i];
        if(info == NULL) {
            continue;
        }

        if(!(present & EMRTD_FILE_BIT(i))) {
            entry->state = EmrtdFileStateAbsent;
            continue;
        }
        if(info->eac_protected) {
            /*
             * DG3 and DG4 need Extended Access Control, which is a terminal
             * certificate issued by a state. They are announced but cannot be
             * read, and saying so is more useful than a failure.
             */
            entry->state = EmrtdFileStateSkipped;
            entry->error = EmrtdErrorUnsupported;
            if(emrtd_worker_tracing(worker)) {
                emrtd_export_trace_note(
                    worker->export_ctx,
                    "# skip name=%s reason=eac announced, and out of reach without a certificate",
                    info->name);
            }
            continue;
        }
        if(!(worker->config.files & EMRTD_FILE_BIT(i))) {
            entry->state = EmrtdFileStateSkipped;
            /*
             * A group the chip holds and the user switched off. Without this
             * line a trace of a partial read looks like a chip that hid it.
             */
            if(emrtd_worker_tracing(worker)) {
                emrtd_export_trace_note(
                    worker->export_ctx, "# skip name=%s reason=deselected", info->name);
            }
            continue;
        }

        const EmrtdError error = emrtd_worker_read_file(worker, id);
        if(error == EmrtdErrorCancelled) {
            return;
        }
        if(error == EmrtdErrorCardLost || error == EmrtdErrorTransport ||
           error == EmrtdErrorSecureMessaging) {
            /* The link or the session is gone; nothing further can be read. */
            worker->result->error = error;
            worker->result->error_file = id;
            return;
        }
    }
}

/**
 * The whole read, start to finish, on the NFC thread.
 *
 * The order is the one a real document accepts, and it is not arbitrary:
 * EF.CardAccess has to be read before the access protocol is chosen, and
 * EF.SOD has to be read before any data group so that the hashes can be
 * computed while the bytes stream past rather than from a second copy.
 */
static void emrtd_worker_read(EmrtdWorker* worker) {
    EmrtdReadResult* result = worker->result;

    if(!emrtd_worker_report(
           worker, EmrtdWorkerStageSelectingApplication, EmrtdFileCom, 0, 0, 2, NULL)) {
        result->error = EmrtdErrorCancelled;
        return;
    }

    uint16_t sw = 0;
    EmrtdError error = emrtd_worker_select_application(worker, &sw);
    if(error != EmrtdErrorNone) {
        result->error = error;
        return;
    }
    if(sw != 0x9000) {
        FURI_LOG_E(TAG, "SELECT application: %04X", sw);
        result->error = sw == 0x6A82 ? EmrtdErrorNotEmrtd : emrtd_error_from_sw(sw);
        return;
    }
    result->application_selected = true;

    /* EF.CardAccess, read in the clear before anything is negotiated. */
    if(!emrtd_worker_report(
           worker, EmrtdWorkerStageReadingCardAccess, EmrtdFileCom, 0, 0, 5, NULL)) {
        result->error = EmrtdErrorCancelled;
        return;
    }

    /* Asked before the allocation, for the reason given at the EF.SOD buffer:
     * a malloc that cannot be met takes the device down rather than returning
     * NULL, so the fallback below is only reachable if nothing is allocated. */
    uint8_t* card_access = NULL;
    size_t card_access_len = 0;
    if(memmgr_heap_get_max_free_block() >=
       EMRTD_WORKER_CARD_ACCESS_MAX + EMRTD_WORKER_BLOCK_HEADER) {
        card_access = malloc(EMRTD_WORKER_CARD_ACCESS_MAX);
        emrtd_secure_wipe(card_access, EMRTD_WORKER_CARD_ACCESS_MAX);
        card_access_len =
            emrtd_worker_read_card_access(worker, card_access, EMRTD_WORKER_CARD_ACCESS_MAX);
    } else {
        FURI_LOG_W(TAG, "No room for EF.CardAccess; access control will have to guess");
        if(emrtd_worker_tracing(worker)) {
            emrtd_export_trace_note(
                worker->export_ctx,
                "# error stage=card-access no room to read it, so the method is a guess");
        }
    }

    /*
     * Reading EF.CardAccess left the master file selected on most chips, so
     * the application is chosen again before a driver is handed the port; the
     * drivers are documented to start from there.
     */
    if(result->card_access_read) {
        sw = 0;
        const EmrtdError reselect = emrtd_worker_select_application(worker, &sw);
        if(reselect != EmrtdErrorNone) {
            FURI_LOG_W(TAG, "Re-select before authentication: %s", emrtd_error_text(reselect));
        } else if(sw != 0x9000) {
            FURI_LOG_W(TAG, "Re-select before authentication: %04X", sw);
        }
    }

    if(worker->stop_requested) {
        free(card_access);
        result->error = EmrtdErrorCancelled;
        return;
    }

    if(!emrtd_worker_report(worker, EmrtdWorkerStageAuthenticating, EmrtdFileCom, 0, 0, 10, NULL)) {
        free(card_access);
        result->error = EmrtdErrorCancelled;
        return;
    }

    error = emrtd_worker_authenticate(worker, card_access, card_access_len);
    if(card_access != NULL) {
        emrtd_secure_wipe(card_access, EMRTD_WORKER_CARD_ACCESS_MAX);
        free(card_access);
    }
    if(error != EmrtdErrorNone) {
        result->error = error;
        return;
    }

    FURI_LOG_I(TAG, "Opened with %s", result->access.summary);
    /*
     * The driver names are string literals held by the registry, so they stay
     * valid long after this report has been turned into an event - which the
     * progress structure requires of @c detail.
     */
    if(!emrtd_worker_report(
           worker,
           EmrtdWorkerStageAuthenticating,
           EmrtdFileCom,
           0,
           0,
           EMRTD_WORKER_FILES_BASE,
           worker->driver_name)) {
        result->error = EmrtdErrorCancelled;
        return;
    }

    /*
     * EF.COM names what is on the chip, so the plan is only an estimate until
     * it has been read; without it the default set is attempted instead.
     */
    worker->files_total = emrtd_worker_count_files(worker, emrtd_file_default_mask());
    const EmrtdError com_error = emrtd_worker_read_file(worker, EmrtdFileCom);
    if(com_error == EmrtdErrorCardLost || com_error == EmrtdErrorTransport ||
       com_error == EmrtdErrorSecureMessaging || com_error == EmrtdErrorCancelled) {
        result->error = com_error;
        result->error_file = EmrtdFileCom;
        return;
    }

    const EmrtdFileMask present = result->has_com ? result->com.present :
                                                    emrtd_file_default_mask();
    worker->files_total = emrtd_worker_count_files(worker, present);

    if(worker->stop_requested) {
        result->error = EmrtdErrorCancelled;
        return;
    }

    /*
     * EF.SOD comes next, before any data group: it names the digest algorithm,
     * and without that the hashes would have to be computed from a second copy
     * of every file, which there is no room for.
     */
    const EmrtdError sod_error = emrtd_worker_read_file(worker, EmrtdFileSod);
    if(sod_error == EmrtdErrorCardLost || sod_error == EmrtdErrorTransport ||
       sod_error == EmrtdErrorSecureMessaging) {
        result->error = sod_error;
        result->error_file = EmrtdFileSod;
        return;
    }
    if(worker->stop_requested) {
        result->error = EmrtdErrorCancelled;
        return;
    }

    emrtd_worker_read_data_groups(worker, present);
    if(worker->stop_requested && result->error == EmrtdErrorNone) {
        result->error = EmrtdErrorCancelled;
        return;
    }

    /*
     * Each file was compared with EF.SOD as it streamed past, so by now the
     * verdict is complete; the stage exists so that the screen can show it
     * before the export begins.
     */
    emrtd_worker_report(
        worker,
        EmrtdWorkerStageVerifying,
        result->error_file,
        result->hashes_matched,
        result->hashes_checked,
        92,
        NULL);
}

/* --- Export ------------------------------------------------------------- */

static void emrtd_worker_export_result(EmrtdWorker* worker) {
    if(worker->export_ctx == NULL) {
        return;
    }

    emrtd_worker_report(worker, EmrtdWorkerStageExporting, EmrtdFileCom, 0, 0, 95, NULL);

    if(worker->result->has_mrz) {
        emrtd_export_write_mrz(worker->export_ctx, &worker->result->mrz);
    }

    /*
     * The trace ends where the read did, so that a file which stops in the
     * middle is telling a reader something - the card went away - rather than
     * leaving them to wonder whether the log itself was cut short.
     */
    if(emrtd_worker_tracing(worker)) {
        const EmrtdError error = worker->result->error;
        emrtd_export_trace_note(
            worker->export_ctx,
            "# end files=%u hashes=%u/%u %s",
            (unsigned)worker->files_done,
            (unsigned)worker->result->hashes_matched,
            (unsigned)worker->result->hashes_checked,
            error == EmrtdErrorNone ? "the read finished" : emrtd_error_text(error));
    }

    emrtd_export_write_report(worker->export_ctx, worker->result, &worker->config);

    if(!emrtd_export_failed(worker->export_ctx)) {
        worker->result->exported = true;
        snprintf(
            worker->result->export_path,
            sizeof(worker->result->export_path),
            "%s",
            emrtd_export_path(worker->export_ctx));
    }
}

/* --- The poller callback ------------------------------------------------ */

/** Record how far a failed read got, once, before anything reports on it. */
static void emrtd_worker_mark_stop(EmrtdWorker* worker) {
    if(worker->result->error != EmrtdErrorNone &&
       worker->result->error_stage == EmrtdWorkerStageIdle) {
        worker->result->error_stage = worker->stage;
    }
}

/** Everything the callback is allowed to do once the read is over. */
static NfcCommand emrtd_worker_finish(EmrtdWorker* worker) {
    emrtd_worker_mark_stop(worker);

    worker->sm_active = false;
    emrtd_sm_clear(&worker->sm);
    emrtd_iso14443_4_set_trace(worker->transport, NULL, NULL);
    emrtd_iso14443_4_unbind(worker->transport);

    /*
     * The worker is started and stopped over and over, and these buffers were
     * last holding the plaintext of somebody's data page.
     */
    emrtd_secure_wipe(worker->command, EMRTD_WORKER_APDU_BUFFER_SIZE);
    emrtd_secure_wipe(worker->response, EMRTD_WORKER_APDU_BUFFER_SIZE);
    emrtd_secure_wipe(worker->lookahead, EMRTD_WORKER_DG2_LOOKAHEAD);
    emrtd_secure_wipe(worker->digest_value, sizeof(worker->digest_value));

    furi_event_flag_set(worker->events, EMRTD_WORKER_FLAG_FINISHED);
    return NfcCommandStop;
}

/**
 * Drive the read from the poller's Ready event.
 *
 * On type A the Ready event means anticollision and selection are done, and
 * nothing more: RATS is sent from emrtd_iso14443_4_bind_3a(), because the
 * waiting times the firmware's own ISO 14443-4A poller uses are too short for
 * a passport. So on that path the session is opened here, and failing to open
 * it is an ordinary activation failure rather than a dead read.
 *
 * The callback may answer Continue, Reset or Stop. Stopping the poller from
 * inside it would have the NFC thread wait for itself, so the flag above is
 * set and the control thread does the stopping.
 */
static NfcCommand emrtd_worker_poller_callback(NfcGenericEvent event, void* context) {
    EmrtdWorker* worker = context;

    if(worker->stop_requested) {
        worker->result->error = EmrtdErrorCancelled;
        return emrtd_worker_finish(worker);
    }

    bool ready = false;
    bool type_a = false;

    if(event.protocol == NfcProtocolIso14443_3a) {
        type_a = true;
        const Iso14443_3aPollerEvent* data = event.event_data;
        if(data->type == Iso14443_3aPollerEventTypeReady) {
            const EmrtdError error = emrtd_iso14443_4_bind_3a(worker->transport, event.instance);
            ready = error == EmrtdErrorNone;
            /*
             * The chip answered the anticollision a moment ago, so it is
             * there; what failed was the step that opens an ISO 14443-4
             * session with it.
             */
            worker->activation_error = ready ? EmrtdErrorNone : EmrtdErrorActivation;
        } else {
            worker->activation_error = EmrtdErrorCardLost;
        }
    } else if(event.protocol == NfcProtocolIso14443_4b) {
        const Iso14443_4bPollerEvent* data = event.event_data;
        if(data->type == Iso14443_4bPollerEventTypeReady) {
            emrtd_iso14443_4_bind_4b(
                worker->transport,
                event.instance,
                (const Iso14443_4bData*)nfc_poller_get_data(worker->poller));
            ready = true;
        } else {
            worker->activation_error = EmrtdErrorCardLost;
        }
    } else {
        worker->result->error = EmrtdErrorProtocol;
        return emrtd_worker_finish(worker);
    }

    if(!ready) {
        worker->activation_failures++;
        FURI_LOG_W(
            TAG,
            "Activation attempt %u of %u failed: %s",
            worker->activation_failures,
            (unsigned)EMRTD_WORKER_ACTIVATION_RETRIES,
            emrtd_error_text(worker->activation_error));

        if(worker->activation_failures >= EMRTD_WORKER_ACTIVATION_RETRIES) {
            worker->result->error = worker->activation_error != EmrtdErrorNone ?
                                        worker->activation_error :
                                        EmrtdErrorActivation;
            return emrtd_worker_finish(worker);
        }
        /*
         * Reported once and not on every retry: the poller repeats this event
         * as fast as the radio allows, and the screen is only told things a
         * person can see.
         */
        if(worker->activation_failures == 1) {
            emrtd_worker_report(
                worker, EmrtdWorkerStageWaitingForCard, EmrtdFileCom, 0, 0, 0, NULL);
        }
        /*
         * Reset drops the carrier for a moment, which is what puts a chip
         * that has already entered the ISO 14443-4 protocol state back where
         * anticollision can find it. The type B poller has no Reset branch,
         * so it is asked only to carry on.
         */
        return type_a ? NfcCommandReset : NfcCommandContinue;
    }

    worker->activation_failures = 0;
    worker->activation_error = EmrtdErrorNone;
    worker->transceiver = emrtd_iso14443_4_transceiver(worker->transport);
    if(emrtd_worker_tracing(worker)) {
        emrtd_iso14443_4_set_trace(worker->transport, emrtd_worker_trace, worker);

        /*
         * The card's own timing parameters head the trace, because they are
         * the first thing worth knowing about a read that failed on a
         * document which never moved. This is the one note whose tail is
         * prose rather than fields; see docs/trace.md.
         */
        char description[96];
        emrtd_iso14443_4_describe(worker->transport, description, sizeof(description));
        emrtd_export_trace_note(worker->export_ctx, "# card %s", description);
    }

    emrtd_worker_read(worker);
    /* Before the export, so that report.txt can say where the read stopped. */
    emrtd_worker_mark_stop(worker);
    emrtd_worker_export_result(worker);

    if(worker->result->error != EmrtdErrorNone) {
        FURI_LOG_W(TAG, "Read ended: %s", emrtd_error_text(worker->result->error));
    }

    /*
     * The stage that ends the read is announced by the control thread once the
     * poller is down and the export is closed, so that the result the scene
     * reads when the event arrives is the finished one.
     */
    return emrtd_worker_finish(worker);
}

/* --- The control thread ------------------------------------------------- */

/**
 * Ask whether a document of this flavour is in the field.
 *
 * A fresh poller is needed for every attempt: stopping one leaves the NFC
 * hardware unconfigured, and it is the allocation of the next that sets it up
 * again. This is how the firmware's own scanner works.
 */
static bool emrtd_worker_detect(EmrtdWorker* worker, NfcProtocol protocol) {
    NfcPoller* poller = nfc_poller_alloc(worker->nfc, protocol);
    const bool detected = nfc_poller_detect(poller);
    nfc_poller_free(poller);
    return detected;
}

static int32_t emrtd_worker_thread(void* context) {
    EmrtdWorker* worker = context;

    emrtd_worker_report(worker, EmrtdWorkerStageWaitingForCard, EmrtdFileCom, 0, 0, 0, NULL);

    /*
     * Detection and the read are not run over the same poller. Detecting with
     * the type A poller is worth it because that is what looks at the select
     * acknowledge and says whether the card speaks ISO 14443-4 at all, and it
     * does so without sending RATS. The read then runs over the type 3A
     * poller, so that the block transmission protocol - and with it the frame
     * waiting time a passport needs - belongs to this application. See
     * transport/emrtd_isodep.c.
     */
    NfcProtocol detected = NfcProtocolInvalid;
    while(!worker->stop_requested) {
        if(emrtd_worker_detect(worker, NfcProtocolIso14443_4a)) {
            detected = NfcProtocolIso14443_4a;
            break;
        }
        if(worker->stop_requested) {
            break;
        }
        if(emrtd_worker_detect(worker, NfcProtocolIso14443_4b)) {
            detected = NfcProtocolIso14443_4b;
            break;
        }
        furi_delay_ms(EMRTD_WORKER_DETECT_PAUSE_MS);
    }

    if(detected == NfcProtocolInvalid) {
        worker->result->error = EmrtdErrorCancelled;
        emrtd_worker_report(worker, EmrtdWorkerStageError, EmrtdFileCom, 0, 0, 0, NULL);
        return 0;
    }

    const bool type_a = detected == NfcProtocolIso14443_4a;
    const NfcProtocol protocol = type_a ? NfcProtocolIso14443_3a : NfcProtocolIso14443_4b;

    FURI_LOG_I(TAG, "Detected %s", type_a ? "ISO 14443-4A" : "ISO 14443-4B");

    /*
     * Export to SD is the master switch: with it off nothing is written at
     * all, the trace included. The trace used to open the export on its own,
     * which meant a user who had turned exporting off still got every data
     * group, the facial image and the report on the card as soon as they
     * turned the trace on to file a bug report.
     */
    if(worker->config.export_to_sd) {
        worker->export_ctx = emrtd_export_alloc(
            worker->config.credentials.document_number, worker->config.write_trace);
    }

    worker->poller = nfc_poller_alloc(worker->nfc, protocol);
    nfc_poller_start(worker->poller, emrtd_worker_poller_callback, worker);

    /*
     * The flag is only ever set from inside the poller callback, which the NFC
     * worker thread reaches after it has declared itself running - and
     * nfc_stop() insists on that. Waiting until the flag really is set, rather
     * than until the wait returns, is therefore what makes the stop below safe.
     */
    while((furi_event_flag_get(worker->events) & EMRTD_WORKER_FLAG_FINISHED) == 0) {
        furi_event_flag_wait(
            worker->events,
            EMRTD_WORKER_FLAG_FINISHED,
            FuriFlagWaitAny | FuriFlagNoClear,
            EMRTD_WORKER_WAIT_SLICE_MS);
    }

    nfc_poller_stop(worker->poller);
    nfc_poller_free(worker->poller);
    worker->poller = NULL;

    if(worker->export_ctx != NULL) {
        emrtd_export_free(worker->export_ctx);
        worker->export_ctx = NULL;
    }

    emrtd_worker_report(
        worker,
        worker->result->error == EmrtdErrorNone ? EmrtdWorkerStageDone : EmrtdWorkerStageError,
        worker->result->error_file,
        0,
        0,
        100,
        NULL);

    return 0;
}

/* --- The public interface ------------------------------------------------ */

EmrtdWorker* emrtd_worker_alloc(EmrtdReadResult* result) {
    furi_check(result);

    EmrtdWorker* worker = malloc(sizeof(EmrtdWorker));
    emrtd_secure_wipe(worker, sizeof(EmrtdWorker));
    worker->result = result;

    worker->events = furi_event_flag_alloc();
    worker->transport = emrtd_iso14443_4_alloc();
    worker->transceiver = emrtd_iso14443_4_transceiver(worker->transport);

    worker->command = malloc(EMRTD_WORKER_APDU_BUFFER_SIZE);
    worker->response = malloc(EMRTD_WORKER_APDU_BUFFER_SIZE);
    worker->lookahead = malloc(EMRTD_WORKER_DG2_LOOKAHEAD);
    furi_check(worker->command && worker->response && worker->lookahead);

    worker->config.files = emrtd_file_default_mask();
    worker->config.export_to_sd = true;

    return worker;
}

void emrtd_worker_free(EmrtdWorker* worker) {
    furi_check(worker);

    emrtd_worker_stop(worker);

    emrtd_iso14443_4_free(worker->transport);
    furi_event_flag_free(worker->events);

    /* Both buffers have held plaintext from the document. */
    emrtd_secure_wipe(worker->command, EMRTD_WORKER_APDU_BUFFER_SIZE);
    emrtd_secure_wipe(worker->response, EMRTD_WORKER_APDU_BUFFER_SIZE);
    emrtd_secure_wipe(worker->lookahead, EMRTD_WORKER_DG2_LOOKAHEAD);
    free(worker->command);
    free(worker->response);
    free(worker->lookahead);

    emrtd_sm_clear(&worker->sm);
    emrtd_secure_wipe(worker, sizeof(EmrtdWorker));
    free(worker);
}

void emrtd_worker_set_config(EmrtdWorker* worker, const EmrtdWorkerConfig* config) {
    furi_check(worker);
    furi_check(config);
    furi_check(!worker->running);

    worker->config = *config;
    if(worker->config.files == 0) {
        worker->config.files = emrtd_file_default_mask();
    }
}

void emrtd_worker_set_callback(EmrtdWorker* worker, EmrtdWorkerCallback callback, void* context) {
    furi_check(worker);
    furi_check(!worker->running);

    worker->callback = callback;
    worker->callback_context = context;
}

void emrtd_worker_start(EmrtdWorker* worker, struct Nfc* nfc) {
    furi_check(worker);
    furi_check(nfc);
    furi_check(!worker->running);

    emrtd_secure_wipe(worker->result, sizeof(EmrtdReadResult));
    worker->files_total = 0;
    worker->files_done = 0;
    worker->activation_failures = 0;
    worker->stage = EmrtdWorkerStageIdle;
    worker->driver_name = NULL;
    worker->sm_active = false;
    emrtd_sm_clear(&worker->sm);

    worker->nfc = (Nfc*)nfc;
    worker->stop_requested = false;
    furi_event_flag_clear(worker->events, EMRTD_WORKER_FLAG_FINISHED);

    worker->thread =
        furi_thread_alloc_ex("EmrtdWorker", EMRTD_WORKER_STACK_SIZE, emrtd_worker_thread, worker);
    worker->running = true;
    furi_thread_start(worker->thread);
}

void emrtd_worker_stop(EmrtdWorker* worker) {
    furi_check(worker);

    if(!worker->running) {
        return;
    }

    /*
     * The flag is raised before the join so that the read, which checks it at
     * every exchange, is already on its way out by the time the join begins.
     */
    worker->stop_requested = true;
    furi_thread_join(worker->thread);
    furi_thread_free(worker->thread);
    worker->thread = NULL;
    worker->running = false;
    worker->nfc = NULL;
}

const EmrtdReadResult* emrtd_worker_result(const EmrtdWorker* worker) {
    furi_check(worker);

    return worker->result;
}
