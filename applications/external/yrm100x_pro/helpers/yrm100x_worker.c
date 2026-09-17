#include "yrm100x_tag.h"
#include "yrm100x_worker.h"
#include "uhf_reader_settings.h"
#include <furi.h>
#include "debug_log.h"

/* Production diagnostics compile out through debug_log.h. */
#undef FURI_LOG_D
#undef FURI_LOG_I
#define FURI_LOG_D(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)

static const char* UHF_WK_TAG = "UHF_WK";

#define debug_bank_name(...)   ""
#define debug_log_tag_epc(...) ((void)0)

/**
 * File that handles the worker for the YRM100
 * @author frux-c
 * @author modified by haffnerriley
*/

// yrm100 module commands
UHFWorkerEvent verify_module_connected(UHFWorker* uhf_worker) {
    FURI_LOG_I(UHF_WK_TAG, "Verifying module connection...");
    char* hw_version = m100_get_hardware_version(uhf_worker->module);
    char* sw_version = m100_get_software_version(uhf_worker->module);
    char* manufacturer = m100_get_manufacturers(uhf_worker->module);
    // verify all data exists
    if(hw_version == NULL || sw_version == NULL || manufacturer == NULL) {
        FURI_LOG_E(UHF_WK_TAG, "Module verification failed - missing info");
        FURI_LOG_E(
            UHF_WK_TAG,
            "HW: %s, SW: %s, Mfg: %s",
            hw_version ? hw_version : "NULL",
            sw_version ? sw_version : "NULL",
            manufacturer ? manufacturer : "NULL");
        return UHFWorkerEventFail;
    }
    FURI_LOG_I(UHF_WK_TAG, "Module OK - HW:%s SW:%s Mfg:%s", hw_version, sw_version, manufacturer);
    return UHFWorkerEventSuccess;
}

UHFTag* send_polling_command(UHFWorker* uhf_worker) {
    // read epc bank
    UHFTag* uhf_tag = uhf_tag_alloc();
    if(!uhf_tag) {
        uhf_debug_heap(UHFDebugError, "poll_tag", "alloc_failed", 0U);
        UHF_E("MEM", "poll tag allocation failed");
        return NULL;
    }
    M100ResponseType status;
    int poll_attempts = 0;
    do {
        if(uhf_worker_stop_requested(uhf_worker)) {
            FURI_LOG_I(UHF_WK_TAG, "Polling aborted by user");
            uhf_tag_free(uhf_tag);
            return NULL;
        }
        poll_attempts++;
        status = m100_single_poll(uhf_worker->module, uhf_tag, uhf_worker);
        if(poll_attempts % 10 == 0) {
            FURI_LOG_I(UHF_WK_TAG, "Polling attempt %d, status=%d", poll_attempts, (int)status);
        }
        if(status != M100SuccessResponse) furi_delay_ms(1);
    } while(status != M100SuccessResponse);
    FURI_LOG_I(UHF_WK_TAG, "Tag polled successfully after %d attempts", poll_attempts);
    return uhf_tag;
}

//Modified by William Riley Haffner to use a default access password that is set through the ap UI
// Number of times to retry a single word_size probe when the tag fails to
// answer (RF dropout). A timeout/partial frame does NOT mean the bank ends
// here — the tag is momentarily out of range — so we retry before deciding.
// The bank boundary is signalled ONLY by a memory-overrun (0xA3); a timeout is
// never a boundary, so we retry generously to keep the length discovery exact.
#define BANK_READ_MAX_RETRIES 4

/*
 * Get Size Bank needs a stronger RF policy than ordinary reads.
 *
 * Reader response 0x09 / empty RX / failed Select are transient on the tested
 * YRM100X + tag combination and MUST NOT be interpreted as a memory boundary.
 * Only a stable MemoryOverrun is allowed to move the upper size bound.
 */
#define SIZE_RF_ATTEMPTS_PER_POINT 10U
#define SIZE_RF_REPOLL_EVERY       3U
#define SIZE_RF_REPOLL_ATTEMPTS    8U
#define SIZE_RF_RETRY_DELAY_MS     8U
#define SIZE_RF_VERIFY_ROUNDS      2U

#define FULL_DUMP_ATTEMPTS_FALLBACK 4U
#define FULL_DUMP_ATTEMPTS_MIN      1U
#define FULL_DUMP_ATTEMPTS_MAX      10U

static uint8_t uhf_worker_full_dump_max_attempts(const UHFWorker* worker) {
    if(!worker) return FULL_DUMP_ATTEMPTS_FALLBACK;

    uint8_t attempts = worker->FullDumpMaxAttempts;
    if(attempts < FULL_DUMP_ATTEMPTS_MIN || attempts > FULL_DUMP_ATTEMPTS_MAX) {
        return FULL_DUMP_ATTEMPTS_FALLBACK;
    }

    return attempts;
}

static uint8_t uhf_worker_clone_max_attempts(const UHFWorker* worker) {
    if(!worker) return UHF_READER_CLONE_ATTEMPTS_DEFAULT;

    uint8_t attempts = worker->CloneMaxAttempts;
    if(attempts < UHF_READER_CLONE_ATTEMPTS_MIN || attempts > UHF_READER_CLONE_ATTEMPTS_MAX) {
        return UHF_READER_CLONE_ATTEMPTS_DEFAULT;
    }

    return attempts;
}

static void clone_publish_attempt(UHFWorker* worker, uint8_t attempt, uint8_t total) {
    if(!worker) return;

    if(total == 0U) total = UHF_READER_CLONE_ATTEMPTS_DEFAULT;
    if(attempt == 0U) attempt = 1U;
    if(attempt > total) attempt = total;

    bool changed = worker->CloneAttemptCurrent != attempt || worker->CloneAttemptTotal != total;
    worker->CloneAttemptCurrent = attempt;
    worker->CloneAttemptTotal = total;

    if(changed && worker->callback && worker->state == UHFWorkerStateCloneWrite) {
        worker->callback(UHFWorkerEventCloneProgress, worker->ctx);
    }
}

#define SELECT_MAX_RETRIES 3

static M100ResponseType select_tag_bounded(UHFWorker* worker, UHFTag* tag) {
    M100ResponseType status = M100EmptyResponse;
    UHF_I("SELECT", "bounded select BEGIN max=%u", (unsigned int)SELECT_MAX_RETRIES);
    debug_log_tag_epc("select EPC", tag);

    for(int attempt = 0; attempt < SELECT_MAX_RETRIES; attempt++) {
        if(uhf_worker_stop_requested(worker)) return M100EmptyResponse;

        status = m100_set_select(worker->module, tag);
        UHF_I(
            "SELECT",
            "bounded select attempt=%d/%d status=%d",
            attempt + 1,
            SELECT_MAX_RETRIES,
            (int)status);
        if(status == M100SuccessResponse) return status;

        furi_delay_ms(3);
    }

    return status;
}

static size_t worker_bank_size(const UHFTag* tag, BankType bank) {
    if(!tag) return 0;

    switch(bank) {
    case ReservedBank:
        return tag->reserved ? tag->reserved->size : 0;
    case EPCBank:
        return tag->epc ? tag->epc->size : 0;
    case TIDBank:
        return tag->tid ? tag->tid->size : 0;
    case UserBank:
        return tag->user ? tag->user->size : 0;
    default:
        return 0;
    }
}

static const uint8_t* worker_bank_data(const UHFTag* tag, BankType bank) {
    if(!tag) return NULL;

    switch(bank) {
    case ReservedBank:
        return tag->reserved ? tag->reserved->data : NULL;
    case EPCBank:
        return tag->epc ? tag->epc->data : NULL;
    case TIDBank:
        return tag->tid ? tag->tid->data : NULL;
    case UserBank:
        return tag->user ? tag->user->data : NULL;
    default:
        return NULL;
    }
}

static void worker_bank_restore(UHFTag* tag, BankType bank, const uint8_t* data, size_t size) {
    if(!tag || !data || size == 0) return;

    switch(bank) {
    case ReservedBank:
        uhf_tag_set_reserved(tag, (uint8_t*)data, size);
        break;
    case EPCBank:
        uhf_tag_set_epc(tag, (uint8_t*)data, size);
        break;
    case TIDBank:
        uhf_tag_set_tid(tag, (uint8_t*)data, size);
        break;
    case UserBank:
        uhf_tag_set_user(tag, (uint8_t*)data, size);
        break;
    default:
        break;
    }
}

static void worker_bank_capture_best(
    const UHFTag* tag,
    BankType bank,
    uint8_t* best_data,
    size_t* best_size) {
    if(!tag || !best_data || !best_size) return;

    size_t current_size = worker_bank_size(tag, bank);
    const uint8_t* current_data = worker_bank_data(tag, bank);

    if(current_data && current_size > *best_size && current_size <= USER_MAX_BANK_SIZE) {
        memcpy(best_data, current_data, current_size);
        *best_size = current_size;
    }
}

static bool uhf_tag_bank_status_resolved(UHFTagBankStatus status) {
    return status == UHFTagBankReadable || status == UHFTagBankEmpty ||
           status == UHFTagBankLocked || status == UHFTagBankWrongPassword;
}

static bool uhf_tag_bank_status_is_partial(UHFTagBankStatus status) {
    return status == UHFTagBankLocked || status == UHFTagBankWrongPassword ||
           status == UHFTagBankReadError;
}

static UHFWorkerEvent
    read_bank_till_max_length(UHFWorker* uhf_worker, UHFTag* uhf_tag, BankType bank_type);

static const char* uhf_read_profile_name(uint8_t profile) {
    return profile == UHF_READER_TAG_PROFILE_MONZA4QT ? "Impinj Monza 4QT" : "Generic";
}

static bool uhf_tag_is_monza4qt_tid_prefix(const UHFTag* tag) {
    static const uint8_t monza4qt_prefix[4] = {0xE2, 0x80, 0x11, 0x05};

    return tag && tag->tid && tag->tid->size >= sizeof(monza4qt_prefix) &&
           memcmp(tag->tid->data, monza4qt_prefix, sizeof(monza4qt_prefix)) == 0;
}

/*
 * Read an exact TID word count with the same bounded RF-retry policy used by
 * the generic bank-length search.
 */
static UHFWorkerEvent read_tid_exact_words(UHFWorker* uhf_worker, UHFTag* tag, uint16_t words) {
    M100ResponseType status = M100ValidationFail;

    for(uint8_t retry = 0; retry < BANK_READ_MAX_RETRIES; retry++) {
        if(uhf_worker_stop_requested(uhf_worker)) {
            return UHFWorkerEventAborted;
        }

        M100ResponseType reselect = m100_set_select(uhf_worker->module, tag);

        UHF_T(
            "PROFILE",
            "TID exact words=%u retry=%u/%u reselect=%d",
            (unsigned int)words,
            (unsigned int)(retry + 1U),
            (unsigned int)BANK_READ_MAX_RETRIES,
            (int)reselect);

        status = m100_read_label_data_storage(
            uhf_worker->module, tag, TIDBank, uhf_worker->DefaultAP, words);

        if(status == M100SuccessResponse) return UHFWorkerEventSuccess;
        if(status == M100MemoryOverrun) return UHFWorkerEventFail;
        if(status == M100MemoryLocked) return UHFWorkerEventAccessDenied;
        if(status == M100APWrong) return UHFWorkerEventWrongPassword;
    }

    return UHFWorkerEventFail;
}

/*
 * Monza 4QT profile
 * -----------------
 * The original tag discussed for this project exposes a 24-byte MB2 window:
 *
 *   bytes  0..11 = 96-bit serialized TID
 *   bytes 12..23 = manufacturer-specific following area (EPC_Public in the
 *                  documented Private-profile memory map)
 *
 * The generic reader quite reasonably calls all 24 bytes "TID". This profile
 * deliberately separates them so the canonical TID remains exactly 12 bytes.
 *
 * If the E2801105 signature does not match, nothing is forced: the function
 * falls back to generic TID acquisition and preserves the other tag type.
 */
static UHFWorkerEvent read_tid_profiled(UHFWorker* uhf_worker, UHFTag* tag) {
    if(!tag) return UHFWorkerEventFail;

    tag->read_profile = uhf_worker->ReadProfile;
    tag->profile_match = false;
    tag->monza4qt_epc_public_size = 0;
    tag->monza4qt_epc_public_status = UHFTagBankUnknown;
    memset(tag->monza4qt_epc_public, 0, sizeof(tag->monza4qt_epc_public));

    if(uhf_worker->ReadProfile != UHF_READER_TAG_PROFILE_MONZA4QT) {
        return read_bank_till_max_length(uhf_worker, tag, TIDBank);
    }

    UHF_I(
        "PROFILE",
        "Monza4QT TID BEGIN request_window=%u bytes",
        (unsigned int)MONZA4QT_TID_WINDOW_SIZE);

    /*
     * First ask for the whole 24-byte window in one operation. The module's
     * read command starts at WordPtr 0, so 12 words gives exactly bytes 0..23.
     */
    UHFWorkerEvent event = read_tid_exact_words(uhf_worker, tag, MONZA4QT_TID_WINDOW_SIZE / 2U);

    if(event == UHFWorkerEventSuccess) {
        size_t raw_size = tag->tid->size;

        if(!uhf_tag_is_monza4qt_tid_prefix(tag)) {
            UHF_W(
                "PROFILE",
                "Monza4QT signature mismatch; fallback Generic raw_tid=%u",
                (unsigned int)raw_size);

            tag->profile_match = false;
            tag->monza4qt_epc_public_status = UHFTagBankUnknown;

            // Re-run normal acquisition so a non-Monza tag is never truncated.
            return read_bank_till_max_length(uhf_worker, tag, TIDBank);
        }

        tag->profile_match = true;

        uint8_t canonical_tid[MONZA4QT_CANONICAL_TID_SIZE];
        memcpy(canonical_tid, tag->tid->data, MONZA4QT_CANONICAL_TID_SIZE);

        if(raw_size >= MONZA4QT_TID_WINDOW_SIZE) {
            memcpy(
                tag->monza4qt_epc_public,
                tag->tid->data + MONZA4QT_CANONICAL_TID_SIZE,
                MONZA4QT_EPC_PUBLIC_SIZE);
            tag->monza4qt_epc_public_size = MONZA4QT_EPC_PUBLIC_SIZE;
            tag->monza4qt_epc_public_status = UHFTagBankReadable;
        } else {
            tag->monza4qt_epc_public_size = 0;
            tag->monza4qt_epc_public_status = UHFTagBankEmpty;
        }

        // IMPORTANT: only the first 96 bits are the canonical serialized TID.
        uhf_tag_set_tid(tag, canonical_tid, MONZA4QT_CANONICAL_TID_SIZE);

        UHF_I(
            "PROFILE",
            "Monza4QT MATCH canonical_tid=%u epc_public=%s/%u PC=0x%04X UMI=%u",
            (unsigned int)tag->tid->size,
            uhf_tag_bank_status_name(tag->monza4qt_epc_public_status),
            (unsigned int)tag->monza4qt_epc_public_size,
            (unsigned int)tag->epc->pc,
            (tag->epc->pc & 0x0400U) ? 1U : 0U);
        uhf_debug_hex(UHFDebugInfo, "PROFILE", "Monza4QT TID", tag->tid->data, tag->tid->size);
        if(tag->monza4qt_epc_public_size > 0) {
            uhf_debug_hex(
                UHFDebugInfo,
                "PROFILE",
                "Monza4QT EPC_Public",
                tag->monza4qt_epc_public,
                tag->monza4qt_epc_public_size);
        }
        uhf_debug_flush();

        return UHFWorkerEventSuccess;
    }

    /*
     * A different Monza profile/memory state may expose only the canonical
     * 96-bit TID. Memory-overrun on the 24-byte request is therefore not fatal:
     * attempt exactly 6 words. If E2801105 matches, record EPC_Public as EMPTY.
     */
    UHF_I(
        "PROFILE",
        "Monza4QT 24-byte window unavailable event=%d; try canonical 12 bytes",
        (int)event);

    event = read_tid_exact_words(uhf_worker, tag, MONZA4QT_CANONICAL_TID_SIZE / 2U);

    if(event != UHFWorkerEventSuccess) return event;

    if(!uhf_tag_is_monza4qt_tid_prefix(tag)) {
        UHF_W("PROFILE", "Monza4QT 12-byte signature mismatch; fallback Generic");
        return read_bank_till_max_length(uhf_worker, tag, TIDBank);
    }

    tag->profile_match = true;
    tag->monza4qt_epc_public_status = UHFTagBankEmpty;
    tag->monza4qt_epc_public_size = 0;

    if(tag->tid->size > MONZA4QT_CANONICAL_TID_SIZE) {
        uhf_tag_set_tid_size(tag, MONZA4QT_CANONICAL_TID_SIZE);
    }

    UHF_I(
        "PROFILE",
        "Monza4QT MATCH canonical-only TID=%u EPC_Public=EMPTY PC=0x%04X UMI=%u",
        (unsigned int)tag->tid->size,
        (unsigned int)tag->epc->pc,
        (tag->epc->pc & 0x0400U) ? 1U : 0U);
    uhf_debug_flush();

    return UHFWorkerEventSuccess;
}

static UHFWorkerEvent
    read_bank_till_max_length(UHFWorker* uhf_worker, UHFTag* uhf_tag, BankType bank) {
    unsigned int word_low = 0, word_high = 64;
    unsigned int word_size;
    M100ResponseType status;
    int iterations = 0;
    bool saw_one_word_overrun = false;

    /*
     * A later RF attempt can temporarily succeed with fewer words than an
     * earlier attempt. Keep the largest successfully returned prefix so a
     * PARTIAL capture contains the maximum real data ever observed.
     */
    uint8_t best_data[USER_MAX_BANK_SIZE];
    memset(best_data, 0, sizeof(best_data));

    size_t best_size = worker_bank_size(uhf_tag, bank);
    const uint8_t* existing = worker_bank_data(uhf_tag, bank);
    if(existing && best_size > 0 && best_size <= USER_MAX_BANK_SIZE) {
        memcpy(best_data, existing, best_size);
    } else {
        best_size = 0;
    }

    FURI_LOG_I(UHF_WK_TAG, "Reading bank %d, binary search: 0-64 words", (int)bank);
    UHF_I(
        "BANK",
        "BEGIN bank=%s(%d) range=0..64 carry_best=%u",
        debug_bank_name(bank),
        (int)bank,
        (unsigned int)best_size);

    do {
        if(uhf_worker_stop_requested(uhf_worker)) {
            worker_bank_restore(uhf_tag, bank, best_data, best_size);
            FURI_LOG_I(UHF_WK_TAG, "Bank read aborted by user");
            return UHFWorkerEventAborted;
        }

        if(word_low > word_high) {
            worker_bank_capture_best(uhf_tag, bank, best_data, &best_size);
            worker_bank_restore(uhf_tag, bank, best_data, best_size);
            UHF_I(
                "BANK",
                "COMPLETE bank=%s best_bytes=%u",
                debug_bank_name(bank),
                (unsigned int)best_size);
            return UHFWorkerEventSuccess;
        }

        word_size = (word_low + word_high) / 2;

        if(word_size == 0) {
            worker_bank_restore(uhf_tag, bank, best_data, best_size);
            /*
             * An overrun from a larger request only proves that the bank is
             * smaller than that request. It does NOT prove that the bank is
             * empty. Report zero bytes only when a one-word request itself
             * returned MemoryOverrun; transient/no-tag responses at one word
             * remain an RF failure.
             */
            if(best_size == 0U && !saw_one_word_overrun) {
                UHF_W(
                    "BANK",
                    "ZERO bank=%s without one-word overrun -> FAIL",
                    debug_bank_name(bank));
                return UHFWorkerEventFail;
            }
            UHF_I(
                "BANK",
                "ZERO bank=%s best_bytes=%u",
                debug_bank_name(bank),
                (unsigned int)best_size);
            return UHFWorkerEventSuccess;
        }

        iterations++;

        int retries = 0;
        do {
            if(uhf_worker_stop_requested(uhf_worker)) {
                worker_bank_restore(uhf_tag, bank, best_data, best_size);
                return UHFWorkerEventAborted;
            }

            M100ResponseType reselect = m100_set_select(uhf_worker->module, uhf_tag);

            UHF_T(
                "BANK",
                "probe bank=%s words=%u retry=%d reselect=%d best=%u",
                debug_bank_name(bank),
                word_size,
                retries,
                (int)reselect,
                (unsigned int)best_size);

            UHF_T(
                "BANK",
                "before READ bank=%s words=%u free_heap=%lu",
                debug_bank_name(bank),
                (unsigned int)word_size,
                (unsigned long)memmgr_get_free_heap());
            uhf_debug_flush();

            status = m100_read_label_data_storage(
                uhf_worker->module, uhf_tag, bank, uhf_worker->DefaultAP, word_size);

            if(status == M100SuccessResponse) {
                worker_bank_capture_best(uhf_tag, bank, best_data, &best_size);
            }

            UHF_T(
                "BANK",
                "result bank=%s words=%u retry=%d status=%d best=%u",
                debug_bank_name(bank),
                word_size,
                retries,
                (int)status,
                (unsigned int)best_size);

            if(status == M100SuccessResponse || status == M100MemoryOverrun ||
               status == M100APWrong || status == M100MemoryLocked) {
                break;
            }

            retries++;
        } while(retries < BANK_READ_MAX_RETRIES);

        if(status == M100SuccessResponse) {
            word_low = word_size + 1;
        } else if(status == M100MemoryOverrun) {
            if(word_size == 1U) saw_one_word_overrun = true;
            word_high = word_size - 1;
        } else if(status == M100MemoryLocked) {
            worker_bank_restore(uhf_tag, bank, best_data, best_size);
            UHF_W(
                "BANK",
                "TERMINAL bank=%s status=LOCKED best_bytes=%u",
                debug_bank_name(bank),
                (unsigned int)best_size);
            uhf_debug_flush();
            return UHFWorkerEventAccessDenied;
        } else if(status == M100APWrong) {
            worker_bank_restore(uhf_tag, bank, best_data, best_size);
            UHF_W(
                "BANK",
                "TERMINAL bank=%s status=WRONG_AP best_bytes=%u",
                debug_bank_name(bank),
                (unsigned int)best_size);
            uhf_debug_flush();
            return UHFWorkerEventWrongPassword;
        } else {
            UHF_W(
                "BANK",
                "TRANSIENT bank=%s unresolved words=%u retries=%u status=%d best_bytes=%u -> SHRINK",
                debug_bank_name(bank),
                word_size,
                (unsigned int)BANK_READ_MAX_RETRIES,
                (int)status,
                (unsigned int)best_size);
            /*
             * Some clone ICs answer an oversized User read with a transient
             * Gen2 code (for example 0x09) instead of MemoryOverrun. After the
             * bounded RF retries, treat that probe as an upper bound and keep
             * searching smaller sizes. If every size is transient, the
             * word_size==0 guard above still returns Fail rather than falsely
             * reporting an empty bank.
             */
            word_high = word_size - 1;
        }
    } while(true);
}

//Modified by Riley Haffner to read the reserved bank using the default AP set by the user
UHFWorkerEvent read_single_card(UHFWorker* uhf_worker) {
    FURI_LOG_I(UHF_WK_TAG, "=== Starting one-shot full read_single_card ===");
    m100_set_select_mode(uhf_worker->module, 0x02);

    UHFTagWrapper* wrapper = uhf_worker->uhf_tag_wrapper;
    uhf_tag_wrapper_reset_list(wrapper);

    UHFTag* uhf_tag = send_polling_command(uhf_worker);
    if(uhf_tag == NULL) {
        FURI_LOG_I(UHF_WK_TAG, "read_single_card: poll returned NULL");
        return UHFWorkerEventAborted;
    }

    M100ResponseType set_status = select_tag_bounded(uhf_worker, uhf_tag);
    if(uhf_worker_stop_requested(uhf_worker)) {
        uhf_tag_free(uhf_tag);
        return UHFWorkerEventAborted;
    }
    if(set_status != M100SuccessResponse) {
        FURI_LOG_W(UHF_WK_TAG, "set_select failed after bounded retries: %d", (int)set_status);
        uhf_tag_free(uhf_tag);
        return UHFWorkerEventFail;
    }

    UHFWorkerEvent event = read_tid_profiled(uhf_worker, uhf_tag);
    if(event != UHFWorkerEventSuccess) {
        uhf_tag_free(uhf_tag);
        return event;
    }

    event = read_bank_till_max_length(uhf_worker, uhf_tag, UserBank);
    if(event != UHFWorkerEventSuccess) {
        uhf_tag_free(uhf_tag);
        return event;
    }

    event = read_bank_till_max_length(uhf_worker, uhf_tag, ReservedBank);
    if(event != UHFWorkerEventSuccess) {
        uhf_tag_free(uhf_tag);
        return event;
    }

    uhf_tag->full_dump_complete = true;

    if(!uhf_tag_wrapper_add_tag(wrapper, uhf_tag)) {
        uhf_tag_free(uhf_tag);
        return UHFWorkerEventFail;
    }

    FURI_LOG_I(UHF_WK_TAG, "=== one-shot full read_single_card complete ===");
    return UHFWorkerEventSuccess;
}

// Deep-read a single, pre-selected tag from the multi-poll list.
// The caller populates uhf_worker->SelectedTag with the target EPC before starting
// the worker; here we select that specific EPC and read its TID, User and Reserved
// banks using the user's configured default access password.
static UHFWorkerEvent deep_read_tag(UHFWorker* uhf_worker, UHFTag* uhf_tag) {
    if(!uhf_tag || !uhf_tag->epc || uhf_tag->epc->size == 0) {
        UHF_E("FULL", "deep_read_tag invalid tag");
        return UHFWorkerEventFail;
    }

    uint32_t started = furi_get_tick();
    uhf_tag->dump_attempts++;

    /*
     * IMPORTANT:
     * Do not wipe successful banks on every whole-dump retry.
     * Different tag ICs/RF orientations can make TID succeed in one pass
     * and USER succeed in another. We resume unresolved banks and retain
     * the largest real byte prefix from previous attempts.
     */
    if(uhf_tag->dump_attempts == 1U) {
        uhf_tag_reset_dump_banks(uhf_tag);
    } else {
        uhf_tag->dump_finalized = false;
        uhf_tag->full_dump_complete = false;
    }

    const uint8_t max_attempts = uhf_worker_full_dump_max_attempts(uhf_worker);

    UHF_I(
        "FULL",
        "BEGIN attempt=%u/%u profile=%s rssi=%d epc=%u carry TID=%s/%u USER=%s/%u RSV=%s/%u",
        (unsigned int)uhf_tag->dump_attempts,
        (unsigned int)max_attempts,
        uhf_read_profile_name(uhf_worker->ReadProfile),
        (int)uhf_tag->epc->rssi,
        (unsigned int)uhf_tag->epc->size,
        uhf_tag_bank_status_name(uhf_tag->tid_status),
        (unsigned int)uhf_tag->tid->size,
        uhf_tag_bank_status_name(uhf_tag->user_status),
        (unsigned int)uhf_tag->user->size,
        uhf_tag_bank_status_name(uhf_tag->reserved_status),
        (unsigned int)uhf_tag->reserved->size);
    debug_log_tag_epc("full EPC", uhf_tag);

    M100ResponseType set_status = select_tag_bounded(uhf_worker, uhf_tag);
    if(uhf_worker_stop_requested(uhf_worker)) return UHFWorkerEventAborted;
    if(set_status != M100SuccessResponse) return UHFWorkerEventFail;

    UHFWorkerEvent event = UHFWorkerEventSuccess;

    // ---------------- TID ----------------
    if(!uhf_tag_bank_status_resolved(uhf_tag->tid_status)) {
        UHF_I("FULL", "TID BEGIN");
        event = read_tid_profiled(uhf_worker, uhf_tag);

        if(event == UHFWorkerEventSuccess) {
            uhf_tag->tid_status = uhf_tag->tid->size ? UHFTagBankReadable : UHFTagBankEmpty;
        } else if(event == UHFWorkerEventAccessDenied) {
            uhf_tag->tid_status = UHFTagBankLocked;
        } else if(event == UHFWorkerEventWrongPassword) {
            uhf_tag->tid_status = UHFTagBankWrongPassword;
        } else if(event == UHFWorkerEventAborted) {
            uhf_tag->tid_status = UHFTagBankAborted;
            return event;
        } else {
            uhf_tag->tid_status = UHFTagBankReadError;
            UHF_W(
                "FULL",
                "TID transient; keep best=%u and retry later",
                (unsigned int)uhf_tag->tid->size);
            return UHFWorkerEventFail;
        }
    } else {
        UHF_I(
            "FULL",
            "TID REUSE status=%s bytes=%u",
            uhf_tag_bank_status_name(uhf_tag->tid_status),
            (unsigned int)uhf_tag->tid->size);
    }

    // ---------------- USER ----------------
    if(!uhf_tag_bank_status_resolved(uhf_tag->user_status)) {
        UHF_I("FULL", "USER BEGIN");
        event = read_bank_till_max_length(uhf_worker, uhf_tag, UserBank);

        if(event == UHFWorkerEventSuccess) {
            uhf_tag->user_status = uhf_tag->user->size ? UHFTagBankReadable : UHFTagBankEmpty;
        } else if(event == UHFWorkerEventAccessDenied) {
            uhf_tag->user_status = UHFTagBankLocked;
        } else if(event == UHFWorkerEventWrongPassword) {
            uhf_tag->user_status = UHFTagBankWrongPassword;
        } else if(event == UHFWorkerEventAborted) {
            uhf_tag->user_status = UHFTagBankAborted;
            return event;
        } else {
            uhf_tag->user_status = UHFTagBankReadError;
            UHF_W(
                "FULL",
                "USER transient; keep best=%u and retry later",
                (unsigned int)uhf_tag->user->size);
            return UHFWorkerEventFail;
        }
    } else {
        UHF_I(
            "FULL",
            "USER REUSE status=%s bytes=%u",
            uhf_tag_bank_status_name(uhf_tag->user_status),
            (unsigned int)uhf_tag->user->size);
    }

    // ---------------- RESERVED ----------------
    if(!uhf_tag_bank_status_resolved(uhf_tag->reserved_status)) {
        UHF_I("FULL", "RESERVED BEGIN");
        event = read_bank_till_max_length(uhf_worker, uhf_tag, ReservedBank);

        if(event == UHFWorkerEventSuccess) {
            uhf_tag->reserved_status = uhf_tag->reserved->size ? UHFTagBankReadable :
                                                                 UHFTagBankEmpty;
        } else if(event == UHFWorkerEventAccessDenied) {
            uhf_tag->reserved_status = UHFTagBankLocked;
        } else if(event == UHFWorkerEventWrongPassword) {
            uhf_tag->reserved_status = UHFTagBankWrongPassword;
        } else if(event == UHFWorkerEventAborted) {
            uhf_tag->reserved_status = UHFTagBankAborted;
            return event;
        } else {
            uhf_tag->reserved_status = UHFTagBankReadError;
            UHF_W(
                "FULL",
                "RESERVED transient; keep best=%u and retry later",
                (unsigned int)uhf_tag->reserved->size);
            return UHFWorkerEventFail;
        }
    } else {
        UHF_I(
            "FULL",
            "RESERVED REUSE status=%s bytes=%u",
            uhf_tag_bank_status_name(uhf_tag->reserved_status),
            (unsigned int)uhf_tag->reserved->size);
    }

    bool partial = uhf_tag_bank_status_is_partial(uhf_tag->tid_status) ||
                   uhf_tag_bank_status_is_partial(uhf_tag->user_status) ||
                   uhf_tag_bank_status_is_partial(uhf_tag->reserved_status);

    uhf_tag->dump_finalized = true;
    uhf_tag->full_dump_complete = !partial;

    UHF_I(
        "FULL",
        "%s elapsed=%lu TID=%s/%u USER=%s/%u RSV=%s/%u",
        uhf_tag->full_dump_complete ? "COMPLETE" : "PARTIAL",
        (unsigned long)(furi_get_tick() - started),
        uhf_tag_bank_status_name(uhf_tag->tid_status),
        (unsigned int)uhf_tag->tid->size,
        uhf_tag_bank_status_name(uhf_tag->user_status),
        (unsigned int)uhf_tag->user->size,
        uhf_tag_bank_status_name(uhf_tag->reserved_status),
        (unsigned int)uhf_tag->reserved->size);
    uhf_debug_flush();

    return UHFWorkerEventSuccess;
}

static UHFWorkerEvent read_single_full_card(UHFWorker* uhf_worker) {
    FURI_LOG_I(UHF_WK_TAG, "=== Starting read_single_full_card ===");

    if(!m100_set_select_mode(uhf_worker->module, 0x02)) {
        UHF_W("FULLSINGLE", "Could not enforce Select Mode 0x02");
    }

    UHFTagWrapper* wrapper = uhf_worker->uhf_tag_wrapper;
    uhf_tag_wrapper_reset_list(wrapper);

    uhf_worker->FullMultiEpcCount = 0;
    uhf_worker->FullMultiDumpCount = 0;
    uhf_worker->FullMultiPartialCount = 0;

    /*
     * Exactly ONE tag is accepted for this run.
     * Once an EPC answers, that object becomes the permanent target; any other
     * tags entering the RF field later are irrelevant until the user starts a
     * new Read Full (Single).
     */
    UHFTag* target = send_polling_command(uhf_worker);
    if(!target) {
        UHF_I("FULLSINGLE", "Polling ended without a target");
        return UHFWorkerEventAborted;
    }

    if(!uhf_tag_wrapper_add_tag(wrapper, target)) {
        uhf_tag_free(target);
        return UHFWorkerEventFail;
    }

    uhf_worker->FullMultiEpcCount = 1;

    UHF_I(
        "FULLSINGLE",
        "TARGET LOCKED EPC acquired rssi=%d max_attempts=%u",
        (int)target->epc->rssi,
        (unsigned int)uhf_worker_full_dump_max_attempts(uhf_worker));
    debug_log_tag_epc("full-single EPC", target);
    uhf_debug_flush();

    const uint8_t max_attempts = uhf_worker_full_dump_max_attempts(uhf_worker);

    while(!uhf_worker_stop_requested(uhf_worker) && !target->dump_finalized &&
          target->dump_attempts < max_attempts) {
        UHF_I(
            "FULLSINGLE",
            "deep_read_tag CALL next_attempt=%u/%u",
            (unsigned int)(target->dump_attempts + 1U),
            (unsigned int)max_attempts);

        UHFWorkerEvent event = deep_read_tag(uhf_worker, target);

        UHF_I(
            "FULLSINGLE",
            "deep_read_tag RETURN event=%d finalized=%d complete=%d attempts=%u",
            (int)event,
            target->dump_finalized ? 1 : 0,
            target->full_dump_complete ? 1 : 0,
            (unsigned int)target->dump_attempts);
        uhf_debug_flush();

        if(event == UHFWorkerEventAborted || uhf_worker_stop_requested(uhf_worker)) {
            return UHFWorkerEventAborted;
        }

        if(target->dump_finalized) break;

        // Transient RF failure: retry the SAME EPC, preserving best bank data.
        furi_delay_ms(10);
    }

    if(!target->dump_finalized) {
        /*
         * EPC was definitely acquired, therefore Read Full (Single) must always
         * produce a terminal dump. Do not silently lose the capture merely
         * because one or more banks stayed unstable through all attempts.
         */
        if(target->tid_status == UHFTagBankUnknown || target->tid_status == UHFTagBankAborted)
            target->tid_status = UHFTagBankReadError;

        if(target->user_status == UHFTagBankUnknown || target->user_status == UHFTagBankAborted)
            target->user_status = UHFTagBankReadError;

        if(target->reserved_status == UHFTagBankUnknown ||
           target->reserved_status == UHFTagBankAborted)
            target->reserved_status = UHFTagBankReadError;

        target->dump_finalized = true;
        target->full_dump_complete = false;

        UHF_W(
            "FULLSINGLE",
            "Terminalized PARTIAL after %u/%u attempts: TID=%s/%u USER=%s/%u RSV=%s/%u",
            (unsigned int)target->dump_attempts,
            (unsigned int)max_attempts,
            uhf_tag_bank_status_name(target->tid_status),
            (unsigned int)target->tid->size,
            uhf_tag_bank_status_name(target->user_status),
            (unsigned int)target->user->size,
            uhf_tag_bank_status_name(target->reserved_status),
            (unsigned int)target->reserved->size);
    }

    if(target->full_dump_complete) {
        uhf_worker->FullMultiDumpCount = 1;
        UHF_I("FULLSINGLE", "COMPLETE -> FULL");
    } else {
        uhf_worker->FullMultiPartialCount = 1;
        UHF_W("FULLSINGLE", "COMPLETE -> PARTIAL");
    }

    uhf_debug_flush();
    return UHFWorkerEventSuccess;
}

UHFWorkerEvent deep_read_selected_card(UHFWorker* uhf_worker) {
    FURI_LOG_I(UHF_WK_TAG, "=== Starting deep_read_selected_card ===");
    UHFWorkerEvent event = deep_read_tag(uhf_worker, uhf_worker->SelectedTag);
    FURI_LOG_I(UHF_WK_TAG, "=== deep_read_selected_card result=%d ===", (int)event);
    return event;
}

/*
 * After a fast multi inventory stops, address every discovered EPC individually
 * and read TID + User + Reserved. Tags must remain in the RF field while this runs.
 *
 * A tag that temporarily cannot be read is left in the list with
 * full_dump_complete=false; the rest of the batch still continues.
 */
static UHFWorkerEvent deep_read_all_cards(UHFWorker* uhf_worker) {
    UHFTagWrapper* wrapper = uhf_worker->uhf_tag_wrapper;
    uhf_worker->DumpTotal = wrapper ? wrapper->tag_count : 0;
    uhf_worker->DumpCurrent = 0;

    if(!wrapper || wrapper->tag_count == 0) return UHFWorkerEventNoTagDetected;

    size_t completed = 0;

    for(size_t i = 0; i < wrapper->tag_count; i++) {
        if(uhf_worker_stop_requested(uhf_worker)) return UHFWorkerEventAborted;

        UHFTag* tag = wrapper->tags[i];
        UHFWorkerEvent event = UHFWorkerEventFail;

        // Three complete attempts cover brief RF fades without trapping the batch.
        for(uint8_t attempt = 0; attempt < 3; attempt++) {
            if(uhf_worker_stop_requested(uhf_worker)) return UHFWorkerEventAborted;

            event = deep_read_tag(uhf_worker, tag);
            if(event == UHFWorkerEventSuccess) break;
            if(event == UHFWorkerEventAborted) return event;
            furi_delay_ms(10);
        }

        if(event == UHFWorkerEventSuccess) {
            completed++;
        } else {
            FURI_LOG_W(UHF_WK_TAG, "Full dump incomplete for tag %d", (int)(i + 1));
        }

        uhf_worker->DumpCurrent = i + 1;
    }

    FURI_LOG_I(
        UHF_WK_TAG,
        "Full multi dump complete: %d/%d tags",
        (int)completed,
        (int)wrapper->tag_count);

    return completed > 0 ? UHFWorkerEventSuccess : UHFWorkerEventFail;
}

// Read a single memory bank (uhf_worker->TargetBank) from the already-known
// SelectedTag. Mirrors deep_read_selected_card but for one bank on demand, so
// the per-bank memory screens can fetch TID/Reserved/User individually.
UHFWorkerEvent read_single_bank_selected(UHFWorker* uhf_worker) {
    UHFTag* uhf_tag = uhf_worker->SelectedTag;
    BankType bank = uhf_worker->TargetBank;
    FURI_LOG_I(UHF_WK_TAG, "=== Starting read_single_bank_selected bank=%d ===", (int)bank);

    // Select the specific EPC first with bounded retries.
    M100ResponseType set_status = select_tag_bounded(uhf_worker, uhf_tag);
    if(uhf_worker_stop_requested(uhf_worker)) return UHFWorkerEventAborted;
    if(set_status != M100SuccessResponse) return UHFWorkerEventFail;

    UHFWorkerEvent event = read_bank_till_max_length(uhf_worker, uhf_tag, bank);
    if(event != UHFWorkerEventSuccess) {
        FURI_LOG_I(UHF_WK_TAG, "single bank %d read failed: %d", (int)bank, (int)event);
        return event;
    }

    FURI_LOG_I(UHF_WK_TAG, "=== read_single_bank_selected complete ===");
    return UHFWorkerEventSuccess;
}

//Modified by Riley Haffner to be able to write to the reserved bank
UHFWorkerEvent write_single_card(UHFWorker* uhf_worker) {
    //Targeted (unsaved) writes address a specific tag whose current EPC/PC/CRC is
    //preloaded into SelectedTag: select by that EPC and never poll a random tag.
    //Single-poll (saved) writes fall back to the first responder in the field.
    UHFTag* uhf_tag_des;
    bool owns_destination = false;
    UHFWorkerEvent result = UHFWorkerEventSuccess;
    bool targeted = uhf_worker->Targeted;
    uint32_t deadline = targeted ? furi_get_tick() + furi_ms_to_ticks(10000) : 0;

    if(targeted) {
        //Poll repeatedly and only proceed once the tag whose current EPC matches
        //our target answers. This targets the exact scanned tag AND gives us a
        //real tag object (correct PC/CRC/size) for the write frame — a fabricated
        //tag built from on-screen strings can carry an unread ("----") PC/CRC and
        //make the EPC write silently ineffective.
        UHFTag* target = uhf_worker->SelectedTag;
        UHFTag* probe = uhf_tag_alloc();
        if(!probe) {
            uhf_debug_heap(UHFDebugError, "write_target", "alloc_failed", 0U);
            // The write view handles Aborted as a terminal error and clears its
            // busy state; a generic Fail event is not part of that UI protocol.
            return UHFWorkerEventAborted;
        }
        bool found = false;
        while(!found) {
            if(uhf_worker_stop_requested(uhf_worker)) {
                uhf_tag_free(probe);
                return UHFWorkerEventAborted;
            }
            if(furi_get_tick() >= deadline) {
                uhf_tag_free(probe);
                return UHFWorkerEventAborted;
            }
            if(m100_single_poll(uhf_worker->module, probe, uhf_worker) != M100SuccessResponse) {
                continue;
            }
            if(probe->epc->size == target->epc->size &&
               memcmp(probe->epc->data, target->epc->data, target->epc->size) == 0) {
                found = true;
            }
        }
        //Copy the freshly polled tag's real EPC/PC/CRC into SelectedTag so the
        //select + write frames use accurate values.
        uhf_tag_set_epc(target, probe->epc->data, probe->epc->size);
        uhf_tag_set_epc_pc(target, probe->epc->pc);
        uhf_tag_set_epc_crc(target, probe->epc->crc);
        uhf_tag_free(probe);
        uhf_tag_des = target;
    } else {
        uhf_tag_des = send_polling_command(uhf_worker);
        if(uhf_tag_des == NULL) return UHFWorkerEventAborted;
        owns_destination = true;
    }

    UHFTag* uhf_tag_from = uhf_worker->NewTag;
    M100ResponseType rp_type;
    do {
        rp_type = m100_set_select(uhf_worker->module, uhf_tag_des);
        if(uhf_worker_stop_requested(uhf_worker)) {
            result = UHFWorkerEventAborted;
            goto cleanup;
        }
        if(targeted && furi_get_tick() >= deadline) {
            result = UHFWorkerEventAborted;
            goto cleanup;
        }
        if(rp_type == M100SuccessResponse) break;
    } while(true);
    while(m100_is_write_mask_enabled(uhf_worker->module, WRITE_USER)) {
        rp_type = m100_write_label_data_storage(
            uhf_worker->module, uhf_tag_from, uhf_tag_des, UserBank, 0, uhf_worker->DefaultAP);
        if(uhf_worker_stop_requested(uhf_worker)) {
            m100_disable_write_mask(uhf_worker->module, WRITE_USER);
            result = UHFWorkerEventAborted;
            goto cleanup;
        }
        if(targeted && furi_get_tick() >= deadline) {
            m100_disable_write_mask(uhf_worker->module, WRITE_USER);
            result = UHFWorkerEventAborted;
            goto cleanup;
        }
        if(rp_type == M100MemoryLocked) {
            m100_disable_write_mask(uhf_worker->module, WRITE_USER);
            result = UHFWorkerEventAccessDenied;
            goto cleanup;
        }
        if(rp_type == M100APWrong) {
            m100_disable_write_mask(uhf_worker->module, WRITE_USER);
            result = UHFWorkerEventWrongPassword;
            goto cleanup;
        }
        if(rp_type == M100SuccessResponse) {
            m100_disable_write_mask(uhf_worker->module, WRITE_USER);
            break;
        }
    }
    while(m100_is_write_mask_enabled(uhf_worker->module, WRITE_TID)) {
        rp_type = m100_write_label_data_storage(
            uhf_worker->module, uhf_tag_from, uhf_tag_des, TIDBank, 0, uhf_worker->DefaultAP);
        if(uhf_worker_stop_requested(uhf_worker)) {
            m100_disable_write_mask(uhf_worker->module, WRITE_TID);
            result = UHFWorkerEventAborted;
            goto cleanup;
        }
        if(targeted && furi_get_tick() >= deadline) {
            m100_disable_write_mask(uhf_worker->module, WRITE_TID);
            result = UHFWorkerEventAborted;
            goto cleanup;
        }
        if(rp_type == M100MemoryLocked) {
            m100_disable_write_mask(uhf_worker->module, WRITE_TID);
            result = UHFWorkerEventAccessDenied;
            goto cleanup;
        }
        if(rp_type == M100APWrong) {
            m100_disable_write_mask(uhf_worker->module, WRITE_TID);
            result = UHFWorkerEventWrongPassword;
            goto cleanup;
        }
        if(rp_type == M100SuccessResponse) {
            m100_disable_write_mask(uhf_worker->module, WRITE_TID);
            break;
        }
    }
    while(m100_is_write_mask_enabled(uhf_worker->module, WRITE_EPC)) {
        //The EPC bank's PC (Protocol Control) word encodes the EPC length in its
        //top 5 bits. When the new EPC differs in length from the tag's current
        //EPC we MUST write a matching PC or the tag backscatters the wrong length
        //and becomes unreadable. Take the real tag's lower PC bits (NSI/UMI/XI/
        //toggle) and overwrite the length field with the new EPC's word count.
        uint16_t base_pc = uhf_tag_get_epc_pc(uhf_tag_des);
        uint16_t new_words = uhf_tag_get_epc_size(uhf_tag_from) / 2;
        //Never commit a zero-length EPC: that clears the tag and makes it
        //undetectable. The caller validates length, but guard here too.
        if(new_words == 0) {
            m100_disable_write_mask(uhf_worker->module, WRITE_EPC);
            break;
        }
        uint16_t new_pc = (uint16_t)((base_pc & 0x07FF) | ((new_words & 0x1F) << 11));
        uhf_tag_set_epc_pc(uhf_tag_from, new_pc);
        rp_type = m100_write_label_data_storage(
            uhf_worker->module, uhf_tag_from, uhf_tag_des, EPCBank, 0, uhf_worker->DefaultAP);
        if(uhf_worker_stop_requested(uhf_worker)) {
            m100_disable_write_mask(uhf_worker->module, WRITE_EPC);
            result = UHFWorkerEventAborted;
            goto cleanup;
        }
        if(targeted && furi_get_tick() >= deadline) {
            m100_disable_write_mask(uhf_worker->module, WRITE_EPC);
            result = UHFWorkerEventAborted;
            goto cleanup;
        }
        if(rp_type == M100MemoryLocked) {
            m100_disable_write_mask(uhf_worker->module, WRITE_EPC);
            result = UHFWorkerEventAccessDenied;
            goto cleanup;
        }
        if(rp_type == M100APWrong) {
            m100_disable_write_mask(uhf_worker->module, WRITE_EPC);
            result = UHFWorkerEventWrongPassword;
            goto cleanup;
        }
        if(rp_type == M100SuccessResponse) {
            m100_disable_write_mask(uhf_worker->module, WRITE_EPC);
            break;
        }
    }
    while(m100_is_write_mask_enabled(uhf_worker->module, WRITE_RFU)) {
        if(uhf_worker->KillPwd && uhf_worker->AccessPwd) {
            rp_type = m100_write_label_data_storage(
                uhf_worker->module,
                uhf_tag_from,
                uhf_tag_des,
                ReservedBank,
                1,
                uhf_worker->DefaultAP);
        } else if(uhf_worker->KillPwd) {
            rp_type = m100_write_label_data_storage(
                uhf_worker->module,
                uhf_tag_from,
                uhf_tag_des,
                ReservedBank,
                0,
                uhf_worker->DefaultAP);
        } else if(uhf_worker->AccessPwd) {
            rp_type = m100_write_label_data_storage(
                uhf_worker->module,
                uhf_tag_from,
                uhf_tag_des,
                ReservedBank,
                32,
                uhf_worker->DefaultAP);
        }

        if(uhf_worker_stop_requested(uhf_worker)) {
            m100_disable_write_mask(uhf_worker->module, WRITE_RFU);
            result = UHFWorkerEventAborted;
            goto cleanup;
        }
        if(targeted && furi_get_tick() >= deadline) {
            m100_disable_write_mask(uhf_worker->module, WRITE_RFU);
            result = UHFWorkerEventAborted;
            goto cleanup;
        }
        if(rp_type == M100SuccessResponse) {
            m100_disable_write_mask(uhf_worker->module, WRITE_RFU);
            break;
        }
        if(rp_type == M100MemoryLocked) {
            m100_disable_write_mask(uhf_worker->module, WRITE_RFU);
            result = UHFWorkerEventAccessDenied;
            goto cleanup;
        }
        if(rp_type == M100APWrong) {
            m100_disable_write_mask(uhf_worker->module, WRITE_RFU);
            result = UHFWorkerEventWrongPassword;
            goto cleanup;
        }
    }

cleanup:
    if(owns_destination) {
        uhf_tag_free(uhf_tag_des);
    }
    return result;
}

/*
 * Read Full(Multi)
 * ----------------
 * This intentionally does NOT use the module's continuous multi-inventory stream.
 * A continuous inventory owns the UART until it is stopped, so memory reads cannot be
 * interleaved safely. Instead we repeatedly issue Single Poll:
 *
 *   EPC/PC/CRC/RSSI -> publish unique EPC -> full TID/User/Reserved read -> continue
 *
 * This is slower than Read (Multi), but every unique tag is fully captured immediately.
 * If a full read fails due to a short RF fade, the tag remains in the list and the next
 * time that EPC answers we retry its unfinished dump.
 */
static UHFTag* full_multi_find_tag(UHFTagWrapper* wrapper, const UHFTag* probe) {
    if(!wrapper || !probe || !probe->epc) return NULL;

    for(size_t i = 0; i < wrapper->tag_count; i++) {
        UHFTag* existing = wrapper->tags[i];
        if(existing && existing->epc && existing->epc->size == probe->epc->size &&
           memcmp(existing->epc->data, probe->epc->data, probe->epc->size) == 0) {
            return existing;
        }
    }

    return NULL;
}

static UHFWorkerEvent detect_multiple_full_cards(UHFWorker* uhf_worker) {
    FURI_LOG_I(UHF_WK_TAG, "=== Starting detect_multiple_full_cards ===");

    /*
     * Critical for EPC -> full dump -> next EPC:
     * Select parameters may point at the previously dumped tag.
     * Mode 0x02 explicitly prevents Select from filtering Inventory while still
     * applying it to Read/Write/Lock/Kill operations.
     */
    if(!m100_set_select_mode(uhf_worker->module, 0x02)) {
        FURI_LOG_W(UHF_WK_TAG, "Could not enforce Select Mode 0x02");
    }

    UHFTagWrapper* wrapper = uhf_worker->uhf_tag_wrapper;
    uhf_tag_wrapper_reset_list(wrapper);
    uhf_worker->FullMultiEpcCount = 0;
    uhf_worker->FullMultiDumpCount = 0;
    uhf_worker->FullMultiPartialCount = 0;

    UHFTag* probe = uhf_tag_alloc();
    if(!probe) return UHFWorkerEventFail;

    uint32_t loop_count = 0;
    uint32_t poll_misses = 0;
    UHF_I(
        "FULLMULTI",
        "LOOP BEGIN max_attempts=%u bank_retries=%u",
        (unsigned int)uhf_worker_full_dump_max_attempts(uhf_worker),
        (unsigned int)BANK_READ_MAX_RETRIES);
    uhf_debug_flush();

    while(!uhf_worker_stop_requested(uhf_worker) &&
          wrapper->tag_count < UHF_TAG_WRAPPER_MAX_TAGS) {
        loop_count++;
        uhf_tag_reset(probe);

        M100ResponseType poll_status = m100_single_poll(uhf_worker->module, probe, uhf_worker);

        if(poll_status != M100SuccessResponse) {
            poll_misses++;
            if((poll_misses % 25U) == 0U) {
                UHF_T(
                    "FULLMULTI",
                    "POLL MISS count=%lu loop=%lu status=%d tags=%u full=%u stop=%d",
                    (unsigned long)poll_misses,
                    (unsigned long)loop_count,
                    (int)poll_status,
                    (unsigned int)wrapper->tag_count,
                    (unsigned int)uhf_worker->FullMultiDumpCount,
                    uhf_worker_stop_requested(uhf_worker) ? 1 : 0);
            }
            furi_delay_ms(1);
            continue;
        }

        UHF_I(
            "FULLMULTI",
            "POLL SUCCESS loop=%lu rssi=%d tags_before=%u",
            (unsigned long)loop_count,
            (int)probe->epc->rssi,
            (unsigned int)wrapper->tag_count);
        debug_log_tag_epc("poll EPC", probe);

        UHFTag* target = full_multi_find_tag(wrapper, probe);
        if(target) {
            UHF_I(
                "FULLMULTI",
                "DUPLICATE finalized=%d complete=%d saved=%d attempts=%u",
                target->dump_finalized ? 1 : 0,
                target->full_dump_complete ? 1 : 0,
                target->auto_dump_saved ? 1 : 0,
                (unsigned int)target->dump_attempts);
            target->epc->rssi = probe->epc->rssi;
        } else {
            // Transfer this freshly polled object to the wrapper; allocate a new probe.
            target = probe;
            if(!uhf_tag_wrapper_add_tag(wrapper, target)) {
                uhf_tag_free(probe);
                return wrapper->tag_count > 0 ? UHFWorkerEventSuccess : UHFWorkerEventFail;
            }

            uhf_worker->FullMultiEpcCount = wrapper->tag_count;
            UHF_I(
                "FULLMULTI",
                "NEW TAG index=%u total=%u",
                (unsigned int)wrapper->tag_count,
                (unsigned int)uhf_worker->FullMultiEpcCount);
            uhf_debug_flush();
            probe = uhf_tag_alloc();
            if(!probe) return UHFWorkerEventFail;
        }

        if(target->dump_finalized) {
            continue;
        }

        UHF_I(
            "FULLMULTI",
            "deep_read_tag CALL full=%u partial=%u total=%u next_attempt=%u",
            (unsigned int)uhf_worker->FullMultiDumpCount,
            (unsigned int)uhf_worker->FullMultiPartialCount,
            (unsigned int)wrapper->tag_count,
            (unsigned int)(target->dump_attempts + 1U));

        UHFWorkerEvent dump_event = deep_read_tag(uhf_worker, target);

        UHF_I(
            "FULLMULTI",
            "deep_read_tag RETURN event=%d finalized=%d complete=%d attempts=%u stop=%d",
            (int)dump_event,
            target->dump_finalized ? 1 : 0,
            target->full_dump_complete ? 1 : 0,
            (unsigned int)target->dump_attempts,
            uhf_worker_stop_requested(uhf_worker) ? 1 : 0);
        uhf_debug_flush();

        if(dump_event == UHFWorkerEventAborted) {
            uhf_tag_free(probe);
            return UHFWorkerEventAborted;
        }

        if(dump_event == UHFWorkerEventSuccess && target->dump_finalized) {
            if(target->full_dump_complete) {
                uhf_worker->FullMultiDumpCount++;
            } else {
                uhf_worker->FullMultiPartialCount++;
            }
        } else {
            if(target->dump_attempts >= uhf_worker_full_dump_max_attempts(uhf_worker)) {
                if(target->tid_status == UHFTagBankUnknown)
                    target->tid_status = UHFTagBankReadError;
                if(target->user_status == UHFTagBankUnknown)
                    target->user_status = UHFTagBankReadError;
                if(target->reserved_status == UHFTagBankUnknown)
                    target->reserved_status = UHFTagBankReadError;

                target->dump_finalized = true;
                target->full_dump_complete = false;
                uhf_worker->FullMultiPartialCount++;

                UHF_W(
                    "FULLMULTI",
                    "PARTIAL max attempts=%u TID=%s/%u USER=%s/%u RSV=%s/%u; continue queue",
                    (unsigned int)target->dump_attempts,
                    uhf_tag_bank_status_name(target->tid_status),
                    (unsigned int)target->tid->size,
                    uhf_tag_bank_status_name(target->user_status),
                    (unsigned int)target->user->size,
                    uhf_tag_bank_status_name(target->reserved_status),
                    (unsigned int)target->reserved->size);
                uhf_debug_flush();
            } else {
                FURI_LOG_W(
                    UHF_WK_TAG,
                    "Full(Multi): transient failure %u/%u; retry on re-detect",
                    (unsigned int)target->dump_attempts,
                    (unsigned int)uhf_worker_full_dump_max_attempts(uhf_worker));
            }
            furi_delay_ms(5);
        }
    }

    UHF_I(
        "FULLMULTI",
        "LOOP EXIT loops=%lu misses=%lu tags=%u full=%u stop=%d",
        (unsigned long)loop_count,
        (unsigned long)poll_misses,
        (unsigned int)wrapper->tag_count,
        (unsigned int)uhf_worker->FullMultiDumpCount,
        uhf_worker_stop_requested(uhf_worker) ? 1 : 0);
    uhf_debug_flush();

    uhf_tag_free(probe);

    if(wrapper->tag_count > 0) return UHFWorkerEventSuccess;
    if(uhf_worker_stop_requested(uhf_worker)) return UHFWorkerEventAborted;
    return UHFWorkerEventNoTagDetected;
}

/*
 * Read Forever
 * ------------
 *
 * Unlike Read Full(Multi), this mode intentionally has NO session-wide EPC
 * deduplication. Each completed capture is an immutable history entry:
 *
 *   poll any EPC
 *   -> try to terminalize FULL/PARTIAL
 *   -> GUI thread saves FT/PT
 *   -> wait configured delay
 *   -> poll again, even if the same EPC is still present
 *
 * The worker never performs Storage access. It publishes a monotonically
 * increasing ForeverCaptureSerial and waits for the GUI dispatcher to advance
 * ForeverSavedSerial after uhf_auto_dump_save_tag() succeeds. The GUI bounds
 * the in-RAM history before acknowledging the next cycle.
 */
static void forever_terminalize_partial(UHFTag* tag) {
    furi_assert(tag);

    if(tag->tid_status == UHFTagBankUnknown || tag->tid_status == UHFTagBankAborted) {
        tag->tid_status = UHFTagBankReadError;
    }

    if(tag->user_status == UHFTagBankUnknown || tag->user_status == UHFTagBankAborted) {
        tag->user_status = UHFTagBankReadError;
    }

    if(tag->reserved_status == UHFTagBankUnknown || tag->reserved_status == UHFTagBankAborted) {
        tag->reserved_status = UHFTagBankReadError;
    }

    tag->dump_finalized = true;
    tag->full_dump_complete = false;
}

static bool forever_wait_interruptible(UHFWorker* worker, uint8_t seconds) {
    worker->ForeverWaiting = seconds > 0U;

    for(uint8_t remaining = seconds; remaining > 0U; remaining--) {
        worker->ForeverWaitRemainingSeconds = remaining;

        for(uint8_t slice = 0U; slice < 50U; slice++) {
            if(uhf_worker_stop_requested(worker)) {
                worker->ForeverWaitRemainingSeconds = 0U;
                worker->ForeverWaiting = false;
                return false;
            }
            furi_delay_ms(20U);
        }
    }

    worker->ForeverWaitRemainingSeconds = 0U;
    worker->ForeverWaiting = false;
    return !uhf_worker_stop_requested(worker);
}

static UHFWorkerEvent detect_forever_cards(UHFWorker* uhf_worker) {
    FURI_LOG_I(UHF_WK_TAG, "=== Starting detect_forever_cards ===");

    if(!m100_set_select_mode(uhf_worker->module, 0x02)) {
        UHF_W("FOREVER", "Could not enforce Select Mode 0x02");
    }

    UHFTagWrapper* wrapper = uhf_worker->uhf_tag_wrapper;
    uhf_tag_wrapper_reset_list(wrapper);

    uhf_worker->FullMultiEpcCount = 0;
    uhf_worker->FullMultiDumpCount = 0;
    uhf_worker->FullMultiPartialCount = 0;
    uhf_worker->ForeverCaptureSerial = 0;
    uhf_worker->ForeverSavedSerial = 0;
    uhf_worker->ForeverWaiting = false;
    uhf_worker->ForeverWaitRemainingSeconds = 0;

    uint8_t delay_seconds = uhf_worker->ForeverDelaySeconds;
    if(delay_seconds < UHF_READER_FOREVER_DELAY_MIN_SEC ||
       delay_seconds > UHF_READER_FOREVER_DELAY_MAX_SEC) {
        delay_seconds = UHF_READER_FOREVER_DELAY_DEFAULT_SEC;
    }

    const uint8_t max_attempts = uhf_worker_full_dump_max_attempts(uhf_worker);

    UHFTag* probe = uhf_tag_alloc();
    if(!probe) return UHFWorkerEventFail;

    uint32_t cycle = 0;

    UHF_I(
        "FOREVER",
        "LOOP BEGIN delay=%us max_attempts=%u",
        (unsigned int)delay_seconds,
        (unsigned int)max_attempts);
    uhf_debug_flush();

    while(!uhf_worker_stop_requested(uhf_worker)) {
        cycle++;

        UHFTag* slot = uhf_tag_alloc();
        if(!slot) {
            uhf_tag_free(probe);
            return UHFWorkerEventFail;
        }

        /*
         * Keep polling indefinitely until any tag answers or Back/Stop is
         * pressed. There is deliberately no EPC filter here.
         */
        uint32_t misses = 0;
        bool got_tag = false;

        while(!uhf_worker_stop_requested(uhf_worker)) {
            uhf_tag_reset(probe);

            M100ResponseType poll_status = m100_single_poll(uhf_worker->module, probe, uhf_worker);

            if(poll_status == M100SuccessResponse) {
                got_tag = true;
                break;
            }

            misses++;
            if((misses % 50U) == 0U) {
                UHF_T(
                    "FOREVER",
                    "WAIT cycle=%lu misses=%lu saved=%lu captures=%lu",
                    (unsigned long)cycle,
                    (unsigned long)misses,
                    (unsigned long)uhf_worker->ForeverSavedSerial,
                    (unsigned long)uhf_worker->ForeverCaptureSerial);
            }
            furi_delay_ms(2);
        }

        if(!got_tag || uhf_worker_stop_requested(uhf_worker)) {
            uhf_tag_free(slot);
            break;
        }

        /*
         * Publish a new immutable entry. The worker waits for GUI save/trim
         * acknowledgement before allocating the following capture.
         */
        uhf_tag_set_epc(slot, probe->epc->data, probe->epc->size);
        uhf_tag_set_epc_pc(slot, probe->epc->pc);
        uhf_tag_set_epc_crc(slot, probe->epc->crc);
        slot->epc->rssi = probe->epc->rssi;

        if(!uhf_tag_wrapper_add_tag(wrapper, slot)) {
            uhf_tag_free(slot);
            uhf_tag_free(probe);
            return UHFWorkerEventFail;
        }

        uhf_worker->FullMultiEpcCount++;
        uhf_worker->ForeverCaptureSerial++;
        uint32_t capture_serial = uhf_worker->ForeverCaptureSerial;

        UHF_I(
            "FOREVER",
            "EPC cycle=%lu serial=%lu total_epc=%u rssi=%d pc=0x%04X crc=0x%04X",
            (unsigned long)cycle,
            (unsigned long)capture_serial,
            (unsigned int)uhf_worker->FullMultiEpcCount,
            (int)slot->epc->rssi,
            (unsigned int)slot->epc->pc,
            (unsigned int)slot->epc->crc);
        debug_log_tag_epc("forever EPC", slot);
        uhf_debug_flush();

        while(!uhf_worker_stop_requested(uhf_worker) && !slot->dump_finalized &&
              slot->dump_attempts < max_attempts) {
            UHF_I(
                "FOREVER",
                "FULL CALL serial=%lu attempt=%u/%u",
                (unsigned long)capture_serial,
                (unsigned int)(slot->dump_attempts + 1U),
                (unsigned int)max_attempts);

            UHFWorkerEvent dump_event = deep_read_tag(uhf_worker, slot);

            UHF_I(
                "FOREVER",
                "FULL RETURN serial=%lu event=%d finalized=%d complete=%d attempts=%u",
                (unsigned long)capture_serial,
                (int)dump_event,
                slot->dump_finalized ? 1 : 0,
                slot->full_dump_complete ? 1 : 0,
                (unsigned int)slot->dump_attempts);
            uhf_debug_flush();

            if(dump_event == UHFWorkerEventAborted || uhf_worker_stop_requested(uhf_worker)) {
                uhf_tag_free(probe);
                return UHFWorkerEventAborted;
            }

            if(slot->dump_finalized) break;

            /*
             * A transient bank/RF failure does not lose this capture. Retry
             * the SAME EPC immediately, preserving successful bank prefixes.
             */
            furi_delay_ms(10);
        }

        if(!slot->dump_finalized) {
            forever_terminalize_partial(slot);

            UHF_W(
                "FOREVER",
                "PARTIAL serial=%lu after %u/%u attempts "
                "TID=%s/%u USER=%s/%u RSV=%s/%u",
                (unsigned long)capture_serial,
                (unsigned int)slot->dump_attempts,
                (unsigned int)max_attempts,
                uhf_tag_bank_status_name(slot->tid_status),
                (unsigned int)slot->tid->size,
                uhf_tag_bank_status_name(slot->user_status),
                (unsigned int)slot->user->size,
                uhf_tag_bank_status_name(slot->reserved_status),
                (unsigned int)slot->reserved->size);
            uhf_debug_flush();
        }

        if(slot->full_dump_complete) {
            uhf_worker->FullMultiDumpCount++;
        } else {
            uhf_worker->FullMultiPartialCount++;
        }

        UHF_I(
            "FOREVER",
            "CAPTURE READY serial=%lu type=%s totals E=%u F=%u P=%u; wait GUI save",
            (unsigned long)capture_serial,
            slot->full_dump_complete ? "FT" : "PT",
            (unsigned int)uhf_worker->FullMultiEpcCount,
            (unsigned int)uhf_worker->FullMultiDumpCount,
            (unsigned int)uhf_worker->FullMultiPartialCount);
        uhf_debug_flush();

        /*
         * GUI dispatcher owns Storage. It marks ForeverSavedSerial only after
         * uhf_auto_dump_save_tag() has returned success.
         *
         * On Back/Stop we break immediately; the read-view exit/finalize path
         * performs one last GUI-thread save of a terminal unsaved slot.
         */
        while(!uhf_worker_stop_requested(uhf_worker) &&
              uhf_worker->ForeverSavedSerial < capture_serial) {
            furi_delay_ms(10);
        }

        if(uhf_worker_stop_requested(uhf_worker)) break;

        UHF_I(
            "FOREVER",
            "SAVED serial=%lu; delay=%us before next capture",
            (unsigned long)capture_serial,
            (unsigned int)delay_seconds);
        uhf_debug_flush();

        if(!forever_wait_interruptible(uhf_worker, delay_seconds)) {
            break;
        }
    }

    UHF_I(
        "FOREVER",
        "LOOP EXIT cycles=%lu E=%u F=%u P=%u captures=%lu saved=%lu stop=%d",
        (unsigned long)cycle,
        (unsigned int)uhf_worker->FullMultiEpcCount,
        (unsigned int)uhf_worker->FullMultiDumpCount,
        (unsigned int)uhf_worker->FullMultiPartialCount,
        (unsigned long)uhf_worker->ForeverCaptureSerial,
        (unsigned long)uhf_worker->ForeverSavedSerial,
        uhf_worker_stop_requested(uhf_worker) ? 1 : 0);
    uhf_debug_flush();

    uhf_worker->ForeverWaitRemainingSeconds = 0U;
    uhf_worker->ForeverWaiting = false;

    uhf_tag_free(probe);

    return uhf_worker_stop_requested(uhf_worker) ? UHFWorkerEventAborted : UHFWorkerEventSuccess;
}

static UHFWorkerEvent detect_multiple_cards(UHFWorker* uhf_worker) {
    FURI_LOG_I(UHF_WK_TAG, "=== Starting detect_multiple_cards ===");
    m100_set_select_mode(uhf_worker->module, 0x02);
    // Clear any tags from a previous multi-poll round
    uhf_tag_wrapper_reset_list(uhf_worker->uhf_tag_wrapper);
    m100_multi_poll(uhf_worker->module, uhf_worker->uhf_tag_wrapper, uhf_worker);
    size_t tag_count = uhf_worker->uhf_tag_wrapper->tag_count;
    // Whenever we collected at least one tag, report success so the view displays
    // the list — even if the user pressed Stop to end the scan early.
    if(tag_count > 0) {
        return UHFWorkerEventSuccess;
    }
    if(uhf_worker_stop_requested(uhf_worker)) {
        FURI_LOG_I(UHF_WK_TAG, "detect_multiple_cards: aborted with no tags");
        return UHFWorkerEventAborted;
    }
    return UHFWorkerEventNoTagDetected;
}

// Live single-tag read (Read (Single)). The first successful poll defines the target
// for the entire scan. Later polls use a separate probe and software filtering: only
// a matching EPC may update the published slot, and it updates RSSI only. No hardware
// Select state is changed, so later Multi scans remain unaffected. The aggressive
// single-poll duty cycle preserves range; the view's 300ms timer snapshots the stable
// slot for live RSSI without worker-to-GUI events (ADR-0002).
static UHFWorkerEvent read_single_live(UHFWorker* uhf_worker) {
    FURI_LOG_I(UHF_WK_TAG, "=== Starting read_single_live ===");
    UHFTagWrapper* wrapper = uhf_worker->uhf_tag_wrapper;
    uhf_tag_wrapper_reset_list(wrapper);

    // The published slot has a fixed EPC for its entire lifetime. Poll into probe so
    // another responder can never overwrite data concurrently displayed by the GUI.
    UHFTag* slot = uhf_tag_alloc();
    UHFTag* probe = uhf_tag_alloc();
    if(!slot || !probe) {
        uhf_tag_free(slot);
        uhf_tag_free(probe);
        uhf_debug_heap(UHFDebugError, "read_single", "alloc_failed", 0U);
        return UHFWorkerEventFail;
    }
    bool published = false;

    while(!uhf_worker_stop_requested(uhf_worker)) {
        if(m100_single_poll(uhf_worker->module, probe, uhf_worker) != M100SuccessResponse) {
            continue; // No valid tag this round; keep hammering for maximum range.
        }

        if(!published) {
            uhf_tag_set_epc(slot, probe->epc->data, probe->epc->size);
            uhf_tag_set_epc_pc(slot, probe->epc->pc);
            uhf_tag_set_epc_crc(slot, probe->epc->crc);
            slot->epc->rssi = probe->epc->rssi;
            wrapper->tags[0] = slot;
            wrapper->tag_count = 1;
            published = true;
            continue;
        }

        // Ignore every later responder except the first EPC. For the locked tag,
        // refresh signal strength only; EPC/PC/CRC remain immutable on screen.
        if(probe->epc->size == slot->epc->size &&
           memcmp(probe->epc->data, slot->epc->data, slot->epc->size) == 0) {
            slot->epc->rssi = probe->epc->rssi;
        }
    }

    uhf_tag_free(probe);
    if(!published) {
        uhf_tag_free(slot);
        return UHFWorkerEventAborted;
    }
    // slot is owned by the wrapper now (freed by reset_list / wrapper_free).
    return UHFWorkerEventSuccess;
}

// Clone phase 1: poll for the first tag in field, giving up after 10 seconds.
// Stores the found tag in uhf_tag_wrapper->uhf_tag and fires CardDetected.
// Returns NoTagDetected on timeout, or Aborted if the user pressed Back.
static UHFWorkerEvent clone_scan_card(UHFWorker* uhf_worker) {
    UHFTag* uhf_tag = uhf_tag_alloc();
    if(!uhf_tag) {
        uhf_debug_heap(UHFDebugError, "clone_scan", "alloc_failed", 0U);
        return UHFWorkerEventFail;
    }
    M100ResponseType status;
    const uint32_t timeout_ticks = furi_ms_to_ticks(10000);
    const uint32_t start_tick = furi_get_tick();
    do {
        if(uhf_worker_stop_requested(uhf_worker)) {
            uhf_tag_free(uhf_tag);
            return UHFWorkerEventAborted;
        }
        if((furi_get_tick() - start_tick) >= timeout_ticks) {
            FURI_LOG_I(UHF_WK_TAG, "Clone scan timed out (no tag in 10s)");
            uhf_tag_free(uhf_tag);
            return UHFWorkerEventNoTagDetected;
        }
        status = m100_single_poll(uhf_worker->module, uhf_tag, uhf_worker);
    } while(status != M100SuccessResponse);
    uhf_tag_wrapper_set_tag(uhf_worker->uhf_tag_wrapper, uhf_tag);
    return UHFWorkerEventCardDetected;
}

// Clone phase 2: write selected banks from NewTag onto the first tag in field.
// Uses uhf_worker->CloneMask to determine which banks to write.
// Returns Success, Aborted, or AccessDenied (M100APWrong).
static bool clone_source_is_monza4qt_legacy_tid(const UHFTag* source) {
    static const uint8_t monza4qt_prefix[4] = {0xE2, 0x80, 0x11, 0x05};

    return source && source->tid && source->tid->size >= MONZA4QT_CANONICAL_TID_SIZE &&
           memcmp(source->tid->data, monza4qt_prefix, sizeof(monza4qt_prefix)) == 0;
}

static UHFWorkerEvent clone_write_bank_bounded(
    UHFWorker* uhf_worker,
    UHFTag* source,
    UHFTag* target,
    BankType bank,
    uint16_t source_address) {
    const uint8_t max_attempts = uhf_worker_clone_max_attempts(uhf_worker);

    for(uint8_t attempt = 1; attempt <= max_attempts; attempt++) {
        if(uhf_worker_stop_requested(uhf_worker)) {
            return UHFWorkerEventAborted;
        }

        clone_publish_attempt(uhf_worker, attempt, max_attempts);

        M100ResponseType status = m100_write_label_data_storage(
            uhf_worker->module, source, target, bank, source_address, uhf_worker->DefaultAP);

        UHF_I(
            "CLONE",
            "WRITE bank=%s attempt=%u/%u status=%d AP=0x%08lX",
            debug_bank_name(bank),
            (unsigned int)attempt,
            (unsigned int)max_attempts,
            (int)status,
            (unsigned long)uhf_worker->DefaultAP);
        uhf_debug_flush();

        if(status == M100SuccessResponse) return UHFWorkerEventSuccess;

        if(status == M100APWrong) {
            UHF_W(
                "CLONE",
                "WRITE bank=%s definitive access denied / wrong AP",
                debug_bank_name(bank));
            uhf_debug_flush();
            return UHFWorkerEventAccessDenied;
        }

        if(status == M100MemoryLocked) {
            UHF_W("CLONE", "WRITE bank=%s memory locked / not writable", debug_bank_name(bank));
            uhf_debug_flush();
            return UHFWorkerEventFail;
        }

        if(status == M100MemoryOverrun) return UHFWorkerEventFail;

        // NoTag / Validation / transient reader response: bounded retry.
        furi_delay_ms(10);
    }

    UHF_W(
        "CLONE",
        "WRITE bank=%s failed after %u retries",
        debug_bank_name(bank),
        (unsigned int)max_attempts);
    uhf_debug_flush();

    return UHFWorkerEventFail;
}

/*
 * User writes need a stricter retry policy than the legacy clone helper.
 *
 * A User write never changes the EPC used by Select, so it is safe and useful
 * to re-apply Select before every attempt.  Most importantly, transient
 * reader/tag errors (0x09, 0x10, timeout, checksum, validation) always retry
 * the SAME byte count. If all retries are exhausted, the fit layer may still
 * search for a shorter proven-writable prefix; it never treats 0x10 itself as
 * proof of the bank boundary.
 *
 * honor_stop=false is reserved for mandatory PC3400 rollback: once a temporary
 * marker has been written, Back/Stop must not prevent the best-effort restore.
 */
static UHFWorkerEvent clone_write_user_bytes_bounded(
    UHFWorker* worker,
    UHFTag* target,
    uint16_t start_word,
    const uint8_t* data,
    size_t data_size,
    bool honor_stop,
    M100ResponseType* final_status) {
    if(final_status) *final_status = M100ValidationFail;

    if(!worker || !target || !data || data_size == 0U || (data_size & 1U) != 0U ||
       data_size > USER_MAX_BANK_SIZE) {
        return UHFWorkerEventFail;
    }

    M100ResponseType status = M100ValidationFail;
    const uint8_t max_attempts = honor_stop ? uhf_worker_clone_max_attempts(worker) :
                                              UHF_READER_CLONE_ATTEMPTS_MAX;

    for(uint8_t attempt = 1U; attempt <= max_attempts; attempt++) {
        if(honor_stop && uhf_worker_stop_requested(worker)) {
            if(final_status) *final_status = M100EmptyResponse;
            return UHFWorkerEventAborted;
        }

        if(honor_stop) clone_publish_attempt(worker, attempt, max_attempts);

        M100ResponseType select_status = M100EmptyResponse;
        for(uint8_t select_attempt = 1U; select_attempt <= SELECT_MAX_RETRIES; select_attempt++) {
            if(honor_stop && uhf_worker_stop_requested(worker)) {
                if(final_status) *final_status = M100EmptyResponse;
                return UHFWorkerEventAborted;
            }

            select_status = m100_set_select(worker->module, target);
            if(select_status == M100SuccessResponse) break;
            furi_delay_ms(3);
        }

        if(select_status != M100SuccessResponse) {
            status = select_status;
            if(final_status) *final_status = status;
            UHF_W(
                "CLONE",
                "USER select attempt=%u/%u status=%d rollback=%u",
                (unsigned int)attempt,
                (unsigned int)max_attempts,
                (int)select_status,
                honor_stop ? 0U : 1U);
            furi_delay_ms(10);
            continue;
        }

        status = m100_write_raw_words(
            worker->module, UserBank, start_word, data, data_size, worker->DefaultAP);
        if(final_status) *final_status = status;

        UHF_I(
            "CLONE",
            "USER write attempt=%u/%u sa=%u bytes=%u status=%d rollback=%u",
            (unsigned int)attempt,
            (unsigned int)max_attempts,
            (unsigned int)start_word,
            (unsigned int)data_size,
            (int)status,
            honor_stop ? 0U : 1U);

        if(status == M100SuccessResponse) return UHFWorkerEventSuccess;
        if(status == M100MemoryOverrun) return UHFWorkerEventFail;
        if(status == M100APWrong) return UHFWorkerEventAccessDenied;
        if(status == M100MemoryLocked) return UHFWorkerEventFail;

        // Transient/unknown write failure: reselect and retry the same length.
        furi_delay_ms(10);
    }

    UHF_W(
        "CLONE",
        "USER write failed after=%u sa=%u bytes=%u status=%d rollback=%u",
        (unsigned int)max_attempts,
        (unsigned int)start_word,
        (unsigned int)data_size,
        (int)status,
        honor_stop ? 0U : 1U);
    return UHFWorkerEventFail;
}

static bool clone_user_fit_terminal(UHFWorkerEvent event, M100ResponseType status) {
    return event == UHFWorkerEventAborted || event == UHFWorkerEventAccessDenied ||
           status == M100APWrong || status == M100MemoryLocked;
}

/*
 * Write as much of a User payload as the target can demonstrably accept.
 *
 * Some rewritable tags reliably report B3/MemoryOverrun for an oversized
 * write, while the same boundary occasionally appears as generic 0x10 after
 * all retries. The old algorithm aborted on that 0x10 even after a shorter
 * prefix had already succeeded. Here both cases narrow the search, but only a
 * successful WRITE increases `best_words`; therefore a transient response can
 * never make us claim bytes that were not confirmed writable.
 */
static UHFWorkerEvent clone_write_user_fitted(
    UHFWorker* worker,
    UHFTag* target,
    const uint8_t* data,
    size_t requested_bytes,
    const char* label,
    size_t* written_bytes) {
    if(written_bytes) *written_bytes = 0U;
    if(!worker || !target || !data || !label || !written_bytes || requested_bytes == 0U ||
       requested_bytes > USER_MAX_BANK_SIZE || (requested_bytes & 1U) != 0U) {
        return UHFWorkerEventFail;
    }

    M100ResponseType status = M100ValidationFail;
    UHFWorkerEvent event =
        clone_write_user_bytes_bounded(worker, target, 0U, data, requested_bytes, true, &status);

    if(event == UHFWorkerEventSuccess) {
        *written_bytes = requested_bytes;
        UHF_I(
            "CLONE",
            "%s exact accepted=%u/%u",
            label,
            (unsigned int)*written_bytes,
            (unsigned int)requested_bytes);
        return event;
    }

    if(clone_user_fit_terminal(event, status)) {
        UHF_W(
            "CLONE",
            "%s terminal event=%d st=%d bytes=%u",
            label,
            (int)event,
            (int)status,
            (unsigned int)requested_bytes);
        return event;
    }

    const uint16_t requested_words = (uint16_t)(requested_bytes / 2U);
    uint16_t low_words = 1U;
    uint16_t high_words = requested_words > 0U ? (uint16_t)(requested_words - 1U) : 0U;
    uint16_t best_words = 0U;
    bool word_one_overrun = requested_words == 1U && status == M100MemoryOverrun;

    UHF_W(
        "CLONE",
        "%s FIT fallback requested=%u st=%d reason=%s range=1..%u",
        label,
        (unsigned int)requested_bytes,
        (int)status,
        status == M100MemoryOverrun ? "overrun" : "transient",
        (unsigned int)high_words);
    uhf_debug_flush();

    while(low_words <= high_words && high_words > 0U) {
        const uint16_t candidate_words =
            (uint16_t)(low_words + (uint16_t)((high_words - low_words) / 2U));
        const size_t candidate_bytes = (size_t)candidate_words * 2U;
        M100ResponseType candidate_status = M100ValidationFail;

        event = clone_write_user_bytes_bounded(
            worker, target, 0U, data, candidate_bytes, true, &candidate_status);

        if(event == UHFWorkerEventSuccess) {
            best_words = candidate_words;
            low_words = (uint16_t)(candidate_words + 1U);
            UHF_I(
                "CLONE",
                "%s FIT success words=%u best=%u next_low=%u",
                label,
                (unsigned int)candidate_words,
                (unsigned int)best_words,
                (unsigned int)low_words);
            continue;
        }

        if(clone_user_fit_terminal(event, candidate_status)) {
            UHF_W(
                "CLONE",
                "%s FIT terminal words=%u event=%d st=%d",
                label,
                (unsigned int)candidate_words,
                (int)event,
                (int)candidate_status);
            return event;
        }

        if(candidate_words == 1U && candidate_status == M100MemoryOverrun) {
            word_one_overrun = true;
        }

        UHF_W(
            "CLONE",
            "%s FIT shrink words=%u event=%d st=%d reason=%s best=%u",
            label,
            (unsigned int)candidate_words,
            (int)event,
            (int)candidate_status,
            candidate_status == M100MemoryOverrun ? "overrun" : "transient",
            (unsigned int)best_words);

        high_words = candidate_words > 0U ? (uint16_t)(candidate_words - 1U) : 0U;
    }

    if(best_words == 0U) {
        if(word_one_overrun) {
            UHF_I("CLONE", "%s FIT confirmed no User bank", label);
            return UHFWorkerEventSuccess;
        }

        UHF_W(
            "CLONE",
            "%s FIT failed: no writable prefix confirmed requested=%u",
            label,
            (unsigned int)requested_bytes);
        return UHFWorkerEventFail;
    }

    *written_bytes = (size_t)best_words * 2U;
    UHF_I(
        "CLONE",
        "%s FIT accepted=%u/%u",
        label,
        (unsigned int)*written_bytes,
        (unsigned int)requested_bytes);
    uhf_debug_flush();
    return UHFWorkerEventSuccess;
}

/* Exact one-word read used to preserve and verify PC-mode rollback state. */
static UHFWorkerEvent clone_read_user_word_bounded(
    UHFWorker* worker,
    UHFTag* target,
    uint16_t word,
    uint16_t* value,
    bool honor_stop,
    M100ResponseType* final_status) {
    if(final_status) *final_status = M100ValidationFail;
    if(!worker || !target || !value) return UHFWorkerEventFail;

    M100ResponseType status = M100ValidationFail;
    const uint8_t max_attempts = honor_stop ? uhf_worker_clone_max_attempts(worker) :
                                              UHF_READER_CLONE_ATTEMPTS_MAX;

    for(uint8_t attempt = 1U; attempt <= max_attempts; attempt++) {
        if(honor_stop && uhf_worker_stop_requested(worker)) {
            if(final_status) *final_status = M100EmptyResponse;
            return UHFWorkerEventAborted;
        }

        if(honor_stop) clone_publish_attempt(worker, attempt, max_attempts);

        M100ResponseType select_status = M100EmptyResponse;
        for(uint8_t select_attempt = 1U; select_attempt <= SELECT_MAX_RETRIES; select_attempt++) {
            if(honor_stop && uhf_worker_stop_requested(worker)) {
                if(final_status) *final_status = M100EmptyResponse;
                return UHFWorkerEventAborted;
            }

            select_status = m100_set_select(worker->module, target);
            if(select_status == M100SuccessResponse) break;
            furi_delay_ms(3);
        }

        if(select_status != M100SuccessResponse) {
            status = select_status;
            if(final_status) *final_status = status;
            UHF_W(
                "CLONEMODE",
                "User read select attempt=%u/%u status=%d rollback=%u",
                (unsigned int)attempt,
                (unsigned int)max_attempts,
                (int)select_status,
                honor_stop ? 0U : 1U);
            furi_delay_ms(10);
            continue;
        }

        uint8_t bytes[2] = {0U, 0U};
        size_t returned = 0U;
        status = m100_read_raw_words(
            worker->module,
            target,
            UserBank,
            word,
            1U,
            worker->DefaultAP,
            bytes,
            sizeof(bytes),
            &returned);
        if(final_status) *final_status = status;

        UHF_I(
            "CLONEMODE",
            "User read attempt=%u/%u word=%u status=%d bytes=%u rollback=%u",
            (unsigned int)attempt,
            (unsigned int)max_attempts,
            (unsigned int)word,
            (int)status,
            (unsigned int)returned,
            honor_stop ? 0U : 1U);

        if(status == M100SuccessResponse && returned == sizeof(bytes)) {
            *value = (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
            return UHFWorkerEventSuccess;
        }
        if(status == M100APWrong) return UHFWorkerEventAccessDenied;
        if(status == M100MemoryLocked || status == M100MemoryOverrun) {
            return UHFWorkerEventFail;
        }

        furi_delay_ms(10);
    }

    UHF_W(
        "CLONEMODE",
        "User read failed after=%u word=%u status=%d rollback=%u",
        (unsigned int)max_attempts,
        (unsigned int)word,
        (int)status,
        honor_stop ? 0U : 1U);
    return UHFWorkerEventFail;
}

/* Read only the EPCglobal TID model fields; never include the unique serial. */
static UHFWorkerEvent read_tid_model_id_bounded(
    UHFWorker* worker,
    UHFTag* target,
    uint8_t model_id[UHF_UMI_MODEL_ID_SIZE],
    uint8_t max_attempts,
    bool publish_clone_attempt,
    const char* component) {
    if(!worker || !target || !model_id || max_attempts == 0U) return UHFWorkerEventFail;

    memset(model_id, 0, UHF_UMI_MODEL_ID_SIZE);
    M100ResponseType status = M100ValidationFail;

    for(uint8_t attempt = 1U; attempt <= max_attempts; attempt++) {
        if(uhf_worker_stop_requested(worker)) return UHFWorkerEventAborted;
        if(publish_clone_attempt) clone_publish_attempt(worker, attempt, max_attempts);

        M100ResponseType select_status = m100_set_select(worker->module, target);
        if(select_status != M100SuccessResponse) {
            UHF_W(
                component,
                "TID model select attempt=%u/%u status=%d",
                (unsigned int)attempt,
                (unsigned int)max_attempts,
                (int)select_status);
            furi_delay_ms(10);
            continue;
        }

        uint8_t bytes[UHF_UMI_MODEL_ID_SIZE] = {0};
        size_t returned = 0U;
        status = m100_read_raw_words(
            worker->module,
            target,
            TIDBank,
            0U,
            UHF_UMI_MODEL_ID_SIZE / 2U,
            worker->DefaultAP,
            bytes,
            sizeof(bytes),
            &returned);

        UHF_I(
            component,
            "TID model read attempt=%u/%u status=%d bytes=%u",
            (unsigned int)attempt,
            (unsigned int)max_attempts,
            (int)status,
            (unsigned int)returned);

        if(status == M100SuccessResponse && returned == sizeof(bytes) &&
           uhf_umi_model_id_valid(bytes)) {
            memcpy(model_id, bytes, sizeof(bytes));
            uhf_debug_hex(UHFDebugInfo, component, "TID model", model_id, sizeof(bytes));
            uhf_debug_flush();
            return UHFWorkerEventSuccess;
        }
        if(status == M100APWrong) return UHFWorkerEventAccessDenied;
        if(status == M100MemoryLocked || status == M100MemoryOverrun) break;

        furi_delay_ms(10);
    }

    UHF_W(component, "TID model unavailable status=%d", (int)status);
    uhf_debug_flush();
    return UHFWorkerEventFail;
}

static bool size_probe_same_epc(const UHFTag* expected, const UHFTag* observed) {
    return expected && observed && expected->epc && observed->epc && expected->epc->size > 0U &&
           expected->epc->size == observed->epc->size &&
           memcmp(expected->epc->data, observed->epc->data, expected->epc->size) == 0;
}

static M100ResponseType size_probe_refresh_target(UHFWorker* worker, UHFTag* target) {
    UHFTag* probe = uhf_tag_alloc();
    if(!probe) return M100ValidationFail;

    M100ResponseType last_status = M100EmptyResponse;

    UHF_I("SIZE", "RF REFRESH BEGIN attempts=%u", (unsigned int)SIZE_RF_REPOLL_ATTEMPTS);

    for(uint8_t attempt = 0; attempt < SIZE_RF_REPOLL_ATTEMPTS; attempt++) {
        if(uhf_worker_stop_requested(worker)) {
            uhf_tag_free(probe);
            return M100EmptyResponse;
        }

        uhf_tag_reset(probe);

        last_status = m100_single_poll(worker->module, probe, worker);

        if(last_status == M100SuccessResponse) {
            if(size_probe_same_epc(target, probe)) {
                /*
                 * Refresh volatile inventory information while keeping the
                 * original target EPC identity fixed.
                 */
                uhf_tag_set_epc_pc(target, probe->epc->pc);
                uhf_tag_set_epc_crc(target, probe->epc->crc);
                target->epc->rssi = probe->epc->rssi;

                UHF_I(
                    "SIZE",
                    "RF REFRESH SAME EPC attempt=%u/%u "
                    "rssi=%d pc=0x%04X crc=0x%04X",
                    (unsigned int)(attempt + 1U),
                    (unsigned int)SIZE_RF_REPOLL_ATTEMPTS,
                    (int)probe->epc->rssi,
                    (unsigned int)probe->epc->pc,
                    (unsigned int)probe->epc->crc);
                uhf_debug_flush();

                uhf_tag_free(probe);
                return M100SuccessResponse;
            }

            /*
             * A different tag answered. Do not silently switch the size test
             * to another physical tag; continue polling for the original EPC.
             */
            UHF_W(
                "SIZE",
                "RF REFRESH other EPC ignored attempt=%u/%u",
                (unsigned int)(attempt + 1U),
                (unsigned int)SIZE_RF_REPOLL_ATTEMPTS);
            debug_log_tag_epc("size unexpected EPC", probe);
        } else {
            UHF_T(
                "SIZE",
                "RF REFRESH miss attempt=%u/%u status=%d",
                (unsigned int)(attempt + 1U),
                (unsigned int)SIZE_RF_REPOLL_ATTEMPTS,
                (int)last_status);
        }

        furi_delay_ms(SIZE_RF_RETRY_DELAY_MS);
    }

    UHF_W("SIZE", "RF REFRESH FAILED last=%d", (int)last_status);
    uhf_debug_flush();

    uhf_tag_free(probe);
    return last_status;
}

static bool size_probe_is_terminal_status(M100ResponseType status) {
    return status == M100SuccessResponse || status == M100MemoryOverrun ||
           status == M100MemoryLocked || status == M100APWrong;
}

static M100ResponseType size_probe_words_hardened(
    UHFWorker* worker,
    UHFTag* target,
    BankType bank,
    uint16_t words,
    size_t* returned_bytes) {
    furi_assert(worker);
    furi_assert(target);
    furi_assert(returned_bytes);

    *returned_bytes = 0;

    M100ResponseType last_status = M100EmptyResponse;

    for(uint8_t attempt = 0; attempt < SIZE_RF_ATTEMPTS_PER_POINT; attempt++) {
        if(uhf_worker_stop_requested(worker)) {
            return M100EmptyResponse;
        }

        M100ResponseType reselect = m100_set_select(worker->module, target);

        if(reselect == M100SuccessResponse) {
            last_status = m100_probe_label_data_storage(
                worker->module, target, bank, worker->DefaultAP, words, returned_bytes);
        } else {
            last_status = reselect;
        }

        UHF_T(
            "SIZE",
            "RF POINT bank=%s words=%u attempt=%u/%u "
            "select=%d status=%d returned=%u",
            debug_bank_name(bank),
            (unsigned int)words,
            (unsigned int)(attempt + 1U),
            (unsigned int)SIZE_RF_ATTEMPTS_PER_POINT,
            (int)reselect,
            (int)last_status,
            (unsigned int)*returned_bytes);

        if(size_probe_is_terminal_status(last_status)) {
            return last_status;
        }

        /*
         * 0x09 / timeout / empty RX / validation failure are transient.
         * Every third miss, reacquire the ORIGINAL EPC by inventory before
         * continuing with exactly the same candidate word count.
         */
        if(((attempt + 1U) % SIZE_RF_REPOLL_EVERY) == 0U &&
           (attempt + 1U) < SIZE_RF_ATTEMPTS_PER_POINT) {
            M100ResponseType refresh = size_probe_refresh_target(worker, target);

            UHF_I(
                "SIZE",
                "RF POINT refresh bank=%s words=%u after_attempt=%u "
                "refresh=%d",
                debug_bank_name(bank),
                (unsigned int)words,
                (unsigned int)(attempt + 1U),
                (int)refresh);
            uhf_debug_flush();
        }

        furi_delay_ms(SIZE_RF_RETRY_DELAY_MS);
    }

    UHF_W(
        "SIZE",
        "RF POINT UNRESOLVED bank=%s words=%u "
        "after=%u last=%d",
        debug_bank_name(bank),
        (unsigned int)words,
        (unsigned int)SIZE_RF_ATTEMPTS_PER_POINT,
        (int)last_status);
    uhf_debug_flush();

    return last_status;
}

static M100ResponseType size_probe_verify_point(
    UHFWorker* worker,
    UHFTag* target,
    BankType bank,
    uint16_t words,
    size_t* returned_bytes) {
    M100ResponseType status = M100EmptyResponse;

    for(uint8_t round = 0; round < SIZE_RF_VERIFY_ROUNDS; round++) {
        status = size_probe_words_hardened(worker, target, bank, words, returned_bytes);

        UHF_I(
            "SIZE",
            "VERIFY POINT bank=%s words=%u round=%u/%u "
            "status=%d bytes=%u",
            debug_bank_name(bank),
            (unsigned int)words,
            (unsigned int)(round + 1U),
            (unsigned int)SIZE_RF_VERIFY_ROUNDS,
            (int)status,
            (unsigned int)*returned_bytes);
        uhf_debug_flush();

        if(size_probe_is_terminal_status(status)) {
            return status;
        }

        if(round + 1U < SIZE_RF_VERIFY_ROUNDS) {
            size_probe_refresh_target(worker, target);
        }
    }

    return status;
}

static UHFWorkerEvent probe_bank_size_bytes(
    UHFWorker* worker,
    UHFTag* target,
    BankType bank,
    uint16_t* bytes_out,
    UHFSizeBankStatus* result_status) {
    furi_assert(worker);
    furi_assert(target);
    furi_assert(bytes_out);
    furi_assert(result_status);

    *bytes_out = 0;
    *result_status = UHFSizeBankStatusUnknown;

    unsigned int word_low = 1U;
    unsigned int word_high = 64U;
    size_t best_bytes = 0U;
    bool saw_success = false;
    bool saw_overrun = false;

    UHF_I(
        "SIZE",
        "BANK BEGIN RF-HARDENED bank=%s(%d) "
        "range=1..64 point_attempts=%u",
        debug_bank_name(bank),
        (int)bank,
        (unsigned int)SIZE_RF_ATTEMPTS_PER_POINT);

    while(word_low <= word_high) {
        if(uhf_worker_stop_requested(worker)) {
            return UHFWorkerEventAborted;
        }

        unsigned int words = (word_low + word_high) / 2U;

        size_t returned_bytes = 0U;

        M100ResponseType status =
            size_probe_words_hardened(worker, target, bank, (uint16_t)words, &returned_bytes);

        UHF_I(
            "SIZE",
            "BSEARCH bank=%s candidate=%u "
            "low=%u high=%u status=%d bytes=%u best=%u",
            debug_bank_name(bank),
            words,
            word_low,
            word_high,
            (int)status,
            (unsigned int)returned_bytes,
            (unsigned int)best_bytes);
        uhf_debug_flush();

        if(status == M100SuccessResponse) {
            saw_success = true;

            if(returned_bytes > best_bytes) {
                best_bytes = returned_bytes;
            }

            word_low = words + 1U;
            continue;
        }

        if(status == M100MemoryOverrun) {
            saw_overrun = true;

            if(words == 0U) break;
            word_high = words - 1U;
            continue;
        }

        if(status == M100MemoryLocked) {
            *bytes_out = (uint16_t)best_bytes;
            *result_status = UHFSizeBankStatusLocked;

            UHF_W(
                "SIZE",
                "BANK END bank=%s status=LOCKED best=%u",
                debug_bank_name(bank),
                (unsigned int)best_bytes);
            uhf_debug_flush();

            return UHFWorkerEventSuccess;
        }

        if(status == M100APWrong) {
            *bytes_out = (uint16_t)best_bytes;
            *result_status = UHFSizeBankStatusWrongPassword;

            UHF_W(
                "SIZE",
                "BANK END bank=%s status=WRONG_AP best=%u",
                debug_bank_name(bank),
                (unsigned int)best_bytes);
            uhf_debug_flush();

            return UHFWorkerEventSuccess;
        }

        /*
         * Even after ten RF attempts + periodic re-polling this candidate was
         * unresolved. Critically: do NOT move either binary-search boundary.
         * We cannot infer memory size from a transient RF failure.
         */
        *bytes_out = (uint16_t)best_bytes;
        *result_status = UHFSizeBankStatusError;

        UHF_W(
            "SIZE",
            "BANK END bank=%s status=RF_UNRESOLVED "
            "best=%u candidate=%u last=%d",
            debug_bank_name(bank),
            (unsigned int)best_bytes,
            words,
            (int)status);
        uhf_debug_flush();

        return UHFWorkerEventSuccess;
    }

    /*
     * Binary search found the highest successful word count in best_bytes.
     * Now require an explicit adjacent-boundary proof:
     *
     *   N words     -> SUCCESS returning exactly N*2 bytes
     *   N + 1 words -> MEMORY OVERRUN
     *
     * This prevents a lucky earlier success + later RF dropout from being
     * reported as an exact physical bank size.
     */
    uint16_t best_words = (uint16_t)(best_bytes / 2U);

    if(!saw_success || best_words == 0U) {
        size_t one_word_bytes = 0U;

        M100ResponseType one_status =
            size_probe_verify_point(worker, target, bank, 1U, &one_word_bytes);

        if(one_status == M100MemoryOverrun) {
            *bytes_out = 0U;
            *result_status = UHFSizeBankStatusOk;

            UHF_I(
                "SIZE",
                "BANK VERIFIED bank=%s bytes=0 "
                "word1=OVERRUN",
                debug_bank_name(bank));
            uhf_debug_flush();

            return UHFWorkerEventSuccess;
        }

        if(one_status == M100MemoryLocked) {
            *result_status = UHFSizeBankStatusLocked;
            return UHFWorkerEventSuccess;
        }

        if(one_status == M100APWrong) {
            *result_status = UHFSizeBankStatusWrongPassword;
            return UHFWorkerEventSuccess;
        }

        *bytes_out = 0U;
        *result_status = UHFSizeBankStatusError;

        UHF_W(
            "SIZE",
            "BANK VERIFY FAILED bank=%s no stable word1 "
            "status=%d bytes=%u",
            debug_bank_name(bank),
            (int)one_status,
            (unsigned int)one_word_bytes);
        uhf_debug_flush();

        return UHFWorkerEventSuccess;
    }

    size_t exact_bytes = 0U;
    size_t next_bytes = 0U;

    M100ResponseType exact_status =
        size_probe_verify_point(worker, target, bank, best_words, &exact_bytes);

    if(exact_status == M100MemoryLocked) {
        *bytes_out = (uint16_t)best_bytes;
        *result_status = UHFSizeBankStatusLocked;
        return UHFWorkerEventSuccess;
    }

    if(exact_status == M100APWrong) {
        *bytes_out = (uint16_t)best_bytes;
        *result_status = UHFSizeBankStatusWrongPassword;
        return UHFWorkerEventSuccess;
    }

    if(exact_status != M100SuccessResponse || exact_bytes != (size_t)best_words * 2U) {
        *bytes_out = (uint16_t)best_bytes;
        *result_status = UHFSizeBankStatusError;

        UHF_W(
            "SIZE",
            "BANK VERIFY EXACT FAILED bank=%s "
            "words=%u status=%d returned=%u expected=%u",
            debug_bank_name(bank),
            (unsigned int)best_words,
            (int)exact_status,
            (unsigned int)exact_bytes,
            (unsigned int)(best_words * 2U));
        uhf_debug_flush();

        return UHFWorkerEventSuccess;
    }

    /*
     * If search hit the configured ceiling (64 words) without an overrun,
     * exact physical end is unknown. Report >=128 B instead of lying.
     */
    if(best_words >= 64U && !saw_overrun) {
        *bytes_out = (uint16_t)exact_bytes;
        *result_status = UHFSizeBankStatusError;

        UHF_W(
            "SIZE",
            "BANK VERIFY CEILING bank=%s >=%u bytes; "
            "no upper overrun within 64 words",
            debug_bank_name(bank),
            (unsigned int)exact_bytes);
        uhf_debug_flush();

        return UHFWorkerEventSuccess;
    }

    M100ResponseType next_status =
        size_probe_verify_point(worker, target, bank, (uint16_t)(best_words + 1U), &next_bytes);

    if(next_status != M100MemoryOverrun) {
        *bytes_out = (uint16_t)exact_bytes;
        *result_status = UHFSizeBankStatusError;

        UHF_W(
            "SIZE",
            "BANK VERIFY NEXT FAILED bank=%s "
            "exact_words=%u next_words=%u next_status=%d "
            "next_bytes=%u",
            debug_bank_name(bank),
            (unsigned int)best_words,
            (unsigned int)(best_words + 1U),
            (int)next_status,
            (unsigned int)next_bytes);
        uhf_debug_flush();

        return UHFWorkerEventSuccess;
    }

    *bytes_out = (uint16_t)exact_bytes;
    *result_status = UHFSizeBankStatusOk;

    UHF_I(
        "SIZE",
        "BANK VERIFIED bank=%s bytes=%u words=%u "
        "exact=SUCCESS next=OVERRUN",
        debug_bank_name(bank),
        (unsigned int)*bytes_out,
        (unsigned int)best_words);
    uhf_debug_flush();

    return UHFWorkerEventSuccess;
}

static UHFWorkerEvent get_bank_sizes_card(UHFWorker* worker) {
    worker->BankSizeEpcBytes = 0;
    worker->BankSizeTidBytes = 0;
    worker->BankSizeUserBytes = 0;
    worker->BankSizeReservedBytes = 0;
    worker->BankSizeEpcStatus = UHFSizeBankStatusUnknown;
    worker->BankSizeTidStatus = UHFSizeBankStatusUnknown;
    worker->BankSizeUserStatus = UHFSizeBankStatusUnknown;
    worker->BankSizeReservedStatus = UHFSizeBankStatusUnknown;
    worker->BankSizePc = 0;
    worker->BankSizeCrc = 0;

    UHF_I("SIZE", "GET SIZE BEGIN DefaultAP=0x%08lX", (unsigned long)worker->DefaultAP);
    uhf_debug_flush();

    UHFTag* target = send_polling_command(worker);
    if(!target) {
        return uhf_worker_stop_requested(worker) ? UHFWorkerEventAborted : UHFWorkerEventFail;
    }

    worker->BankSizePc = target->epc->pc;
    worker->BankSizeCrc = target->epc->crc;
    debug_log_tag_epc("size target EPC", target);

    const BankType banks[4] = {EPCBank, TIDBank, UserBank, ReservedBank};
    uint16_t* sizes[4] = {
        &worker->BankSizeEpcBytes,
        &worker->BankSizeTidBytes,
        &worker->BankSizeUserBytes,
        &worker->BankSizeReservedBytes,
    };
    UHFSizeBankStatus* statuses[4] = {
        &worker->BankSizeEpcStatus,
        &worker->BankSizeTidStatus,
        &worker->BankSizeUserStatus,
        &worker->BankSizeReservedStatus,
    };

    for(uint8_t i = 0; i < 4U; i++) {
        UHFWorkerEvent event =
            probe_bank_size_bytes(worker, target, banks[i], sizes[i], statuses[i]);

        if(event == UHFWorkerEventAborted) {
            uhf_tag_free(target);
            return event;
        }
    }

    UHF_I(
        "SIZE",
        "GET SIZE COMPLETE PC=0x%04X CRC=0x%04X "
        "EPC=%u(%d) TID=%u(%d) USER=%u(%d) RSV=%u(%d)",
        (unsigned int)worker->BankSizePc,
        (unsigned int)worker->BankSizeCrc,
        (unsigned int)worker->BankSizeEpcBytes,
        (int)worker->BankSizeEpcStatus,
        (unsigned int)worker->BankSizeTidBytes,
        (int)worker->BankSizeTidStatus,
        (unsigned int)worker->BankSizeUserBytes,
        (int)worker->BankSizeUserStatus,
        (unsigned int)worker->BankSizeReservedBytes,
        (int)worker->BankSizeReservedStatus);
    uhf_debug_flush();

    uhf_tag_free(target);
    return UHFWorkerEventSuccess;
}

static bool clone_epc_equal(const UHFTag* a, const UHFTag* b) {
    return a && b && a->epc && b->epc && a->epc->size == b->epc->size && a->epc->size > 0 &&
           memcmp(a->epc->data, b->epc->data, a->epc->size) == 0;
}

static UHFWorkerEvent clone_mode_write_user_word(
    UHFWorker* worker,
    UHFTag* target,
    uint16_t word,
    uint16_t value,
    bool honor_stop) {
    uint8_t bytes[2] = {
        (uint8_t)((value >> 8) & 0xFFU),
        (uint8_t)(value & 0xFFU),
    };

    M100ResponseType final_status = M100ValidationFail;
    UHFWorkerEvent event = clone_write_user_bytes_bounded(
        worker, target, word, bytes, sizeof(bytes), honor_stop, &final_status);

    UHF_I(
        "CLONEMODE",
        "UserW%u=%04X event=%d st=%d stop=%u",
        (unsigned int)word,
        (unsigned int)value,
        (int)event,
        (int)final_status,
        honor_stop ? 1U : 0U);
    return event;
}

static UHFWorkerEvent clone_mode_apply_marker(
    UHFWorker* worker,
    UHFTag* source,
    UHFTag* target,
    uint16_t expected_pc,
    uint16_t marker_word,
    uint16_t marker_value) {
    const char* component = expected_pc == UHF_CLONE_PC3400_SOURCE_PC ? "CLONE3400" : "CLONE3000";

    if(source->epc->pc != expected_pc) {
        UHF_W(
            component,
            "SOURCE PC mismatch got=0x%04X expected=0x%04X",
            (unsigned int)source->epc->pc,
            (unsigned int)expected_pc);
        return UHFWorkerEventFail;
    }

    /*
     * Do not run a full User capacity probe here.  On this clone IC an
     * oversized READ can return transient 0x09 instead of MemoryOverrun and
     * falsely report 14/0/2 bytes.  Writing the one required word is the exact
     * capability check and cannot be confused with a larger-bank boundary.
     */
    UHF_I(
        component,
        "MARKER write UserW%u=%04X",
        (unsigned int)marker_word,
        (unsigned int)marker_value);

    UHFWorkerEvent event =
        clone_mode_write_user_word(worker, target, marker_word, marker_value, true);

    if(event != UHFWorkerEventSuccess) return event;

    worker->ClonePc3400MarkerApplied = true;
    const bool expect_umi = (expected_pc & 0x0400U) != 0U;

    /*
     * Verify immediately, before TID/EPC writes, that this target IC reacts to
     * the mode marker by reporting the requested UMI state. At this stage the
     * EPC length may still differ, so only the UMI bit is checked.
     */
    for(uint8_t attempt = 1; attempt <= 12U; attempt++) {
        if(uhf_worker_stop_requested(worker)) return UHFWorkerEventAborted;

        UHFTag* probe_tag = uhf_tag_alloc();
        if(!probe_tag) return UHFWorkerEventFail;

        M100ResponseType status = m100_single_poll(worker->module, probe_tag, worker);

        bool same_target = status == M100SuccessResponse && clone_epc_equal(target, probe_tag);

        const bool observed_umi = (probe_tag->epc->pc & 0x0400U) != 0U;
        if(same_target && observed_umi == expect_umi) {
            UHF_I(
                component,
                "MARKER VERIFIED attempt=%u pc=0x%04X crc=0x%04X",
                (unsigned int)attempt,
                (unsigned int)probe_tag->epc->pc,
                (unsigned int)probe_tag->epc->crc);
            uhf_debug_flush();
            uhf_tag_free(probe_tag);
            return UHFWorkerEventSuccess;
        }

        uhf_tag_free(probe_tag);
        furi_delay_ms(10);
    }

    UHF_W(component, "MARKER did not set expected UMI=%u on target", expect_umi ? 1U : 0U);
    uhf_debug_flush();
    return UHFWorkerEventFail;
}

static UHFWorkerEvent
    clone_mode_verify_final(UHFWorker* worker, UHFTag* source, uint16_t expected_pc) {
    const char* component = expected_pc == UHF_CLONE_PC3400_SOURCE_PC ? "CLONE3400" : "CLONE3000";

    for(uint8_t attempt = 1; attempt <= 20U; attempt++) {
        if(uhf_worker_stop_requested(worker)) return UHFWorkerEventAborted;

        UHFTag* probe = uhf_tag_alloc();
        if(!probe) return UHFWorkerEventFail;

        M100ResponseType status = m100_single_poll(worker->module, probe, worker);

        bool same_epc = status == M100SuccessResponse && clone_epc_equal(source, probe);

        if(same_epc) {
            worker->ClonePc3400FinalPc = probe->epc->pc;
            worker->ClonePc3400FinalCrc = probe->epc->crc;

            UHF_I(
                component,
                "FINAL VERIFY attempt=%u pc=0x%04X crc=0x%04X expected_pc=0x%04X",
                (unsigned int)attempt,
                (unsigned int)probe->epc->pc,
                (unsigned int)probe->epc->crc,
                (unsigned int)expected_pc);
            uhf_debug_flush();

            if(probe->epc->pc == expected_pc) {
                worker->ClonePc3400Verified = true;
                uhf_tag_free(probe);
                return UHFWorkerEventSuccess;
            }
        }

        uhf_tag_free(probe);
        furi_delay_ms(10);
    }

    worker->ClonePc3400Verified = false;
    UHF_W(
        component,
        "FINAL VERIFY FAILED pc=0x%04X crc=0x%04X",
        (unsigned int)worker->ClonePc3400FinalPc,
        (unsigned int)worker->ClonePc3400FinalCrc);
    uhf_debug_flush();
    return UHFWorkerEventFail;
}

// Clone phase 2: write selected banks from NewTag onto the first tag in field.
static UHFWorkerEvent clone_write_card(UHFWorker* uhf_worker) {
    const uint8_t max_attempts = uhf_worker_clone_max_attempts(uhf_worker);
    clone_publish_attempt(uhf_worker, 1U, max_attempts);

    UHF_I(
        "CLONE",
        "WRITE SESSION BEGIN mask=0x%04X attempts=%u DefaultAP=0x%08lX",
        (unsigned int)uhf_worker->CloneMask,
        (unsigned int)max_attempts,
        (unsigned long)uhf_worker->DefaultAP);
    uhf_debug_flush();

    UHFTag* target = send_polling_command(uhf_worker);
    if(target == NULL) {
        UHF_W("CLONE", "Target poll aborted/failed");
        return UHFWorkerEventAborted;
    }

    UHFTag* source = uhf_worker->NewTag;
    uint16_t mask = uhf_worker->CloneMask;
    const uint16_t source_pc = source->epc->pc;
    const uint16_t target_pc = target->epc->pc;
    const bool source_pc3000 = source_pc == UHF_CLONE_PC3000_SOURCE_PC;
    const bool source_pc3400 = source_pc == UHF_CLONE_PC3400_SOURCE_PC;
    const bool target_pc3000 = target_pc == UHF_CLONE_PC3000_SOURCE_PC;
    const bool target_pc3400 = target_pc == UHF_CLONE_PC3400_SOURCE_PC;
    const bool clone_epc_requested = (mask & WRITE_EPC) != 0U;
    const bool clone_user_requested = (mask & (WRITE_USER | UHF_CLONE_FORCE_USER_ZERO)) != 0U;
    const bool mode_mismatch = clone_epc_requested && ((source_pc3000 && target_pc3400) ||
                                                       (source_pc3400 && target_pc3000));
    const bool convert_to_pc3000 = mode_mismatch && source_pc3000 &&
                                   uhf_worker->CloneModeConversionApproved;
    const bool convert_to_pc3400 = mode_mismatch && source_pc3400 &&
                                   uhf_worker->CloneModeConversionApproved;
    const bool same_known_mode = clone_epc_requested && ((source_pc3000 && target_pc3000) ||
                                                         (source_pc3400 && target_pc3400));

    /*
     * A learned User word controls PC3000/PC3400 on supported rewritable
     * tags. Re-assert the source mode after every selected User write, before
     * attempting MB1.
     */
    const bool preserve_same_mode = same_known_mode && clone_user_requested;
    const bool marker_required_pc3400 = ((mask & UHF_CLONE_FORCE_PC3400) != 0U) ||
                                        convert_to_pc3400;
    const bool marker_required_pc3000 = convert_to_pc3000;
    const bool marker_required = marker_required_pc3400 || marker_required_pc3000;
    bool force_pc3400 = marker_required_pc3400 || (preserve_same_mode && source_pc3400);
    bool force_pc3000 = marker_required_pc3000 || (preserve_same_mode && source_pc3000);
    bool mode_marker_control = force_pc3400 || force_pc3000;

    /*
     * Rollback state must be initialized before every possible
     * `goto clone_done`. GCC inlines this function into uhf_worker_task() and
     * treats a jump across a later declaration as maybe-uninitialized.
     */
    UHFUmiMarkerEntry mode_marker = {0};
    uint16_t mode_target_marker_word = 0U;
    uint16_t mode_marker_value = 0U;
    bool mode_target_marker_valid = false;
    bool mode_user_may_have_changed = false;

    uhf_worker->CloneModeSourcePc = source_pc;
    uhf_worker->CloneModeTargetPc = target_pc;
    uhf_worker->CloneModeConversionApplied = false;
    uhf_worker->ClonePc3400MarkerApplied = false;
    uhf_worker->ClonePc3400Verified = false;
    uhf_worker->ClonePc3400FinalPc = 0;
    uhf_worker->ClonePc3400FinalCrc = 0;
    uhf_worker->CloneMarkerKnown = false;
    memset(uhf_worker->CloneMarkerModelId, 0, sizeof(uhf_worker->CloneMarkerModelId));
    uhf_worker->CloneMarkerWord = 0U;
    uhf_worker->CloneMarkerMask = 0U;
    uhf_worker->CloneMarkerPc3000Bits = 0U;
    uhf_worker->CloneMarkerPc3400Bits = 0U;
    uhf_worker->CloneMarkerAppliedValue = 0U;
    uhf_worker->CloneReservedSourceBytes = (uint16_t)source->reserved->size;
    uhf_worker->CloneReservedTargetBytes = 0;
    uhf_worker->CloneReservedWrittenBytes = 0;
    uhf_worker->CloneReservedSkippedBytes = 0;

    UHF_I(
        "CLONEMODE",
        "CHECK source=0x%04X target=0x%04X mismatch=%u approved=%u "
        "user=%u preserve=%u enforce=%s",
        (unsigned int)source_pc,
        (unsigned int)target_pc,
        mode_mismatch ? 1U : 0U,
        uhf_worker->CloneModeConversionApproved ? 1U : 0U,
        clone_user_requested ? 1U : 0U,
        preserve_same_mode ? 1U : 0U,
        force_pc3400 ? "PC3400" : (force_pc3000 ? "PC3000" : "none"));
    uhf_debug_flush();

    if(mode_mismatch && !uhf_worker->CloneModeConversionApproved) {
        UHF_W(
            "CLONEMODE",
            "NO WRITE until user approves conversion 0x%04X -> 0x%04X",
            (unsigned int)target_pc,
            (unsigned int)source_pc);
        uhf_debug_flush();
        uhf_tag_free(target);
        return UHFWorkerEventCloneModeMismatch;
    }

    if(force_pc3400) {
        if(source->epc->pc != UHF_CLONE_PC3400_SOURCE_PC) {
            UHF_W("CLONE3400", "Refuse: source is not exact PC=3400");
            uhf_tag_free(target);
            return UHFWorkerEventFail;
        }

        if(!(mask & WRITE_EPC)) {
            UHF_W("CLONE3400", "Refuse: EPC is mandatory");
            uhf_tag_free(target);
            return UHFWorkerEventFail;
        }
    }

    debug_log_tag_epc("clone target EPC", target);
    debug_log_tag_epc("clone source EPC", source);

    // Select target with bounded retries.
    M100ResponseType select_status = select_tag_bounded(uhf_worker, target);
    if(select_status != M100SuccessResponse) {
        UHF_W("CLONE", "Target select failed status=%d", (int)select_status);
        uhf_tag_free(target);
        return UHFWorkerEventFail;
    }

    UHFWorkerEvent event = UHFWorkerEventSuccess;

    /*
     * Resolve the mode marker from the target's TID model before the first
     * write. An unknown or unreadable model is a safe refusal: Test UMI Auto
     * must learn it first.
     */
    if(mode_marker_control) {
        uint8_t target_model_id[UHF_UMI_MODEL_ID_SIZE] = {0};
        event = read_tid_model_id_bounded(
            uhf_worker, target, target_model_id, max_attempts, true, "CLONEMODE");

        if(event != UHFWorkerEventSuccess) {
            if(event == UHFWorkerEventAborted) goto clone_done;
            if(marker_required) {
                UHF_W("CLONEMODE", "REFUSE before write: target TID model unreadable");
                uhf_debug_flush();
                uhf_tag_free(target);
                return UHFWorkerEventCloneMarkerUnknown;
            }

            UHF_W("CLONEMODE", "Optional same-mode guard skipped: TID model unreadable");
            force_pc3400 = false;
            force_pc3000 = false;
            mode_marker_control = false;
            event = UHFWorkerEventSuccess;
        }

        const UHFUmiMarkerEntry* learned = NULL;
        if(mode_marker_control) {
            memcpy(
                uhf_worker->CloneMarkerModelId,
                target_model_id,
                sizeof(uhf_worker->CloneMarkerModelId));
            learned = uhf_umi_marker_registry_find(uhf_worker->UmiMarkerRegistry, target_model_id);
        }
        if(mode_marker_control && !learned && marker_required) {
            UHF_W(
                "CLONEMODE",
                "REFUSE before write: no marker for model=%02X%02X%02X%02X",
                target_model_id[0],
                target_model_id[1],
                target_model_id[2],
                target_model_id[3]);
            uhf_debug_flush();
            uhf_tag_free(target);
            return UHFWorkerEventCloneMarkerUnknown;
        }
        if(mode_marker_control && !learned) {
            UHF_W(
                "CLONEMODE",
                "Optional same-mode guard skipped: no marker for model=%02X%02X%02X%02X",
                target_model_id[0],
                target_model_id[1],
                target_model_id[2],
                target_model_id[3]);
            force_pc3400 = false;
            force_pc3000 = false;
            mode_marker_control = false;
        }

        if(mode_marker_control) {
            mode_marker = *learned;
            uhf_worker->CloneMarkerKnown = true;
            uhf_worker->CloneMarkerWord = mode_marker.word;
            uhf_worker->CloneMarkerMask = mode_marker.mask;
            uhf_worker->CloneMarkerPc3000Bits = mode_marker.pc3000_bits;
            uhf_worker->CloneMarkerPc3400Bits = mode_marker.pc3400_bits;

            UHF_I(
                force_pc3400 ? "CLONE3400" : "CLONE3000",
                "MARKER DB model=%02X%02X%02X%02X UserW%u mask=%04X "
                "P3000=%04X P3400=%04X",
                target_model_id[0],
                target_model_id[1],
                target_model_id[2],
                target_model_id[3],
                (unsigned int)mode_marker.word,
                (unsigned int)mode_marker.mask,
                (unsigned int)mode_marker.pc3000_bits,
                (unsigned int)mode_marker.pc3400_bits);
            uhf_debug_flush();
        }

        /*
         * Preserve the TARGET word before any User write. Using source data
         * for rollback could leave a failed clone modified.
         */
        M100ResponseType backup_status = M100ValidationFail;
        if(mode_marker_control) {
            event = clone_read_user_word_bounded(
                uhf_worker,
                target,
                mode_marker.word,
                &mode_target_marker_word,
                true,
                &backup_status);
        }

        if(mode_marker_control && event != UHFWorkerEventSuccess) {
            UHF_W(
                "CLONEMODE",
                "TARGET BACKUP FAILED UserW%u event=%d status=%d; NO WRITE",
                (unsigned int)mode_marker.word,
                (int)event,
                (int)backup_status);
            uhf_debug_flush();
            goto clone_done;
        }

        if(mode_marker_control) {
            mode_target_marker_valid = true;
            UHF_I(
                "CLONEMODE",
                "TARGET BACKUP UserW%u=%04X",
                (unsigned int)mode_marker.word,
                (unsigned int)mode_target_marker_word);
            uhf_debug_flush();
        }

        if(mode_marker_control) {
            uint16_t marker_base = mode_target_marker_word;
            const size_t source_marker_offset = (size_t)mode_marker.word * 2U;
            if(mask & UHF_CLONE_FORCE_USER_ZERO) {
                marker_base = 0U;
            } else if((mask & WRITE_USER) && source->user->size >= source_marker_offset + 2U) {
                marker_base =
                    (uint16_t)(((uint16_t)source->user->data[source_marker_offset] << 8) |
                               source->user->data[source_marker_offset + 1U]);
            }

            const uint16_t desired_bits = force_pc3400 ? mode_marker.pc3400_bits :
                                                         mode_marker.pc3000_bits;
            mode_marker_value = (uint16_t)((marker_base & (uint16_t)~mode_marker.mask) |
                                           (desired_bits & mode_marker.mask));
            uhf_worker->CloneMarkerAppliedValue = mode_marker_value;
            UHF_I(
                force_pc3400 ? "CLONE3400" : "CLONE3000",
                "APPLY plan base=%04X mask=%04X bits=%04X -> UserW%u=%04X",
                (unsigned int)marker_base,
                (unsigned int)mode_marker.mask,
                (unsigned int)desired_bits,
                (unsigned int)mode_marker.word,
                (unsigned int)mode_marker_value);
            uhf_debug_flush();
        }
    }

    /*
     * FORCED User: source dump has no User bytes, so zero the largest proven
     * writable prefix. A target with no User bank remains a valid no-op.
     */
    if(mask & UHF_CLONE_FORCE_USER_ZERO) {
        uint8_t zeros[USER_MAX_BANK_SIZE];
        memset(zeros, 0, sizeof(zeros));

        const size_t max_zero_bytes = sizeof(zeros) & ~(size_t)1U;
        size_t written_zero_bytes = 0U;

        UHF_I("CLONE", "USER0 begin bytes=%u", (unsigned int)max_zero_bytes);

        if(mode_marker_control) mode_user_may_have_changed = true;
        event = clone_write_user_fitted(
            uhf_worker, target, zeros, max_zero_bytes, "USER0", &written_zero_bytes);
        if(event != UHFWorkerEventSuccess) goto clone_done;

        if(written_zero_bytes == 0U) {
            UHF_I("CLONE", "USER0 no bank");
        } else {
            UHF_I("CLONE", "USER0 done bytes=%u", (unsigned int)written_zero_bytes);
        }
    } else if((mask & WRITE_USER) && source->user->size > 0) {
        /* Fit by confirmed writes; never trust an oversized read as capacity. */
        const size_t source_user_size = source->user->size;
        if(source_user_size > USER_MAX_BANK_SIZE || (source_user_size & 1U) != 0U) {
            UHF_W(
                "CLONE",
                "USER invalid bytes=%u max=%u",
                (unsigned int)source_user_size,
                (unsigned int)USER_MAX_BANK_SIZE);
            event = UHFWorkerEventFail;
            goto clone_done;
        }

        UHF_I("CLONE", "USER begin bytes=%u", (unsigned int)source_user_size);
        size_t written_user_bytes = 0U;
        if(mode_marker_control) mode_user_may_have_changed = true;
        event = clone_write_user_fitted(
            uhf_worker, target, source->user->data, source_user_size, "USER", &written_user_bytes);
        if(event != UHFWorkerEventSuccess) goto clone_done;

        if(written_user_bytes == 0U) {
            UHF_I("CLONE", "USER fit no bank");
        } else {
            UHF_I(
                "CLONE",
                "USER done=%u/%u",
                (unsigned int)written_user_bytes,
                (unsigned int)source_user_size);
        }
    }

    /* Apply only the learned bits, preserving all unrelated User data. */
    if(force_pc3400) {
        /* A lost ACK can mean that the marker changed despite a failed event. */
        mode_user_may_have_changed = true;
        event = clone_mode_apply_marker(
            uhf_worker,
            source,
            target,
            UHF_CLONE_PC3400_SOURCE_PC,
            mode_marker.word,
            mode_marker_value);

        if(event != UHFWorkerEventSuccess) goto clone_done;
        if(convert_to_pc3400) uhf_worker->CloneModeConversionApplied = true;
    } else if(force_pc3000) {
        /* Apply the learned PC3000 state; preserve the rest of User memory. */
        mode_user_may_have_changed = true;
        event = clone_mode_apply_marker(
            uhf_worker,
            source,
            target,
            UHF_CLONE_PC3000_SOURCE_PC,
            mode_marker.word,
            mode_marker_value);

        if(event != UHFWorkerEventSuccess) goto clone_done;
        if(convert_to_pc3000) uhf_worker->CloneModeConversionApplied = true;
    }

    // Rewritable-TID tags only.
    if((mask & WRITE_TID) && source->tid->size > 0) {
        /*
         * Legacy Monza 4QT Saved_EPCs compatibility
         * ------------------------------------------
         * Older dumps can contain:
         *
         *   E2801105............... + extra MB2 bytes
         *
         * as one 16/24/32-byte "TID" string. The actual serialized Monza 4QT
         * TID is always the first 12 bytes. Writing the legacy 24-byte string
         * to a common rewritable-TID target causes Gen2 error 0xB3
         * (MemoryOverrun).
         *
         * Canonicalize only the unmistakable E2801105 Monza 4QT prefix.
         * Generic TIDs are never silently shortened.
         */
        uint8_t source_tid_backup[TID_MAX_BANK_SIZE];
        size_t source_tid_original_size = source->tid->size;

        if(source_tid_original_size > TID_MAX_BANK_SIZE) {
            source_tid_original_size = TID_MAX_BANK_SIZE;
        }

        memcpy(source_tid_backup, source->tid->data, source_tid_original_size);

        size_t clone_tid_size = source_tid_original_size;
        bool monza_legacy_normalized = false;

        if(clone_source_is_monza4qt_legacy_tid(source) &&
           clone_tid_size > MONZA4QT_CANONICAL_TID_SIZE) {
            clone_tid_size = MONZA4QT_CANONICAL_TID_SIZE;
            monza_legacy_normalized = true;

            UHF_I(
                "CLONE",
                "TID MONZA canonicalize legacy source=%u -> %u bytes",
                (unsigned int)source_tid_original_size,
                (unsigned int)clone_tid_size);
            uhf_debug_hex(
                UHFDebugInfo, "CLONE", "TID MONZA canonical", source_tid_backup, clone_tid_size);
            uhf_debug_flush();
        }

        /*
         * Determine the target's readable TID capacity before writing.
         * For a selected TID clone we require the complete canonical source to
         * fit. Unlike User, a genuine TID is an identity value, so silently
         * truncating it would produce a misleading "successful" clone.
         */
        uhf_tag_set_tid_size(target, 0);

        UHF_I(
            "CLONE",
            "TID FIT probe BEGIN source=%u canonical=%u monza_legacy=%d",
            (unsigned int)source_tid_original_size,
            (unsigned int)clone_tid_size,
            monza_legacy_normalized ? 1 : 0);
        uhf_debug_flush();

        UHFWorkerEvent tid_probe = read_bank_till_max_length(uhf_worker, target, TIDBank);

        if(tid_probe == UHFWorkerEventAborted) {
            event = UHFWorkerEventAborted;
            goto clone_tid_restore_done;
        }

        if(tid_probe == UHFWorkerEventAccessDenied || tid_probe == UHFWorkerEventWrongPassword) {
            UHF_W("CLONE", "TID FIT target TID cannot be read/accessed event=%d", (int)tid_probe);
            event = UHFWorkerEventAccessDenied;
            goto clone_tid_restore_done;
        }

        if(tid_probe != UHFWorkerEventSuccess) {
            UHF_W("CLONE", "TID FIT target capacity probe failed event=%d", (int)tid_probe);
            event = UHFWorkerEventFail;
            goto clone_tid_restore_done;
        }

        size_t target_tid_size = target->tid->size;
        if(target_tid_size > TID_MAX_BANK_SIZE) {
            target_tid_size = TID_MAX_BANK_SIZE;
        }
        target_tid_size &= ~(size_t)1U;
        clone_tid_size &= ~(size_t)1U;

        UHF_I(
            "CLONE",
            "TID FIT source=%u canonical=%u target=%u",
            (unsigned int)source_tid_original_size,
            (unsigned int)clone_tid_size,
            (unsigned int)target_tid_size);
        uhf_debug_flush();

        if(clone_tid_size == 0 || target_tid_size < clone_tid_size) {
            UHF_W(
                "CLONE",
                "TID FIT FAIL target=%u < required=%u",
                (unsigned int)target_tid_size,
                (unsigned int)clone_tid_size);
            event = UHFWorkerEventFail;
            goto clone_tid_restore_done;
        }

        // Temporarily expose exactly the canonical/full TID that must be written.
        uhf_tag_set_tid(source, source_tid_backup, clone_tid_size);

        event = clone_write_bank_bounded(uhf_worker, source, target, TIDBank, 0);

        if(event == UHFWorkerEventSuccess) {
            UHF_I(
                "CLONE",
                "TID FIT COMPLETE wrote=%u target_capacity=%u",
                (unsigned int)clone_tid_size,
                (unsigned int)target_tid_size);
            uhf_debug_flush();
        }

    clone_tid_restore_done:
        // Restore the exact Saved/Live source representation in RAM.
        uhf_tag_set_tid(source, source_tid_backup, source_tid_original_size);

        if(event != UHFWorkerEventSuccess) goto clone_done;
    }

    /*
     * EPC + PC:
     *
     * Clone means source semantics, not target semantics. Alpha19 preserved
     * target lower PC bits, which changed a source PC=0x3400 into 0x3000 on a
     * target whose UMI bit was clear.
     *
     * Preserve the SOURCE PC lower 11 bits (UMI/XI/toggle/etc.) and only
     * recalculate the top five EPC-length bits from the actual source EPC.
     * The CRC bytes in the write frame still come from the currently selected
     * target, as required by the existing YRM100X write flow.
     */
    if((mask & WRITE_EPC) && source->epc->size > 0) {
        uint16_t source_pc_original = uhf_tag_get_epc_pc(source);
        uint16_t target_pc = uhf_tag_get_epc_pc(target);
        uint16_t new_words = (uint16_t)(uhf_tag_get_epc_size(source) / 2);

        if(new_words > 0) {
            uint16_t clone_pc =
                (uint16_t)((source_pc_original & 0x07FFU) | ((new_words & 0x1FU) << 11));

            UHF_I(
                "CLONE",
                "EPC PC source=0x%04X target=0x%04X words=%u -> write=0x%04X UMI=%u",
                (unsigned int)source_pc_original,
                (unsigned int)target_pc,
                (unsigned int)new_words,
                (unsigned int)clone_pc,
                (clone_pc & 0x0400U) ? 1U : 0U);
            uhf_debug_flush();

            uhf_tag_set_epc_pc(source, clone_pc);

            if(mode_marker_control && clone_epc_equal(source, target)) {
                /*
                 * Current Tag1/rewrite-tag case:
                 * EPC bytes are already identical before clone. The marker has
                 * already made the tag report PC=3400 and CRC=C41E, so writing
                 * MB1 again is unnecessary and, on this IC, attempting to span
                 * StoredCRC word 0 returns Gen2 B4 (memory locked).
                 */
                UHF_I(
                    force_pc3400 ? "CLONE3400" : "CLONE3000",
                    "EPC already matches target; skip MB1 write. "
                    "Mode marker owns UMI/PC, StoredCRC left tag-managed");
                uhf_debug_flush();
                event = UHFWorkerEventSuccess;
            } else {
                /*
                 * StoredCRC (MB1 word 0) is tag-managed and can be read-only
                 * even on an otherwise rewritable clone tag. Every Clone mode
                 * therefore starts at word 1 and writes only PC + EPC.
                 */
                const uint16_t epc_source_address = 1U;

                UHF_I(
                    mode_marker_control ? (force_pc3400 ? "CLONE3400" : "CLONE3000") : "CLONE",
                    "EPC write start_word=%u include_crc=%u",
                    (unsigned int)epc_source_address,
                    0U);
                uhf_debug_flush();

                event = clone_write_bank_bounded(
                    uhf_worker, source, target, EPCBank, epc_source_address);
            }

            // Keep the in-memory saved source exactly as it was loaded.
            uhf_tag_set_epc_pc(source, source_pc_original);

            if(event != UHFWorkerEventSuccess) goto clone_done;

            /*
             * The target EPC now equals the source either because it already
             * matched (special skip) or because the write succeeded.
             */
            uhf_tag_set_epc(target, source->epc->data, source->epc->size);
            uhf_tag_set_epc_pc(target, clone_pc);

            UHF_I(
                "CLONE",
                "EPC PC STAGE COMPLETE expected_pc=0x%04X mode3400=%u",
                (unsigned int)clone_pc,
                force_pc3400 ? 1U : 0U);
            uhf_debug_flush();
        }
    }

    /*
     * Reserved PW clone: only standard MB0 password bytes are claimed.
     * Source bytes after the first 8 are explicitly counted as skipped.
     */
    if((mask & WRITE_RFU) && source->reserved->size >= 4U) {
        uint16_t target_reserved_bytes = 0;
        UHFSizeBankStatus target_reserved_status = UHFSizeBankStatusUnknown;

        UHFWorkerEvent probe_event = probe_bank_size_bytes(
            uhf_worker, target, ReservedBank, &target_reserved_bytes, &target_reserved_status);

        if(probe_event == UHFWorkerEventAborted) {
            event = probe_event;
            goto clone_done;
        }

        uhf_worker->CloneReservedTargetBytes = target_reserved_bytes;

        if(target_reserved_status == UHFSizeBankStatusLocked ||
           target_reserved_status == UHFSizeBankStatusWrongPassword) {
            UHF_W(
                "CLONE",
                "RESERVED FIT protected status=%d source=%u target_best=%u",
                (int)target_reserved_status,
                (unsigned int)source->reserved->size,
                (unsigned int)target_reserved_bytes);
            event = UHFWorkerEventAccessDenied;
            goto clone_done;
        }

        if(target_reserved_status != UHFSizeBankStatusOk) {
            UHF_W(
                "CLONE",
                "RESERVED FIT target probe failed status=%d source=%u",
                (int)target_reserved_status,
                (unsigned int)source->reserved->size);
            event = UHFWorkerEventFail;
            goto clone_done;
        }

        uint16_t write_bytes = 0;
        if(source->reserved->size >= 8U && target_reserved_bytes >= 8U) {
            write_bytes = 8U;
        } else if(source->reserved->size >= 4U && target_reserved_bytes >= 4U) {
            write_bytes = 4U;
        }

        if(write_bytes == 0U) {
            UHF_W(
                "CLONE",
                "RESERVED FIT cannot write source=%u target=%u",
                (unsigned int)source->reserved->size,
                (unsigned int)target_reserved_bytes);
            event = UHFWorkerEventFail;
            goto clone_done;
        }

        uhf_worker->CloneReservedWrittenBytes = write_bytes;
        uhf_worker->CloneReservedSkippedBytes =
            source->reserved->size > write_bytes ?
                (uint16_t)(source->reserved->size - write_bytes) :
                0U;

        UHF_I(
            "CLONE",
            "RESERVED FIT source=%u target=%u write=%u skipped=%u mode=%s",
            (unsigned int)source->reserved->size,
            (unsigned int)target_reserved_bytes,
            (unsigned int)write_bytes,
            (unsigned int)uhf_worker->CloneReservedSkippedBytes,
            write_bytes == 8U ? "Kill+Access" : "Kill-only");
        uhf_debug_flush();

        M100ResponseType reserved_select = select_tag_bounded(uhf_worker, target);
        if(reserved_select != M100SuccessResponse) {
            UHF_W(
                "CLONE",
                "Reserved reselect after EPC write failed status=%d",
                (int)reserved_select);
            event = UHFWorkerEventFail;
            goto clone_done;
        }

        event = clone_write_bank_bounded(
            uhf_worker, source, target, ReservedBank, write_bytes == 8U ? 1U : 0U);

        if(event != UHFWorkerEventSuccess) goto clone_done;

        if(write_bytes == 8U) {
            const uint8_t* ap = source->reserved->access_password;
            uhf_worker->DefaultAP = ((uint32_t)ap[0] << 24) | ((uint32_t)ap[1] << 16) |
                                    ((uint32_t)ap[2] << 8) | (uint32_t)ap[3];

            UHF_I(
                "CLONE",
                "RESERVED FIT Access PW cloned; worker AP updated=0x%08lX",
                (unsigned long)uhf_worker->DefaultAP);
            uhf_debug_flush();
        }
    }

    if(mode_marker_control) {
        const uint16_t expected_pc = force_pc3400 ? UHF_CLONE_PC3400_SOURCE_PC :
                                                    UHF_CLONE_PC3000_SOURCE_PC;
        const char* component = force_pc3400 ? "CLONE3400" : "CLONE3000";

        event = clone_mode_verify_final(uhf_worker, source, expected_pc);
        if(event != UHFWorkerEventSuccess) goto clone_done;

        UHF_I(
            component,
            "SUCCESS PC=0x%04X CRC=0x%04X marker=UserW%u=%04X",
            (unsigned int)uhf_worker->ClonePc3400FinalPc,
            (unsigned int)uhf_worker->ClonePc3400FinalCrc,
            (unsigned int)mode_marker.word,
            (unsigned int)mode_marker_value);
        uhf_debug_flush();
    }

clone_done:
    /*
     * A mode conversion or same-mode guard leaves its requested marker ONLY
     * after a fully verified success. On every other exit restore the value
     * captured from the TARGET. Verify it even if the write reported failure:
     * a lost write ACK is ambiguous and restoration may still have reached
     * the tag.
     */
    if(mode_marker_control && mode_target_marker_valid && mode_user_may_have_changed &&
       (!uhf_worker->ClonePc3400Verified || event != UHFWorkerEventSuccess)) {
        UHF_W(
            "CLONEMODE",
            "ROLLBACK because clone did not verify; restore TARGET UserW%u=%04X",
            (unsigned int)mode_marker.word,
            (unsigned int)mode_target_marker_word);
        uhf_debug_flush();

        UHFWorkerEvent rollback_write = clone_mode_write_user_word(
            uhf_worker, target, mode_marker.word, mode_target_marker_word, false);
        uint16_t rollback_observed = 0U;
        M100ResponseType rollback_read_status = M100ValidationFail;
        UHFWorkerEvent rollback_read = clone_read_user_word_bounded(
            uhf_worker, target, mode_marker.word, &rollback_observed, false, &rollback_read_status);
        bool rollback_ok = rollback_read == UHFWorkerEventSuccess &&
                           rollback_observed == mode_target_marker_word;

        UHF_I(
            "CLONEMODE",
            "ROLLBACK write_event=%d read_event=%d read_status=%d "
            "expected=%04X observed=%04X verified=%u",
            (int)rollback_write,
            (int)rollback_read,
            (int)rollback_read_status,
            (unsigned int)mode_target_marker_word,
            (unsigned int)rollback_observed,
            rollback_ok ? 1U : 0U);
        uhf_debug_flush();

        if(rollback_ok) {
            uhf_worker->ClonePc3400MarkerApplied = false;
            uhf_worker->CloneModeConversionApplied = false;
        } else {
            /* Surface the unsafe state instead of claiming the marker is gone. */
            uhf_worker->ClonePc3400MarkerApplied = true;
            uhf_worker->ClonePc3400Verified = false;
            event = UHFWorkerEventFail;
            UHF_E(
                "CLONEMODE",
                "ROLLBACK FAILED: target UserW%u may remain modified",
                (unsigned int)mode_marker.word);
            uhf_debug_flush();
        }
    }

    UHF_I("CLONE", "WRITE SESSION END event=%d mask=0x%04X", (int)event, (unsigned int)mask);
    uhf_debug_flush();

    uhf_tag_free(target);
    return event;
}

#define UMI_TEST_IO_RETRIES     4U
#define UMI_TEST_RESTORE_CYCLES 3U
#define UMI_TEST_POLL_LIMIT     200U

static bool umi_test_epc_matches(const UHFTag* a, const UHFTag* b) {
    return a && b && a->epc && b->epc && a->epc->size == b->epc->size && a->epc->size > 0 &&
           memcmp(a->epc->data, b->epc->data, a->epc->size) == 0;
}

/*
 * Poll the same EPC and capture the PC.
 *
 * When honor_stop is false this helper is being used after a mandatory
 * restoration path. Once a temporary User write succeeds, Back/Stop must not
 * be able to leave that temporary value behind.
 */
static bool
    umi_test_poll_pc(UHFWorker* worker, const UHFTag* target, uint16_t* pc_out, bool honor_stop) {
    UHFTag* probe = uhf_tag_alloc();
    if(!probe) return false;

    bool ok = false;

    for(uint32_t attempt = 0; attempt < UMI_TEST_POLL_LIMIT; attempt++) {
        if(honor_stop && uhf_worker_stop_requested(worker)) break;

        uhf_tag_reset(probe);
        M100ResponseType status = m100_single_poll(worker->module, probe, worker);

        if(status == M100SuccessResponse && umi_test_epc_matches(target, probe)) {
            *pc_out = probe->epc->pc;
            ok = true;

            UHF_I(
                "UMI",
                "POLL PC same-tag attempt=%lu pc=0x%04X UMI=%u",
                (unsigned long)(attempt + 1U),
                (unsigned int)*pc_out,
                (*pc_out & 0x0400U) ? 1U : 0U);
            break;
        }

        furi_delay_ms(2);
    }

    uhf_tag_free(probe);
    return ok;
}

/*
 * Select helper used during mandatory restoration. Deliberately does not
 * honor the worker Stop flag.
 */
static M100ResponseType umi_test_select_restore_bounded(UHFWorker* worker, UHFTag* target) {
    M100ResponseType status = M100EmptyResponse;

    for(uint8_t attempt = 1; attempt <= SELECT_MAX_RETRIES; attempt++) {
        status = m100_set_select(worker->module, target);

        UHF_I(
            "UMI",
            "RESTORE select attempt=%u/%u status=%d",
            (unsigned int)attempt,
            (unsigned int)SELECT_MAX_RETRIES,
            (int)status);

        if(status == M100SuccessResponse) return status;
        furi_delay_ms(5);
    }

    return status;
}

static M100ResponseType umi_test_write_user_word(
    UHFWorker* worker,
    UHFTag* target,
    uint16_t word_index,
    uint16_t word_value,
    bool restoration) {
    uint8_t data[2] = {
        (uint8_t)((word_value >> 8) & 0xFFU),
        (uint8_t)(word_value & 0xFFU),
    };

    M100ResponseType status = M100EmptyResponse;

    for(uint8_t attempt = 1; attempt <= UMI_TEST_IO_RETRIES; attempt++) {
        if(!restoration && uhf_worker_stop_requested(worker)) {
            status = M100EmptyResponse;
            break;
        }

        M100ResponseType select_status = restoration ?
                                             umi_test_select_restore_bounded(worker, target) :
                                             select_tag_bounded(worker, target);

        if(select_status != M100SuccessResponse) {
            status = select_status;
            if(restoration) furi_delay_ms(5);
            continue;
        }

        status = m100_write_raw_words(
            worker->module, UserBank, word_index, data, sizeof(data), worker->DefaultAP);

        UHF_I(
            "UMI",
            "%s UserW%u attempt=%u/%u value=%04X status=%d",
            restoration ? "RESTORE" : "TEMP WRITE",
            (unsigned int)word_index,
            (unsigned int)attempt,
            (unsigned int)UMI_TEST_IO_RETRIES,
            (unsigned int)word_value,
            (int)status);
        uhf_debug_flush();

        if(status == M100SuccessResponse) break;
        if(status == M100APWrong || status == M100MemoryLocked || status == M100MemoryOverrun) {
            break;
        }

        furi_delay_ms(8);
    }

    return status;
}

/* Read exactly one User word so adjacent/special words are never touched. */
static M100ResponseType umi_test_read_user_word(
    UHFWorker* worker,
    UHFTag* target,
    uint16_t word_index,
    uint16_t* word_out,
    bool restoration) {
    M100ResponseType status = M100EmptyResponse;

    for(uint8_t attempt = 1; attempt <= UMI_TEST_IO_RETRIES; attempt++) {
        if(!restoration && uhf_worker_stop_requested(worker)) {
            return M100EmptyResponse;
        }

        M100ResponseType select_status = restoration ?
                                             umi_test_select_restore_bounded(worker, target) :
                                             select_tag_bounded(worker, target);

        if(select_status != M100SuccessResponse) {
            status = select_status;
            continue;
        }

        uint8_t data[2] = {0};
        size_t data_size = 0U;
        status = m100_read_raw_words(
            worker->module,
            target,
            UserBank,
            word_index,
            1U,
            worker->DefaultAP,
            data,
            sizeof(data),
            &data_size);

        UHF_I(
            "UMI",
            "%s UserW%u READ attempt=%u/%u status=%d bytes=%u",
            restoration ? "RESTORE VERIFY" : "READ",
            (unsigned int)word_index,
            (unsigned int)attempt,
            (unsigned int)UMI_TEST_IO_RETRIES,
            (int)status,
            (unsigned int)data_size);
        uhf_debug_flush();

        if(status == M100SuccessResponse && data_size == sizeof(data)) {
            *word_out = (uint16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
            return M100SuccessResponse;
        }

        if(status == M100APWrong || status == M100MemoryLocked || status == M100MemoryOverrun) {
            return status;
        }

        furi_delay_ms(8);
    }

    return status;
}

static UHFWorkerEvent umi_test_restore_word(
    UHFWorker* worker,
    UHFTag* target,
    uint16_t word_index,
    uint16_t original_value) {
    M100ResponseType write_status = M100EmptyResponse;
    M100ResponseType verify_status = M100EmptyResponse;
    uint16_t verify_value = 0U;

    for(uint8_t cycle = 1U; cycle <= UMI_TEST_RESTORE_CYCLES; cycle++) {
        write_status = umi_test_write_user_word(worker, target, word_index, original_value, true);

        /*
         * A reader can return an ambiguous command error even when the tag
         * accepted the write. Always verify the actual word, regardless of
         * the write response, before declaring restoration failed.
         */
        furi_delay_ms(25U);
        verify_value = 0U;
        verify_status = umi_test_read_user_word(worker, target, word_index, &verify_value, true);

        UHF_I(
            "UMI",
            "RESTORE cycle=%u/%u UserW%u write=%d verify=%d value=%04X expected=%04X",
            (unsigned int)cycle,
            (unsigned int)UMI_TEST_RESTORE_CYCLES,
            (unsigned int)word_index,
            (int)write_status,
            (int)verify_status,
            (unsigned int)verify_value,
            (unsigned int)original_value);
        uhf_debug_flush();

        if(verify_status == M100SuccessResponse && verify_value == original_value) {
            worker->UmiTestLastStatus = M100SuccessResponse;
            worker->UmiTestUserAfter[0] = (uint8_t)((verify_value >> 8) & 0xFFU);
            worker->UmiTestUserAfter[1] = (uint8_t)(verify_value & 0xFFU);
            worker->UmiTestTemporaryWriteDone = false;
            worker->UmiTestRestoreVerified = true;

            UHF_I(
                "UMI",
                "RESTORE VERIFIED UserW%u=%04X",
                (unsigned int)word_index,
                (unsigned int)verify_value);
            uhf_debug_flush();
            return UHFWorkerEventSuccess;
        }

        furi_delay_ms(30U);
    }

    worker->UmiTestLastStatus = verify_status != M100EmptyResponse ? verify_status : write_status;
    worker->UmiTestFailureStage = write_status == M100SuccessResponse ?
                                      UHFUmiTestStageVerifyRestore :
                                      UHFUmiTestStageRestore;
    worker->UmiTestOutcome = UHFUmiTestOutcomeRestoreFailed;
    if(verify_status == M100SuccessResponse) {
        worker->UmiTestUserAfter[0] = (uint8_t)((verify_value >> 8) & 0xFFU);
        worker->UmiTestUserAfter[1] = (uint8_t)(verify_value & 0xFFU);
    }

    UHF_W(
        "UMI",
        "RESTORE FAILED UserW%u write=%d verify=%d expected=%04X actual=%04X",
        (unsigned int)word_index,
        (int)write_status,
        (int)verify_status,
        (unsigned int)original_value,
        (unsigned int)verify_value);
    uhf_debug_flush();
    return UHFWorkerEventFail;
}

/*
 * Try one candidate, observe PC, and ALWAYS restore the original word before
 * returning. The result is therefore safe to chain into an automatic search.
 */
static UHFWorkerEvent umi_test_try_candidate(
    UHFWorker* worker,
    UHFTag* target,
    uint16_t word_index,
    uint16_t original_value,
    uint16_t candidate_value,
    uint16_t* pc_during) {
    if(uhf_worker_stop_requested(worker)) return UHFWorkerEventAborted;

    worker->UmiTestCurrentWord = word_index;
    worker->UmiTestCurrentValue = candidate_value;
    worker->UmiTestRestoreVerified = false;

    M100ResponseType status =
        umi_test_write_user_word(worker, target, word_index, candidate_value, false);
    worker->UmiTestLastStatus = status;

    if(status != M100SuccessResponse) {
        worker->UmiTestFailureStage = UHFUmiTestStageWriteTemp;
        worker->UmiTestOutcome = UHFUmiTestOutcomeFailed;

        UHF_W(
            "UMI",
            "Candidate write failed UserW%u=%04X status=%d",
            (unsigned int)word_index,
            (unsigned int)candidate_value,
            (int)status);
        uhf_debug_flush();

        /*
         * Validation/no-tag/checksum failures are ambiguous: the command may
         * have reached the tag even though its reply was unusable. Restore the
         * backed-up word before returning from those cases.
         */
        if(status == M100ValidationFail || status == M100NoTagResponse ||
           status == M100EmptyResponse || status == M100ChecksumFail) {
            M100ResponseType failed_write_status = status;
            worker->UmiTestTemporaryWriteDone = true;
            UHFWorkerEvent restore_event =
                umi_test_restore_word(worker, target, word_index, original_value);
            if(restore_event != UHFWorkerEventSuccess) return restore_event;
            worker->UmiTestLastStatus = failed_write_status;
            worker->UmiTestFailureStage = UHFUmiTestStageWriteTemp;
            worker->UmiTestOutcome = UHFUmiTestOutcomeFailed;
        }

        if(status == M100APWrong || status == M100MemoryLocked) return UHFWorkerEventAccessDenied;
        return UHFWorkerEventFail;
    }

    worker->UmiTestTemporaryWriteDone = true;
    worker->UmiTestCandidatesTested++;

    furi_delay_ms(20U);
    uint16_t candidate_readback = 0U;
    M100ResponseType candidate_read_status =
        umi_test_read_user_word(worker, target, word_index, &candidate_readback, true);
    bool candidate_verified = candidate_read_status == M100SuccessResponse &&
                              candidate_readback == candidate_value;

    UHF_I(
        "UMI",
        "TEMP VERIFY UserW%u status=%d expected=%04X actual=%04X ok=%u",
        (unsigned int)word_index,
        (int)candidate_read_status,
        (unsigned int)candidate_value,
        (unsigned int)candidate_readback,
        candidate_verified ? 1U : 0U);
    uhf_debug_flush();

    bool aborted_after_write = uhf_worker_stop_requested(worker);
    bool pc_ok = false;

    if(candidate_verified && !aborted_after_write) {
        pc_ok = umi_test_poll_pc(worker, target, pc_during, true);

        if(!pc_ok) {
            worker->UmiTestFailureStage = UHFUmiTestStageReadPc;
            UHF_W(
                "UMI",
                "Could not read PC for candidate UserW%u=%04X",
                (unsigned int)word_index,
                (unsigned int)candidate_value);
        }
    }

    // Mandatory restore — never allow Stop/Back to bypass this.
    UHFWorkerEvent restore_event =
        umi_test_restore_word(worker, target, word_index, original_value);

    if(restore_event != UHFWorkerEventSuccess) return restore_event;

    if(aborted_after_write || uhf_worker_stop_requested(worker)) {
        UHF_I("UMI", "AUTO SEARCH aborted AFTER safe restore UserW%u", (unsigned int)word_index);
        return UHFWorkerEventAborted;
    }

    if(!candidate_verified) {
        worker->UmiTestLastStatus = candidate_read_status;
        worker->UmiTestFailureStage = UHFUmiTestStageWriteTemp;
        worker->UmiTestOutcome = UHFUmiTestOutcomeFailed;
        return UHFWorkerEventFail;
    }

    if(!pc_ok) {
        worker->UmiTestOutcome = UHFUmiTestOutcomeFailed;
        return UHFWorkerEventFail;
    }

    UHF_I(
        "UMI",
        "CANDIDATE UserW%u %04X>%04X pc=0x%04X UMI=%u restore=OK",
        (unsigned int)word_index,
        (unsigned int)original_value,
        (unsigned int)candidate_value,
        (unsigned int)*pc_during,
        (*pc_during & 0x0400U) ? 1U : 0U);
    uhf_debug_flush();

    return UHFWorkerEventSuccess;
}

/*
 * Automatic reversible UMI search
 * -------------------------------
 *
 * Goal for either baseline UMI state:
 *  1. Read/backup the complete readable User bank.
 *  2. Scan words from LAST to FIRST with a broad inverted-word candidate.
 *     The first hit is the furthest User word that toggles UMI.
 *  3. On that word, test 16 one-bit changes to find a minimal Hamming-distance
 *     marker. For an all-zero word this searches:
 *       0001, 0002, 0004, ... 8000
 *  4. Confirm the winning value one more time.
 *  5. Restore and verify the original word after EVERY candidate.
 *  6. Final-poll PC after all restoration.
 *
 * The search never intentionally leaves the marker written. The restored and
 * toggled masked-bit states are saved per TID model for Clone PC3400/PC3000.
 */
static UHFWorkerEvent umi_test_card(UHFWorker* worker) {
    worker->UmiTestOutcome = UHFUmiTestOutcomeNone;
    worker->UmiTestFailureStage = UHFUmiTestStageNone;
    worker->UmiTestPcBefore = 0;
    worker->UmiTestPcDuring = 0;
    worker->UmiTestPcAfter = 0;
    memset(worker->UmiTestUserBefore, 0, sizeof(worker->UmiTestUserBefore));
    memset(worker->UmiTestUserDuring, 0, sizeof(worker->UmiTestUserDuring));
    memset(worker->UmiTestUserAfter, 0, sizeof(worker->UmiTestUserAfter));
    worker->UmiTestUserBytes = 0;
    worker->UmiTestBestWord = 0;
    worker->UmiTestBestValue = 0;
    worker->UmiTestBestMask = 0U;
    worker->UmiTestPc3000Bits = 0U;
    worker->UmiTestPc3400Bits = 0U;
    memset(worker->UmiTestModelId, 0, sizeof(worker->UmiTestModelId));
    worker->UmiTestModelValid = false;
    worker->UmiTestCurrentWord = 0;
    worker->UmiTestCurrentValue = 0;
    worker->UmiTestCandidatesTested = 0;
    worker->UmiTestPositionTests = 0;
    worker->UmiTestBitTests = 0;
    worker->UmiTestBestFound = false;
    worker->UmiTestBestSingleBit = false;
    worker->UmiTestTemporaryWriteDone = false;
    worker->UmiTestRestoreVerified = false;
    worker->UmiTestLastStatus = M100EmptyResponse;

    UHF_I("UMI", "AUTO SEARCH BEGIN DefaultAP=0x%08lX", (unsigned long)worker->DefaultAP);
    uhf_debug_flush();

    UHFTag* target = send_polling_command(worker);
    if(!target) {
        worker->UmiTestFailureStage = UHFUmiTestStagePoll;
        worker->UmiTestOutcome = UHFUmiTestOutcomeFailed;
        return uhf_worker_stop_requested(worker) ? UHFWorkerEventAborted : UHFWorkerEventFail;
    }

    worker->UmiTestPcBefore = target->epc->pc;

    UHF_I(
        "UMI",
        "BASE PC=0x%04X UMI=%u",
        (unsigned int)worker->UmiTestPcBefore,
        (worker->UmiTestPcBefore & 0x0400U) ? 1U : 0U);
    debug_log_tag_epc("UMI target EPC", target);

    UHFWorkerEvent model_event = read_tid_model_id_bounded(
        worker, target, worker->UmiTestModelId, UMI_TEST_IO_RETRIES, false, "UMI");
    if(model_event == UHFWorkerEventAborted) {
        uhf_tag_free(target);
        return model_event;
    }
    worker->UmiTestModelValid = model_event == UHFWorkerEventSuccess;
    if(!worker->UmiTestModelValid) {
        UHF_W("UMI", "Test will run, but marker cannot be saved without TID model");
        uhf_debug_flush();
    }

    // Read the complete User bank once so every word can be restored exactly.
    uhf_tag_set_user_size(target, 0);
    UHFWorkerEvent probe_event = read_bank_till_max_length(worker, target, UserBank);

    if(probe_event != UHFWorkerEventSuccess) {
        worker->UmiTestFailureStage = UHFUmiTestStageReadUser;
        worker->UmiTestOutcome = UHFUmiTestOutcomeFailed;
        UHF_W("UMI", "AUTO User capacity/read failed event=%d", (int)probe_event);
        uhf_tag_free(target);
        return probe_event;
    }

    size_t user_bytes = target->user->size;
    if(user_bytes > USER_MAX_BANK_SIZE) user_bytes = USER_MAX_BANK_SIZE;
    user_bytes &= ~(size_t)1U;
    worker->UmiTestUserBytes = (uint16_t)user_bytes;

    uint8_t user_backup[USER_MAX_BANK_SIZE];
    memset(user_backup, 0, sizeof(user_backup));
    if(user_bytes > 0) {
        memcpy(user_backup, target->user->data, user_bytes);
    }

    uint16_t word_count = (uint16_t)(user_bytes / 2U);

    UHF_I(
        "UMI",
        "AUTO User capacity=%u bytes / %u words",
        (unsigned int)user_bytes,
        (unsigned int)word_count);
    uhf_debug_flush();

    if(word_count == 0U) {
        worker->UmiTestOutcome = UHFUmiTestOutcomeUnchanged;
        worker->UmiTestRestoreVerified = true;
        worker->UmiTestPcDuring = worker->UmiTestPcBefore;
        worker->UmiTestPcAfter = worker->UmiTestPcBefore;

        UHF_I("UMI", "AUTO SEARCH no User words available");
        uhf_tag_free(target);
        return UHFWorkerEventSuccess;
    }

    int32_t hit_word = -1;
    uint16_t hit_value = 0;
    int32_t first_test_word = (int32_t)word_count - 1;
    const bool baseline_umi = (worker->UmiTestPcBefore & 0x0400U) != 0U;
    const bool toggled_umi = !baseline_umi;

    UHF_I(
        "UMI",
        "AUTO direction UMI %u -> %u -> %u",
        baseline_umi ? 1U : 0U,
        toggled_umi ? 1U : 0U,
        baseline_umi ? 1U : 0U);

    /*
     * Monza 4QT can expose a short, profile-dependent User window. The field
     * log showed W0 was independently writable/restorable while touching W1
     * made the bank stop answering. In this profile, test only W0; that is
     * also the word used by the dedicated PC=3400 clone marker.
     */
    if(worker->ReadProfile == UHF_READER_TAG_PROFILE_MONZA4QT) {
        first_test_word = 0;
        UHF_I(
            "UMI",
            "Monza4QT safety mode: test UserW0 only (bank_words=%u)",
            (unsigned int)word_count);
        uhf_debug_flush();
    }

    /*
     * Phase A: find the furthest word capable of toggling UMI using a broad
     * 16-bit inversion. Search from the end because a later word is preferable
     * for a future clone marker: it keeps the beginning of User memory intact.
     * Monza 4QT is the conservative exception described above.
     */
    for(int32_t word = first_test_word; word >= 0; word--) {
        if(uhf_worker_stop_requested(worker)) {
            uhf_tag_free(target);
            return UHFWorkerEventAborted;
        }

        size_t offset = (size_t)word * 2U;
        uint16_t original =
            (uint16_t)(((uint16_t)user_backup[offset] << 8) | (uint16_t)user_backup[offset + 1U]);
        uint16_t candidate = (uint16_t)(original ^ 0xFFFFU);
        uint16_t candidate_pc = 0;

        worker->UmiTestPositionTests++;

        UHF_I(
            "UMI",
            "AUTO POS test=%u UserW%u original=%04X candidate=%04X",
            (unsigned int)worker->UmiTestPositionTests,
            (unsigned int)word,
            (unsigned int)original,
            (unsigned int)candidate);
        uhf_debug_flush();

        UHFWorkerEvent event = umi_test_try_candidate(
            worker, target, (uint16_t)word, original, candidate, &candidate_pc);

        if(event != UHFWorkerEventSuccess) {
            uhf_tag_free(target);
            return event;
        }

        if(((candidate_pc & 0x0400U) != 0U) == toggled_umi) {
            hit_word = word;
            hit_value = candidate;

            UHF_I(
                "UMI",
                "AUTO POS HIT UserW%u value=%04X pc=0x%04X",
                (unsigned int)word,
                (unsigned int)candidate,
                (unsigned int)candidate_pc);
            uhf_debug_flush();
            break;
        }
    }

    if(hit_word < 0) {
        worker->UmiTestOutcome = UHFUmiTestOutcomeUnchanged;
        worker->UmiTestPcDuring = worker->UmiTestPcBefore;

        furi_delay_ms(20);
        if(!umi_test_poll_pc(worker, target, &worker->UmiTestPcAfter, true)) {
            worker->UmiTestFailureStage = UHFUmiTestStageFinalPc;
            worker->UmiTestOutcome = UHFUmiTestOutcomeFailed;
            uhf_tag_free(target);
            return UHFWorkerEventFail;
        }

        worker->UmiTestRestoreVerified = true;

        UHF_I(
            "UMI",
            "AUTO SEARCH COMPLETE no candidate tests=%u pos=%u bit=%u final_pc=0x%04X",
            (unsigned int)worker->UmiTestCandidatesTested,
            (unsigned int)worker->UmiTestPositionTests,
            (unsigned int)worker->UmiTestBitTests,
            (unsigned int)worker->UmiTestPcAfter);
        uhf_debug_flush();

        uhf_tag_free(target);
        return UHFWorkerEventSuccess;
    }

    size_t hit_offset = (size_t)hit_word * 2U;
    uint16_t hit_original = (uint16_t)(((uint16_t)user_backup[hit_offset] << 8) |
                                       (uint16_t)user_backup[hit_offset + 1U]);

    uint16_t best_value = hit_value;
    bool best_single_bit = false;

    /*
     * Phase B: same word, but exactly one bit changed from the original.
     * Prefer clearing an existing bit when baseline UMI=1 and setting a clear
     * bit when baseline UMI=0. This finds 0100->0000 and 0000->0100 before
     * unrelated exact-value disruptions such as 0100->0101.
     */
    for(uint8_t pass = 0U; pass < 2U && !best_single_bit; pass++) {
        for(uint8_t bit = 0U; bit < 16U; bit++) {
            uint16_t bit_mask = (uint16_t)(1U << bit);
            bool original_bit_set = (hit_original & bit_mask) != 0U;
            bool preferred_direction = baseline_umi ? original_bit_set : !original_bit_set;
            if((pass == 0U) != preferred_direction) continue;

            if(uhf_worker_stop_requested(worker)) {
                uhf_tag_free(target);
                return UHFWorkerEventAborted;
            }

            uint16_t candidate = (uint16_t)(hit_original ^ bit_mask);
            uint16_t candidate_pc = 0;

            worker->UmiTestBitTests++;

            UHF_I(
                "UMI",
                "AUTO BIT test=%u pass=%u UserW%u bit=%u original=%04X candidate=%04X",
                (unsigned int)worker->UmiTestBitTests,
                (unsigned int)(pass + 1U),
                (unsigned int)hit_word,
                (unsigned int)bit,
                (unsigned int)hit_original,
                (unsigned int)candidate);
            uhf_debug_flush();

            UHFWorkerEvent event = umi_test_try_candidate(
                worker, target, (uint16_t)hit_word, hit_original, candidate, &candidate_pc);

            if(event != UHFWorkerEventSuccess) {
                uhf_tag_free(target);
                return event;
            }

            if(((candidate_pc & 0x0400U) != 0U) == toggled_umi) {
                best_value = candidate;
                best_single_bit = true;

                UHF_I(
                    "UMI",
                    "AUTO BIT HIT UserW%u bit=%u value=%04X pc=0x%04X",
                    (unsigned int)hit_word,
                    (unsigned int)bit,
                    (unsigned int)candidate,
                    (unsigned int)candidate_pc);
                uhf_debug_flush();
                break;
            }
        }
    }

    /*
     * Phase C: confirm the winning marker once more. This is useful on these
     * clone chips because an individual write can occasionally return a
     * transient reader/tag error before succeeding on a retry.
     */
    uint16_t confirm_pc = 0;
    UHF_I(
        "UMI",
        "AUTO CONFIRM UserW%u value=%04X single_bit=%u",
        (unsigned int)hit_word,
        (unsigned int)best_value,
        best_single_bit ? 1U : 0U);
    uhf_debug_flush();

    UHFWorkerEvent confirm_event = umi_test_try_candidate(
        worker, target, (uint16_t)hit_word, hit_original, best_value, &confirm_pc);

    if(confirm_event != UHFWorkerEventSuccess) {
        uhf_tag_free(target);
        return confirm_event;
    }

    if(((confirm_pc & 0x0400U) != 0U) != toggled_umi) {
        worker->UmiTestFailureStage = UHFUmiTestStageReadPc;
        worker->UmiTestOutcome = UHFUmiTestOutcomeFailed;

        UHF_W(
            "UMI",
            "AUTO CONFIRM unstable UserW%u value=%04X pc=0x%04X",
            (unsigned int)hit_word,
            (unsigned int)best_value,
            (unsigned int)confirm_pc);
        uhf_debug_flush();

        uhf_tag_free(target);
        return UHFWorkerEventFail;
    }

    worker->UmiTestBestFound = true;
    worker->UmiTestBestWord = (uint16_t)hit_word;
    worker->UmiTestBestValue = best_value;
    /*
     * Persist complete observed word states. A single-bit change proves a
     * transition only in the tested surrounding word value; it does not prove
     * that the same bit is independent of all other bits on this chip.
     */
    worker->UmiTestBestMask = 0xFFFFU;
    if(baseline_umi) {
        worker->UmiTestPc3400Bits = (uint16_t)(hit_original & worker->UmiTestBestMask);
        worker->UmiTestPc3000Bits = (uint16_t)(best_value & worker->UmiTestBestMask);
    } else {
        worker->UmiTestPc3000Bits = (uint16_t)(hit_original & worker->UmiTestBestMask);
        worker->UmiTestPc3400Bits = (uint16_t)(best_value & worker->UmiTestBestMask);
    }
    worker->UmiTestBestSingleBit = best_single_bit;
    worker->UmiTestPcDuring = confirm_pc;

    worker->UmiTestUserBefore[0] = (uint8_t)((hit_original >> 8) & 0xFFU);
    worker->UmiTestUserBefore[1] = (uint8_t)(hit_original & 0xFFU);
    worker->UmiTestUserDuring[0] = (uint8_t)((best_value >> 8) & 0xFFU);
    worker->UmiTestUserDuring[1] = (uint8_t)(best_value & 0xFFU);
    worker->UmiTestUserAfter[0] = worker->UmiTestUserBefore[0];
    worker->UmiTestUserAfter[1] = worker->UmiTestUserBefore[1];

    // Every candidate is already restored; final PC should return to baseline.
    furi_delay_ms(20);
    if(!umi_test_poll_pc(worker, target, &worker->UmiTestPcAfter, true)) {
        worker->UmiTestFailureStage = UHFUmiTestStageFinalPc;
        worker->UmiTestOutcome = UHFUmiTestOutcomeFailed;
        uhf_tag_free(target);
        return UHFWorkerEventFail;
    }
    if(((worker->UmiTestPcAfter & 0x0400U) != 0U) != baseline_umi) {
        worker->UmiTestFailureStage = UHFUmiTestStageFinalPc;
        worker->UmiTestOutcome = UHFUmiTestOutcomeFailed;
        UHF_W(
            "UMI",
            "FINAL PC did not restore UMI expected=%u pc=0x%04X",
            baseline_umi ? 1U : 0U,
            (unsigned int)worker->UmiTestPcAfter);
        uhf_debug_flush();
        uhf_tag_free(target);
        return UHFWorkerEventFail;
    }

    worker->UmiTestOutcome = UHFUmiTestOutcomeAffected;
    worker->UmiTestRestoreVerified = true;

    UHF_I(
        "UMI",
        "AUTO BEST UserW%u original=%04X toggled=%04X mask=%04X "
        "P3000=%04X P3400=%04X single_bit=%u PC=%04X>%04X>%04X "
        "tests=%u pos=%u bit=%u restore=OK model=%02X%02X%02X%02X valid=%u",
        (unsigned int)worker->UmiTestBestWord,
        (unsigned int)hit_original,
        (unsigned int)worker->UmiTestBestValue,
        (unsigned int)worker->UmiTestBestMask,
        (unsigned int)worker->UmiTestPc3000Bits,
        (unsigned int)worker->UmiTestPc3400Bits,
        worker->UmiTestBestSingleBit ? 1U : 0U,
        (unsigned int)worker->UmiTestPcBefore,
        (unsigned int)worker->UmiTestPcDuring,
        (unsigned int)worker->UmiTestPcAfter,
        (unsigned int)worker->UmiTestCandidatesTested,
        (unsigned int)worker->UmiTestPositionTests,
        (unsigned int)worker->UmiTestBitTests,
        worker->UmiTestModelId[0],
        worker->UmiTestModelId[1],
        worker->UmiTestModelId[2],
        worker->UmiTestModelId[3],
        worker->UmiTestModelValid ? 1U : 0U);
    uhf_debug_flush();

    uhf_tag_free(target);
    return UHFWorkerEventSuccess;
}

#define REWRITE_IO_RETRIES 4U

static UHFRewriteBankStatus rewrite_status_from_error(M100ResponseType status) {
    switch(status) {
    case M100MemoryLocked:
        return UHFRewriteBankStatusReadOnly;
    case M100APWrong:
        return UHFRewriteBankStatusProtected;
    case M100MemoryOverrun:
        return UHFRewriteBankStatusUnavailable;
    default:
        return UHFRewriteBankStatusError;
    }
}

static M100ResponseType rewrite_select(UHFWorker* worker, UHFTag* target, bool restoration) {
    M100ResponseType status = M100EmptyResponse;
    for(uint8_t attempt = 0U; attempt < REWRITE_IO_RETRIES; attempt++) {
        if(!restoration && uhf_worker_stop_requested(worker)) {
            return M100EmptyResponse;
        }
        status = m100_set_select(worker->module, target);
        if(status == M100SuccessResponse) return status;
        furi_delay_ms(5U);
    }
    return status;
}

static M100ResponseType rewrite_read_word(
    UHFWorker* worker,
    UHFTag* target,
    BankType bank,
    uint16_t word,
    uint8_t output[2],
    bool restoration) {
    M100ResponseType status = M100EmptyResponse;
    for(uint8_t attempt = 0U; attempt < REWRITE_IO_RETRIES; attempt++) {
        status = rewrite_select(worker, target, restoration);
        if(status != M100SuccessResponse) continue;

        size_t output_size = 0U;
        status = m100_read_raw_words(
            worker->module, target, bank, word, 1U, worker->DefaultAP, output, 2U, &output_size);
        if(status == M100SuccessResponse && output_size == 2U) return status;
        if(status == M100APWrong || status == M100MemoryLocked || status == M100MemoryOverrun) {
            return status;
        }
        furi_delay_ms(8U);
    }
    return status;
}

static M100ResponseType rewrite_write_word(
    UHFWorker* worker,
    UHFTag* target,
    BankType bank,
    uint16_t word,
    const uint8_t value[2],
    bool restoration) {
    M100ResponseType status = M100EmptyResponse;
    for(uint8_t attempt = 0U; attempt < REWRITE_IO_RETRIES; attempt++) {
        status = rewrite_select(worker, target, restoration);
        if(status != M100SuccessResponse) continue;

        status = m100_write_raw_words(worker->module, bank, word, value, 2U, worker->DefaultAP);
        if(status == M100SuccessResponse) return status;
        if(status == M100APWrong || status == M100MemoryLocked || status == M100MemoryOverrun) {
            return status;
        }
        furi_delay_ms(8U);
    }
    return status;
}

static void rewrite_update_epc_word(UHFTag* target, uint16_t epc_word, const uint8_t value[2]) {
    if(!target || !target->epc || epc_word < 2U) return;
    size_t offset = (size_t)(epc_word - 2U) * 2U;
    if(offset + 2U <= target->epc->size) {
        target->epc->data[offset] = value[0];
        target->epc->data[offset + 1U] = value[1];
    }
}

static UHFRewriteBankStatus
    rewrite_probe_bank(UHFWorker* worker, UHFTag* target, BankType bank, uint16_t word) {
    uint8_t original[2] = {0};
    uint8_t candidate[2] = {0};
    uint8_t verify[2] = {0};

    M100ResponseType status = rewrite_read_word(worker, target, bank, word, original, false);
    if(status != M100SuccessResponse) {
        UHF_W(
            "REWRITE",
            "READ bank=%s W%u status=%d",
            debug_bank_name(bank),
            (unsigned int)word,
            (int)status);
        return rewrite_status_from_error(status);
    }

    candidate[0] = (uint8_t)(original[0] ^ 0xA5U);
    candidate[1] = (uint8_t)(original[1] ^ 0x5AU);

    status = rewrite_write_word(worker, target, bank, word, candidate, false);
    if(status != M100SuccessResponse) {
        UHF_I(
            "REWRITE",
            "WRITE bank=%s W%u status=%d",
            debug_bank_name(bank),
            (unsigned int)word,
            (int)status);
        return rewrite_status_from_error(status);
    }

    /* EPC selection must immediately follow the changed EPC value. */
    if(bank == EPCBank) rewrite_update_epc_word(target, word, candidate);

    bool interrupted = uhf_worker_stop_requested(worker);
    bool candidate_read_ok = false;
    bool candidate_verified = false;
    if(!interrupted) {
        status = rewrite_read_word(worker, target, bank, word, verify, false);
        candidate_read_ok = status == M100SuccessResponse;
        candidate_verified = candidate_read_ok && memcmp(verify, candidate, 2U) == 0;
    }

    /* Mandatory best-effort restore: Stop/Back is deliberately ignored here. */
    M100ResponseType restore_status =
        rewrite_write_word(worker, target, bank, word, original, true);
    if(bank == EPCBank && restore_status == M100SuccessResponse) {
        rewrite_update_epc_word(target, word, original);
    }

    uint8_t restored[2] = {0};
    M100ResponseType verify_restore_status = M100EmptyResponse;
    if(restore_status == M100SuccessResponse) {
        verify_restore_status = rewrite_read_word(worker, target, bank, word, restored, true);
    }

    bool restored_ok = restore_status == M100SuccessResponse &&
                       verify_restore_status == M100SuccessResponse &&
                       memcmp(restored, original, 2U) == 0;

    UHF_I(
        "REWRITE",
        "RESULT bank=%s W%u original=%02X%02X test=%02X%02X "
        "test_ok=%u restore_status=%d verify_status=%d restore_ok=%u",
        debug_bank_name(bank),
        (unsigned int)word,
        original[0],
        original[1],
        candidate[0],
        candidate[1],
        candidate_verified ? 1U : 0U,
        (int)restore_status,
        (int)verify_restore_status,
        restored_ok ? 1U : 0U);
    uhf_debug_flush();

    if(!restored_ok) return UHFRewriteBankStatusRestoreFailed;
    if(interrupted) return UHFRewriteBankStatusUnknown;
    if(!candidate_read_ok) return UHFRewriteBankStatusError;
    return candidate_verified ? UHFRewriteBankStatusWritable : UHFRewriteBankStatusReadOnly;
}

static UHFWorkerEvent check_rewritable_card(UHFWorker* worker) {
    worker->RewriteEpcStatus = UHFRewriteBankStatusUnknown;
    worker->RewriteTidStatus = UHFRewriteBankStatusUnknown;
    worker->RewriteUserStatus = UHFRewriteBankStatusUnknown;
    worker->RewriteReservedStatus = UHFRewriteBankStatusUnknown;

    UHF_I("REWRITE", "CHECK BEGIN DefaultAP=0x%08lX", (unsigned long)worker->DefaultAP);
    uhf_debug_flush();

    UHFTag* target = send_polling_command(worker);
    if(!target) {
        return uhf_worker_stop_requested(worker) ? UHFWorkerEventAborted : UHFWorkerEventFail;
    }
    debug_log_tag_epc("rewritable target EPC", target);

    worker->RewriteTidStatus = rewrite_probe_bank(worker, target, TIDBank, 0U);
    if(uhf_worker_stop_requested(worker)) goto rewrite_aborted;

    worker->RewriteUserStatus = rewrite_probe_bank(worker, target, UserBank, 0U);
    if(uhf_worker_stop_requested(worker)) goto rewrite_aborted;

    worker->RewriteReservedStatus = rewrite_probe_bank(worker, target, ReservedBank, 0U);
    if(uhf_worker_stop_requested(worker)) goto rewrite_aborted;

    if(!target->epc || target->epc->size < 2U || (target->epc->size & 1U) != 0U) {
        worker->RewriteEpcStatus = UHFRewriteBankStatusError;
    } else {
        uint16_t last_epc_word = (uint16_t)(2U + (target->epc->size / 2U) - 1U);
        worker->RewriteEpcStatus = rewrite_probe_bank(worker, target, EPCBank, last_epc_word);
    }

    UHF_I(
        "REWRITE",
        "CHECK END EPC=%d TID=%d USER=%d RSV=%d",
        (int)worker->RewriteEpcStatus,
        (int)worker->RewriteTidStatus,
        (int)worker->RewriteUserStatus,
        (int)worker->RewriteReservedStatus);
    uhf_debug_flush();
    uhf_tag_free(target);
    return UHFWorkerEventSuccess;

rewrite_aborted:
    uhf_tag_free(target);
    return UHFWorkerEventAborted;
}

int32_t uhf_worker_task(void* ctx) {
    UHFWorker* uhf_worker = ctx;
    // A reused FuriThread retains its flags. Clear the previous run's stop bit
    // before sampling state. A Stop racing startup also sets state=Stop, so it
    // remains observable even if its just-posted flag was part of this clear.
    furi_thread_flags_clear(UHF_WORKER_FLAG_STOP);
    const UHFWorkerState operation_state = uhf_worker->state;
    UHFWorkerEvent event = UHFWorkerEventFail;
    bool handled = true;

    if(operation_state == UHFWorkerStateStop) {
        return 0;
    }
    if(operation_state == UHFWorkerStateVerify) {
        event = verify_module_connected(uhf_worker);
    } else if(operation_state == UHFWorkerStateDetectSingle) {
        event = read_single_card(uhf_worker);
    } else if(operation_state == UHFWorkerStateDetectSingleFull) {
        event = read_single_full_card(uhf_worker);
    } else if(operation_state == UHFWorkerStateDetectMultiple) {
        event = detect_multiple_cards(uhf_worker);
    } else if(operation_state == UHFWorkerStateDetectMultipleFull) {
        event = detect_multiple_full_cards(uhf_worker);
    } else if(operation_state == UHFWorkerStateDetectForever) {
        event = detect_forever_cards(uhf_worker);
    } else if(operation_state == UHFWorkerStateReadSingleLive) {
        event = read_single_live(uhf_worker);
    } else if(operation_state == UHFWorkerStateDeepReadSelected) {
        event = deep_read_selected_card(uhf_worker);
    } else if(operation_state == UHFWorkerStateDeepReadAll) {
        event = deep_read_all_cards(uhf_worker);
    } else if(operation_state == UHFWorkerStateReadSingleBank) {
        event = read_single_bank_selected(uhf_worker);
    } else if(operation_state == UHFWorkerStateWriteSingle) {
        event = write_single_card(uhf_worker);
    } else if(operation_state == UHFWorkerStateCloneScan) {
        event = clone_scan_card(uhf_worker);
    } else if(operation_state == UHFWorkerStateCloneWrite) {
        event = clone_write_card(uhf_worker);
    } else if(operation_state == UHFWorkerStateUmiTest) {
        event = umi_test_card(uhf_worker);
    } else if(operation_state == UHFWorkerStateGetBankSizes) {
        event = get_bank_sizes_card(uhf_worker);
    } else if(operation_state == UHFWorkerStateCheckRewritable) {
        event = check_rewritable_card(uhf_worker);
    } else {
        handled = false;
    }

    if(handled) {
        if(uhf_worker->callback) {
            uhf_worker->callback(event, uhf_worker->ctx);
        }
    }
    return 0;
}

UHFWorker* uhf_worker_alloc() {
    UHFWorker* uhf_worker = (UHFWorker*)calloc(1, sizeof(UHFWorker));
    if(!uhf_worker) return NULL;

    uhf_worker->thread =
        furi_thread_alloc_ex("UHFWorker", UHF_WORKER_STACK_SIZE, uhf_worker_task, uhf_worker);
    if(!uhf_worker->thread) goto fail;

    uhf_worker->module = m100_module_alloc();
    if(!uhf_worker->module) goto fail;

    uhf_worker->callback = NULL;
    uhf_worker->ctx = NULL;
    uhf_worker->NewTag = uhf_tag_alloc();
    uhf_worker->SelectedTag = uhf_tag_alloc();
    if(!uhf_worker->NewTag || !uhf_worker->SelectedTag) goto fail;

    uhf_worker->KillPwd = false;
    uhf_worker->AccessPwd = false;
    uhf_worker->DefaultAP = 0;
    uhf_worker->DefaultKP = 0;
    uhf_worker->Targeted = false;
    uhf_worker->CloneMask = 0;
    uhf_worker->CloneMaxAttempts = UHF_READER_CLONE_ATTEMPTS_DEFAULT;
    uhf_worker->CloneAttemptCurrent = 0U;
    uhf_worker->CloneAttemptTotal = UHF_READER_CLONE_ATTEMPTS_DEFAULT;
    uhf_worker->CloneModeConversionApproved = false;
    uhf_worker->CloneModeConversionApplied = false;
    uhf_worker->CloneModeSourcePc = 0U;
    uhf_worker->CloneModeTargetPc = 0U;
    uhf_worker->ClonePc3400MarkerApplied = false;
    uhf_worker->ClonePc3400Verified = false;
    uhf_worker->ClonePc3400FinalPc = 0;
    uhf_worker->ClonePc3400FinalCrc = 0;
    uhf_worker->UmiMarkerRegistry = NULL;
    uhf_worker->CloneMarkerKnown = false;
    memset(uhf_worker->CloneMarkerModelId, 0, sizeof(uhf_worker->CloneMarkerModelId));
    uhf_worker->CloneMarkerWord = 0U;
    uhf_worker->CloneMarkerMask = 0U;
    uhf_worker->CloneMarkerPc3000Bits = 0U;
    uhf_worker->CloneMarkerPc3400Bits = 0U;
    uhf_worker->CloneMarkerAppliedValue = 0U;
    uhf_worker->CloneReservedSourceBytes = 0;
    uhf_worker->CloneReservedTargetBytes = 0;
    uhf_worker->CloneReservedWrittenBytes = 0;
    uhf_worker->CloneReservedSkippedBytes = 0;
    uhf_worker->BankSizeEpcBytes = 0;
    uhf_worker->BankSizeTidBytes = 0;
    uhf_worker->BankSizeUserBytes = 0;
    uhf_worker->BankSizeReservedBytes = 0;
    uhf_worker->BankSizeEpcStatus = UHFSizeBankStatusUnknown;
    uhf_worker->BankSizeTidStatus = UHFSizeBankStatusUnknown;
    uhf_worker->BankSizeUserStatus = UHFSizeBankStatusUnknown;
    uhf_worker->BankSizeReservedStatus = UHFSizeBankStatusUnknown;
    uhf_worker->BankSizePc = 0;
    uhf_worker->BankSizeCrc = 0;
    uhf_worker->RewriteEpcStatus = UHFRewriteBankStatusUnknown;
    uhf_worker->RewriteTidStatus = UHFRewriteBankStatusUnknown;
    uhf_worker->RewriteUserStatus = UHFRewriteBankStatusUnknown;
    uhf_worker->RewriteReservedStatus = UHFRewriteBankStatusUnknown;
    uhf_worker->UmiTestOutcome = UHFUmiTestOutcomeNone;
    uhf_worker->UmiTestFailureStage = UHFUmiTestStageNone;
    uhf_worker->UmiTestPcBefore = 0;
    uhf_worker->UmiTestPcDuring = 0;
    uhf_worker->UmiTestPcAfter = 0;
    memset(uhf_worker->UmiTestUserBefore, 0, sizeof(uhf_worker->UmiTestUserBefore));
    memset(uhf_worker->UmiTestUserDuring, 0, sizeof(uhf_worker->UmiTestUserDuring));
    memset(uhf_worker->UmiTestUserAfter, 0, sizeof(uhf_worker->UmiTestUserAfter));
    uhf_worker->UmiTestUserBytes = 0;
    uhf_worker->UmiTestBestWord = 0;
    uhf_worker->UmiTestBestValue = 0;
    uhf_worker->UmiTestBestMask = 0U;
    uhf_worker->UmiTestPc3000Bits = 0U;
    uhf_worker->UmiTestPc3400Bits = 0U;
    memset(uhf_worker->UmiTestModelId, 0, sizeof(uhf_worker->UmiTestModelId));
    uhf_worker->UmiTestModelValid = false;
    uhf_worker->UmiTestCurrentWord = 0;
    uhf_worker->UmiTestCurrentValue = 0;
    uhf_worker->UmiTestCandidatesTested = 0;
    uhf_worker->UmiTestPositionTests = 0;
    uhf_worker->UmiTestBitTests = 0;
    uhf_worker->UmiTestBestFound = false;
    uhf_worker->UmiTestBestSingleBit = false;
    uhf_worker->UmiTestTemporaryWriteDone = false;
    uhf_worker->UmiTestRestoreVerified = false;
    uhf_worker->UmiTestLastStatus = M100EmptyResponse;
    uhf_worker->DumpCurrent = 0;
    uhf_worker->DumpTotal = 0;
    uhf_worker->FullMultiEpcCount = 0;
    uhf_worker->FullMultiDumpCount = 0;
    uhf_worker->FullMultiPartialCount = 0;
    uhf_worker->FullDumpMaxAttempts = FULL_DUMP_ATTEMPTS_FALLBACK;
    uhf_worker->ForeverDelaySeconds = UHF_READER_FOREVER_DELAY_DEFAULT_SEC;
    uhf_worker->ForeverWaiting = false;
    uhf_worker->ForeverWaitRemainingSeconds = 0;
    uhf_worker->ForeverCaptureSerial = 0;
    uhf_worker->ForeverSavedSerial = 0;
    uhf_worker->ReadProfile = UHF_READER_TAG_PROFILE_GENERIC;
    return uhf_worker;

fail:
    UHF_E(
        "WORKER",
        "ALLOC FAIL thread=%p module=%p new_tag=%p selected_tag=%p",
        (void*)uhf_worker->thread,
        (void*)uhf_worker->module,
        (void*)uhf_worker->NewTag,
        (void*)uhf_worker->SelectedTag);
    uhf_debug_flush();

    uhf_tag_free(uhf_worker->NewTag);
    uhf_tag_free(uhf_worker->SelectedTag);
    m100_module_free(uhf_worker->module);
    if(uhf_worker->thread) furi_thread_free(uhf_worker->thread);
    free(uhf_worker);
    return NULL;
}

bool uhf_worker_is_ready(const UHFWorker* worker) {
    return worker && worker->thread && worker->module && worker->module->uart &&
           worker->module->uart->handle && worker->module->uart->buffer && worker->NewTag &&
           worker->SelectedTag;
}

bool uhf_worker_stop_requested(const UHFWorker* worker) {
    furi_assert(worker);
    return (furi_thread_flags_get() & UHF_WORKER_FLAG_STOP) != 0;
}

void uhf_worker_change_state(UHFWorker* worker, UHFWorkerState state) {
    furi_assert(worker);
    worker->state = state;
    if(state == UHFWorkerStateStop &&
       furi_thread_get_state(worker->thread) != FuriThreadStateStopped) {
        furi_thread_flags_set(furi_thread_get_id(worker->thread), UHF_WORKER_FLAG_STOP);
    }
}

void uhf_worker_start(
    UHFWorker* uhf_worker,
    UHFWorkerState state,
    UHFWorkerCallback callback,
    void* ctx) {
    uhf_worker->state = state;
    uhf_worker->callback = callback;
    uhf_worker->ctx = ctx;
    furi_thread_start(uhf_worker->thread);
}

void uhf_worker_stop(UHFWorker* uhf_worker) {
    furi_assert(uhf_worker);
    furi_assert(uhf_worker->thread);

    FuriThreadState state = furi_thread_get_state(uhf_worker->thread);

    if(state != FuriThreadStateStopped) {
        uhf_worker_change_state(uhf_worker, UHFWorkerStateStop);
        furi_thread_join(uhf_worker->thread);
    }
}

void uhf_worker_free(UHFWorker* uhf_worker) {
    if(!uhf_worker) return;

    if(uhf_worker->thread) furi_thread_free(uhf_worker->thread);
    m100_module_free(uhf_worker->module);
    uhf_tag_free(uhf_worker->NewTag);
    uhf_tag_free(uhf_worker->SelectedTag);
    free(uhf_worker);
}
