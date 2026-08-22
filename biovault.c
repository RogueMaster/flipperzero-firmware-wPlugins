/*
 * BioVault — Flipper Zero FAP
 *
 * Menu-driven app (ViewDispatcher): Read Implant / Diagnostics. Vault browser,
 * Settings/PIN, and the write/provision paths land in later milestones.
 *
 * NFC read path (verified on hardware): activate (anti-collision + SELECT) ->
 * GET_VERSION -> two-part SECTOR SELECT 1 -> READ Sector 1 user memory.
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <input/input.h>

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/helpers/iso14443_crc.h>

#include "bv_crypto.h"
#include "bv_vault.h"
#include "bv_records.h"

#define TAG "BioVault"

// --- NTAG I2C Plus 2K command set ---
#define CMD_GET_VERSION 0x60
#define CMD_READ 0x30
#define CMD_SECTOR_SELECT_1 0xC2
#define CMD_SECTOR_SELECT_2 0xFF

static const uint8_t NTAG_I2C_PLUS_2K_VERSION[] =
    {0x00, 0x04, 0x04, 0x05, 0x02, 0x02, 0x15, 0x03};

#define SECTOR1 0x01
#define SECTOR1_PAGES 256u
#define SECTOR1_BYTES (SECTOR1_PAGES * 4u) // 1024
#define READ_PAGES_PER_CMD 4u
#define READ_CMD_COUNT (SECTOR1_PAGES / READ_PAGES_PER_CMD) // 64

#define FWT_NORMAL 60000u
#define FWT_SECTOR_ACK 20000u

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

// View ids and menu indices.
typedef enum {
    BvViewMenu,
    BvViewRead,
    BvViewDiag,
} BvViewId;

typedef enum {
    BvMenuRead,
    BvMenuDiag,
} BvMenuIndex;

typedef enum {
    BvCustomEventReadFinished = 1,
} BvCustomEvent;

// Read view model (heap-allocated by the View).
typedef struct {
    BvState state;
    BvError error;
    uint8_t version[8];
    bool version_valid;
    uint8_t sector1[SECTOR1_BYTES];
    size_t sector1_len;
    uint16_t view_offset;
} BvReadModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* menu;
    View* read_view;
    Widget* diag;

    Nfc* nfc;
    NfcPoller* poller;
    bool poller_running;

    bool enclave_ok;
    bool gcm_ok;
    bool dek_ok;
    bool vault_ok;
    bool records_ok;
} BioVault;

// --- ISO14443-3A exchange helpers (only inside the poller callback) ---

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

// Two-part SECTOR SELECT. Packet 1 (C2 FF) -> 4-bit ACK; packet 2
// (<sector> 00 00 00) is a passive ACK, so a poller timeout means success.
static bool bv_select_sector(Iso14443_3aPoller* poller, uint8_t sector) {
    BitBuffer* rx = bit_buffer_alloc(16);
    const uint8_t p1[2] = {CMD_SECTOR_SELECT_1, CMD_SECTOR_SELECT_2};
    Iso14443_3aError e1 = bv_exchange(poller, p1, sizeof(p1), rx, FWT_NORMAL, true);
    bool p1_ok = (e1 == Iso14443_3aErrorNone) && (bit_buffer_get_size(rx) >= 4);

    bit_buffer_reset(rx);
    const uint8_t p2[4] = {sector, 0x00, 0x00, 0x00};
    Iso14443_3aError e2 = bv_exchange(poller, p2, sizeof(p2), rx, FWT_SECTOR_ACK, true);
    bool p2_ok = (e2 == Iso14443_3aErrorTimeout) || (e2 == Iso14443_3aErrorNone);

    bit_buffer_free(rx);
    return p1_ok && p2_ok;
}

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

// Full read sequence (runs on the NFC worker thread). Commits into the read model.
static void bv_do_read(BioVault* app, Iso14443_3aPoller* poller) {
    BvError err = BvErrNone;

    Iso14443_3aData* iso_data = iso14443_3a_alloc();
    Iso14443_3aError act = iso14443_3a_poller_activate(poller, iso_data);
    iso14443_3a_free(iso_data);
    if(act != Iso14443_3aErrorNone) {
        with_view_model(
            app->read_view,
            BvReadModel * m,
            {
                m->error = BvErrNoTag;
                m->state = BvStateError;
            },
            true);
        return;
    }

    uint8_t version[8] = {0};
    bool version_ok = bv_get_version(poller, version);
    bool version_match =
        version_ok && (memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) == 0);

    uint8_t buf[SECTOR1_BYTES];
    size_t got = 0;

    if(!version_ok) {
        err = BvErrNoTag;
    } else if(!version_match) {
        err = BvErrWrongTag;
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

    with_view_model(
        app->read_view,
        BvReadModel * m,
        {
            memcpy(m->version, version, sizeof(version));
            m->version_valid = version_ok;
            if(err == BvErrNone) {
                memcpy(m->sector1, buf, got);
                m->sector1_len = got;
                m->view_offset = 0;
                m->state = BvStateDone;
            } else {
                m->error = err;
                m->state = BvStateError;
            }
        },
        true);
    FURI_LOG_I(TAG, "read complete: err=%d bytes=%u", err, (unsigned)got);
}

static NfcCommand bv_poller_callback(NfcGenericEventEx event, void* context) {
    BioVault* app = context;
    Iso14443_3aPoller* poller = event.poller;
    const NfcEvent* nfc_event = event.parent_event_data;
    NfcCommand cmd = NfcCommandContinue;

    if(nfc_event->type == NfcEventTypePollerReady) {
        bv_do_read(app, poller);
        cmd = NfcCommandStop;
    }
    if(cmd == NfcCommandStop) {
        // Hand off to the main thread to reap the poller (can't free it here).
        view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventReadFinished);
    }
    return cmd;
}

static void bv_start_read(BioVault* app) {
    if(app->poller_running) return;
    with_view_model(
        app->read_view,
        BvReadModel * m,
        {
            m->state = BvStateReading;
            m->error = BvErrNone;
        },
        true);
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start_ex(app->poller, bv_poller_callback, app);
    app->poller_running = true;
}

static void bv_stop_read(BioVault* app) {
    if(!app->poller_running) return;
    nfc_poller_stop(app->poller);
    nfc_poller_free(app->poller);
    app->poller = NULL;
    app->poller_running = false;
}

// --- Read view UI ---

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

static void bv_read_draw(Canvas* canvas, void* model) {
    BvReadModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Read Implant");
    canvas_set_font(canvas, FontSecondary);

    char line[40];
    switch(m->state) {
    case BvStateReading:
        canvas_draw_str(canvas, 2, 32, "Hold to implant...");
        break;
    case BvStateError:
        canvas_draw_str(canvas, 2, 28, bv_error_text(m->error));
        canvas_draw_str(canvas, 2, 52, "OK: retry  Back: menu");
        break;
    case BvStateDone: {
        snprintf(line, sizeof(line), "Sector 1: %u bytes", (unsigned)m->sector1_len);
        canvas_draw_str(canvas, 2, 22, line);
        for(uint32_t row = 0; row < 3; row++) {
            uint32_t off = m->view_offset + row * 8;
            if(off >= m->sector1_len) break;
            int n = snprintf(line, sizeof(line), "%03lX:", (unsigned long)off);
            for(uint32_t b = 0; b < 8 && (off + b) < m->sector1_len; b++) {
                n += snprintf(line + n, sizeof(line) - n, "%02X", m->sector1[off + b]);
            }
            canvas_draw_str(canvas, 2, 34 + row * 9, line);
        }
        break;
    }
    default:
        canvas_draw_str(canvas, 2, 32, "Reading...");
        break;
    }
}

static bool bv_read_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        if(!app->poller_running) bv_start_read(app);
        return true;
    }
    if(event->key == InputKeyDown) {
        with_view_model(
            app->read_view,
            BvReadModel * m,
            {
                if(m->state == BvStateDone && (size_t)m->view_offset + 24 < m->sector1_len)
                    m->view_offset += 8;
            },
            true);
        return true;
    }
    if(event->key == InputKeyUp) {
        with_view_model(
            app->read_view,
            BvReadModel * m,
            {
                if(m->state == BvStateDone && m->view_offset >= 8) m->view_offset -= 8;
            },
            true);
        return true;
    }
    return false; // Back falls through to the previous-view callback
}

static void bv_read_enter(void* context) {
    bv_start_read((BioVault*)context);
}

static void bv_read_exit(void* context) {
    bv_stop_read((BioVault*)context);
}

static uint32_t bv_read_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
}

// --- Menu / dispatcher callbacks ---

static void bv_menu_callback(void* context, uint32_t index) {
    BioVault* app = context;
    switch(index) {
    case BvMenuRead:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewRead);
        break;
    case BvMenuDiag:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewDiag);
        break;
    default:
        break;
    }
}

static uint32_t bv_diag_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
}

static bool bv_custom_event_callback(void* context, uint32_t event) {
    BioVault* app = context;
    if(event == BvCustomEventReadFinished) {
        bv_stop_read(app); // reap the finished poller on the main thread
        return true;
    }
    return false;
}

static bool bv_navigation_callback(void* context) {
    UNUSED(context);
    return false; // Back on the menu exits the app
}

// --- Diagnostics widget ---

static void bv_build_diag(BioVault* app) {
    widget_reset(app->diag);
    widget_add_string_element(app->diag, 64, 2, AlignCenter, AlignTop, FontPrimary, "Diagnostics");
    char line[40];
    const struct {
        const char* label;
        bool ok;
    } rows[] = {
        {"Enclave KEK", app->enclave_ok},
        {"AES-GCM KAT", app->gcm_ok},
        {"KEK/DEK wrap", app->dek_ok},
        {"Vault codec", app->vault_ok},
        {"Records", app->records_ok},
    };
    for(size_t i = 0; i < COUNT_OF(rows); i++) {
        snprintf(line, sizeof(line), "%s: %s", rows[i].label, rows[i].ok ? "OK" : "FAIL");
        widget_add_string_element(
            app->diag, 2, 16 + i * 10, AlignLeft, AlignTop, FontSecondary, line);
    }
}

// --- App lifecycle ---

static BioVault* bv_alloc(void) {
    BioVault* app = malloc(sizeof(BioVault));
    memset(app, 0, sizeof(BioVault));

    // Crypto/data self-tests (bring-up), results shown in Diagnostics.
    app->enclave_ok = bv_crypto_enclave_selftest();
    app->gcm_ok = bv_crypto_gcm_kat();
    app->dek_ok = bv_crypto_dek_selftest();
    app->vault_ok = bv_vault_selftest();
    app->records_ok = bv_records_selftest();
    FURI_LOG_I(
        TAG,
        "self-test: enclave=%d gcm=%d dek=%d vault=%d records=%d",
        app->enclave_ok,
        app->gcm_ok,
        app->dek_ok,
        app->vault_ok,
        app->records_ok);

    app->nfc = nfc_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, bv_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, bv_navigation_callback);

    // Main menu
    app->menu = submenu_alloc();
    submenu_set_header(app->menu, "BioVault");
    submenu_add_item(app->menu, "Read Implant", BvMenuRead, bv_menu_callback, app);
    submenu_add_item(app->menu, "Diagnostics", BvMenuDiag, bv_menu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, BvViewMenu, submenu_get_view(app->menu));

    // Read view
    app->read_view = view_alloc();
    view_allocate_model(app->read_view, ViewModelTypeLocking, sizeof(BvReadModel));
    view_set_context(app->read_view, app);
    view_set_draw_callback(app->read_view, bv_read_draw);
    view_set_input_callback(app->read_view, bv_read_input);
    view_set_enter_callback(app->read_view, bv_read_enter);
    view_set_exit_callback(app->read_view, bv_read_exit);
    view_set_previous_callback(app->read_view, bv_read_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewRead, app->read_view);

    // Diagnostics view
    app->diag = widget_alloc();
    bv_build_diag(app);
    view_set_previous_callback(widget_get_view(app->diag), bv_diag_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewDiag, widget_get_view(app->diag));

    return app;
}

static void bv_free(BioVault* app) {
    bv_stop_read(app);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewRead);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewDiag);
    submenu_free(app->menu);
    view_free(app->read_view);
    widget_free(app->diag);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    nfc_free(app->nfc);
    free(app);
}

int32_t biovault_app(void* p) {
    UNUSED(p);
    BioVault* app = bv_alloc();
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewMenu);
    view_dispatcher_run(app->view_dispatcher);
    bv_free(app);
    return 0;
}
