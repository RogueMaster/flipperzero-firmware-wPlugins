/*
 * BioVault — Flipper Zero FAP
 *
 * Milestone 1: prove the NFC path against an NTAG I2C Plus 2K (xSIID).
 *   GET_VERSION -> SECTOR SELECT 1 -> READ Sector 1 user memory -> show hex.
 *
 * This is the port of the Proxmark3 `hf_i2c_plus_2k_utils.lua` read path to the
 * Flipper's ISO14443-3A poller. Crypto (enclave-wrapped key + AES-GCM) and the
 * write/provision paths land in later milestones — see README.
 *
 * NOTE (needs on-hardware tuning): the two-part SECTOR SELECT handshake is the
 * one spot that may need timing/return-code adjustment on a real tag. Packet 1
 * (C2 FF) returns a 4-bit ACK; packet 2 (<sector> 00 00 00) returns a PASSIVE
 * ACK — i.e. no response — so a timeout there is SUCCESS, not failure.
 */

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/helpers/iso14443_crc.h>

#define TAG "BioVault"

// --- NTAG I2C Plus 2K command set (from the Lua utils script) ---
#define CMD_GET_VERSION 0x60
#define CMD_READ 0x30
#define CMD_SECTOR_SELECT_1 0xC2
#define CMD_SECTOR_SELECT_2 0xFF

// GET_VERSION reply for NTAG I2C Plus 2K, minus the 2 trailing CRC bytes
// (the poller trims CRC). Full on-air value is 0004040502021503C859.
static const uint8_t NTAG_I2C_PLUS_2K_VERSION[] =
    {0x00, 0x04, 0x04, 0x05, 0x02, 0x02, 0x15, 0x03};

#define SECTOR1 0x01
#define SECTOR1_PAGES 256u // Sector 1 user memory: pages 00h..FFh
#define SECTOR1_BYTES (SECTOR1_PAGES * 4u) // 1024 bytes
#define READ_PAGES_PER_CMD 4u // READ (0x30) returns 4 pages (16 bytes)
#define READ_CMD_COUNT (SECTOR1_PAGES / READ_PAGES_PER_CMD) // 64 reads

// Frame wait times (carrier cycles). ~1 cycle = 1/13.56MHz.
#define FWT_NORMAL 60000u // ~4.4 ms, generous for READ/GET_VERSION
#define FWT_SECTOR_ACK 20000u // shorter wait for the passive-ACK packet 2

typedef enum {
    BvStateIdle,
    BvStateReading,
    BvStateDone,
    BvStateError,
} BvState;

typedef enum {
    BvErrNone,
    BvErrNoTag,
    BvErrWrongTag,
    BvErrSectorSelect,
    BvErrRead,
} BvError;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    FuriMutex* mutex;

    Nfc* nfc;
    NfcPoller* poller;

    BvState state;
    BvError error;
    bool poller_running;
    bool worker_done; // set by poller callback, consumed by main loop for cleanup

    uint8_t version[8];
    bool version_valid;

    uint8_t sector1[SECTOR1_BYTES];
    size_t sector1_len;
    uint16_t view_offset; // byte offset shown on screen (page-aligned)
} BioVault;

// --- Low-level ISO14443-3A exchange helpers (run only inside poller callback) ---

// Send `tx` (optionally CRC-appended) and capture the response. Returns the
// poller error so callers can distinguish timeout (expected for passive-ACK).
static Iso14443_3aError bv_exchange(
    Iso14443_3aPoller* poller,
    const uint8_t* tx,
    size_t tx_len,
    BitBuffer* rx,
    uint32_t fwt,
    bool append_crc) {
    BitBuffer* txb = bit_buffer_alloc(tx_len + 2);
    bit_buffer_copy_bytes(txb, tx, tx_len);
    if(append_crc) iso14443_crc_append(Iso14443CrcTypeA, txb);
    Iso14443_3aError err = iso14443_3a_poller_txrx(poller, txb, rx, fwt);
    bit_buffer_free(txb);
    return err;
}

// GET_VERSION — CRC-checked standard frame; on success rx holds 8 version bytes.
static bool bv_get_version(Iso14443_3aPoller* poller, uint8_t out[8]) {
    BitBuffer* tx = bit_buffer_alloc(2);
    BitBuffer* rx = bit_buffer_alloc(16);
    bit_buffer_append_byte(tx, CMD_GET_VERSION);
    Iso14443_3aError err = iso14443_3a_poller_send_standard_frame(poller, tx, rx, FWT_NORMAL);
    size_t sz = bit_buffer_get_size_bytes(rx);
    FURI_LOG_D(TAG, "GET_VERSION txrx: err=%d rx_bytes=%u", err, (unsigned)sz);
    bool ok = (err == Iso14443_3aErrorNone) && (sz >= 8);
    if(ok) bit_buffer_write_bytes(rx, out, 8);
    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return ok;
}

// Two-part SECTOR SELECT. See NOTE at top of file re: passive ACK / timeout.
static bool bv_select_sector(Iso14443_3aPoller* poller, uint8_t sector) {
    BitBuffer* rx = bit_buffer_alloc(16);

    // Packet 1: C2 FF (+CRC) -> tag returns a 4-bit ACK (0xA).
    const uint8_t p1[2] = {CMD_SECTOR_SELECT_1, CMD_SECTOR_SELECT_2};
    Iso14443_3aError e1 = bv_exchange(poller, p1, sizeof(p1), rx, FWT_NORMAL, true);
    size_t p1_bits = bit_buffer_get_size(rx);
    uint8_t p1_b0 = (p1_bits >= 4) ? bit_buffer_get_byte(rx, 0) : 0xFF;
    bool p1_ok = (e1 == Iso14443_3aErrorNone) && (p1_bits >= 4);
    FURI_LOG_D(
        TAG, "sector-select p1: err=%d rx_bits=%u b0=0x%02X", e1, (unsigned)p1_bits, p1_b0);

    // Packet 2: <sector> 00 00 00 (+CRC) -> PASSIVE ACK (no reply). Timeout == OK.
    bit_buffer_reset(rx);
    const uint8_t p2[4] = {sector, 0x00, 0x00, 0x00};
    Iso14443_3aError e2 = bv_exchange(poller, p2, sizeof(p2), rx, FWT_SECTOR_ACK, true);
    bool p2_ok = (e2 == Iso14443_3aErrorTimeout) || (e2 == Iso14443_3aErrorNone);
    FURI_LOG_D(
        TAG, "sector-select p2: err=%d rx_bits=%u (timeout=OK)", e2, (unsigned)bit_buffer_get_size(rx));

    bit_buffer_free(rx);
    return p1_ok && p2_ok;
}

// READ (0x30) one 16-byte chunk (4 pages) starting at `page`.
static bool bv_read_pages(Iso14443_3aPoller* poller, uint8_t page, uint8_t out[16]) {
    BitBuffer* tx = bit_buffer_alloc(2);
    BitBuffer* rx = bit_buffer_alloc(32);
    bit_buffer_append_byte(tx, CMD_READ);
    bit_buffer_append_byte(tx, page);
    Iso14443_3aError err = iso14443_3a_poller_send_standard_frame(poller, tx, rx, FWT_NORMAL);
    size_t sz = bit_buffer_get_size_bytes(rx);
    bool ok = (err == Iso14443_3aErrorNone) && (sz >= 16);
    if(page == 0x00 || !ok)
        FURI_LOG_D(TAG, "READ page 0x%02X: err=%d rx_bytes=%u ok=%d", page, err, (unsigned)sz, ok);
    if(ok) bit_buffer_write_bytes(rx, out, 16);
    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return ok;
}

// Full read sequence, executed once a card is present (PollerReady event).
static void bv_do_read(BioVault* app, Iso14443_3aPoller* poller) {
    BvError err = BvErrNone;

    // NfcEventTypePollerReady only means a card answered the initial request; it
    // is not yet SELECTed. Run anti-collision + SELECT to bring it to ACTIVE
    // state, otherwise it ignores READ/GET_VERSION and every command times out.
    Iso14443_3aData* iso_data = iso14443_3a_alloc();
    Iso14443_3aError act = iso14443_3a_poller_activate(poller, iso_data);
    size_t uid_len = 0;
    const uint8_t* uid = (act == Iso14443_3aErrorNone) ? iso14443_3a_get_uid(iso_data, &uid_len) : NULL;
    FURI_LOG_I(
        TAG,
        "activate: err=%d uid_len=%u uid=%02X%02X%02X%02X%02X%02X%02X",
        act,
        (unsigned)uid_len,
        uid && uid_len > 0 ? uid[0] : 0,
        uid && uid_len > 1 ? uid[1] : 0,
        uid && uid_len > 2 ? uid[2] : 0,
        uid && uid_len > 3 ? uid[3] : 0,
        uid && uid_len > 4 ? uid[4] : 0,
        uid && uid_len > 5 ? uid[5] : 0,
        uid && uid_len > 6 ? uid[6] : 0);
    if(act != Iso14443_3aErrorNone) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->error = BvErrNoTag;
        app->state = BvStateError;
        furi_mutex_release(app->mutex);
        iso14443_3a_free(iso_data);
        return;
    }

    uint8_t version[8] = {0};
    bool version_ok = bv_get_version(poller, version);
    bool version_match =
        version_ok && (memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) == 0);
    FURI_LOG_I(
        TAG,
        "GET_VERSION ok=%d match=%d: %02X%02X%02X%02X%02X%02X%02X%02X",
        version_ok,
        version_match,
        version[0],
        version[1],
        version[2],
        version[3],
        version[4],
        version[5],
        version[6],
        version[7]);

    // Cross-check: a plain READ of page 0 (works on any activated Type 2 tag,
    // returns UID + manufacturer bytes). Tells us if ANY command works even
    // when GET_VERSION does not.
    uint8_t probe[16] = {0};
    bool read0_ok = bv_read_pages(poller, 0x00, probe);
    FURI_LOG_I(
        TAG,
        "probe READ0 ok=%d: %02X %02X %02X %02X %02X %02X %02X %02X",
        read0_ok,
        probe[0],
        probe[1],
        probe[2],
        probe[3],
        probe[4],
        probe[5],
        probe[6],
        probe[7]);

    uint8_t buf[SECTOR1_BYTES];
    size_t got = 0;

    if(!version_ok) {
        err = BvErrNoTag;
    } else if(!version_match) {
        err = BvErrWrongTag; // wrong version but still report it on screen
    } else if(!bv_select_sector(poller, SECTOR1)) {
        err = BvErrSectorSelect;
    } else {
        for(uint32_t i = 0; i < READ_CMD_COUNT; i++) {
            uint8_t page = (uint8_t)(i * READ_PAGES_PER_CMD);
            if(!bv_read_pages(poller, page, buf + got)) {
                err = BvErrRead;
                FURI_LOG_I(TAG, "READ failed at page 0x%02X (got %u bytes)", page, (unsigned)got);
                break;
            }
            // Verify SECTOR SELECT actually switched: Sector 1 page 0 must NOT be
            // the UID we read from Sector 0 (04 78 A5 D2 ...).
            if(i == 0)
                FURI_LOG_I(
                    TAG,
                    "sector1 p0: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
                    buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
            got += 16;
        }
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    memcpy(app->version, version, sizeof(version));
    app->version_valid = version_ok;
    if(err == BvErrNone) {
        memcpy(app->sector1, buf, got);
        app->sector1_len = got;
        app->view_offset = 0;
        app->state = BvStateDone;
    } else {
        app->error = err;
        app->state = BvStateError;
    }
    furi_mutex_release(app->mutex);
    FURI_LOG_I(TAG, "read complete: err=%d bytes=%u", err, (unsigned)got);
    iso14443_3a_free(iso_data);
}

// Extended-mode poller callback — called from the NFC worker thread.
//
// The iso14443_3a base protocol has no parent protocol, so start_ex delivers
// raw NfcEvents from the Nfc instance. The card-activated event is
// NfcEventTypePollerReady (NOT Iso14443_3aPollerEventTypeReady, which the
// iso14443_3a poller only emits to child protocol pollers such as MfUltralight).
static NfcCommand bv_poller_callback(NfcGenericEventEx event, void* context) {
    BioVault* app = context;
    Iso14443_3aPoller* poller = event.poller;
    const NfcEvent* nfc_event = event.parent_event_data;
    NfcCommand cmd = NfcCommandContinue;

    if(nfc_event->type == NfcEventTypePollerReady) {
        bv_do_read(app, poller);
        cmd = NfcCommandStop;
    }

    if(cmd == NfcCommandStop) app->worker_done = true;
    view_port_update(app->view_port);
    return cmd;
}

static void bv_start_read(BioVault* app) {
    if(app->poller_running) return;
    app->state = BvStateReading;
    app->error = BvErrNone;
    app->worker_done = false;
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start_ex(app->poller, bv_poller_callback, app);
    app->poller_running = true;
}

// Tear down the poller (must run on the main thread, not the callback).
static void bv_stop_read(BioVault* app) {
    if(!app->poller_running) return;
    nfc_poller_stop(app->poller);
    nfc_poller_free(app->poller);
    app->poller = NULL;
    app->poller_running = false;
}

// --- GUI ---

static const char* bv_error_text(BvError e) {
    switch(e) {
    case BvErrNoTag:
        return "No tag / no response";
    case BvErrWrongTag:
        return "Not an NTAG I2C 2K";
    case BvErrSectorSelect:
        return "Sector select failed";
    case BvErrRead:
        return "Read failed";
    default:
        return "Unknown error";
    }
}

static void bv_draw_callback(Canvas* canvas, void* context) {
    BioVault* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "BioVault");
    canvas_set_font(canvas, FontSecondary);

    char line[40];
    switch(app->state) {
    case BvStateIdle:
        canvas_draw_str(canvas, 2, 26, "Sector 1 reader");
        canvas_draw_str(canvas, 2, 40, "OK: read implant");
        canvas_draw_str(canvas, 2, 52, "Back: exit");
        break;
    case BvStateReading:
        canvas_draw_str(canvas, 2, 30, "Hold to implant...");
        canvas_draw_str(canvas, 2, 44, "Reading Sector 1");
        break;
    case BvStateError:
        canvas_draw_str(canvas, 2, 26, bv_error_text(app->error));
        if(app->version_valid) {
            snprintf(
                line,
                sizeof(line),
                "ver %02X%02X%02X%02X%02X%02X%02X%02X",
                app->version[0],
                app->version[1],
                app->version[2],
                app->version[3],
                app->version[4],
                app->version[5],
                app->version[6],
                app->version[7]);
            canvas_draw_str(canvas, 2, 40, line);
        }
        canvas_draw_str(canvas, 2, 52, "OK: retry  Back: exit");
        break;
    case BvStateDone: {
        snprintf(line, sizeof(line), "Read %u bytes", (unsigned)app->sector1_len);
        canvas_draw_str(canvas, 2, 22, line);
        // Show 3 rows of 8 bytes from view_offset.
        for(uint32_t row = 0; row < 3; row++) {
            uint32_t off = app->view_offset + row * 8;
            if(off >= app->sector1_len) break;
            int n = snprintf(line, sizeof(line), "%03lX:", (unsigned long)off);
            for(uint32_t b = 0; b < 8 && (off + b) < app->sector1_len; b++) {
                n += snprintf(line + n, sizeof(line) - n, "%02X", app->sector1[off + b]);
            }
            canvas_draw_str(canvas, 2, 34 + row * 10, line);
        }
        break;
    }
    }

    furi_mutex_release(app->mutex);
}

static void bv_input_callback(InputEvent* input_event, void* context) {
    BioVault* app = context;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

static BioVault* bv_alloc(void) {
    BioVault* app = malloc(sizeof(BioVault));
    memset(app, 0, sizeof(BioVault));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->nfc = nfc_alloc();
    app->state = BvStateIdle;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, bv_draw_callback, app);
    view_port_input_callback_set(app->view_port, bv_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    return app;
}

static void bv_free(BioVault* app) {
    bv_stop_read(app);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    nfc_free(app->nfc);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t biovault_app(void* p) {
    UNUSED(p);
    BioVault* app = bv_alloc();

    bool running = true;
    InputEvent event;
    while(running) {
        // Reap a finished poller before handling more input.
        if(app->worker_done && app->poller_running) {
            bv_stop_read(app);
            app->worker_done = false;
        }

        if(furi_message_queue_get(app->input_queue, &event, 100) != FuriStatusOk) {
            continue;
        }
        if(event.type != InputTypeShort) continue; // ignore Repeat to avoid restart storm

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        BvState state = app->state;
        furi_mutex_release(app->mutex);

        switch(event.key) {
        case InputKeyOk:
            if(state == BvStateIdle || state == BvStateError || state == BvStateDone) {
                bv_start_read(app);
            }
            break;
        case InputKeyBack:
            if(state == BvStateReading) {
                // Cancel an in-progress read (e.g. no tag presented).
                bv_stop_read(app);
                app->worker_done = false;
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->state = BvStateIdle;
                furi_mutex_release(app->mutex);
            } else {
                running = false;
            }
            break;
        case InputKeyDown:
            if(state == BvStateDone) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                if((size_t)app->view_offset + 24 < app->sector1_len) app->view_offset += 8;
                furi_mutex_release(app->mutex);
            }
            break;
        case InputKeyUp:
            if(state == BvStateDone) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                if(app->view_offset >= 8) app->view_offset -= 8;
                furi_mutex_release(app->mutex);
            }
            break;
        default:
            break;
        }
        view_port_update(app->view_port);
    }

    bv_free(app);
    return 0;
}
