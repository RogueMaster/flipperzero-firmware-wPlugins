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
    bool ok = (err == Iso14443_3aErrorNone) && (bit_buffer_get_size_bytes(rx) >= 8);
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
    bool p1_ok = (e1 == Iso14443_3aErrorNone) && (bit_buffer_get_size(rx) >= 4);

    // Packet 2: <sector> 00 00 00 (+CRC) -> PASSIVE ACK (no reply). Timeout == OK.
    const uint8_t p2[4] = {sector, 0x00, 0x00, 0x00};
    Iso14443_3aError e2 = bv_exchange(poller, p2, sizeof(p2), rx, FWT_SECTOR_ACK, true);
    bool p2_ok = (e2 == Iso14443_3aErrorTimeout) || (e2 == Iso14443_3aErrorNone);

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
    bool ok = (err == Iso14443_3aErrorNone) && (bit_buffer_get_size_bytes(rx) >= 16);
    if(ok) bit_buffer_write_bytes(rx, out, 16);
    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return ok;
}

// Full read sequence, executed once the card is activated (Ready event).
static void bv_do_read(BioVault* app, Iso14443_3aPoller* poller) {
    BvError err = BvErrNone;
    uint8_t version[8] = {0};
    bool version_ok = bv_get_version(poller, version);
    bool version_match =
        version_ok && (memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) == 0);

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
                break;
            }
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
}

// Extended-mode poller callback — called from the NFC worker thread.
static NfcCommand bv_poller_callback(NfcGenericEventEx event, void* context) {
    BioVault* app = context;
    Iso14443_3aPoller* poller = event.poller;
    const Iso14443_3aPollerEvent* iso_event = event.parent_event_data;
    NfcCommand cmd = NfcCommandContinue;

    if(iso_event->type == Iso14443_3aPollerEventTypeReady) {
        bv_do_read(app, poller);
        cmd = NfcCommandStop;
    } else if(iso_event->type == Iso14443_3aPollerEventTypeError) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->error = BvErrNoTag;
        app->state = BvStateError;
        furi_mutex_release(app->mutex);
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
        if(event.type != InputTypeShort && event.type != InputTypeRepeat) continue;

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
                // let the worker finish/stop naturally; ignore back while reading
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
