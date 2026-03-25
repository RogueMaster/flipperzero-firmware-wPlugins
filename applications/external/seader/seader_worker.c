#include "seader_worker_i.h"
#include "trace_log.h"

#include <flipper_format/flipper_format.h>
#include <lib/bit_lib/bit_lib.h>

#define TAG "SeaderWorker"

#define APDU_HEADER_LEN 5
#define ASN1_PREFIX     6
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

typedef struct {
    volatile bool done;
    volatile bool detected;
} SeaderPicopassDetectContext;

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

static bool __attribute__((unused))
seader_worker_start_read_for_type(Seader* seader, SeaderCredentialType type) {
    NfcPoller* poller_detect = NULL;

    if(type == SeaderCredentialType14A) {
        poller_detect = nfc_poller_alloc(seader->nfc, NfcProtocolIso14443_4a);
        if(!nfc_poller_detect(poller_detect)) {
            nfc_poller_free(poller_detect);
            return false;
        }
        FURI_LOG_I(TAG, "Detected ISO14443-4A card");
        nfc_poller_free(poller_detect);
        seader->poller = nfc_poller_alloc(seader->nfc, NfcProtocolIso14443_4a);
        seader->worker->stage = SeaderPollerEventTypeCardDetect;
        seader->credential->type = SeaderCredentialType14A;
        nfc_poller_start(seader->poller, seader_worker_poller_callback_iso14443_4a, seader);
        return true;
    } else if(type == SeaderCredentialTypeMifareClassic) {
        poller_detect = nfc_poller_alloc(seader->nfc, NfcProtocolMfClassic);
        if(!nfc_poller_detect(poller_detect)) {
            nfc_poller_free(poller_detect);
            return false;
        }
        FURI_LOG_I(TAG, "Detected Mifare Classic card");
        nfc_poller_free(poller_detect);
        seader->poller = nfc_poller_alloc(seader->nfc, NfcProtocolMfClassic);
        seader->worker->stage = SeaderPollerEventTypeCardDetect;
        seader->credential->type = SeaderCredentialTypeMifareClassic;
        nfc_poller_start(seader->poller, seader_worker_poller_callback_mfc, seader);
        return true;
    } else if(type == SeaderCredentialTypePicopass) {
        if(!seader_worker_detect_picopass(seader->nfc)) {
            return false;
        }
        FURI_LOG_I(TAG, "Detected Picopass card");
        seader->picopass_poller = picopass_poller_alloc(seader->nfc);
        seader->worker->stage = SeaderPollerEventTypeCardDetect;
        seader->credential->type = SeaderCredentialTypePicopass;
        picopass_poller_start(
            seader->picopass_poller, seader_worker_poller_callback_picopass, seader);
        return true;
    }

    return false;
}

/***************************** Seader Worker API *******************************/

SeaderWorker* seader_worker_alloc() {
    SeaderWorker* seader_worker = calloc(1, sizeof(SeaderWorker));

    // Worker thread attributes
    seader_worker->thread =
        furi_thread_alloc_ex("SeaderWorker", 8192, seader_worker_task, seader_worker);
    seader_worker->messages = furi_message_queue_alloc(3, sizeof(SeaderAPDU));

    seader_worker->callback = NULL;
    seader_worker->context = NULL;
    seader_worker->storage = furi_record_open(RECORD_STORAGE);

    seader_worker_change_state(seader_worker, SeaderWorkerStateReady);

    return seader_worker;
}

void seader_worker_free(SeaderWorker* seader_worker) {
    furi_assert(seader_worker);

    furi_thread_free(seader_worker->thread);
    furi_message_queue_free(seader_worker->messages);

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
            FURI_LOG_I(
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

        FURI_LOG_I(
            TAG,
            "Enqueue SAM message, %d bytes, stage=%d, sam=%d",
            len,
            seader_worker->stage,
            seader->samCommand);
        seader_trace(
            TAG, "enqueue len=%d stage=%d sam=%d", len, seader_worker->stage, seader->samCommand);
        uint32_t space = furi_message_queue_get_space(seader_worker->messages);
        if(space > 0) {
            SeaderAPDU seaderApdu = {};
            seaderApdu.len = len;
            memcpy(seaderApdu.buf, apdu, len);

            furi_message_queue_put(seader_worker->messages, &seaderApdu, FuriWaitForever);
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

    char* display = malloc(len * 2 + 1);
    memset(display, 0, len * 2 + 1);
    for(size_t i = 0; i < len; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", apdu[i]);
    }
    FURI_LOG_I(TAG, "APDU: %s", display);
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
    free(display);

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
            FURI_LOG_I(TAG, "Dequeue SAM message [%ld messages]", count);

            SeaderAPDU seaderApdu = {};
            FuriStatus status =
                furi_message_queue_get(seader_worker->messages, &seaderApdu, FuriWaitForever);
            if(status != FuriStatusOk) {
                FURI_LOG_W(TAG, "furi_message_queue_get fail %d", status);
                view_dispatcher_send_custom_event(
                    seader->view_dispatcher, SeaderCustomEventWorkerExit);
            }
            if(seader_process_success_response_i(
                   seader, seaderApdu.buf, seaderApdu.len, true, NULL)) {
                // no-op
            } else {
                FURI_LOG_I(TAG, "Response false");
                running = false;
            }
        } else {
            dead_loops--;
            running = (dead_loops > 0);
            FURI_LOG_D(
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
        FURI_LOG_D(TAG, "Final dead loops: %d", dead_loops);
    } else {
        view_dispatcher_send_custom_event(seader->view_dispatcher, SeaderCustomEventWorkerExit);
    }
}

int32_t seader_worker_task(void* context) {
    SeaderWorker* seader_worker = context;
    Seader* seader = seader_worker->context;
    SeaderUartBridge* seader_uart = seader_worker->uart;

    if(seader_worker->state == SeaderWorkerStateCheckSam) {
        FURI_LOG_D(TAG, "Check for SAM");
        seader_ccid_check_for_sam(seader_uart);
    } else if(seader_worker->state == SeaderWorkerStateVirtualCredential) {
        FURI_LOG_D(TAG, "Virtual Credential");
        seader_worker_virtual_credential(seader);
    } else if(seader_worker->state == SeaderWorkerStateAPDURunner) {
        FURI_LOG_D(TAG, "APDU Runner");
        seader_apdu_runner_init(seader);
        return 0;
    } else if(seader_worker->state == SeaderWorkerStateHfTeardown) {
        FURI_LOG_I(TAG, "HF teardown started");
        seader_worker_release_hf_session(seader);
        if(seader_worker->callback) {
            seader_worker->callback(SeaderWorkerEventHfTeardownComplete, seader_worker->context);
        }
    } else if(seader_worker->state == SeaderWorkerStateReading) {
        FURI_LOG_D(TAG, "Reading mode started");
        seader_worker_reading(seader);
    }
    seader_worker_change_state(seader_worker, SeaderWorkerStateReady);

    return 0;
}

void seader_worker_reading(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;
    FURI_LOG_I(TAG, "Reading loop started");

    if(!seader_hf_plugin_acquire(seader) || !seader->plugin_hf || !seader->hf_plugin_ctx) {
        FURI_LOG_E(TAG, "HF plugin unavailable");
        strlcpy(seader->read_error, "HF plugin unavailable", sizeof(seader->read_error));
        if(seader_worker->callback) {
            seader_worker->callback(SeaderWorkerEventFail, seader_worker->context);
        }
        return;
    }

    while(seader_worker->state == SeaderWorkerStateReading) {
        bool detected = false;
        SeaderPollerEventType result_stage = SeaderPollerEventTypeFail;
        SeaderCredentialType type_to_read = seader_hf_mode_get_selected_read_type(seader);
        FURI_LOG_D(TAG, "HF loop selected type=%d stage=%d", type_to_read, seader_worker->stage);

        if(type_to_read == SeaderCredentialTypeNone) {
            SeaderCredentialType detected_types[SEADER_MAX_DETECTED_CARD_TYPES] = {0};
            const size_t detected_type_count = seader->plugin_hf->detect_supported_types(
                seader->hf_plugin_ctx, detected_types, COUNT_OF(detected_types));
            FURI_LOG_I(TAG, "HF plugin detected %u type(s)", detected_type_count);

            if(detected_type_count > 1) {
                seader_hf_mode_set_detected_types(seader, detected_types, detected_type_count);
                if(seader_worker->callback) {
                    seader_worker->callback(
                        SeaderWorkerEventSelectCardType, seader_worker->context);
                }
                break;
            } else if(detected_type_count == 1) {
                type_to_read = detected_types[0];
            }
        }

        if(type_to_read != SeaderCredentialTypeNone) {
            FURI_LOG_I(TAG, "HF start read for type=%d", type_to_read);
            detected = seader->plugin_hf->start_read_for_type(seader->hf_plugin_ctx, type_to_read);
            if(detected) {
                seader->hf_session_state = SeaderHfSessionStateActive;
            }
            FURI_LOG_I(TAG, "HF start read result=%d", detected);
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

    FURI_LOG_I(TAG, "Reading loop stopped");
}

void seader_worker_run_hf_conversation(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;

    furi_thread_set_current_priority(FuriThreadPriorityHighest);

    /* The NFC callback thread stays in this loop while the SAM drives the conversation.
       The worker queue is the bridge between SAM APDUs and the poller callback thread. */
    while(seader_worker->stage == SeaderPollerEventTypeConversation &&
          seader_worker->state == SeaderWorkerStateReading) {
        SeaderAPDU seaderApdu = {};
        // Short wait for SAM message
        FuriStatus status = furi_message_queue_get(seader_worker->messages, &seaderApdu, 100);

        if(status == FuriStatusOk) {
            FURI_LOG_D(TAG, "Dequeue SAM message [%d bytes]", seaderApdu.len);
            if(seader_process_success_response_i(
                   seader, seaderApdu.buf, seaderApdu.len, true, NULL)) {
                // message was processed, loop again to see if SAM has more to say
            } else {
                FURI_LOG_I(TAG, "Response false, ending conversation");
                seader_worker->stage = SeaderPollerEventTypeComplete;
                view_dispatcher_send_custom_event(
                    seader->view_dispatcher, SeaderCustomEventWorkerExit);
            }
        } else if(status == FuriStatusErrorTimeout) {
            // No message yet, keep looping to stay in callback
            // This is "properly idling" while waiting for SAM
        } else {
            FURI_LOG_W(TAG, "furi_message_queue_get fail %d", status);
            seader_worker->stage = SeaderPollerEventTypeFail;
            view_dispatcher_send_custom_event(
                seader->view_dispatcher, SeaderCustomEventWorkerExit);
        }
    }
}

NfcCommand seader_worker_poller_callback_iso14443_4a(NfcGenericEvent event, void* context) {
    if(event.protocol != NfcProtocolIso14443_4a || !event.event_data) {
        FURI_LOG_W(TAG, "Ignore invalid host 14A callback");
        return NfcCommandStop;
    }
    furi_check(context);
    NfcCommand ret = NfcCommandContinue;

    Seader* seader = context;
    SeaderWorker* seader_worker = seader->worker;

    const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;
    if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
        if(seader_worker->stage == SeaderPollerEventTypeCardDetect) {
            FURI_LOG_D(TAG, "14a stage CardDetect -> Conversation");
            seader_trace(TAG, "14a CardDetect->Conversation");
            view_dispatcher_send_custom_event(
                seader->view_dispatcher, SeaderCustomEventPollerDetect);

            if(!seader_sam_can_accept_card(seader)) {
                seader_trace(
                    TAG,
                    "14a defer detect sam_state=%d intent=%d",
                    seader->sam_state,
                    seader->sam_intent);
                return NfcCommandContinue;
            }

            nfc_device_set_data(
                seader->nfc_device, NfcProtocolIso14443_4a, nfc_poller_get_data(seader->poller));

            size_t uid_len;
            const uint8_t* uid = nfc_device_get_uid(seader->nfc_device, &uid_len);

            const Iso14443_4aData* iso14443_4a_data =
                nfc_device_get_data(seader->nfc_device, NfcProtocolIso14443_4a);
            const Iso14443_3aData* iso14443_3a_data = iso14443_4a_get_base_data(iso14443_4a_data);

            uint32_t t1_tk_size = 0;
            if(iso14443_4a_data->ats_data.t1_tk != NULL) {
                t1_tk_size = simple_array_get_count(iso14443_4a_data->ats_data.t1_tk);
                if(t1_tk_size > 0xFF) {
                    t1_tk_size = 0;
                }
            }

            uint8_t ats_len = 0;
            uint8_t* ats = malloc(4 + t1_tk_size);
            if(!ats) {
                FURI_LOG_E(TAG, "Failed to allocate host ATS buffer");
                seader_worker->stage = SeaderPollerEventTypeFail;
                return NfcCommandStop;
            }

            if(iso14443_4a_data->ats_data.tl > 1) {
                ats[ats_len++] = iso14443_4a_data->ats_data.t0;
                if(iso14443_4a_data->ats_data.t0 & ISO14443_4A_ATS_T0_TA1) {
                    ats[ats_len++] = iso14443_4a_data->ats_data.ta_1;
                }
                if(iso14443_4a_data->ats_data.t0 & ISO14443_4A_ATS_T0_TB1) {
                    ats[ats_len++] = iso14443_4a_data->ats_data.tb_1;
                }
                if(iso14443_4a_data->ats_data.t0 & ISO14443_4A_ATS_T0_TC1) {
                    ats[ats_len++] = iso14443_4a_data->ats_data.tc_1;
                }

                if(t1_tk_size != 0) {
                    memcpy(
                        ats + ats_len,
                        simple_array_cget_data(iso14443_4a_data->ats_data.t1_tk),
                        t1_tk_size);
                    ats_len += t1_tk_size;
                }
            }

            uint8_t sak = iso14443_3a_get_sak(iso14443_3a_data);

            seader_worker_card_detect(
                seader, sak, (uint8_t*)iso14443_3a_data->atqa, uid, uid_len, ats, ats_len);
            seader_trace(TAG, "14a card_detect sent uid_len=%d sak=%d", uid_len, sak);

            free(ats);

            if(seader_worker->state == SeaderWorkerStateReading) {
                seader_worker->stage = SeaderPollerEventTypeConversation;
                return NfcCommandContinue;
            }

            // nfc_set_fdt_poll_fc(event.instance, SEADER_POLLER_MAX_FWT);
            furi_thread_set_current_priority(FuriThreadPriorityLowest);
            seader_worker->stage = SeaderPollerEventTypeConversation;
        } else if(seader_worker->stage == SeaderPollerEventTypeConversation) {
            seader_trace(TAG, "14a ready in Conversation");
            seader_worker_run_hf_conversation(seader);
        } else if(seader_worker->stage == SeaderPollerEventTypeComplete) {
            seader_trace(TAG, "14a ready in Complete");
            ret = NfcCommandStop;
        } else if(seader_worker->stage == SeaderPollerEventTypeFail) {
            seader_trace(TAG, "14a ready in Fail");
            ret = NfcCommandStop;
            view_dispatcher_send_custom_event(
                seader->view_dispatcher, SeaderCustomEventWorkerExit);
            FURI_LOG_W(TAG, "SeaderPollerEventTypeFail");
        }
    } else if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeError) {
        Iso14443_4aPollerEventData* data = iso14443_4a_event->data;
        Iso14443_4aError error = data->error;
        FURI_LOG_W(TAG, "Iso14443_4aError %i", error);
        seader_trace(TAG, "14a error=%d stage=%d", error, seader_worker->stage);
        // I was hoping to catch MFC here, but it seems to be treated the same (None) as no card being present.
        switch(error) {
        case Iso14443_4aErrorNone:
            break;
        case Iso14443_4aErrorNotPresent:
            break;
        case Iso14443_4aErrorProtocol:
            ret = NfcCommandStop;
            break;
        case Iso14443_4aErrorTimeout:
            break;
        default:
            break;
        }
    }

    return ret;
}

NfcCommand seader_worker_poller_callback_mfc(NfcGenericEvent event, void* context) {
    if(event.protocol != NfcProtocolMfClassic || !event.event_data) {
        FURI_LOG_W(TAG, "Ignore invalid host MFC callback");
        return NfcCommandStop;
    }
    furi_check(context);
    NfcCommand ret = NfcCommandContinue;

    Seader* seader = context;
    SeaderWorker* seader_worker = seader->worker;

    MfClassicPollerEvent* mfc_event = event.event_data;
    if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        if(seader_worker->stage == SeaderPollerEventTypeCardDetect) {
            FURI_LOG_D(TAG, "MFC stage CardDetect -> Conversation");
            seader_trace(TAG, "mfc CardDetect->Conversation");
            view_dispatcher_send_custom_event(
                seader->view_dispatcher, SeaderCustomEventPollerDetect);

            if(!seader_sam_can_accept_card(seader)) {
                seader_trace(
                    TAG,
                    "mfc defer detect sam_state=%d intent=%d",
                    seader->sam_state,
                    seader->sam_intent);
                return NfcCommandContinue;
            }

            const MfClassicData* mfc_data = nfc_poller_get_data(seader->poller);
            uint8_t sak = iso14443_3a_get_sak(mfc_data->iso14443_3a_data);
            size_t uid_len = 0;
            const uint8_t* uid = mf_classic_get_uid(mfc_data, &uid_len);
            seader_worker_card_detect(seader, sak, NULL, uid, uid_len, NULL, 0);

            if(seader_worker->state == SeaderWorkerStateReading) {
                seader_worker->stage = SeaderPollerEventTypeConversation;
                return NfcCommandContinue;
            }

            furi_thread_set_current_priority(FuriThreadPriorityLowest);
            seader_worker->stage = SeaderPollerEventTypeConversation;
        } else if(seader_worker->stage == SeaderPollerEventTypeConversation) {
            seader_worker_run_hf_conversation(seader);
        } else if(seader_worker->stage == SeaderPollerEventTypeComplete) {
            ret = NfcCommandStop;
        } else if(seader_worker->stage == SeaderPollerEventTypeFail) {
            seader_trace(TAG, "mfc ready in Fail");
            view_dispatcher_send_custom_event(
                seader->view_dispatcher, SeaderCustomEventWorkerExit);
            ret = NfcCommandStop;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        seader_trace(TAG, "mfc poller event fail");
        view_dispatcher_send_custom_event(seader->view_dispatcher, SeaderCustomEventWorkerExit);
        ret = NfcCommandStop;
    }

    return ret;
}

NfcCommand seader_worker_poller_callback_picopass(PicopassPollerEvent event, void* context) {
    furi_check(context);
    NfcCommand ret = NfcCommandContinue;

    Seader* seader = context;
    SeaderWorker* seader_worker = seader->worker;
    // I know this is is passing the same thing that is on seader all the way down, but I prefer the symmetry between the 15a and iso15 stuff
    PicopassPoller* instance = seader->picopass_poller;
    if(event.type == PicopassPollerEventTypeCardDetected) {
        seader_worker->stage = SeaderPollerEventTypeCardDetect;
    } else if(event.type == PicopassPollerEventTypeSuccess) {
        if(seader_worker->stage == SeaderPollerEventTypeCardDetect) {
            FURI_LOG_D(TAG, "Picopass stage CardDetect -> Conversation");
            seader_trace(TAG, "picopass CardDetect->Conversation");
            view_dispatcher_send_custom_event(
                seader->view_dispatcher, SeaderCustomEventPollerDetect);
            if(!seader_sam_can_accept_card(seader)) {
                seader_trace(
                    TAG,
                    "picopass defer detect sam_state=%d intent=%d",
                    seader->sam_state,
                    seader->sam_intent);
                return NfcCommandContinue;
            }
            uint8_t* csn = picopass_poller_get_csn(instance);
            seader_worker_card_detect(seader, 0, NULL, csn, sizeof(PicopassSerialNum), NULL, 0);

            if(seader_worker->state == SeaderWorkerStateReading) {
                seader_worker->stage = SeaderPollerEventTypeConversation;
                return NfcCommandContinue;
            }

            furi_thread_set_current_priority(FuriThreadPriorityLowest);
            seader_worker->stage = SeaderPollerEventTypeConversation;
        } else if(seader_worker->stage == SeaderPollerEventTypeConversation) {
            seader_worker_run_hf_conversation(seader);
        } else if(seader_worker->stage == SeaderPollerEventTypeComplete) {
            ret = NfcCommandStop;
        } else if(seader_worker->stage == SeaderPollerEventTypeFail) {
            view_dispatcher_send_custom_event(
                seader->view_dispatcher, SeaderCustomEventWorkerExit);
            ret = NfcCommandStop;
        }
    } else if(event.type == PicopassPollerEventTypeFail) {
        ret = NfcCommandStop;
        FURI_LOG_W(TAG, "PicopassPollerEventTypeFail");
    } else {
        FURI_LOG_D(TAG, "picopass event type %x", event.type);
    }

    return ret;
}
