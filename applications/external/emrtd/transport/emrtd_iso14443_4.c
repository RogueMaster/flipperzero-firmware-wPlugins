/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_iso14443_4.h"

#include <stdio.h>
#include <string.h>

#include <furi.h>
#include <nfc/protocols/iso14443_3b/iso14443_3b.h>
#include <toolbox/bit_buffer.h>

#include "emrtd_isodep.h"

#define TAG "EmrtdIso14443_4"

/** Prologue (PCB) plus epilogue (CRC) of an ISO 14443-4 block. */
#define EMRTD_ISO14443_4_FRAME_OVERHEAD 3

/** ISO 14443-4 section 5.1: the frame size before the ATS has been read. */
#define EMRTD_ISO14443_4_FSC_DEFAULT 32

/** Carrier cycles per millisecond, for turning a waiting time into words. */
#define EMRTD_ISO14443_4_FC_PER_MS 13560u

struct EmrtdIso14443_4 {
    EmrtdTransceiver transceiver;
    EmrtdIso14443_4Variant variant;
    Iso14443_3aPoller* poller_3a;
    Iso14443_4bPoller* poller_4b;
    EmrtdIsoDep isodep;
    /*
     * What the radio said about each frame of the APDU being exchanged. One
     * EmrtdError covers a timeout, a checksum failure and an internal fault,
     * and when a command goes unanswered three times it matters whether all
     * three failed the same way. One block that fails outright records
     * 1 + EMRTD_ISODEP_RETRIES of them, since a recovery round loses at most
     * one frame; an APDU can span several blocks and extension rounds, so
     * there is room to spare, and the count goes on past what is kept.
     */
    Iso14443_3aError last_error;
    uint8_t frame_errors[2 * EMRTD_ISODEP_RETRIES + 2];
    uint8_t frame_error_count;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;
    EmrtdIso14443_4TraceCallback trace;
    void* trace_context;
};

/** How many of the radio codes counted in frame_error_count were kept. */
static uint8_t emrtd_iso14443_4_kept_errors(const EmrtdIso14443_4* instance) {
    return instance->frame_error_count < sizeof(instance->frame_errors) ?
               instance->frame_error_count :
               (uint8_t)sizeof(instance->frame_errors);
}

static EmrtdError emrtd_iso14443_4_transceive(
    void* ctx,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len);

static const EmrtdTransceiverApi emrtd_iso14443_4a_api = {
    .name = "ISO 14443-4A",
    .transceive = emrtd_iso14443_4_transceive,
};

static const EmrtdTransceiverApi emrtd_iso14443_4b_api = {
    .name = "ISO 14443-4B",
    .transceive = emrtd_iso14443_4_transceive,
};

/** Nothing is bound yet; an exchange attempted now is a programming error. */
static const EmrtdTransceiverApi emrtd_iso14443_4_idle_api = {
    .name = "ISO 14443-4",
    .transceive = emrtd_iso14443_4_transceive,
};

static EmrtdError emrtd_iso14443_4_frame(
    void* context,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len,
    uint32_t fwt_fc);

/**
 * Leave the card alone for a moment, because it asked.
 *
 * This blocks the NFC thread inside the poller callback, which is where the
 * whole read runs anyway: the field stays on, the card stays selected, and
 * ISO 14443-4 gives a card no reason to mind a reader that is quiet.
 */
static void emrtd_iso14443_4_delay(void* context, uint32_t ms) {
    UNUSED(context);
    furi_delay_ms(ms);
}

EmrtdIso14443_4* emrtd_iso14443_4_alloc(void) {
    EmrtdIso14443_4* instance = malloc(sizeof(EmrtdIso14443_4));
    memset(instance, 0, sizeof(EmrtdIso14443_4));

    instance->variant = EmrtdIso14443_4VariantNone;
    instance->transceiver.api = &emrtd_iso14443_4_idle_api;
    instance->transceiver.ctx = instance;
    instance->transceiver.fsc = EMRTD_ISO14443_4_FSC_DEFAULT;
    instance->transceiver.fsd = EMRTD_ISO14443_4_FSD;

    instance->tx_buffer = bit_buffer_alloc(EMRTD_ISO14443_4_BUFFER_SIZE);
    instance->rx_buffer = bit_buffer_alloc(EMRTD_ISO14443_4_BUFFER_SIZE);

    emrtd_isodep_init(&instance->isodep, emrtd_iso14443_4_frame, emrtd_iso14443_4_delay, instance);

    return instance;
}

void emrtd_iso14443_4_free(EmrtdIso14443_4* instance) {
    furi_check(instance);

    bit_buffer_free(instance->tx_buffer);
    bit_buffer_free(instance->rx_buffer);
    free(instance);
}

/**
 * Decide the card's frame size.
 *
 * A card that announces more than the reader can hold is capped, because the
 * limit that binds is whichever of the two is smaller.
 */
static void emrtd_iso14443_4_set_fsc(EmrtdIso14443_4* instance, uint16_t fsc) {
    if(fsc < EMRTD_ISO14443_4_FSC_DEFAULT) {
        fsc = EMRTD_ISO14443_4_FSC_DEFAULT;
    }
    if(fsc > EMRTD_ISO14443_4_MAX_INF + EMRTD_ISO14443_4_FRAME_OVERHEAD) {
        fsc = EMRTD_ISO14443_4_MAX_INF + EMRTD_ISO14443_4_FRAME_OVERHEAD;
    }
    instance->transceiver.fsc = fsc;
}

EmrtdError emrtd_iso14443_4_bind_3a(EmrtdIso14443_4* instance, Iso14443_3aPoller* poller) {
    furi_check(instance);
    furi_check(poller);

    instance->variant = EmrtdIso14443_4VariantNone;
    instance->poller_3a = poller;
    instance->poller_4b = NULL;
    instance->transceiver.api = &emrtd_iso14443_4_idle_api;
    emrtd_isodep_reset(&instance->isodep);
    instance->last_error = Iso14443_3aErrorNone;

    const EmrtdError error = emrtd_isodep_activate(&instance->isodep);
    if(error != EmrtdErrorNone) {
        FURI_LOG_W(
            TAG, "RATS failed: %s (radio said %d)", emrtd_error_text(error), instance->last_error);
        instance->poller_3a = NULL;
        return error;
    }

    instance->variant = EmrtdIso14443_4VariantA;
    instance->transceiver.api = &emrtd_iso14443_4a_api;
    emrtd_iso14443_4_set_fsc(instance, emrtd_isodep_fsc(&instance->isodep));

    char description[96];
    emrtd_iso14443_4_describe(instance, description, sizeof(description));
    FURI_LOG_I(TAG, "%s", description);

    return EmrtdErrorNone;
}

void emrtd_iso14443_4_bind_4b(
    EmrtdIso14443_4* instance,
    Iso14443_4bPoller* poller,
    const Iso14443_4bData* data) {
    furi_check(instance);
    furi_check(poller);

    instance->variant = EmrtdIso14443_4VariantB;
    instance->poller_3a = NULL;
    instance->poller_4b = poller;
    instance->transceiver.api = &emrtd_iso14443_4b_api;

    /*
     * Type B carries the frame size in the protocol info of the ATQB rather
     * than in an ATS, and the firmware exposes it only through the type 3B
     * accessor, which already applies the ISO 14443-3 table.
     */
    uint16_t fsc = EMRTD_ISO14443_4_FSC_DEFAULT;
    if(data != NULL) {
        const Iso14443_3bData* base = iso14443_4b_get_base_data(data);
        if(base != NULL) {
            const uint16_t announced = iso14443_3b_get_frame_size_max(base);
            if(announced > 0) {
                fsc = announced;
            }
        }
    }
    emrtd_iso14443_4_set_fsc(instance, fsc);

    char description[96];
    emrtd_iso14443_4_describe(instance, description, sizeof(description));
    FURI_LOG_I(TAG, "%s", description);
}

void emrtd_iso14443_4_unbind(EmrtdIso14443_4* instance) {
    furi_check(instance);

    if(instance->variant == EmrtdIso14443_4VariantA && instance->poller_3a != NULL) {
        emrtd_isodep_deselect(&instance->isodep);
    }

    instance->variant = EmrtdIso14443_4VariantNone;
    instance->poller_3a = NULL;
    instance->poller_4b = NULL;
    instance->transceiver.api = &emrtd_iso14443_4_idle_api;
    instance->transceiver.fsc = EMRTD_ISO14443_4_FSC_DEFAULT;
    emrtd_isodep_reset(&instance->isodep);
    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);
}

EmrtdTransceiver* emrtd_iso14443_4_transceiver(EmrtdIso14443_4* instance) {
    furi_check(instance);

    return &instance->transceiver;
}

EmrtdIso14443_4Variant emrtd_iso14443_4_variant(const EmrtdIso14443_4* instance) {
    furi_check(instance);

    return instance->variant;
}

void emrtd_iso14443_4_describe(const EmrtdIso14443_4* instance, char* out, size_t out_size) {
    if(out == NULL || out_size == 0) {
        return;
    }
    if(instance == NULL || instance->variant == EmrtdIso14443_4VariantNone) {
        snprintf(out, out_size, "no card bound");
        return;
    }

    if(instance->variant == EmrtdIso14443_4VariantB) {
        snprintf(out, out_size, "Type B card, FSC %u", instance->transceiver.fsc);
        return;
    }

    /*
     * The ATS is the one piece of evidence that explains a read which fails
     * on a document that has not moved: it carries FWI, and a card that names
     * none is the case the firmware's own poller gets wrong.
     */
    char ats_hex[2 * EMRTD_ISODEP_ATS_MAX + 1] = "none";
    size_t ats_len = 0;
    const uint8_t* ats = emrtd_isodep_ats(&instance->isodep, &ats_len);
    if(ats != NULL && ats_len > 0) {
        size_t pos = 0;
        for(size_t i = 0; i < ats_len && pos + 3 <= sizeof(ats_hex); i++) {
            pos += (size_t)snprintf(ats_hex + pos, sizeof(ats_hex) - pos, "%02X", ats[i]);
        }
    }

    snprintf(
        out,
        out_size,
        "Type A, FSC %u, FWI %u%s waiting %lu ms, SFGI %u guard %lu ms, ATS %s",
        instance->transceiver.fsc,
        instance->isodep.fwi,
        instance->isodep.fwi_announced ? "" : " (assumed)",
        (unsigned long)(instance->isodep.fwt_fc / EMRTD_ISO14443_4_FC_PER_MS),
        instance->isodep.sfgi,
        (unsigned long)instance->isodep.sfgt_ms,
        ats_hex);
}

void emrtd_iso14443_4_failure_detail(
    const EmrtdIso14443_4* instance,
    EmrtdError error,
    char* out,
    size_t out_size) {
    if(out == NULL || out_size == 0) {
        return;
    }
    if(instance == NULL) {
        snprintf(out, out_size, "%s", emrtd_error_text(error));
        return;
    }
    /*
     * The mapped error says what it meant for the read; the radio codes say
     * what actually happened on each attempt, and the two are not the same
     * question. A timeout and an internal fault both arrive as "Document moved
     * away", and three identical timeouts mean something different from
     * three different faults.
     */
    size_t pos = (size_t)snprintf(out, out_size, "%s, radio", emrtd_error_text(error));
    if(instance->frame_error_count == 0) {
        snprintf(out + pos, out_size - pos, " %d", instance->last_error);
        return;
    }
    const uint8_t kept = emrtd_iso14443_4_kept_errors(instance);
    for(uint8_t i = 0; i < kept && pos + 4 < out_size; i++) {
        pos += (size_t)snprintf(out + pos, out_size - pos, " %u", instance->frame_errors[i]);
    }
}

bool emrtd_iso14443_4_recovery_detail(const EmrtdIso14443_4* instance, char* out, size_t out_size) {
    if(out == NULL || out_size == 0) {
        return false;
    }
    out[0] = '\0';
    if(instance == NULL || instance->frame_error_count == 0) {
        return false;
    }

    /* Fields of the trace never contain a space, so the codes are joined by commas. */
    size_t pos =
        (size_t)snprintf(out, out_size, "lost=%u radio=", (unsigned)instance->frame_error_count);
    const uint8_t kept = emrtd_iso14443_4_kept_errors(instance);
    for(uint8_t i = 0; i < kept && pos + 4 < out_size; i++) {
        pos += (size_t)snprintf(
            out + pos, out_size - pos, "%s%u", i > 0 ? "," : "", instance->frame_errors[i]);
    }
    return true;
}

void emrtd_iso14443_4_set_trace(
    EmrtdIso14443_4* instance,
    EmrtdIso14443_4TraceCallback callback,
    void* context) {
    furi_check(instance);

    instance->trace = callback;
    instance->trace_context = context;
}

/**
 * What a radio level failure means to the layers above.
 *
 * A timeout is the honest "the chip went quiet" signal; everything else is a
 * fault of the link rather than a statement about the document, and saying so
 * keeps "Document moved away" for the case that really is that.
 */
static EmrtdError emrtd_iso14443_4_map_error_3a(Iso14443_3aError error) {
    switch(error) {
    case Iso14443_3aErrorNone:
        return EmrtdErrorNone;
    case Iso14443_3aErrorTimeout:
    case Iso14443_3aErrorNotPresent:
        return EmrtdErrorCardLost;
    case Iso14443_3aErrorBufferOverflow:
        return EmrtdErrorBufferTooSmall;
    case Iso14443_3aErrorWrongCrc:
    case Iso14443_3aErrorCommunication:
    case Iso14443_3aErrorFieldOff:
    case Iso14443_3aErrorColResFailed:
    default:
        return EmrtdErrorTransport;
    }
}

static EmrtdError emrtd_iso14443_4_map_error_b(Iso14443_4bError error) {
    switch(error) {
    case Iso14443_4bErrorNone:
        return EmrtdErrorNone;
    case Iso14443_4bErrorNotPresent:
    case Iso14443_4bErrorTimeout:
        return EmrtdErrorCardLost;
    case Iso14443_4bErrorProtocol:
        return EmrtdErrorProtocol;
    default:
        return EmrtdErrorTransport;
    }
}

/** Put one bare frame on the wire for the ISO-DEP layer above. */
static EmrtdError emrtd_iso14443_4_frame(
    void* context,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len,
    uint32_t fwt_fc) {
    EmrtdIso14443_4* instance = context;
    *rx_len = 0;

    if(instance == NULL || instance->poller_3a == NULL) {
        return EmrtdErrorInternal;
    }
    /*
     * The poller appends the CRC inside its own buffer, and overflowing a
     * BitBuffer aborts the application rather than failing, so the room it
     * will need is checked here instead of assumed.
     */
    if(tx_len == 0 || tx_len + 2 > bit_buffer_get_capacity_bytes(instance->tx_buffer)) {
        return EmrtdErrorBufferTooSmall;
    }

    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_append_bytes(instance->tx_buffer, tx, tx_len);
    bit_buffer_reset(instance->rx_buffer);

    const Iso14443_3aError error = iso14443_3a_poller_send_standard_frame(
        instance->poller_3a, instance->tx_buffer, instance->rx_buffer, fwt_fc);
    instance->last_error = error;
    if(error != Iso14443_3aErrorNone) {
        if(instance->frame_error_count < sizeof(instance->frame_errors)) {
            instance->frame_errors[instance->frame_error_count] = (uint8_t)error;
        }
        if(instance->frame_error_count < UINT8_MAX) {
            instance->frame_error_count++;
        }
        FURI_LOG_W(TAG, "Frame of %zu bytes went unanswered, radio error %d", tx_len, error);
        return emrtd_iso14443_4_map_error_3a(error);
    }

    if(bit_buffer_has_partial_byte(instance->rx_buffer)) {
        return EmrtdErrorProtocol;
    }

    const size_t received = bit_buffer_get_size_bytes(instance->rx_buffer);
    if(received == 0) {
        return EmrtdErrorProtocol;
    }
    if(received > rx_cap) {
        return EmrtdErrorBufferTooSmall;
    }

    bit_buffer_write_bytes(instance->rx_buffer, rx, received);
    *rx_len = received;
    return EmrtdErrorNone;
}

/** Largest command data field that fits one type B frame towards the card. */
static size_t emrtd_iso14443_4_max_inf(const EmrtdIso14443_4* instance) {
    size_t fsc = instance->transceiver.fsc;
    if(fsc < EMRTD_ISO14443_4_FRAME_OVERHEAD + 1) {
        fsc = EMRTD_ISO14443_4_FSC_DEFAULT;
    }

    size_t inf = fsc - EMRTD_ISO14443_4_FRAME_OVERHEAD;
    if(inf > EMRTD_ISO14443_4_MAX_INF) {
        inf = EMRTD_ISO14443_4_MAX_INF;
    }
    return inf;
}

/** One APDU over the type B poller, which owns the block protocol itself. */
static EmrtdError emrtd_iso14443_4_transceive_b(
    EmrtdIso14443_4* instance,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len) {
    /*
     * The type B poller has no chaining helper, so a command that does not fit
     * one frame is refused rather than silently truncated.
     */
    if(tx_len > emrtd_iso14443_4_max_inf(instance)) {
        FURI_LOG_W(TAG, "Command of %zu bytes exceeds the type B frame size", tx_len);
        return EmrtdErrorProtocol;
    }
    if(bit_buffer_get_capacity_bytes(instance->tx_buffer) < tx_len) {
        return EmrtdErrorBufferTooSmall;
    }

    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_append_bytes(instance->tx_buffer, tx, tx_len);
    bit_buffer_reset(instance->rx_buffer);

    const Iso14443_4bError error = iso14443_4b_poller_send_block(
        instance->poller_4b, instance->tx_buffer, instance->rx_buffer);
    if(error != Iso14443_4bErrorNone) {
        FURI_LOG_W(TAG, "Type B block failed, radio error %d", error);
        return emrtd_iso14443_4_map_error_b(error);
    }

    if(bit_buffer_has_partial_byte(instance->rx_buffer)) {
        return EmrtdErrorProtocol;
    }

    const size_t received = bit_buffer_get_size_bytes(instance->rx_buffer);
    if(received > rx_cap) {
        return EmrtdErrorBufferTooSmall;
    }
    bit_buffer_write_bytes(instance->rx_buffer, rx, received);
    *rx_len = received;
    return EmrtdErrorNone;
}

static EmrtdError emrtd_iso14443_4_transceive(
    void* ctx,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len) {
    EmrtdIso14443_4* instance = ctx;
    if(instance == NULL || tx == NULL || rx == NULL || rx_len == NULL) {
        return EmrtdErrorInternal;
    }

    *rx_len = 0;

    if(instance->variant == EmrtdIso14443_4VariantNone) {
        return EmrtdErrorNoCard;
    }
    /* Four bytes is the shortest legal command APDU, CLA INS P1 P2. */
    if(tx_len < 4 || tx_len > EMRTD_ISO14443_4_BUFFER_SIZE) {
        return EmrtdErrorInvalidInput;
    }

    instance->frame_error_count = 0;

    if(instance->trace != NULL) {
        instance->trace(instance->trace_context, true, tx, tx_len);
    }

    const EmrtdError error =
        instance->variant == EmrtdIso14443_4VariantA ?
            emrtd_isodep_transceive(&instance->isodep, tx, tx_len, rx, rx_cap, rx_len) :
            emrtd_iso14443_4_transceive_b(instance, tx, tx_len, rx, rx_cap, rx_len);
    if(error != EmrtdErrorNone) {
        return error;
    }

    /* Even a bare status word is two bytes; anything shorter is not an APDU. */
    if(*rx_len < 2) {
        *rx_len = 0;
        return EmrtdErrorProtocol;
    }

    if(instance->trace != NULL) {
        instance->trace(instance->trace_context, false, rx, *rx_len);
    }

    return EmrtdErrorNone;
}
