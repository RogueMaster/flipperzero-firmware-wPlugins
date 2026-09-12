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

// Private custom-event id (kept well above the scenes' event ranges).
#define READER_EVENT_UID 401u

struct TimeclockReader {
    ViewDispatcher* vd;
    TimeclockReaderCallback callback;
    void* context;

    bool use_lf;
    bool continuous;

    // NFC (ISO14443-3A poller: reads MIFARE Classic/Ultralight, NTAG, DESFire
    // and other ISO14443-A cards - the badges you actually use). This is the
    // stable, proven path; it does not use the multi-protocol scanner.
    Nfc* nfc;
    NfcPoller* poller;

    // LF RFID (auto: EM4100, HID, Indala, ... whatever the firmware supports)
    ProtocolDict* lf_dict;
    LFRFIDWorker* lf_worker;

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

void timeclock_reader_start(TimeclockReader* reader, bool use_lf, bool continuous) {
    furi_assert(reader);
    timeclock_reader_stop(reader); // idempotent: never leak a previous session

    reader->use_lf = use_lf;
    reader->continuous = continuous;

    if(use_lf) {
        reader->lf_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
        reader->lf_worker = lfrfid_worker_alloc(reader->lf_dict);
        lfrfid_worker_start_thread(reader->lf_worker);
        lfrfid_worker_read_start(
            reader->lf_worker, LFRFIDWorkerReadTypeAuto, reader_lf_callback, reader);
    } else {
        reader->nfc = nfc_alloc();
        reader->poller = nfc_poller_alloc(reader->nfc, NfcProtocolIso14443_3a);
        nfc_poller_start(reader->poller, reader_poller_callback, reader);
    }
}

void timeclock_reader_stop(TimeclockReader* reader) {
    furi_assert(reader);
    reader_stop_nfc(reader);
    reader_stop_lf(reader);
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
