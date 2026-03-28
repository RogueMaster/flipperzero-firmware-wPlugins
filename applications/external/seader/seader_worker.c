#include "seader_worker_i.h"
#include "seader_hf_read_plan.h"
#include "hf_read_lifecycle.h"
#include "trace_log.h"

#include <flipper_format/flipper_format.h>
#include <lib/bit_lib/bit_lib.h>

#define TAG "SeaderWorker"

#define APDU_HEADER_LEN                   5
#define ASN1_PREFIX                       6
#define SEADER_HF_CONVERSATION_TIMEOUT_MS 3000U
#define SEADER_WORKER_STACK_SIZE          2048U
// #define ASN1_DEBUG      true

#define RFAL_PICOPASS_TXRX_FLAGS                                                    \
    (FURI_HAL_NFC_LL_TXRX_FLAGS_CRC_TX_MANUAL | FURI_HAL_NFC_LL_TXRX_FLAGS_AGC_ON | \
     FURI_HAL_NFC_LL_TXRX_FLAGS_PAR_RX_REMV | FURI_HAL_NFC_LL_TXRX_FLAGS_CRC_RX_KEEP)

// Forward declaration
void seader_send_card_detected(SeaderUartBridge* seader_uart, CardDetails_t* cardDetails);
void seader_worker_reading(Seader* seader);

static void seader_worker_release_hf_session(Seader* seader) {
    if(!seader) {
        return;
    }

    seader_hf_plugin_release(seader);
}

static void seader_worker_fail_hf_startup(Seader* seader, const char* detail) {
    if(!seader || !seader->worker) {
        return;
    }

    SeaderWorker* seader_worker = seader->worker;
    seader_hf_plugin_release(seader);
    seader->hf_read_state = SeaderHfReadStateTerminalFail;
    seader->hf_read_failure_reason = SeaderHfReadFailureReasonUnavailable;
    seader->hf_read_last_progress_tick = 0U;
    seader->hf_session_state = SeaderHfSessionStateUnloaded;
    if(seader->mode_runtime == SeaderModeRuntimeHF) {
        seader->mode_runtime = SeaderModeRuntimeNone;
    }
    strlcpy(
        seader->read_error,
        detail ? detail : seader_hf_read_failure_reason_text(seader->hf_read_failure_reason),
        sizeof(seader->read_error));
    seader_sam_force_idle_for_recovery(seader);
    seader_worker->stage = SeaderPollerEventTypeFail;
    if(seader_worker->callback) {
        seader_worker->callback(SeaderWorkerEventFail, seader_worker->context);
    }
}

typedef struct {
    volatile bool done;
    volatile bool detected;
} SeaderPicopassDetectContext;

static void seader_worker_reset_apdu_slots(SeaderWorker* seader_worker) {
    furi_assert(seader_worker);
    memset(seader_worker->apdu_slot_in_use, 0, sizeof(seader_worker->apdu_slot_in_use));
    if(seader_worker->apdu_slots) {
        memset(
            seader_worker->apdu_slots,
            0,
            sizeof(*seader_worker->apdu_slots) * SEADER_WORKER_APDU_SLOT_COUNT);
    }
}

static bool seader_worker_claim_apdu_slot(SeaderWorker* seader_worker, uint8_t* slot_index) {
    furi_assert(seader_worker);
    furi_assert(slot_index);

    for(uint8_t i = 0; i < SEADER_WORKER_APDU_SLOT_COUNT; i++) {
        if(!seader_worker->apdu_slot_in_use[i]) {
            seader_worker->apdu_slot_in_use[i] = true;
            *slot_index = i;
            return true;
        }
    }

    return false;
}

static void seader_worker_release_apdu_slot(SeaderWorker* seader_worker, uint8_t slot_index) {
    furi_assert(seader_worker);
    furi_assert(slot_index < SEADER_WORKER_APDU_SLOT_COUNT);

    seader_worker->apdu_slot_in_use[slot_index] = false;
    if(seader_worker->apdu_slots) {
        seader_worker->apdu_slots[slot_index].len = 0U;
    }
}

static bool
    seader_worker_dequeue_apdu(SeaderWorker* seader_worker, uint8_t* slot_index, FuriWait timeout) {
    furi_assert(seader_worker);
    furi_assert(slot_index);
    return furi_message_queue_get(seader_worker->messages, slot_index, timeout) == FuriStatusOk;
}

static void seader_worker_clear_active_card(Seader* seader, const char* reason) {
    if(!seader) {
        return;
    }

    if(seader_sam_has_active_card(seader)) {
        FURI_LOG_I(TAG, "Clear active SAM card (%s)", reason ? reason : "worker");
        seader_send_no_card_detected(seader);
    }
}

static NfcCommand
    seader_worker_picopass_detect_callback(PicopassPollerEvent event, void* context) {
    SeaderPicopassDetectContext* detect_context = context;

    if(event.type == PicopassPollerEventTypeCardDetected ||
       event.type == PicopassPollerEventTypeSuccess) {
        detect_context->detected = true;
        detect_context->done = true;
        return NfcCommandStop;
    } else if(event.type == PicopassPollerEventTypeFail) {
        detect_context->done = true;
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static bool seader_worker_detect_picopass(Nfc* nfc) {
    bool detected = false;
    PicopassPoller* poller = picopass_poller_alloc(nfc);
    SeaderPicopassDetectContext detect_context = {0};

    if(!poller) {
        FURI_LOG_W(TAG, "Failed to allocate Picopass detect poller");
        return false;
    }

    picopass_poller_start(poller, seader_worker_picopass_detect_callback, &detect_context);

    for(uint8_t i = 0; i < 10 && !detect_context.done; i++) {
        furi_delay_ms(10);
    }

    picopass_poller_stop(poller);
    detected = detect_context.detected;
    picopass_poller_free(poller);

    return detected;
}

static void seader_worker_add_detected_type(
    SeaderCredentialType* detected_types,
    size_t* detected_type_count,
    SeaderCredentialType type) {
    for(size_t i = 0; i < *detected_type_count; i++) {
        if(detected_types[i] == type) {
            return;
        }
    }

    if(*detected_type_count < SEADER_MAX_DETECTED_CARD_TYPES) {
        detected_types[*detected_type_count] = type;
        (*detected_type_count)++;
    }
}

static size_t __attribute__((unused)) seader_worker_detect_supported_types(
    Seader* seader,
    SeaderCredentialType* detected_types,
    size_t detected_capacity) {
    UNUSED(detected_capacity);
    size_t detected_type_count = 0;
    NfcPoller* poller_detect = nfc_poller_alloc(seader->nfc, NfcProtocolIso14443_4a);
    if(nfc_poller_detect(poller_detect)) {
        seader_worker_add_detected_type(
            detected_types, &detected_type_count, SeaderCredentialType14A);
    }
    nfc_poller_free(poller_detect);

    poller_detect = nfc_poller_alloc(seader->nfc, NfcProtocolMfClassic);
    if(nfc_poller_detect(poller_detect)) {
        seader_worker_add_detected_type(
            detected_types, &detected_type_count, SeaderCredentialTypeMifareClassic);
    }
    nfc_poller_free(poller_detect);

    if(seader_worker_detect_picopass(seader->nfc)) {
        seader_worker_add_detected_type(
            detected_types, &detected_type_count, SeaderCredentialTypePicopass);
    }

    return detected_type_count;
}

/***************************** Seader Worker API *******************************/

SeaderWorker* seader_worker_alloc() {
    SeaderWorker* seader_worker = calloc(1, sizeof(SeaderWorker));
    if(!seader_worker) {
        return NULL;
    }

    // Worker thread attributes
    seader_worker->thread = furi_thread_alloc_ex(
        "SeaderWorker", SEADER_WORKER_STACK_SIZE, seader_worker_task, seader_worker);
    seader_worker->messages = furi_message_queue_alloc(2, sizeof(uint8_t));
    seader_worker->apdu_slots = calloc(SEADER_WORKER_APDU_SLOT_COUNT, sizeof(SeaderAPDU));

    if(!seader_worker->thread || !seader_worker->messages || !seader_worker->apdu_slots) {
        if(seader_worker->thread) {
            furi_thread_free(seader_worker->thread);
        }
        if(seader_worker->messages) {
            furi_message_queue_free(seader_worker->messages);
        }
        free(seader_worker->apdu_slots);
        free(seader_worker);
        return NULL;
    }

    seader_worker->callback = NULL;
    seader_worker->context = NULL;
    seader_worker->storage = furi_record_open(RECORD_STORAGE);
    seader_worker_reset_apdu_slots(seader_worker);

    seader_worker_change_state(seader_worker, SeaderWorkerStateReady);

    return seader_worker;
}

void seader_worker_free(SeaderWorker* seader_worker) {
    furi_assert(seader_worker);

    furi_thread_free(seader_worker->thread);
    furi_message_queue_free(seader_worker->messages);
    free(seader_worker->apdu_slots);

    furi_record_close(RECORD_STORAGE);

    free(seader_worker);
}

SeaderWorkerState seader_worker_get_state(SeaderWorker* seader_worker) {
    return seader_worker->state;
}

void seader_worker_start(
    SeaderWorker* seader_worker,
    SeaderWorkerState state,
    SeaderUartBridge* uart,
    SeaderWorkerCallback callback,
    void* context) {
    furi_assert(seader_worker);
    furi_assert(uart);

    if(furi_thread_get_state(seader_worker->thread) != FuriThreadStateStopped) {
        seader_worker_stop(seader_worker);
    }

    /* Worker startup owns queue/stage reset. Scene code must not pre-reset the live
       poller session because the worker is the runtime owner for those objects. */
    seader_worker_reset_poller_session(seader_worker);
    seader_worker->callback = callback;
    seader_worker->context = context;
    seader_worker->uart = uart;
    seader_worker->state = state;
    furi_thread_start(seader_worker->thread);
}

void seader_worker_stop(SeaderWorker* seader_worker) {
    furi_assert(seader_worker);
    if(furi_thread_get_state(seader_worker->thread) == FuriThreadStateStopped) {
        return;
    }

    seader_worker->state = SeaderWorkerStateStop;
    furi_thread_join(seader_worker->thread);
}

void seader_worker_join(SeaderWorker* seader_worker) {
    furi_assert(seader_worker);
    if(furi_thread_get_state(seader_worker->thread) == FuriThreadStateStopped) {
        return;
    }

    furi_thread_join(seader_worker->thread);
}

void seader_worker_change_state(SeaderWorker* seader_worker, SeaderWorkerState state) {
    seader_worker->state = state;
}

void seader_worker_cancel_poller_session(SeaderWorker* seader_worker) {
    furi_assert(seader_worker);
    FURI_LOG_D(
        TAG,
        "Cancel poller session stage=%d queued=%ld",
        seader_worker->stage,
        furi_message_queue_get_count(seader_worker->messages));
    seader_trace(
        TAG,
        "cancel stage=%d queued=%ld",
        seader_worker->stage,
        furi_message_queue_get_count(seader_worker->messages));
    seader_worker->stage = SeaderPollerEventTypeComplete;
}

void seader_worker_reset_poller_session(SeaderWorker* seader_worker) {
    furi_assert(seader_worker);
    FURI_LOG_D(
        TAG,
        "Reset poller session stage=%d queued=%ld",
        seader_worker->stage,
        furi_message_queue_get_count(seader_worker->messages));
    seader_trace(
        TAG,
        "reset stage=%d queued=%ld",
        seader_worker->stage,
        furi_message_queue_get_count(seader_worker->messages));

    furi_message_queue_reset(seader_worker->messages);
    seader_worker_reset_apdu_slots(seader_worker);
    seader_worker->stage = SeaderPollerEventTypeCardDetect;
}

/***************************** Seader Worker Thread *******************************/

bool seader_process_success_response(Seader* seader, uint8_t* apdu, size_t len) {
    SeaderWorker* seader_worker = seader->worker;

    if(seader_process_success_response_i(seader, apdu, len, false, NULL)) {
        // no-op, message was processed
    } else {
        /* Outside an active conversation, an unhandled SAM message is stale noise from a
           previous flow. Enqueueing it would let old maintenance/read traffic bleed forward. */
        if(seader_worker->state != SeaderWorkerStateVirtualCredential &&
           seader_worker->stage != SeaderPollerEventTypeConversation) {
            SEADER_VERBOSE_I(
                TAG,
                "Discard stale SAM message outside active conversation, %d bytes, stage=%d, sam=%d",
                len,
                seader_worker->stage,
                seader->samCommand);
            seader_trace(
                TAG,
                "discard len=%d stage=%d sam=%d state=%d intent=%d",
                len,
                seader_worker->stage,
                seader->samCommand,
                seader->sam_state,
                seader->sam_intent);
            return true;
        }

        SEADER_VERBOSE_I(
            TAG,
            "Enqueue SAM message, %d bytes, stage=%d, sam=%d",
            len,
            seader_worker->stage,
            seader->samCommand);
        seader_trace(
            TAG, "enqueue len=%d stage=%d sam=%d", len, seader_worker->stage, seader->samCommand);
        uint32_t space = furi_message_queue_get_space(seader_worker->messages);
        if(space > 0 && len <= SEADER_POLLER_MAX_BUFFER_SIZE) {
            uint8_t slot_index = 0U;
            if(!seader_worker_claim_apdu_slot(seader_worker, &slot_index)) {
                FURI_LOG_W(TAG, "No free APDU slot for len=%u", (unsigned)len);
                return true;
            }

            seader_worker->apdu_slots[slot_index].len = len;
            memcpy(seader_worker->apdu_slots[slot_index].buf, apdu, len);

            if(furi_message_queue_put(seader_worker->messages, &slot_index, FuriWaitForever) !=
               FuriStatusOk) {
                FURI_LOG_W(TAG, "Failed to queue APDU slot=%u", slot_index);
                seader_worker_release_apdu_slot(seader_worker, slot_index);
            }
        } else if(len > SEADER_POLLER_MAX_BUFFER_SIZE) {
            FURI_LOG_W(TAG, "Drop oversized SAM message len=%u", (unsigned)len);
        }
    }
    return true;
}

bool seader_worker_process_sam_message(Seader* seader, uint8_t* apdu, uint32_t len) {
    furi_check(seader);
    SeaderWorker* seader_worker = seader->worker;
    furi_check(seader_worker);
    SeaderUartBridge* seader_uart = seader_worker->uart;
    furi_check(seader_uart);
    if(len < 2) {
        return false;
    }

    if(seader_worker->state == SeaderWorkerStateAPDURunner) {
        return seader_apdu_runner_response(seader, apdu, len);
    }

    SEADER_VERBOSE_HEX(FuriLogLevelInfo, TAG, "APDU", apdu, len);
    seader_trace(
        TAG,
        "sam apdu len=%lu stage=%d sam=%d state=%d intent=%d sw=%02x%02x",
        len,
        seader_worker->stage,
        seader->samCommand,
        seader->sam_state,
        seader->sam_intent,
        apdu[len - 2],
        apdu[len - 1]);

    uint8_t SW1 = apdu[len - 2];
    uint8_t SW2 = apdu[len - 1];
    uint8_t GET_RESPONSE[] = {0x00, 0xc0, 0x00, 0x00, 0xff};

    switch(SW1) {
    case 0x61:
        // FURI_LOG_I(TAG, "Request %d bytes", SW2);
        GET_RESPONSE[4] = SW2;
        seader_ccid_XfrBlock(seader_uart, GET_RESPONSE, sizeof(GET_RESPONSE));
        return true;
        break;
    case 0x90:
        if(SW2 == 0x00) {
            if(len > 2) {
                return seader_process_success_response(seader, apdu, len - 2);
            }
        }
        break;
    default:
        FURI_LOG_W(TAG, "Unknown SW %02x%02x", SW1, SW2);
        break;
    }

    return false;
}

void seader_worker_virtual_credential(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;

    // Detect card
    seader_worker_card_detect(
        seader, 0, NULL, seader->credential->diversifier, sizeof(PicopassSerialNum), NULL, 0);

    bool running = true;
    // Max times the loop will run with no message to process
    uint8_t dead_loops = 20;

    while(running) {
        uint32_t count = furi_message_queue_get_count(seader_worker->messages);
        if(count > 0) {
            SEADER_VERBOSE_I(TAG, "Dequeue SAM message [%ld messages]", count);

            uint8_t slot_index = 0U;
            if(!seader_worker_dequeue_apdu(seader_worker, &slot_index, FuriWaitForever)) {
                FURI_LOG_W(TAG, "furi_message_queue_get fail");
                view_dispatcher_send_custom_event(
                    seader->view_dispatcher, SeaderCustomEventWorkerExit);
                continue;
            }
            furi_assert(slot_index < SEADER_WORKER_APDU_SLOT_COUNT);
            SeaderAPDU* seaderApdu = &seader_worker->apdu_slots[slot_index];
            if(seader_process_success_response_i(
                   seader, seaderApdu->buf, seaderApdu->len, true, NULL)) {
                // no-op
            } else {
                SEADER_VERBOSE_I(TAG, "Response false");
                running = false;
            }
            seader_worker_release_apdu_slot(seader_worker, slot_index);
        } else {
            dead_loops--;
            running = (dead_loops > 0);
            SEADER_VERBOSE_D(
                TAG, "Dead loops: %d -> Running: %s", dead_loops, running ? "true" : "false");
            if(running) furi_delay_ms(10); // Don't tight loop if empty
        }
        running = (seader_worker->stage != SeaderPollerEventTypeComplete);
    }

    if(dead_loops > 0 && seader_worker->stage == SeaderPollerEventTypeComplete) {
        if(seader_worker->callback) {
            seader_worker->callback(SeaderWorkerEventSuccess, seader_worker->context);
        }
    } else if(dead_loops > 0) {
        SEADER_VERBOSE_D(TAG, "Final dead loops: %d", dead_loops);
    } else {
        view_dispatcher_send_custom_event(seader->view_dispatcher, SeaderCustomEventWorkerExit);
    }
}

int32_t seader_worker_task(void* context) {
    SeaderWorker* seader_worker = context;
    Seader* seader = seader_worker->context;
    SeaderUartBridge* seader_uart = seader_worker->uart;

    if(seader_worker->state == SeaderWorkerStateCheckSam) {
        SEADER_VERBOSE_D(TAG, "Check for SAM");
        seader_ccid_check_for_sam(seader_uart);
    } else if(seader_worker->state == SeaderWorkerStateVirtualCredential) {
        SEADER_VERBOSE_D(TAG, "Virtual Credential");
        seader_worker_virtual_credential(seader);
    } else if(seader_worker->state == SeaderWorkerStateAPDURunner) {
        SEADER_VERBOSE_D(TAG, "APDU Runner");
        seader_apdu_runner_init(seader);
        return 0;
    } else if(seader_worker->state == SeaderWorkerStateHfTeardown) {
        SEADER_VERBOSE_I(TAG, "HF teardown started");
        seader_worker_release_hf_session(seader);
        if(seader_worker->callback) {
            seader_worker->callback(SeaderWorkerEventHfTeardownComplete, seader_worker->context);
        }
    } else if(seader_worker->state == SeaderWorkerStateReading) {
        SEADER_VERBOSE_D(TAG, "Reading mode started");
        seader_worker_reading(seader);
    }
    seader_worker_change_state(seader_worker, SeaderWorkerStateReady);

    return 0;
}

void seader_worker_reading(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;
    SEADER_VERBOSE_I(TAG, "Reading loop started");

    if(!seader_hf_plugin_acquire(seader) || !seader->plugin_hf || !seader->hf_plugin_ctx) {
        FURI_LOG_E(
            TAG,
            "HF plugin unavailable acquire=%d plugin=%p ctx=%p",
            seader->plugin_hf != NULL && seader->hf_plugin_ctx != NULL,
            (void*)seader->plugin_hf,
            seader->hf_plugin_ctx);
        seader_worker_fail_hf_startup(seader, "HF unavailable");
        return;
    }

    while(seader_worker->state == SeaderWorkerStateReading) {
        bool detected = false;
        SeaderPollerEventType result_stage = SeaderPollerEventTypeFail;
        SeaderCredentialType type_to_read = seader_hf_mode_get_selected_read_type(seader);
        SeaderHfReadPlan read_plan = {0};
        if(!seader_sam_can_accept_card(seader) || seader->hf_read_state != SeaderHfReadStateIdle) {
            FURI_LOG_W(
                TAG,
                "Recover stale HF read state=%d sam=%d intent=%d",
                seader->hf_read_state,
                seader->sam_state,
                seader->sam_intent);
            seader_sam_force_idle_for_recovery(seader);
            seader->hf_read_state = SeaderHfReadStateIdle;
            seader->hf_read_failure_reason = SeaderHfReadFailureReasonNone;
            seader->hf_read_last_progress_tick = 0U;
        }
        SEADER_VERBOSE_D(
            TAG, "HF loop selected type=%d stage=%d", type_to_read, seader_worker->stage);

        if(type_to_read == SeaderCredentialTypeNone) {
            SeaderCredentialType detected_types[SEADER_MAX_DETECTED_CARD_TYPES] = {0};
            const size_t detected_type_count = seader->plugin_hf->detect_supported_types(
                seader->hf_plugin_ctx, detected_types, COUNT_OF(detected_types));
            SEADER_VERBOSE_I(TAG, "HF plugin detected %u type(s)", detected_type_count);
            read_plan =
                seader_hf_read_plan_build(type_to_read, detected_types, detected_type_count);
        } else {
            read_plan = seader_hf_read_plan_build(type_to_read, NULL, 0U);
        }

        if(read_plan.decision == SeaderHfReadDecisionSelectType) {
            seader_hf_mode_set_detected_types(
                seader, read_plan.detected_types, read_plan.detected_type_count);
            if(seader_worker->callback) {
                seader_worker->callback(SeaderWorkerEventSelectCardType, seader_worker->context);
            }
            break;
        } else if(read_plan.decision == SeaderHfReadDecisionStartRead) {
            SEADER_VERBOSE_I(TAG, "HF start read for type=%d", read_plan.type_to_read);
            seader->hf_read_state = SeaderHfReadStateDetecting;
            seader->hf_read_failure_reason = SeaderHfReadFailureReasonNone;
            seader->hf_read_last_progress_tick = furi_get_tick();
            detected = seader->plugin_hf->start_read_for_type(
                seader->hf_plugin_ctx, read_plan.type_to_read);
            if(detected) {
                seader->hf_session_state = SeaderHfSessionStateActive;
            }
            SEADER_VERBOSE_I(TAG, "HF start read result=%d", detected);
        }

        if(detected) {
            // Wait for conversation to finish
            while(seader_worker->stage != SeaderPollerEventTypeComplete &&
                  seader_worker->stage != SeaderPollerEventTypeFail &&
                  seader_worker->state == SeaderWorkerStateReading) {
                // The conversation is handled by the poller callback thread.
                // We just wait here for it to finish.
                furi_delay_ms(10);
            }
            result_stage = seader_worker->stage;
            /* SAM active-card state belongs to the read lifecycle, not to the success scene.
               Clear it as soon as the poller conversation reaches a terminal stage. */
            seader_worker_clear_active_card(
                seader,
                result_stage == SeaderPollerEventTypeComplete ? "read-complete" : "read-abort");

            if(result_stage == SeaderPollerEventTypeComplete) {
                // Notify UI of success
                if(seader_worker->callback) {
                    seader_worker->callback(SeaderWorkerEventSuccess, seader_worker->context);
                }
                break;
            }
        }

        if(seader_worker->state == SeaderWorkerStateReading) {
            furi_delay_ms(50);
        }
    }

    SEADER_VERBOSE_I(TAG, "Reading loop stopped");
}

void seader_worker_run_hf_conversation(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;

    furi_thread_set_current_priority(FuriThreadPriorityHighest);

    /* The NFC callback thread stays in this loop while the SAM drives the conversation.
       The worker queue is the bridge between SAM APDUs and the poller callback thread. */
    while(seader_worker->stage == SeaderPollerEventTypeConversation &&
          seader_worker->state == SeaderWorkerStateReading) {
        uint8_t slot_index = 0U;
        // Short wait for SAM message
        FuriStatus status = furi_message_queue_get(seader_worker->messages, &slot_index, 100);

        if(status == FuriStatusOk) {
            seader->hf_read_state = SeaderHfReadStateConversationActive;
            seader->hf_read_last_progress_tick = furi_get_tick();
            furi_assert(slot_index < SEADER_WORKER_APDU_SLOT_COUNT);
            SeaderAPDU* seaderApdu = &seader_worker->apdu_slots[slot_index];
            SEADER_VERBOSE_D(TAG, "Dequeue SAM message [%d bytes]", seaderApdu->len);
            if(seader_process_success_response_i(
                   seader, seaderApdu->buf, seaderApdu->len, true, NULL)) {
                // message was processed, loop again to see if SAM has more to say
            } else {
                SEADER_VERBOSE_I(TAG, "Response false, ending conversation");
                seader_worker->stage = SeaderPollerEventTypeComplete;
                view_dispatcher_send_custom_event(
                    seader->view_dispatcher, SeaderCustomEventWorkerExit);
            }
            seader_worker_release_apdu_slot(seader_worker, slot_index);
        } else if(status == FuriStatusErrorTimeout) {
            const uint32_t elapsed = furi_get_tick() - seader->hf_read_last_progress_tick;
            if(seader_hf_read_should_timeout(
                   seader->hf_read_state, elapsed, SEADER_HF_CONVERSATION_TIMEOUT_MS)) {
                FURI_LOG_W(TAG, "HF conversation timeout after %lu ms", elapsed);
                seader->hf_read_state = SeaderHfReadStateTerminalFail;
                seader->hf_read_failure_reason = SeaderHfReadFailureReasonSamTimeout;
                strlcpy(
                    seader->read_error,
                    seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonSamTimeout),
                    sizeof(seader->read_error));
                seader_sam_force_idle_for_recovery(seader);
                seader_worker->stage = SeaderPollerEventTypeFail;
                view_dispatcher_send_custom_event(
                    seader->view_dispatcher, SeaderCustomEventWorkerExit);
            }
        } else {
            FURI_LOG_W(TAG, "furi_message_queue_get fail %d", status);
            seader->hf_read_state = SeaderHfReadStateTerminalFail;
            seader->hf_read_failure_reason = SeaderHfReadFailureReasonProtocolError;
            strlcpy(
                seader->read_error,
                seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonProtocolError),
                sizeof(seader->read_error));
            seader_sam_force_idle_for_recovery(seader);
            seader_worker->stage = SeaderPollerEventTypeFail;
            view_dispatcher_send_custom_event(
                seader->view_dispatcher, SeaderCustomEventWorkerExit);
        }
    }
}
