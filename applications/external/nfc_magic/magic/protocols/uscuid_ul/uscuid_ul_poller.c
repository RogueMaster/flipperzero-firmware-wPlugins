#include "uscuid_ul_poller_i.h"

#include <nfc/nfc_poller.h>
#include <nfc/protocols/nfc_generic_event.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <furi/furi.h>

#define TAG                               "USCUID_UL_POLLER"
#define USCUID_UL_POLLER_THREAD_FLAG_DONE (1U << 0)

typedef NfcCommand (*UscuidUlPollerStateHandler)(UscuidUlPoller* instance);

// Backdoor wakeups, probed in order; whichever ACKs is recorded.
static const UscuidUlWakeup uscuid_ul_wakeup_variants[] = {UscuidUlWakeupA, UscuidUlWakeupB};

static void uscuid_ul_poller_config_nfc(Nfc* nfc) {
    nfc_config(nfc, NfcModePoller, NfcTechIso14443a);
    nfc_set_guard_time_us(nfc, ISO14443_3A_GUARD_TIME_US);
    nfc_set_fdt_poll_fc(nfc, ISO14443_3A_FDT_POLL_FC);
    nfc_set_fdt_poll_poll_us(nfc, ISO14443_3A_POLL_POLL_MIN_US);
}

UscuidUlPoller* uscuid_ul_poller_alloc(Nfc* nfc) {
    furi_assert(nfc);

    UscuidUlPoller* instance = malloc(sizeof(UscuidUlPoller));
    instance->nfc = nfc;
    // Don't nfc_config() here: the direct engine's iso3 poller self-configures, and a second
    // nfc_config() asserts (config_state==Idle). Raw-path callers configure explicitly.

    instance->tx_buffer = bit_buffer_alloc(USCUID_UL_POLLER_MAX_BUFFER_SIZE);
    instance->rx_buffer = bit_buffer_alloc(USCUID_UL_POLLER_MAX_BUFFER_SIZE);

    instance->event.data = &instance->event_data;
    instance->state = UscuidUlPollerStateIdle;
    instance->session_state = UscuidUlPollerSessionStateIdle;
    instance->wakeup = UscuidUlWakeupNone;
    instance->mode = UscuidUlPollerModeWrite;
    instance->iso3_nfc_poller = NULL;
    instance->iso3_poller = NULL;
    instance->password_set = false;
    instance->authed = false;
    instance->auth_ever_ok = false;

    return instance;
}

void uscuid_ul_poller_set_wakeup(UscuidUlPoller* instance, UscuidUlWakeup wakeup) {
    furi_assert(instance);
    // Must be configured before start: it selects the engine start() then launches.
    furi_assert(instance->session_state == UscuidUlPollerSessionStateIdle);
    // Selects the write engine: None -> direct (normal activation + A2), A/B -> backdoor.
    instance->wakeup = wakeup;
}

void uscuid_ul_poller_set_password(UscuidUlPoller* instance, const uint8_t* password) {
    furi_assert(instance);
    furi_assert(password);
    furi_assert(instance->session_state == UscuidUlPollerSessionStateIdle);
    memcpy(instance->password, password, USCUID_UL_PWD_SIZE);
    instance->password_set = true;
}

void uscuid_ul_poller_free(UscuidUlPoller* instance) {
    furi_assert(instance);

    bit_buffer_free(instance->tx_buffer);
    bit_buffer_free(instance->rx_buffer);
    free(instance);
}

// --- Detection route 1: config exposed as the RATS/ATS response (85-mode, no backdoor) ---

#define USCUID_UL_RATS_CMD   (0xE0)
#define USCUID_UL_RATS_PARAM (0x80) // FSDI=8 (256-byte frame), CID=0
#define USCUID_UL_RATS_FWT   (150000U)

typedef struct {
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;
    FuriThreadId thread_id;
    UscuidUlData* result;
} UscuidUlAtsDetectContext;

static NfcCommand uscuid_ul_poller_ats_detect_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.instance);

    UscuidUlAtsDetectContext* ctx = context;
    Iso14443_3aPoller* iso3_poller = event.instance;
    Iso14443_3aPollerEvent* iso3_event = event.event_data;

    if(iso3_event->type == Iso14443_3aPollerEventTypeReady) {
        bit_buffer_reset(ctx->tx_buffer);
        bit_buffer_append_byte(ctx->tx_buffer, USCUID_UL_RATS_CMD);
        bit_buffer_append_byte(ctx->tx_buffer, USCUID_UL_RATS_PARAM);
        Iso14443_3aError error = iso14443_3a_poller_send_standard_frame(
            iso3_poller, ctx->tx_buffer, ctx->rx_buffer, USCUID_UL_RATS_FWT);

        const size_t rx_len = bit_buffer_get_size_bytes(ctx->rx_buffer);
        if((error == Iso14443_3aErrorNone || error == Iso14443_3aErrorWrongCrc) &&
           rx_len >= USCUID_UL_CONFIG_SIZE) {
            const uint8_t* ats = bit_buffer_get_data(ctx->rx_buffer);
            if(uscuid_ul_config_is_magic(ats)) {
                ctx->result->is_uscuid_ul = true;
                ctx->result->wakeup = UscuidUlWakeupNone;
                memcpy(ctx->result->config, ats, USCUID_UL_CONFIG_SIZE);
                uscuid_ul_classify(ats, ctx->result);
            }
        }
    }

    furi_thread_flags_set(ctx->thread_id, USCUID_UL_POLLER_THREAD_FLAG_DONE);
    return NfcCommandStop;
}

static void uscuid_ul_poller_detect_via_ats(Nfc* nfc, UscuidUlData* data) {
    UscuidUlAtsDetectContext ctx = {
        .tx_buffer = bit_buffer_alloc(USCUID_UL_POLLER_MAX_BUFFER_SIZE),
        .rx_buffer = bit_buffer_alloc(USCUID_UL_POLLER_MAX_BUFFER_SIZE),
        .thread_id = furi_thread_get_current_id(),
        .result = data,
    };

    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(poller, uscuid_ul_poller_ats_detect_callback, &ctx);
    furi_thread_flags_wait(USCUID_UL_POLLER_THREAD_FLAG_DONE, FuriFlagWaitAny, FuriWaitForever);
    furi_thread_flags_clear(USCUID_UL_POLLER_THREAD_FLAG_DONE);
    nfc_poller_stop(poller);
    nfc_poller_free(poller);

    bit_buffer_free(ctx.tx_buffer);
    bit_buffer_free(ctx.rx_buffer);
}

// --- Detection route 2: gen1a-style backdoor wakeup + E050 (7AFF-mode) ---

typedef struct {
    UscuidUlPoller* poller;
    FuriThreadId thread_id;
    UscuidUlData* result;
} UscuidUlDetectContext;

static NfcCommand uscuid_ul_poller_detect_callback(NfcEvent event, void* context) {
    furi_assert(context);
    UscuidUlDetectContext* ctx = context;

    if(event.type == NfcEventTypePollerReady) {
        for(size_t i = 0; i < COUNT_OF(uscuid_ul_wakeup_variants); i++) {
            const UscuidUlWakeup variant = uscuid_ul_wakeup_variants[i];
            if(uscuid_ul_poller_wakeup(ctx->poller, variant) != UscuidUlPollerErrorNone) {
                continue;
            }
            uint8_t config[USCUID_UL_CONFIG_SIZE];
            if(uscuid_ul_poller_read_config(ctx->poller, config) != UscuidUlPollerErrorNone) {
                continue;
            }
            if(!uscuid_ul_config_is_magic(config)) {
                continue;
            }
            ctx->result->is_uscuid_ul = true;
            ctx->result->wakeup = variant;
            memcpy(ctx->result->config, config, USCUID_UL_CONFIG_SIZE);
            uscuid_ul_classify(config, ctx->result);
            break;
        }
    }

    furi_thread_flags_set(ctx->thread_id, USCUID_UL_POLLER_THREAD_FLAG_DONE);
    return NfcCommandStop;
}

UscuidUlPollerError uscuid_ul_poller_detect(Nfc* nfc, UscuidUlData* data) {
    furi_assert(nfc);
    furi_assert(data);

    memset(data, 0, sizeof(UscuidUlData));
    data->wakeup = UscuidUlWakeupNone;
    data->type = MfUltralightTypeOrigin;
    // maybe_ul5 is set by the family-first scanner from the activation UID prefix (AA 55):
    // a UL-5's config is locked, so it cannot be read here.

    // Route 1: config exposed as ATS (85-mode). Non-destructive RATS read.
    uscuid_ul_poller_detect_via_ats(nfc, data);

    // Route 2: gen1a-style backdoor wakeup + E050 (7AFF-mode).
    if(!data->is_uscuid_ul) {
        UscuidUlDetectContext ctx = {
            .poller = uscuid_ul_poller_alloc(nfc),
            .thread_id = furi_thread_get_current_id(),
            .result = data,
        };

        // Raw backdoor session: configure the field (alloc no longer does).
        uscuid_ul_poller_config_nfc(nfc);
        nfc_start(nfc, uscuid_ul_poller_detect_callback, &ctx);
        furi_thread_flags_wait(
            USCUID_UL_POLLER_THREAD_FLAG_DONE, FuriFlagWaitAny, FuriWaitForever);
        furi_thread_flags_clear(USCUID_UL_POLLER_THREAD_FLAG_DONE);
        nfc_stop(nfc);

        uscuid_ul_poller_free(ctx.poller);
    }

    return data->is_uscuid_ul ? UscuidUlPollerErrorNone : UscuidUlPollerErrorNotPresent;
}

bool uscuid_ul_data_is_writable(const UscuidUlData* data) {
    furi_assert(data);
    // Detection states are mutually exclusive (a UL-5 is found only when the USCUID probe failed).
    furi_assert(!(data->maybe_ul5 && data->is_uscuid_ul));

    // A confirmed USCUID-UL of a recognised, supported type (UL-C is display-only), or a
    // suspected UL-5 (cloned via the inverse UID write order; target type comes from the dump).
    return data->maybe_ul5 ||
           (data->is_uscuid_ul && data->type_known && (data->type != MfUltralightTypeMfulC));
}

const char* uscuid_ul_type_name(MfUltralightType type) {
    switch(type) {
    case MfUltralightTypeUL11:
        return "UL11";
    case MfUltralightTypeUL21:
        return "UL21";
    case MfUltralightTypeNTAG213:
        return "NTAG213";
    case MfUltralightTypeNTAG215:
        return "NTAG215";
    case MfUltralightTypeNTAG216:
        return "NTAG216";
    case MfUltralightTypeMfulC:
        return "UL-C";
    default:
        return "Unknown";
    }
}

const char* uscuid_ul_get_variant_name(const UscuidUlData* data) {
    furi_assert(data);
    furi_assert(!(data->maybe_ul5 && data->is_uscuid_ul));

    if(data->maybe_ul5) {
        return "UL-5 (probably)";
    }
    if(!data->is_uscuid_ul || !data->type_known) {
        return "Unknown";
    }
    if(data->type == MfUltralightTypeUL21 && data->is_ultra) {
        return "UL21 (Ultra)";
    }
    if(data->type == MfUltralightTypeMfulC) {
        return "UL-C (write N/A)";
    }
    return uscuid_ul_type_name(data->type);
}

// --- Write state machine (raw model, like gen1a_poller) ---

static NfcCommand uscuid_ul_poller_idle_handler(UscuidUlPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    // Direct engine: the iso3 poller already activated the card, nothing to wake -> ready now.
    // Backdoor engine: ready only once the magic wakeup ACKs (short-circuit keeps the direct engine
    // from sending a wakeup). If it didn't answer, stay idle and retry on the next poll.
    const bool ready =
        uscuid_ul_is_direct(instance) ||
        (uscuid_ul_poller_wakeup(instance, instance->wakeup) == UscuidUlPollerErrorNone);
    if(ready) {
        instance->event.type = UscuidUlPollerEventTypeDetected;
        command = instance->callback(instance->event, instance->context);
        instance->state = UscuidUlPollerStateRequestMode;
    }

    return command;
}

static NfcCommand uscuid_ul_poller_request_mode_handler(UscuidUlPoller* instance) {
    instance->event.type = UscuidUlPollerEventTypeRequestMode;
    NfcCommand command = instance->callback(instance->event, instance->context);
    instance->mode = instance->event_data.poller_mode.mode; // selects the page-order strategy
    instance->state = UscuidUlPollerStateRequestDataToWrite;
    return command;
}

// Full clone: write every page the dump read (data + UID + config). PWD-AUTH runs first only when a
// password was armed (direct engine); a page the tag NAKs is logged and skipped, never aborts.
static NfcCommand uscuid_ul_poller_request_data_handler(UscuidUlPoller* instance) {
    instance->event_data.data_to_write.data = NULL; // defensive: clear stale out-param
    instance->event.type = UscuidUlPollerEventTypeRequestDataToWrite;
    NfcCommand command = instance->callback(instance->event, instance->context);

    instance->data = instance->event_data.data_to_write.data;
    instance->write_index = 0;
    instance->written = 0;
    instance->failed_count = 0;
    memset(instance->failed_bitmap, 0, sizeof(instance->failed_bitmap));
    instance->authed = false;
    instance->auth_ever_ok = false;
    // Clamp to the bitmap's capacity: a malformed dump can declare more pages than its type
    // holds, which would push page numbers past failed_bitmap and corrupt the heap.
    uint16_t pages = (instance->data != NULL) ? instance->data->pages_read : 0;
    instance->pages_total = (pages > USCUID_UL_MAX_PAGES) ? USCUID_UL_MAX_PAGES : pages;

    if(instance->data == NULL || instance->pages_total == 0) {
        // Nothing to write is a failure, not a no-op "success".
        FURI_LOG_E(TAG, "No source data to write");
        instance->state = UscuidUlPollerStateFail;
    } else {
        instance->state = UscuidUlPollerStateWrite;
    }
    return command;
}

static NfcCommand uscuid_ul_poller_write_handler(UscuidUlPoller* instance) {
    if(instance->write_index >= instance->pages_total) {
        if(instance->failed_count == 0) {
            instance->state = UscuidUlPollerStateSuccess;
        } else if(instance->written == 0) {
            // Nothing landed at all -> a plain failure, not a partial clone.
            instance->state = UscuidUlPollerStateFail;
        } else {
            instance->state = UscuidUlPollerStatePartial;
        }
        return NfcCommandContinue;
    }

    // PWD-AUTH before touching any page (direct engine only). authed is cleared on each
    // (re-)activation, so this also re-auths after a reset triggered by a locked-page NAK.
    if(instance->password_set && uscuid_ul_is_direct(instance) && !instance->authed) {
        UscuidUlPollerError auth_error = uscuid_ul_poller_auth_pwd(instance);
        if(auth_error == UscuidUlPollerErrorNone) {
            instance->authed = true;
            instance->auth_ever_ok = true;
        } else if(!instance->auth_ever_ok) {
            // Initial auth failed (wrong password, or the card left the field). Abort without
            // retrying: looping auth attempts would burn the tag's AUTHLIM and can brick it.
            // Nothing was written, so it's clean.
            FURI_LOG_E(TAG, "PWD-AUTH failed (err %d)", auth_error);
            instance->state = UscuidUlPollerStateAuthFailed;
            return NfcCommandContinue;
        } else {
            // A re-auth with the already-accepted password failed (transient RF glitch). Don't
            // loop; finalize with whatever already landed. Count the not-yet-attempted pages as
            // failed too, so the Partial report's written + failed_count == pages_total.
            FURI_LOG_E(TAG, "Re-auth failed (err %d, written %u)", auth_error, instance->written);
            for(uint16_t i = instance->write_index; i < instance->pages_total; i++) {
                uint8_t p =
                    uscuid_ul_poller_page_for_index(instance->mode, i, instance->pages_total);
                if(!(instance->failed_bitmap[p >> 3] & (uint8_t)(1u << (p & 7u)))) {
                    instance->failed_bitmap[p >> 3] |= (uint8_t)(1u << (p & 7u));
                    instance->failed_count++;
                }
            }
            instance->state = (instance->written > 0) ? UscuidUlPollerStatePartial :
                                                        UscuidUlPollerStateFail;
            return NfcCommandContinue;
        }
    }

    const uint8_t page = uscuid_ul_poller_page_for_index(
        instance->mode, instance->write_index, instance->pages_total);
    const uint8_t* src = instance->data->page[page].data;

    // ACK/NAK is the success signal (like the firmware & PM3). No read-back: a plain READ
    // can't verify special registers (PACK) anyway.
    UscuidUlPollerError write_error = uscuid_ul_poller_write_page(instance, page, src);
    instance->write_index++;

    if(write_error != UscuidUlPollerErrorNone) {
        FURI_LOG_E(TAG, "Write failed at page %u (err %d)", page, write_error);
        instance->failed_bitmap[page >> 3] |= (uint8_t)(1u << (page & 7u));
        instance->failed_count++;
        // A genuine tag can go mute after NAKing a locked page. On the direct engine,
        // re-activate (NfcCommandReset re-runs WUPA/anticoll/select, reviving a HALTed card)
        // so the remaining writable pages still land -> Partial instead of a total Fail. The
        // backdoor engine must stay woken (a reset drops backdoor mode), so it just continues.
        if(uscuid_ul_is_direct(instance)) {
            instance->authed = false; // re-select drops auth; re-auth on the next pass
            return NfcCommandReset;
        }
        return NfcCommandContinue;
    }

    instance->written++;
    instance->event.type = UscuidUlPollerEventTypeWriteProgress;
    instance->event_data.write_progress.pages_written = instance->written;
    instance->event_data.write_progress.pages_total = instance->pages_total;
    return instance->callback(instance->event, instance->context);
}

static NfcCommand uscuid_ul_poller_success_handler(UscuidUlPoller* instance) {
    instance->event.type = UscuidUlPollerEventTypeSuccess;
    NfcCommand command = instance->callback(instance->event, instance->context);
    instance->state = UscuidUlPollerStateIdle;
    return command;
}

static NfcCommand uscuid_ul_poller_partial_handler(UscuidUlPoller* instance) {
    instance->event.type = UscuidUlPollerEventTypePartial;
    instance->event_data.partial.pages_written = instance->written;
    instance->event_data.partial.pages_total = instance->pages_total;
    instance->event_data.partial.failed_count = instance->failed_count;
    memcpy(
        instance->event_data.partial.failed_bitmap,
        instance->failed_bitmap,
        sizeof(instance->failed_bitmap));
    NfcCommand command = instance->callback(instance->event, instance->context);
    instance->state = UscuidUlPollerStateIdle;
    return command;
}

static NfcCommand uscuid_ul_poller_auth_failed_handler(UscuidUlPoller* instance) {
    instance->event.type = UscuidUlPollerEventTypeAuthFailed;
    NfcCommand command = instance->callback(instance->event, instance->context);
    instance->state = UscuidUlPollerStateIdle;
    return command;
}

static NfcCommand uscuid_ul_poller_fail_handler(UscuidUlPoller* instance) {
    instance->event.type = UscuidUlPollerEventTypeFail;
    instance->event_data.fail.pages_written = instance->written;
    instance->event_data.fail.pages_total = instance->pages_total;
    NfcCommand command = instance->callback(instance->event, instance->context);
    instance->state = UscuidUlPollerStateIdle;
    return command;
}

static const UscuidUlPollerStateHandler uscuid_ul_poller_state_handlers[UscuidUlPollerStateNum] = {
    [UscuidUlPollerStateIdle] = uscuid_ul_poller_idle_handler,
    [UscuidUlPollerStateRequestMode] = uscuid_ul_poller_request_mode_handler,
    [UscuidUlPollerStateRequestDataToWrite] = uscuid_ul_poller_request_data_handler,
    [UscuidUlPollerStateWrite] = uscuid_ul_poller_write_handler,
    [UscuidUlPollerStateSuccess] = uscuid_ul_poller_success_handler,
    [UscuidUlPollerStatePartial] = uscuid_ul_poller_partial_handler,
    [UscuidUlPollerStateFail] = uscuid_ul_poller_fail_handler,
    [UscuidUlPollerStateAuthFailed] = uscuid_ul_poller_auth_failed_handler,
};

static NfcCommand uscuid_ul_poller_run(NfcEvent event, void* context) {
    furi_assert(context);
    UscuidUlPoller* instance = context;
    NfcCommand command = NfcCommandContinue;

    if(event.type == NfcEventTypePollerReady) {
        command = uscuid_ul_poller_state_handlers[instance->state](instance);
    }

    if(instance->session_state == UscuidUlPollerSessionStateStopRequest) {
        command = NfcCommandStop;
    }

    return command;
}

// Direct-engine run loop: driven by the iso3 poller, which handles normal activation.
static NfcCommand uscuid_ul_poller_iso3_run(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.instance);

    UscuidUlPoller* instance = context;
    instance->iso3_poller = event.instance;
    Iso14443_3aPollerEvent* iso3_event = event.event_data;
    NfcCommand command = NfcCommandContinue;

    if(iso3_event->type == Iso14443_3aPollerEventTypeReady) {
        command = uscuid_ul_poller_state_handlers[instance->state](instance);
    }

    if(instance->session_state == UscuidUlPollerSessionStateStopRequest) {
        command = NfcCommandStop;
    }

    return command;
}

void uscuid_ul_poller_start(
    UscuidUlPoller* instance,
    UscuidUlPollerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;
    instance->state = UscuidUlPollerStateIdle;
    instance->session_state = UscuidUlPollerSessionStateStarted;

    if(uscuid_ul_is_direct(instance)) {
        // Direct engine: the iso3 NfcPoller configures the field itself (do not pre-config).
        instance->iso3_nfc_poller = nfc_poller_alloc(instance->nfc, NfcProtocolIso14443_3a);
        nfc_poller_start(instance->iso3_nfc_poller, uscuid_ul_poller_iso3_run, instance);
    } else {
        // Raw backdoor engine: configure the field ourselves before nfc_start.
        uscuid_ul_poller_config_nfc(instance->nfc);
        nfc_start(instance->nfc, uscuid_ul_poller_run, instance);
    }
}

void uscuid_ul_poller_stop(UscuidUlPoller* instance) {
    furi_assert(instance);

    instance->session_state = UscuidUlPollerSessionStateStopRequest;
    if(instance->iso3_nfc_poller != NULL) {
        nfc_poller_stop(instance->iso3_nfc_poller);
        nfc_poller_free(instance->iso3_nfc_poller);
        instance->iso3_nfc_poller = NULL;
        instance->iso3_poller = NULL;
    } else {
        nfc_stop(instance->nfc);
    }
    instance->session_state = UscuidUlPollerSessionStateIdle;
}
