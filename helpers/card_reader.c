#include "card_reader.h"

#include <nfc/nfc.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <bit_buffer.h>

#define TAG "Warden"
#define POLL_MS 20u

struct CardReader {
    Nfc* nfc; // allocated for the lifetime of a scan run
    FuriThread* thread;
    FuriMutex* mutex;

    volatile bool stop;
    CardReaderState state;

    /* scanner -> worker handoff */
    bool detected;
    size_t scan_num;
    NfcProtocol scan_stack[NfcProtocolNum];

    /* poller -> worker handoff */
    NfcPoller* active_poller;
    bool poll_done;
    bool poll_ok;
    uint8_t tmp_uid[WARDEN_UID_MAX];
    size_t tmp_uid_len;
    uint8_t tmp_sak;
    uint8_t tmp_atqa[2];
    bool tmp_is_emv;

    CardReading result;
};

static void cr_lock(CardReader* cr) {
    furi_mutex_acquire(cr->mutex, FuriWaitForever);
}
static void cr_unlock(CardReader* cr) {
    furi_mutex_release(cr->mutex);
}

/* ------------------------------------------------------------- callbacks */

static void card_reader_scanner_cb(NfcScannerEvent event, void* context) {
    CardReader* cr = context;
    if(event.type != NfcScannerEventTypeDetected) return;

    cr_lock(cr);
    cr->scan_num = event.data.protocol_num;
    if(cr->scan_num > NfcProtocolNum) cr->scan_num = NfcProtocolNum;
    for(size_t i = 0; i < cr->scan_num; i++) {
        cr->scan_stack[i] = event.data.protocols[i];
    }
    cr->detected = true;
    cr_unlock(cr);
}

static NfcCommand card_reader_iso3a_cb(NfcGenericEvent event, void* context) {
    CardReader* cr = context;
    const Iso14443_3aPollerEvent* ev = event.event_data;
    NfcCommand cmd = NfcCommandContinue;

    if(ev->type == Iso14443_3aPollerEventTypeReady) {
        const Iso14443_3aData* data =
            (const Iso14443_3aData*)nfc_poller_get_data(cr->active_poller);

        size_t uid_len = 0;
        const uint8_t* uid = iso14443_3a_get_uid(data, &uid_len);
        uint8_t sak = iso14443_3a_get_sak(data);
        uint8_t atqa[2] = {0};
        iso14443_3a_get_atqa(data, atqa);

        cr_lock(cr);
        if(uid_len > WARDEN_UID_MAX) uid_len = WARDEN_UID_MAX;
        for(size_t i = 0; i < uid_len; i++) cr->tmp_uid[i] = uid[i];
        cr->tmp_uid_len = uid_len;
        cr->tmp_sak = sak;
        cr->tmp_atqa[0] = atqa[0];
        cr->tmp_atqa[1] = atqa[1];
        cr->poll_ok = true;
        cr->poll_done = true;
        cr_unlock(cr);
        cmd = NfcCommandStop;
    } else if(ev->type == Iso14443_3aPollerEventTypeError) {
        cr_lock(cr);
        cr->poll_ok = false;
        cr->poll_done = true;
        cr_unlock(cr);
        cmd = NfcCommandStop;
    }
    return cmd;
}

/* Ask an activated ISO-DEP card for its contactless payment directory
 * (SELECT PPSE, "2PAY.SYS.DDF01"). A card that answers 0x9000 is an EMV bank
 * card. We only SELECT the directory — we never read the PAN or any card data. */
static bool card_reader_probe_emv(Iso14443_4aPoller* poller) {
    static const uint8_t ppse_select[] = {
        0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53,
        0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00};

    BitBuffer* tx = bit_buffer_alloc(sizeof(ppse_select));
    BitBuffer* rx = bit_buffer_alloc(256);
    bit_buffer_copy_bytes(tx, ppse_select, sizeof(ppse_select));

    bool is_emv = false;
    Iso14443_4aError err = iso14443_4a_poller_send_block(poller, tx, rx);
    if(err == Iso14443_4aErrorNone) {
        size_t n = bit_buffer_get_size_bytes(rx);
        if(n >= 2) {
            uint8_t sw1 = bit_buffer_get_byte(rx, n - 2);
            uint8_t sw2 = bit_buffer_get_byte(rx, n - 1);
            is_emv = (sw1 == 0x90 && sw2 == 0x00); // FCI returned = payment card
        }
    }

    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return is_emv;
}

static NfcCommand card_reader_iso4a_cb(NfcGenericEvent event, void* context) {
    CardReader* cr = context;
    const Iso14443_4aPollerEvent* ev = event.event_data;
    NfcCommand cmd = NfcCommandContinue;

    if(ev->type == Iso14443_4aPollerEventTypeReady) {
        const Iso14443_4aData* data =
            (const Iso14443_4aData*)nfc_poller_get_data(cr->active_poller);
        const Iso14443_3aData* base = iso14443_4a_get_base_data(data);

        size_t uid_len = 0;
        const uint8_t* uid = iso14443_3a_get_uid(base, &uid_len);
        uint8_t sak = iso14443_3a_get_sak(base);
        uint8_t atqa[2] = {0};
        iso14443_3a_get_atqa(base, atqa);

        /* still inside the callback: legal to talk to the card */
        bool emv = card_reader_probe_emv((Iso14443_4aPoller*)event.instance);

        cr_lock(cr);
        if(uid_len > WARDEN_UID_MAX) uid_len = WARDEN_UID_MAX;
        for(size_t i = 0; i < uid_len; i++) cr->tmp_uid[i] = uid[i];
        cr->tmp_uid_len = uid_len;
        cr->tmp_sak = sak;
        cr->tmp_atqa[0] = atqa[0];
        cr->tmp_atqa[1] = atqa[1];
        cr->tmp_is_emv = emv;
        cr->poll_ok = true;
        cr->poll_done = true;
        cr_unlock(cr);
        cmd = NfcCommandStop;
    } else if(ev->type == Iso14443_4aPollerEventTypeError) {
        cr_lock(cr);
        cr->poll_ok = false;
        cr->poll_done = true;
        cr_unlock(cr);
        cmd = NfcCommandStop;
    }
    return cmd;
}

/* --------------------------------------------------------- stack analysis */

/* Deepest (most-derived) protocol in a detected stack — the real technology. */
static NfcProtocol pick_top(const NfcProtocol* stack, size_t num) {
    NfcProtocol top = stack[0];
    for(size_t i = 1; i < num; i++) {
        if(nfc_protocol_has_parent(stack[i], top)) top = stack[i];
    }
    return top;
}

/* Shallowest (root) protocol — the layer that carries the UID. */
static NfcProtocol pick_base(const NfcProtocol* stack, size_t num) {
    NfcProtocol base = stack[0];
    for(size_t i = 1; i < num; i++) {
        if(nfc_protocol_has_parent(base, stack[i])) base = stack[i];
    }
    return base;
}

/* ------------------------------------------------------------- worker */

/* Wait (polling) until `*flag` under the mutex is set, or stop is requested.
 * Returns true if the flag fired, false if we were asked to stop. */
static bool wait_flag(CardReader* cr, const bool* flag) {
    for(;;) {
        if(cr->stop) return false;
        cr_lock(cr);
        bool done = *flag;
        cr_unlock(cr);
        if(done) return true;
        furi_delay_ms(POLL_MS);
    }
}

static void read_iso3a_uid(CardReader* cr) {
    cr_lock(cr);
    cr->poll_done = false;
    cr->poll_ok = false;
    cr->tmp_is_emv = false;
    cr_unlock(cr);

    cr->active_poller = nfc_poller_alloc(cr->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(cr->active_poller, card_reader_iso3a_cb, cr);

    wait_flag(cr, &cr->poll_done);

    nfc_poller_stop(cr->active_poller);
    nfc_poller_free(cr->active_poller);
    cr->active_poller = NULL;
}

/* ISO-DEP (ISO14443-4A) read: UID/SAK/ATQA from the base layer + EMV probe.
 * Covers contactless bank cards, transit cards and ID smartcards. */
static void read_iso4a(CardReader* cr) {
    cr_lock(cr);
    cr->poll_done = false;
    cr->poll_ok = false;
    cr->tmp_is_emv = false;
    cr_unlock(cr);

    cr->active_poller = nfc_poller_alloc(cr->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(cr->active_poller, card_reader_iso4a_cb, cr);

    wait_flag(cr, &cr->poll_done);

    nfc_poller_stop(cr->active_poller);
    nfc_poller_free(cr->active_poller);
    cr->active_poller = NULL;
}

static int32_t card_reader_worker(void* context) {
    CardReader* cr = context;

    cr->nfc = nfc_alloc();

    /* --- 1. sweep for a card --- */
    cr_lock(cr);
    cr->detected = false;
    cr->state = CardReaderScanning;
    cr_unlock(cr);

    NfcScanner* scanner = nfc_scanner_alloc(cr->nfc);
    nfc_scanner_start(scanner, card_reader_scanner_cb, cr);
    bool got = wait_flag(cr, &cr->detected);
    nfc_scanner_stop(scanner);
    nfc_scanner_free(scanner);

    if(!got) { // stop requested before a card appeared
        nfc_free(cr->nfc);
        cr->nfc = NULL;
        return 0;
    }

    /* --- 2. resolve the stack --- */
    cr_lock(cr);
    size_t num = cr->scan_num;
    NfcProtocol stack[NfcProtocolNum];
    for(size_t i = 0; i < num; i++) stack[i] = cr->scan_stack[i];
    cr_unlock(cr);

    CardReading r;
    memset(&r, 0, sizeof(r));
    r.protocol_num = num;
    for(size_t i = 0; i < num; i++) r.stack[i] = stack[i];
    r.top = pick_top(stack, num);
    r.base = pick_base(stack, num);

    /* --- 3. pull the UID/SAK on the A family (covers Classic, DESFire,
     *        Ultralight/NTAG, Plus, ISO-DEP-A: the bulk of access badges).
     *        ISO-DEP (bank/transit/ID) gets the 4A path so we can EMV-probe. */
    bool did_read = false;
    if(r.top == NfcProtocolIso14443_4a) {
        read_iso4a(cr);
        did_read = true;
    } else if(r.base == NfcProtocolIso14443_3a) {
        read_iso3a_uid(cr);
        did_read = true;
    }
    if(did_read) {
        cr_lock(cr);
        if(cr->poll_ok) {
            r.has_iso3a = true;
            r.uid_len = cr->tmp_uid_len;
            for(size_t i = 0; i < r.uid_len; i++) r.uid[i] = cr->tmp_uid[i];
            r.sak = cr->tmp_sak;
            r.atqa[0] = cr->tmp_atqa[0];
            r.atqa[1] = cr->tmp_atqa[1];
            r.is_emv = cr->tmp_is_emv;
        }
        cr_unlock(cr);
    }

    /* --- 4. publish --- */
    cr_lock(cr);
    cr->result = r;
    cr->state = CardReaderReady;
    cr_unlock(cr);

    nfc_free(cr->nfc);
    cr->nfc = NULL;
    return 0;
}

/* -------------------------------------------------------------- lifecycle */

CardReader* card_reader_alloc(void) {
    CardReader* cr = malloc(sizeof(CardReader));
    memset(cr, 0, sizeof(CardReader));
    cr->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    cr->state = CardReaderIdle;
    return cr;
}

void card_reader_free(CardReader* cr) {
    furi_assert(cr);
    card_reader_stop(cr);
    furi_mutex_free(cr->mutex);
    free(cr);
}

void card_reader_start(CardReader* cr) {
    furi_assert(cr);
    card_reader_stop(cr); // join any prior run

    cr->stop = false;
    cr_lock(cr);
    cr->state = CardReaderScanning;
    cr->detected = false;
    cr->poll_done = false;
    cr_unlock(cr);

    cr->thread =
        furi_thread_alloc_ex("WardenReader", 4 * 1024, card_reader_worker, cr);
    furi_thread_start(cr->thread);
}

void card_reader_stop(CardReader* cr) {
    furi_assert(cr);
    cr->stop = true;
    if(cr->thread) {
        furi_thread_join(cr->thread);
        furi_thread_free(cr->thread);
        cr->thread = NULL;
    }
    cr_lock(cr);
    if(cr->state != CardReaderReady) cr->state = CardReaderIdle;
    cr_unlock(cr);
}

CardReaderState card_reader_state(CardReader* cr) {
    furi_assert(cr);
    cr_lock(cr);
    CardReaderState s = cr->state;
    cr_unlock(cr);
    return s;
}

bool card_reader_get(CardReader* cr, CardReading* out) {
    furi_assert(cr);
    furi_assert(out);
    cr_lock(cr);
    bool ready = (cr->state == CardReaderReady);
    if(ready) *out = cr->result;
    cr_unlock(cr);
    return ready;
}
