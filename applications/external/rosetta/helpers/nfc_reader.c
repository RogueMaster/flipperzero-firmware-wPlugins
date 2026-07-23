#include "nfc_reader.h"

#include <furi.h>
#include <string.h>
#include <nfc/nfc.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#define POLL_MS 20u

struct NfcReader {
    Nfc* nfc; // allocated for the lifetime of a scan run
    FuriThread* thread;
    FuriMutex* mutex;

    volatile bool stop;
    NfcReaderState state;

    /* scanner -> worker handoff */
    bool detected;
    size_t scan_num;
    NfcProtocol scan_stack[NfcProtocolNum];

    /* poller -> worker handoff */
    NfcPoller* active_poller;
    bool poll_done;
    bool poll_ok;
    uint8_t tmp_uid[NFC_READER_UID_MAX];
    size_t tmp_uid_len;
    uint8_t tmp_sak;
    uint8_t tmp_atqa[2];

    NfcReading result;
};

static void lock(NfcReader* r) {
    furi_mutex_acquire(r->mutex, FuriWaitForever);
}
static void unlock(NfcReader* r) {
    furi_mutex_release(r->mutex);
}

/* ------------------------------------------------------------- callbacks */

static void scanner_cb(NfcScannerEvent event, void* context) {
    NfcReader* r = context;
    if(event.type != NfcScannerEventTypeDetected) return;

    lock(r);
    r->scan_num = event.data.protocol_num;
    if(r->scan_num > NfcProtocolNum) r->scan_num = NfcProtocolNum;
    for(size_t i = 0; i < r->scan_num; i++) {
        r->scan_stack[i] = event.data.protocols[i];
    }
    r->detected = true;
    unlock(r);
}

static NfcCommand iso3a_cb(NfcGenericEvent event, void* context) {
    NfcReader* r = context;
    const Iso14443_3aPollerEvent* ev = event.event_data;
    NfcCommand cmd = NfcCommandContinue;

    if(ev->type == Iso14443_3aPollerEventTypeReady) {
        const Iso14443_3aData* data =
            (const Iso14443_3aData*)nfc_poller_get_data(r->active_poller);

        size_t uid_len = 0;
        const uint8_t* uid = iso14443_3a_get_uid(data, &uid_len);
        uint8_t sak = iso14443_3a_get_sak(data);
        uint8_t atqa[2] = {0};
        iso14443_3a_get_atqa(data, atqa);

        lock(r);
        if(uid_len > NFC_READER_UID_MAX) uid_len = NFC_READER_UID_MAX;
        for(size_t i = 0; i < uid_len; i++)
            r->tmp_uid[i] = uid[i];
        r->tmp_uid_len = uid_len;
        r->tmp_sak = sak;
        r->tmp_atqa[0] = atqa[0];
        r->tmp_atqa[1] = atqa[1];
        r->poll_ok = true;
        r->poll_done = true;
        unlock(r);
        cmd = NfcCommandStop;
    } else if(ev->type == Iso14443_3aPollerEventTypeError) {
        lock(r);
        r->poll_ok = false;
        r->poll_done = true;
        unlock(r);
        cmd = NfcCommandStop;
    }
    return cmd;
}

/* --------------------------------------------------------- stack analysis */

/* Deepest (most-derived) protocol in a detected stack: the real technology. */
static NfcProtocol pick_top(const NfcProtocol* stack, size_t num) {
    NfcProtocol top = stack[0];
    for(size_t i = 1; i < num; i++) {
        if(nfc_protocol_has_parent(stack[i], top)) top = stack[i];
    }
    return top;
}

/* Shallowest (root) protocol: the layer that carries the UID. */
static NfcProtocol pick_base(const NfcProtocol* stack, size_t num) {
    NfcProtocol base = stack[0];
    for(size_t i = 1; i < num; i++) {
        if(nfc_protocol_has_parent(base, stack[i])) base = stack[i];
    }
    return base;
}

/* ------------------------------------------------------------- worker */

static bool wait_flag(NfcReader* r, const bool* flag) {
    for(;;) {
        if(r->stop) return false;
        lock(r);
        bool done = *flag;
        unlock(r);
        if(done) return true;
        furi_delay_ms(POLL_MS);
    }
}

static void read_iso3a_uid(NfcReader* r) {
    lock(r);
    r->poll_done = false;
    r->poll_ok = false;
    unlock(r);

    r->active_poller = nfc_poller_alloc(r->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(r->active_poller, iso3a_cb, r);

    wait_flag(r, &r->poll_done);

    nfc_poller_stop(r->active_poller);
    nfc_poller_free(r->active_poller);
    r->active_poller = NULL;
}

static int32_t nfc_reader_worker(void* context) {
    NfcReader* r = context;

    r->nfc = nfc_alloc();

    lock(r);
    r->detected = false;
    r->state = NfcReaderScanning;
    unlock(r);

    NfcScanner* scanner = nfc_scanner_alloc(r->nfc);
    nfc_scanner_start(scanner, scanner_cb, r);
    bool got = wait_flag(r, &r->detected);
    nfc_scanner_stop(scanner);
    nfc_scanner_free(scanner);

    if(!got) { // stop requested before a card appeared
        nfc_free(r->nfc);
        r->nfc = NULL;
        return 0;
    }

    lock(r);
    size_t num = r->scan_num;
    NfcProtocol stack[NfcProtocolNum];
    for(size_t i = 0; i < num; i++)
        stack[i] = r->scan_stack[i];
    unlock(r);

    NfcReading res;
    memset(&res, 0, sizeof(res));
    NfcProtocol top = num ? pick_top(stack, num) : NfcProtocolInvalid;
    NfcProtocol base = num ? pick_base(stack, num) : NfcProtocolInvalid;

    const char* tech = nfc_device_get_protocol_name(top);
    if(tech) strncpy(res.tech, tech, sizeof(res.tech) - 1);

    if(base == NfcProtocolIso14443_3a) {
        read_iso3a_uid(r);
        lock(r);
        if(r->poll_ok) {
            res.has_iso3a = true;
            res.uid_len = r->tmp_uid_len;
            for(size_t i = 0; i < res.uid_len; i++)
                res.uid[i] = r->tmp_uid[i];
            res.sak = r->tmp_sak;
            res.atqa[0] = r->tmp_atqa[0];
            res.atqa[1] = r->tmp_atqa[1];
        }
        unlock(r);
    }

    lock(r);
    r->result = res;
    r->state = NfcReaderReady;
    unlock(r);

    nfc_free(r->nfc);
    r->nfc = NULL;
    return 0;
}

/* -------------------------------------------------------------- lifecycle */

NfcReader* nfc_reader_alloc(void) {
    NfcReader* r = malloc(sizeof(NfcReader));
    memset(r, 0, sizeof(NfcReader));
    r->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    r->state = NfcReaderIdle;
    return r;
}

void nfc_reader_free(NfcReader* r) {
    furi_assert(r);
    nfc_reader_stop(r);
    furi_mutex_free(r->mutex);
    free(r);
}

void nfc_reader_start(NfcReader* r) {
    furi_assert(r);
    nfc_reader_stop(r); // join any prior run

    r->stop = false;
    lock(r);
    r->state = NfcReaderScanning;
    r->detected = false;
    r->poll_done = false;
    unlock(r);

    r->thread = furi_thread_alloc_ex("RosettaNfc", 4 * 1024, nfc_reader_worker, r);
    furi_thread_start(r->thread);
}

void nfc_reader_stop(NfcReader* r) {
    furi_assert(r);
    r->stop = true;
    if(r->thread) {
        furi_thread_join(r->thread);
        furi_thread_free(r->thread);
        r->thread = NULL;
    }
    lock(r);
    if(r->state != NfcReaderReady) r->state = NfcReaderIdle;
    unlock(r);
}

NfcReaderState nfc_reader_state(NfcReader* r) {
    furi_assert(r);
    lock(r);
    NfcReaderState s = r->state;
    unlock(r);
    return s;
}

bool nfc_reader_get(NfcReader* r, NfcReading* out) {
    furi_assert(r);
    furi_assert(out);
    lock(r);
    bool ready = (r->state == NfcReaderReady);
    if(ready) *out = r->result;
    unlock(r);
    return ready;
}
