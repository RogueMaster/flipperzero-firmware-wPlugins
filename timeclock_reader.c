// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "timeclock_reader.h"

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>

#include <ibutton/ibutton_worker.h>
#include <ibutton/ibutton_key.h>
#include <ibutton/ibutton_protocols.h>

// Private custom-event id (kept well above the scenes' event ranges).
#define READER_EVENT_UID 401u

// All three radios (NFC, LF RFID, iButton) run simultaneously; whichever
// detects a badge first posts the UID. There is no manual reader selection, so
// a workplace can mix NFC, RFID and iButton badges freely. Only the UID is read
// - nothing is written to or emulated onto the card, so badges already issued
// by another company work as identity tokens too.
struct TimeclockReader {
    ViewDispatcher* vd;
    TimeclockReaderCallback callback;
    void* context;

    bool continuous;

    // NFC (ISO14443-3A poller: MIFARE Classic/Ultralight, NTAG, DESFire, ...).
    Nfc* nfc;
    NfcPoller* poller;

    // LF RFID (auto: EM4100, HID, Indala, ... whatever the firmware supports).
    ProtocolDict* lf_dict;
    LFRFIDWorker* lf_worker;

    // iButton / 1-Wire (DS1990A and other Dallas keys).
    iButtonProtocols* ib_protocols;
    iButtonKey* ib_key;
    iButtonWorker* ib_worker;

    char uid[TC_UID_STR_MAX];
    char tech[TC_TECH_MAX];
};

static void reader_format_uid(
    TimeclockReader* reader,
    const uint8_t* uid,
    size_t len,
    const char* tech) {
    if(len > 20) len = 20;
    size_t pos = 0;
    for(size_t i = 0; i < len && pos + 2 < TC_UID_STR_MAX; i++) {
        static const char hexd[] = "0123456789ABCDEF";
        reader->uid[pos++] = hexd[(uid[i] >> 4) & 0xF];
        reader->uid[pos++] = hexd[uid[i] & 0xF];
    }
    reader->uid[pos] = '\0';
    strncpy(reader->tech, tech, TC_TECH_MAX - 1);
    reader->tech[TC_TECH_MAX - 1] = '\0';
}

// ---- NFC poller callback (worker thread) -----------------------------------
static NfcCommand reader_poller_callback(NfcGenericEvent event, void* context) {
    TimeclockReader* reader = context;
    NfcCommand command = NfcCommandContinue;

    Iso14443_3aPollerEvent* iso_event = event.event_data;
    if(iso_event && iso_event->type == Iso14443_3aPollerEventTypeReady) {
        const Iso14443_3aData* data = nfc_poller_get_data(reader->poller);
        size_t uid_len = 0;
        const uint8_t* uid = iso14443_3a_get_uid(data, &uid_len);
        if(uid && uid_len > 0) {
            reader_format_uid(reader, uid, uid_len, "NFC");
            view_dispatcher_send_custom_event(reader->vd, READER_EVENT_UID);
            if(!reader->continuous) command = NfcCommandStop;
        }
    }
    return command;
}

// ---- LF RFID read callback (worker thread) ---------------------------------
static void reader_lf_callback(LFRFIDWorkerReadResult result, ProtocolId protocol, void* context) {
    TimeclockReader* reader = context;
    if(result != LFRFIDWorkerReadDone) return;
    size_t data_size = protocol_dict_get_data_size(reader->lf_dict, protocol);
    if(data_size == 0 || data_size > 20) return;
    uint8_t buffer[20];
    protocol_dict_get_data(reader->lf_dict, protocol, buffer, data_size);
    reader_format_uid(reader, buffer, data_size, "RFID");
    view_dispatcher_send_custom_event(reader->vd, READER_EVENT_UID);
}

// ---- iButton read callback (worker thread) ---------------------------------
static void reader_ibutton_callback(void* context) {
    TimeclockReader* reader = context;
    iButtonEditableData editable = {0};
    ibutton_protocols_get_editable_data(reader->ib_protocols, reader->ib_key, &editable);
    if(editable.ptr && editable.size > 0) {
        reader_format_uid(reader, editable.ptr, editable.size, "iBTN");
        view_dispatcher_send_custom_event(reader->vd, READER_EVENT_UID);
    }
}

static void reader_stop_nfc(TimeclockReader* reader) {
    if(reader->poller) {
        nfc_poller_stop(reader->poller);
        nfc_poller_free(reader->poller);
        reader->poller = NULL;
    }
    if(reader->nfc) {
        nfc_free(reader->nfc);
        reader->nfc = NULL;
    }
}

static void reader_stop_lf(TimeclockReader* reader) {
    if(reader->lf_worker) {
        lfrfid_worker_stop(reader->lf_worker);
        lfrfid_worker_stop_thread(reader->lf_worker);
        lfrfid_worker_free(reader->lf_worker);
        reader->lf_worker = NULL;
    }
    if(reader->lf_dict) {
        protocol_dict_free(reader->lf_dict);
        reader->lf_dict = NULL;
    }
}

static void reader_stop_ibutton(TimeclockReader* reader) {
    if(reader->ib_worker) {
        ibutton_worker_stop(reader->ib_worker);
        ibutton_worker_stop_thread(reader->ib_worker);
        ibutton_worker_free(reader->ib_worker);
        reader->ib_worker = NULL;
    }
    if(reader->ib_key) {
        ibutton_key_free(reader->ib_key);
        reader->ib_key = NULL;
    }
    if(reader->ib_protocols) {
        ibutton_protocols_free(reader->ib_protocols);
        reader->ib_protocols = NULL;
    }
}

TimeclockReader* timeclock_reader_alloc(ViewDispatcher* view_dispatcher) {
    TimeclockReader* reader = malloc(sizeof(TimeclockReader));
    memset(reader, 0, sizeof(TimeclockReader));
    reader->vd = view_dispatcher;
    return reader;
}

void timeclock_reader_free(TimeclockReader* reader) {
    furi_assert(reader);
    timeclock_reader_stop(reader);
    free(reader);
}

void timeclock_reader_set_callback(
    TimeclockReader* reader,
    TimeclockReaderCallback callback,
    void* context) {
    furi_assert(reader);
    reader->callback = callback;
    reader->context = context;
}

void timeclock_reader_start(TimeclockReader* reader, bool continuous) {
    furi_assert(reader);
    timeclock_reader_stop(reader); // idempotent: never leak a previous session

    reader->continuous = continuous;

    // NFC (13.56 MHz)
    reader->nfc = nfc_alloc();
    reader->poller = nfc_poller_alloc(reader->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(reader->poller, reader_poller_callback, reader);

    // LF RFID (125 kHz)
    reader->lf_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    reader->lf_worker = lfrfid_worker_alloc(reader->lf_dict);
    lfrfid_worker_start_thread(reader->lf_worker);
    lfrfid_worker_read_start(
        reader->lf_worker, LFRFIDWorkerReadTypeAuto, reader_lf_callback, reader);

    // iButton (1-Wire)
    reader->ib_protocols = ibutton_protocols_alloc();
    reader->ib_key = ibutton_key_alloc(ibutton_protocols_get_max_data_size(reader->ib_protocols));
    reader->ib_worker = ibutton_worker_alloc(reader->ib_protocols);
    ibutton_worker_start_thread(reader->ib_worker);
    ibutton_worker_read_set_callback(reader->ib_worker, reader_ibutton_callback, reader);
    ibutton_worker_read_start(reader->ib_worker, reader->ib_key);
}

void timeclock_reader_stop(TimeclockReader* reader) {
    furi_assert(reader);
    reader_stop_nfc(reader);
    reader_stop_lf(reader);
    reader_stop_ibutton(reader);
}

bool timeclock_reader_handle_event(TimeclockReader* reader, uint32_t event) {
    furi_assert(reader);
    if(event == READER_EVENT_UID) {
        if(reader->callback) {
            reader->callback(reader->uid, reader->tech, reader->context);
        }
        return true;
    }
    return false;
}
