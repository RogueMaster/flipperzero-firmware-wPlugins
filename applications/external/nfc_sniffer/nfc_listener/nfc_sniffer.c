
#include "nfc_sniffer.h"

#include <furi.h>

#define LOG_TAG "nfc_sniffer"

typedef enum {
    NfcLoggerStateIdle,
    NfcLoggerStateActive,
    NfcLoggerStateStopping,
} NfcLoggerState;

struct _NfcLogger {
    Nfc* nfc;
    NfcLoggerState nfc_polling_state;

    NfcLoggerCallback callback;
    void* context;
};

static void log_iso15693_bit_buffer(const char* tag, const BitBuffer* buffer) {
    size_t buffer_length = bit_buffer_get_size_bytes(buffer);

    char output[2 * buffer_length + 1];

    for(size_t i = 0; i < buffer_length; i++) {
        snprintf(&output[i * 2], 3, "%02X", bit_buffer_get_byte(buffer, i));
    }

    FURI_LOG_D(LOG_TAG, "%s: %s", tag, output);
}

static NfcCommand nfc_callback(NfcEvent event, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context, "NFC Callback Context is null");

    NfcLogger* instance = context;

    if(instance->nfc_polling_state == NfcLoggerStateStopping) {
        return NfcCommandStop;
    }

    if(event.type == NfcEventTypeRxEnd && bit_buffer_get_size_bytes(event.data.buffer) > 0) {
        // A command was received, so validate it and respond
        log_iso15693_bit_buffer("Received", event.data.buffer);
        NfcLoggerEvent loggerEvent = {
            .type = NfcLoggerEventReady,
            .data = event.data.buffer,
        };
        return instance->callback(loggerEvent, instance->context);
    }

    return NfcCommandContinue;
}

NfcLogger* nfc_sniffer_alloc(Nfc* nfc, NfcTech protocol) {
    FURI_LOG_T(LOG_TAG, __func__);

    NfcLogger* instance = malloc(sizeof(NfcLogger));
    instance->nfc_polling_state = NfcLoggerStateIdle;

    instance->nfc = nfc;
    nfc_config(instance->nfc, NfcModeListener, protocol);
    nfc_set_guard_time_us(instance->nfc, 10000);
    nfc_set_fdt_poll_fc(instance->nfc, 5000);
    nfc_set_fdt_poll_poll_us(instance->nfc, 1000);

    return instance;
}

void nfc_sniffer_free(NfcLogger* instance) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(instance, "NfcLogger is null");

    free(instance);
}

void nfc_sniffer_start(NfcLogger* instance, NfcLoggerCallback callback, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(instance, "NfcLogger is null");
    furi_assert(instance->nfc_polling_state == NfcLoggerStateIdle, "NfcLogger is not idle");

    instance->callback = callback;
    instance->context = context;

    instance->nfc_polling_state = NfcLoggerStateActive;
    nfc_start(instance->nfc, nfc_callback, instance);
    FURI_LOG_D(LOG_TAG, "NFC Started");
}

void nfc_sniffer_stop(NfcLogger* instance) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(instance, "NfcLogger is null");

    instance->nfc_polling_state = NfcLoggerStateStopping;
    nfc_stop(instance->nfc);
    FURI_LOG_D(LOG_TAG, "NFC Stopped");
    instance->nfc_polling_state = NfcLoggerStateIdle;
}
