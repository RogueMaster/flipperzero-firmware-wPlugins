// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "timeclock_reader.h"

#include <nfc/nfc.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>

#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>

// Private custom-event ids (kept well above the scenes' event ranges).
#define READER_EVENT_DETECTED 400u
#define READER_EVENT_UID 401u

struct TimeclockReader {
    ViewDispatcher* vd;
    TimeclockReaderCallback callback;
    void* context;

    bool use_lf;
    bool continuous;

    // NFC
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;
    NfcProtocol protocol;

    // LF RFID
    ProtocolDict* lf_dict;
    LFRFIDWorker* lf_worker;

    // Last read result
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

// ---- NFC scanner callback (worker thread) ----------------------------------
static void reader_scanner_callback(NfcScannerEvent event, void* context) {
    TimeclockReader* reader = context;
    if(event.type == NfcScannerEventTypeDetected && event.data.protocol_num > 0) {
        reader->protocol = event.data.protocols[0];
        view_dispatcher_send_custom_event(reader->vd, READER_EVENT_DETECTED);
    }
}

// ---- NFC poller callback (worker thread) -----------------------------------
static NfcCommand reader_poller_callback(NfcGenericEvent event, void* context) {
    UNUSED(event);
    TimeclockReader* reader = context;
    NfcCommand command = NfcCommandContinue;

    const NfcDeviceData* data = nfc_poller_get_data(reader->poller);
    NfcDevice* device = nfc_device_alloc();
    nfc_device_set_data(device, reader->protocol, data);

    size_t uid_len = 0;
    const uint8_t* uid = nfc_device_get_uid(device, &uid_len);
    if(uid && uid_len > 0) {
        reader_format_uid(reader, uid, uid_len, "NFC");
        view_dispatcher_send_custom_event(reader->vd, READER_EVENT_UID);
        command = NfcCommandStop;
    }
    nfc_device_free(device);
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

// ---- Start / stop helpers --------------------------------------------------
static void reader_start_nfc_scan(TimeclockReader* reader) {
    reader->nfc = nfc_alloc();
    reader->scanner = nfc_scanner_alloc(reader->nfc);
    nfc_scanner_start(reader->scanner, reader_scanner_callback, reader);
}

static void reader_stop_nfc(TimeclockReader* reader) {
    if(reader->poller) {
        nfc_poller_stop(reader->poller);
        nfc_poller_free(reader->poller);
        reader->poller = NULL;
    }
    if(reader->scanner) {
        nfc_scanner_stop(reader->scanner);
        nfc_scanner_free(reader->scanner);
        reader->scanner = NULL;
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

// ---- Public API ------------------------------------------------------------
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
        reader_start_nfc_scan(reader);
    }
}

void timeclock_reader_stop(TimeclockReader* reader) {
    furi_assert(reader);
    reader_stop_nfc(reader);
    reader_stop_lf(reader);
}

bool timeclock_reader_handle_event(TimeclockReader* reader, uint32_t event) {
    furi_assert(reader);

    if(event == READER_EVENT_DETECTED) {
        // A protocol was detected: stop the scanner and poll it for the UID.
        if(!reader->scanner) return true; // already transitioned
        nfc_scanner_stop(reader->scanner);
        nfc_scanner_free(reader->scanner);
        reader->scanner = NULL;
        reader->poller = nfc_poller_alloc(reader->nfc, reader->protocol);
        nfc_poller_start(reader->poller, reader_poller_callback, reader);
        return true;
    }

    if(event == READER_EVENT_UID) {
        if(reader->callback) {
            reader->callback(reader->uid, reader->tech, reader->context);
        }
        // Re-arm for the next card in continuous NFC mode. (LF keeps sensing on
        // its own; single-shot mode is stopped by the scene on exit.)
        if(reader->continuous && !reader->use_lf) {
            if(reader->poller) {
                nfc_poller_stop(reader->poller);
                nfc_poller_free(reader->poller);
                reader->poller = NULL;
            }
            reader_start_nfc_scan(reader);
        }
        return true;
    }

    return false;
}
