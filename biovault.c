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
#include <gui/modules/text_box.h>
#include <input/input.h>

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/helpers/iso14443_crc.h>

#include <cli/cli.h>
#include <stdio.h>

#include "bv_crypto.h"
#include "bv_vault.h"
#include "bv_records.h"
#include "bv_text_input.h"

#define TAG "BioVault"

// --- NTAG I2C Plus 2K command set ---
#define CMD_GET_VERSION 0x60
#define CMD_READ 0x30
#define CMD_WRITE 0xA2
#define CMD_SECTOR_SELECT_1 0xC2
#define CMD_SECTOR_SELECT_2 0xFF
#define NTAG_ACK 0x0A // 4-bit ACK returned by WRITE / sector-select packet 1

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
    BvErrWrite,
    BvErrTooBig,
    BvErrCrypto,
    BvErrNoVault,
} BvError;

// View ids and menu indices.
typedef enum {
    BvViewMenu,
    BvViewRead,
    BvViewSave,
    BvViewLoad,
    BvViewBrowser,
    BvViewDetail,
    BvViewInput,
    BvViewZero,
    BvViewDiag,
} BvViewId;

typedef enum {
    BvMenuVault,
    BvMenuAdd,
    BvMenuLoad,
    BvMenuRead,
    BvMenuSeed,
    BvMenuSave,
    BvMenuZero,
    BvMenuDiag,
} BvMenuIndex;

// On-device Add Entry field being edited.
typedef enum {
    BvAddLabel,
    BvAddUser,
    BvAddSecret,
} BvAddState;

typedef enum {
    BvCustomEventPollerDone = 1,
} BvCustomEvent;

// Which NFC operation the shared poller is currently running.
typedef enum {
    BvOpRead,
    BvOpZero,
    BvOpSave,
    BvOpLoad,
} BvOp;

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

typedef enum {
    BvZeroConfirm,
    BvZeroWriting,
    BvZeroDone,
    BvZeroError,
} BvZeroState;

typedef struct {
    BvZeroState state;
    uint16_t pages_written;
    BvError error;
} BvZeroModel;

// Save reuses the confirm->writing->done/error shape of BvZeroState.
typedef struct {
    BvZeroState state;
    uint16_t bytes;
    uint16_t pages_written;
    BvError error;
} BvSaveModel;

typedef struct {
    BvState state; // Reading / Done / Error
    BvError error;
    uint8_t count;
} BvLoadModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* menu;
    View* read_view;
    View* save_view;
    View* load_view;
    Submenu* browser;
    TextBox* detail;
    char detail_text[BV_LABEL_CAP + BV_USER_CAP + BV_SECRET_CAP + 32];
    BvTextInput* input;
    View* zero_view;
    Widget* diag;

    // On-device Add Entry state.
    BvAddState add_state;
    char edit_label[BV_LABEL_CAP];
    char edit_user[BV_USER_CAP];
    char edit_secret[BV_SECRET_CAP];

    Nfc* nfc;
    NfcPoller* poller;
    bool poller_running;
    BvOp op;

    BvVaultData* vault; // in-RAM vault (heap; ~3.8KB, never on the stack)
    FuriMutex* vault_mutex; // guards `vault` (GUI thread vs. CLI thread)
    uint8_t selected; // entry index shown in the detail view
    bool vault_loaded; // true once synced with the tag (or a known-empty tag)

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

// WRITE (0xA2) one 4-byte page. The tag replies with a 4-bit ACK (0x0A).
static bool bv_write_page(Iso14443_3aPoller* poller, uint8_t page, const uint8_t data[4]) {
    const uint8_t frame[6] = {CMD_WRITE, page, data[0], data[1], data[2], data[3]};
    BitBuffer* rx = bit_buffer_alloc(16);
    Iso14443_3aError e = bv_exchange(poller, frame, sizeof(frame), rx, FWT_NORMAL, true);
    bool ok = (e == Iso14443_3aErrorNone) && (bit_buffer_get_size(rx) >= 4) &&
              (bit_buffer_get_byte(rx, 0) == NTAG_ACK);
    bit_buffer_free(rx);
    return ok;
}

// Zero all of Sector 1 user memory (runs on the NFC worker thread).
static void bv_do_zero(BioVault* app, Iso14443_3aPoller* poller) {
    BvError err = BvErrNone;
    uint16_t written = 0;

    Iso14443_3aData* iso_data = iso14443_3a_alloc();
    Iso14443_3aError act = iso14443_3a_poller_activate(poller, iso_data);
    iso14443_3a_free(iso_data);

    uint8_t version[8] = {0};
    bool version_ok = (act == Iso14443_3aErrorNone) && bv_get_version(poller, version);
    bool version_match =
        version_ok && (memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) == 0);

    if(act != Iso14443_3aErrorNone || !version_ok) {
        err = BvErrNoTag;
    } else if(!version_match) {
        err = BvErrWrongTag;
    } else if(!bv_select_sector(poller, SECTOR1)) {
        err = BvErrSectorSelect;
    } else {
        const uint8_t zero[4] = {0, 0, 0, 0};
        for(uint32_t p = 0; p < SECTOR1_PAGES; p++) {
            if(!bv_write_page(poller, (uint8_t)p, zero)) {
                err = BvErrWrite;
                break;
            }
            written++;
        }
    }

    with_view_model(
        app->zero_view,
        BvZeroModel * m,
        {
            m->pages_written = written;
            m->state = (err == BvErrNone) ? BvZeroDone : BvZeroError;
            m->error = err;
        },
        true);
    FURI_LOG_I(TAG, "zero complete: err=%d pages=%u", err, written);
}

// Serialize + seal the in-RAM vault, then write the blob across Sector 1 pages
// (last page zero-padded). Runs on the NFC worker thread.
static void bv_do_save(BioVault* app, Iso14443_3aPoller* poller) {
    BvError err = BvErrNone;
    uint16_t written = 0;
    size_t blob_len = 0;

    // Prepare the sealed blob (heap; these buffers are too big for the stack).
    uint8_t* pt = malloc(4096);
    uint8_t* blob = malloc(1088);
    size_t pt_len = 0;
    bool prepared = false;

    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    bool ser_ok = bv_records_serialize(app->vault, pt, 4096, &pt_len);
    furi_mutex_release(app->vault_mutex);
    if(!ser_ok) {
        err = BvErrTooBig;
    } else if(pt_len + BV_BLOB_OVERHEAD > SECTOR1_BYTES) {
        err = BvErrTooBig;
    } else {
        BvVaultKey key;
        if(bv_vault_key_open(&key)) {
            prepared = bv_vault_encrypt(&key, pt, pt_len, blob, &blob_len);
            bv_vault_key_clear(&key);
            if(!prepared) err = BvErrCrypto;
        } else {
            err = BvErrCrypto;
        }
    }

    if(err == BvErrNone && prepared) {
        Iso14443_3aData* iso_data = iso14443_3a_alloc();
        Iso14443_3aError act = iso14443_3a_poller_activate(poller, iso_data);
        iso14443_3a_free(iso_data);
        uint8_t version[8] = {0};
        bool vok = (act == Iso14443_3aErrorNone) && bv_get_version(poller, version);
        bool vmatch = vok && (memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) == 0);

        if(!vok) {
            err = BvErrNoTag;
        } else if(!vmatch) {
            err = BvErrWrongTag;
        } else if(!bv_select_sector(poller, SECTOR1)) {
            err = BvErrSectorSelect;
        } else {
            uint32_t pages = (blob_len + 3) / 4;
            for(uint32_t p = 0; p < pages; p++) {
                uint8_t pd[4] = {0, 0, 0, 0};
                size_t off = p * 4;
                size_t n = (blob_len - off < 4) ? (blob_len - off) : 4;
                memcpy(pd, blob + off, n);
                if(!bv_write_page(poller, (uint8_t)p, pd)) {
                    err = BvErrWrite;
                    break;
                }
                written++;
            }
        }
    }

    memset(pt, 0, 4096);
    memset(blob, 0, 1088);
    free(pt);
    free(blob);

    with_view_model(
        app->save_view,
        BvSaveModel * m,
        {
            m->bytes = (uint16_t)blob_len;
            m->pages_written = written;
            m->state = (err == BvErrNone) ? BvZeroDone : BvZeroError;
            m->error = err;
        },
        true);
    FURI_LOG_I(TAG, "save complete: err=%d bytes=%u pages=%u", err, (unsigned)blob_len, written);
}

// Read Sector 1 -> parse the vault blob -> decrypt -> populate the in-RAM vault.
static void bv_do_load(BioVault* app, Iso14443_3aPoller* poller) {
    BvError err = BvErrNone;
    uint8_t count = 0;

    uint8_t* buf = malloc(SECTOR1_BYTES);
    uint8_t* pt = malloc(1024);

    Iso14443_3aData* iso_data = iso14443_3a_alloc();
    Iso14443_3aError act = iso14443_3a_poller_activate(poller, iso_data);
    iso14443_3a_free(iso_data);
    uint8_t version[8] = {0};
    bool vok = (act == Iso14443_3aErrorNone) && bv_get_version(poller, version);
    bool vmatch = vok && (memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) == 0);

    if(!vok) {
        err = BvErrNoTag;
    } else if(!vmatch) {
        err = BvErrWrongTag;
    } else if(!bv_select_sector(poller, SECTOR1)) {
        err = BvErrSectorSelect;
    } else {
        size_t got = 0;
        for(uint32_t i = 0; i < READ_CMD_COUNT; i++) {
            if(!bv_read_pages(poller, (uint8_t)(i * READ_PAGES_PER_CMD), buf + got)) {
                err = BvErrRead;
                break;
            }
            got += 16;
        }
        if(err == BvErrNone) {
            size_t blob_len = 0;
            if(!bv_vault_framed_len(buf, SECTOR1_BYTES, &blob_len)) {
                // Empty / non-vault tag: a valid fresh (empty) vault, not an error.
                furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
                bv_records_init(app->vault);
                furi_mutex_release(app->vault_mutex);
                count = 0;
            } else {
                BvVaultKey key;
                if(!bv_vault_key_open(&key)) {
                    err = BvErrCrypto;
                } else {
                    size_t pt_len = 0;
                    if(bv_vault_decrypt(&key, buf, blob_len, pt, 1024, &pt_len)) {
                        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
                        if(bv_records_parse(app->vault, pt, pt_len)) {
                            count = app->vault->count;
                        } else {
                            err = BvErrCrypto;
                        }
                        furi_mutex_release(app->vault_mutex);
                    } else {
                        err = BvErrCrypto; // wrong device/key, or corrupt
                    }
                    bv_vault_key_clear(&key);
                }
            }
        }
    }

    memset(pt, 0, 1024);
    free(pt);
    free(buf);

    if(err == BvErrNone) app->vault_loaded = true;
    with_view_model(
        app->load_view,
        BvLoadModel * m,
        {
            m->count = count;
            m->state = (err == BvErrNone) ? BvStateDone : BvStateError;
            m->error = err;
        },
        true);
    FURI_LOG_I(TAG, "load complete: err=%d entries=%u", err, count);
}

static NfcCommand bv_poller_callback(NfcGenericEventEx event, void* context) {
    BioVault* app = context;
    Iso14443_3aPoller* poller = event.poller;
    const NfcEvent* nfc_event = event.parent_event_data;
    NfcCommand cmd = NfcCommandContinue;

    if(nfc_event->type == NfcEventTypePollerReady) {
        if(app->op == BvOpZero) {
            bv_do_zero(app, poller);
        } else if(app->op == BvOpSave) {
            bv_do_save(app, poller);
        } else if(app->op == BvOpLoad) {
            bv_do_load(app, poller);
        } else {
            bv_do_read(app, poller);
        }
        cmd = NfcCommandStop;
    }
    if(cmd == NfcCommandStop) {
        // Hand off to the main thread to reap the poller (can't free it here).
        view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventPollerDone);
    }
    return cmd;
}

static void bv_start_op(BioVault* app, BvOp op) {
    if(app->poller_running) return;
    app->op = op;
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start_ex(app->poller, bv_poller_callback, app);
    app->poller_running = true;
}

static void bv_poller_stop(BioVault* app) {
    if(!app->poller_running) return;
    nfc_poller_stop(app->poller);
    nfc_poller_free(app->poller);
    app->poller = NULL;
    app->poller_running = false;
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
    bv_start_op(app, BvOpRead);
}

static void bv_start_zero(BioVault* app) {
    if(app->poller_running) return;
    with_view_model(
        app->zero_view,
        BvZeroModel * m,
        {
            m->state = BvZeroWriting;
            m->pages_written = 0;
            m->error = BvErrNone;
        },
        true);
    bv_start_op(app, BvOpZero);
}

static void bv_start_save(BioVault* app) {
    if(app->poller_running) return;
    with_view_model(
        app->save_view,
        BvSaveModel * m,
        {
            m->state = BvZeroWriting;
            m->pages_written = 0;
            m->bytes = 0;
            m->error = BvErrNone;
        },
        true);
    bv_start_op(app, BvOpSave);
}

static void bv_start_load(BioVault* app) {
    if(app->poller_running) return;
    with_view_model(
        app->load_view,
        BvLoadModel * m,
        {
            m->state = BvStateReading;
            m->error = BvErrNone;
            m->count = 0;
        },
        true);
    bv_start_op(app, BvOpLoad);
}

// (Re)build the browser submenu from the current in-RAM vault.
static void bv_menu_callback(void* context, uint32_t index);
static void bv_browser_item_callback(void* context, uint32_t index);
static void bv_build_detail(BioVault* app);

static void bv_build_browser(BioVault* app) {
    submenu_reset(app->browser);
    submenu_set_header(app->browser, "Vault");
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    for(uint8_t i = 0; i < app->vault->count; i++) {
        submenu_add_item(
            app->browser, app->vault->entries[i].label, i, bv_browser_item_callback, app);
    }
    furi_mutex_release(app->vault_mutex);
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
    case BvErrWrite:
        return "Write failed";
    case BvErrTooBig:
        return "Vault too big for tag";
    case BvErrCrypto:
        return "Crypto/keystore error";
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
    bv_poller_stop((BioVault*)context);
}

static uint32_t bv_read_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
}

// --- Zero Sector 1 view UI ---

static void bv_zero_draw(Canvas* canvas, void* model) {
    BvZeroModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Zero Sector 1");
    canvas_set_font(canvas, FontSecondary);

    char line[40];
    switch(m->state) {
    case BvZeroConfirm:
        canvas_draw_str(canvas, 2, 26, "Erase ALL of Sector 1?");
        canvas_draw_str(canvas, 2, 37, "This wipes the vault.");
        canvas_draw_str(canvas, 2, 54, "OK: erase   Back: cancel");
        break;
    case BvZeroWriting:
        canvas_draw_str(canvas, 2, 34, "Hold to implant...");
        canvas_draw_str(canvas, 2, 46, "Erasing Sector 1");
        break;
    case BvZeroDone:
        snprintf(line, sizeof(line), "Erased %u pages", m->pages_written);
        canvas_draw_str(canvas, 2, 32, line);
        canvas_draw_str(canvas, 2, 54, "Back: menu");
        break;
    case BvZeroError:
        canvas_draw_str(canvas, 2, 28, bv_error_text(m->error));
        snprintf(line, sizeof(line), "Wrote %u pages", m->pages_written);
        canvas_draw_str(canvas, 2, 40, line);
        canvas_draw_str(canvas, 2, 54, "OK: retry   Back: menu");
        break;
    }
}

static bool bv_zero_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        BvZeroState state;
        with_view_model(app->zero_view, BvZeroModel * m, { state = m->state; }, false);
        if(state == BvZeroConfirm || state == BvZeroError) {
            if(!app->poller_running) bv_start_zero(app);
        }
        return true;
    }
    return false; // Back falls through to the previous-view callback
}

static void bv_zero_enter(void* context) {
    BioVault* app = context;
    // Always start at the confirmation screen; never auto-write.
    with_view_model(
        app->zero_view,
        BvZeroModel * m,
        {
            m->state = BvZeroConfirm;
            m->pages_written = 0;
            m->error = BvErrNone;
        },
        true);
}

static void bv_zero_exit(void* context) {
    bv_poller_stop((BioVault*)context);
}

static uint32_t bv_zero_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
}

// --- Save to implant view UI ---

static void bv_save_draw(Canvas* canvas, void* model) {
    BvSaveModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Save to Implant");
    canvas_set_font(canvas, FontSecondary);

    char line[40];
    switch(m->state) {
    case BvZeroConfirm:
        canvas_draw_str(canvas, 2, 26, "Write vault to Sector 1?");
        canvas_draw_str(canvas, 2, 37, "Overwrites its contents.");
        canvas_draw_str(canvas, 2, 54, "OK: save   Back: cancel");
        break;
    case BvZeroWriting:
        canvas_draw_str(canvas, 2, 34, "Hold to implant...");
        canvas_draw_str(canvas, 2, 46, "Saving vault");
        break;
    case BvZeroDone:
        snprintf(line, sizeof(line), "Saved %u bytes", m->bytes);
        canvas_draw_str(canvas, 2, 30, line);
        snprintf(line, sizeof(line), "%u pages written", m->pages_written);
        canvas_draw_str(canvas, 2, 42, line);
        canvas_draw_str(canvas, 2, 56, "Back: menu");
        break;
    case BvZeroError:
        canvas_draw_str(canvas, 2, 28, bv_error_text(m->error));
        canvas_draw_str(canvas, 2, 54, "OK: retry   Back: menu");
        break;
    }
}

static bool bv_save_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyOk) {
        BvZeroState state;
        with_view_model(app->save_view, BvSaveModel * m, { state = m->state; }, false);
        if(state == BvZeroConfirm || state == BvZeroError) {
            if(!app->poller_running) bv_start_save(app);
        }
        return true;
    }
    return false;
}

static void bv_save_enter(void* context) {
    BioVault* app = context;
    with_view_model(
        app->save_view,
        BvSaveModel * m,
        {
            m->state = BvZeroConfirm;
            m->pages_written = 0;
            m->bytes = 0;
            m->error = BvErrNone;
        },
        true);
}

static void bv_save_exit(void* context) {
    bv_poller_stop((BioVault*)context);
}

static uint32_t bv_save_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
}

// --- Load from implant view UI ---

static void bv_load_draw(Canvas* canvas, void* model) {
    BvLoadModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Load from Implant");
    canvas_set_font(canvas, FontSecondary);

    char line[40];
    switch(m->state) {
    case BvStateDone:
        snprintf(line, sizeof(line), "Loaded %u entries", m->count);
        canvas_draw_str(canvas, 2, 32, line);
        canvas_draw_str(canvas, 2, 54, "OK: continue");
        break;
    case BvStateError:
        canvas_draw_str(canvas, 2, 28, bv_error_text(m->error));
        canvas_draw_str(canvas, 2, 54, "OK: retry   Back: skip");
        break;
    default:
        canvas_draw_str(canvas, 2, 34, "Hold to implant...");
        break;
    }
}

static bool bv_load_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort || event->key != InputKeyOk) return false;

    BvState state;
    with_view_model(app->load_view, BvLoadModel * m, { state = m->state; }, false);
    if(state == BvStateDone) {
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewMenu);
    } else if(state == BvStateError) {
        if(!app->poller_running) bv_start_load(app);
    }
    return true;
}

static void bv_load_enter(void* context) {
    bv_start_load((BioVault*)context);
}

static void bv_load_exit(void* context) {
    bv_poller_stop((BioVault*)context);
}

static uint32_t bv_load_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
}

// --- Vault browser + detail view UI ---

static void bv_browser_item_callback(void* context, uint32_t index) {
    BioVault* app = context;
    app->selected = (uint8_t)index;
    bv_build_detail(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewDetail);
}

static uint32_t bv_browser_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
}

// Build the scrollable detail text for the selected entry. A credential shows
// label/user/secret; a note (no username) shows label/data. Values go on their
// own lines so the TextBox wraps and scrolls them however long they are.
static void bv_build_detail(BioVault* app) {
    text_box_reset(app->detail);
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    if(app->selected >= app->vault->count) {
        app->detail_text[0] = '\0';
    } else {
        const BvEntry* e = &app->vault->entries[app->selected];
        if(e->type == BvEntryNote) {
            snprintf(
                app->detail_text,
                sizeof(app->detail_text),
                "%s\n\nData:\n%s",
                e->label,
                e->secret);
        } else {
            snprintf(
                app->detail_text,
                sizeof(app->detail_text),
                "%s\n\nUser:\n%s\n\nPass:\n%s",
                e->label,
                e->user,
                e->secret);
        }
    }
    furi_mutex_release(app->vault_mutex);
    text_box_set_font(app->detail, TextBoxFontText);
    text_box_set_text(app->detail, app->detail_text);
}

static uint32_t bv_detail_previous(void* context) {
    UNUSED(context);
    return BvViewBrowser;
}

// --- On-device Add Entry (keyboard) ---

static void bv_configure_input(BioVault* app);

// Fired when the user confirms a field on the keyboard; advances through
// label -> user -> secret, then appends the entry to the in-RAM vault.
static void bv_input_result(void* context) {
    BioVault* app = context;
    switch(app->add_state) {
    case BvAddLabel:
        app->add_state = BvAddUser;
        bv_configure_input(app);
        break;
    case BvAddUser:
        app->add_state = BvAddSecret;
        bv_configure_input(app);
        break;
    case BvAddSecret: {
        // No username -> it's a note/data entry, not a credential.
        BvEntryType type = (strlen(app->edit_user) > 0) ? BvEntryCred : BvEntryNote;
        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
        bv_records_add(app->vault, type, app->edit_label, app->edit_user, app->edit_secret);
        furi_mutex_release(app->vault_mutex);
        FURI_LOG_I(TAG, "added %s '%s' (vault now %u)",
            type == BvEntryNote ? "note" : "cred", app->edit_label, app->vault->count);
        bv_build_browser(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewBrowser);
        break;
    }
    }
}

static void bv_configure_input(BioVault* app) {
    switch(app->add_state) {
    case BvAddLabel:
        bv_text_input_set_header_text(app->input, "Label (site or name)");
        bv_text_input_set_minimum_length(app->input, 1);
        bv_text_input_set_result_callback(
            app->input, bv_input_result, app, app->edit_label, sizeof(app->edit_label), true);
        break;
    case BvAddUser:
        bv_text_input_set_header_text(app->input, "Username (optional)");
        bv_text_input_set_minimum_length(app->input, 0);
        bv_text_input_set_result_callback(
            app->input, bv_input_result, app, app->edit_user, sizeof(app->edit_user), true);
        break;
    case BvAddSecret:
        bv_text_input_set_header_text(app->input, "Password / secret");
        bv_text_input_set_minimum_length(app->input, 0);
        bv_text_input_set_result_callback(
            app->input, bv_input_result, app, app->edit_secret, sizeof(app->edit_secret), true);
        break;
    }
}

static void bv_add_start(BioVault* app) {
    memset(app->edit_label, 0, sizeof(app->edit_label));
    memset(app->edit_user, 0, sizeof(app->edit_user));
    memset(app->edit_secret, 0, sizeof(app->edit_secret));
    app->add_state = BvAddLabel;
    bv_configure_input(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewInput);
}

static uint32_t bv_input_previous(void* context) {
    UNUSED(context);
    return BvViewMenu; // Back cancels the add flow
}

// --- Menu / dispatcher callbacks ---

static void bv_menu_callback(void* context, uint32_t index) {
    BioVault* app = context;
    switch(index) {
    case BvMenuVault:
        bv_build_browser(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewBrowser);
        break;
    case BvMenuAdd:
        bv_add_start(app);
        break;
    case BvMenuLoad:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewLoad);
        break;
    case BvMenuRead:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewRead);
        break;
    case BvMenuSeed:
        // TEMPORARY test scaffolding until CLI / on-device entry lands: fill the
        // in-RAM vault with sample entries so Save/Load have data to round-trip.
        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
        bv_records_init(app->vault);
        bv_records_add(app->vault, BvEntryCred, "example.com", "alice", "hunter2");
        bv_records_add(app->vault, BvEntryCred, "reddit.com", "bob", "s3cret,pw");
        bv_records_add(app->vault, BvEntryNote, "recovery", "", "correct horse battery");
        furi_mutex_release(app->vault_mutex);
        app->vault_loaded = true; // seeded vault is a known state, safe to save
        FURI_LOG_I(TAG, "seeded %u test entries", app->vault->count);
        break;
    case BvMenuSave:
        // Never overwrite the tag with a vault that wasn't synced from it first.
        if(!app->vault_loaded) {
            view_dispatcher_switch_to_view(app->view_dispatcher, BvViewLoad);
        } else {
            view_dispatcher_switch_to_view(app->view_dispatcher, BvViewSave);
        }
        break;
    case BvMenuZero:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewZero);
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
    if(event == BvCustomEventPollerDone) {
        bv_poller_stop(app); // reap the finished poller on the main thread
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

// --- CLI command (registered while the app is running) ---
//
// `biovault list|get|add|remove` drives the same in-RAM vault as the GUI, so
// complex secrets can be entered over USB (arbitrary characters, RAM-only on the
// device). Interactive prompts keep secrets out of the host shell history; note
// they still land in the host terminal's scrollback.

static void bv_cli_read_line(char* buf, size_t size) {
    size_t i = 0;
    while(i + 1 < size) {
        int c = getchar();
        if(c < 0 || c == '\r') break; // Enter
        if(c == '\n') continue; // ignore LF (handles CRLF line endings)
        if(c == 0x08 || c == 0x7f) { // backspace / delete
            if(i > 0) {
                i--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        buf[i++] = (char)c;
        putchar(c);
        fflush(stdout);
    }
    buf[i] = '\0';
    printf("\r\n");
}

static void bv_cli_callback(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    BioVault* app = context;

    char sub[16] = {0};
    char label[BV_LABEL_CAP] = {0};
    sscanf(furi_string_get_cstr(args), "%15s %47s", sub, label);

    if(strcmp(sub, "list") == 0) {
        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
        printf("Vault: %u entries\r\n", app->vault->count);
        for(uint8_t i = 0; i < app->vault->count; i++) {
            const BvEntry* e = &app->vault->entries[i];
            printf("  [%u] %s%s\r\n", i, e->label, e->type == BvEntryNote ? "  (note)" : "");
        }
        furi_mutex_release(app->vault_mutex);

    } else if(strcmp(sub, "get") == 0) {
        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
        int idx = bv_records_find(app->vault, label);
        if(idx < 0) {
            printf("Not found: %s\r\n", label);
        } else {
            const BvEntry* e = &app->vault->entries[idx];
            printf("%s\r\n", e->label);
            if(e->type == BvEntryNote) {
                printf("  data: %s\r\n", e->secret);
            } else {
                printf("  user: %s\r\n  pass: %s\r\n", e->user, e->secret);
            }
        }
        furi_mutex_release(app->vault_mutex);

    } else if(strcmp(sub, "remove") == 0) {
        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
        int idx = bv_records_find(app->vault, label);
        bool ok = (idx >= 0) && bv_records_remove(app->vault, (uint8_t)idx);
        furi_mutex_release(app->vault_mutex);
        printf(ok ? "Removed '%s'. Use Save to Implant to persist.\r\n" : "Not found: %s\r\n", label);

    } else if(strcmp(sub, "add") == 0) {
        if(strlen(label) == 0) {
            printf("Usage: biovault add <label>\r\n");
            return;
        }
        char user[BV_USER_CAP] = {0};
        char secret[BV_SECRET_CAP] = {0};
        printf("Username (blank = note): ");
        fflush(stdout);
        bv_cli_read_line(user, sizeof(user)); // read outside the lock
        printf("Secret: ");
        fflush(stdout);
        bv_cli_read_line(secret, sizeof(secret));

        BvEntryType type = (strlen(user) > 0) ? BvEntryCred : BvEntryNote;
        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
        bool ok = bv_records_add(app->vault, type, label, user, secret);
        furi_mutex_release(app->vault_mutex);
        printf(
            ok ? "Added '%s'. Use Save to Implant to persist.\r\n" :
                 "Add failed (vault full or field too long).\r\n",
            label);

    } else {
        printf("BioVault CLI\r\n");
        printf("  biovault list             list entries (labels only)\r\n");
        printf("  biovault get <label>      show an entry (prints the secret)\r\n");
        printf("  biovault add <label>      add a cred/note (prompts for fields)\r\n");
        printf("  biovault remove <label>   remove an entry\r\n");
        printf("Changes are in RAM; use 'Save to Implant' in the app to persist.\r\n");
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

    app->vault = malloc(sizeof(BvVaultData));
    bv_records_init(app->vault);
    app->vault_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

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
    submenu_add_item(app->menu, "Vault", BvMenuVault, bv_menu_callback, app);
    submenu_add_item(app->menu, "Add Entry", BvMenuAdd, bv_menu_callback, app);
    submenu_add_item(app->menu, "Load from Implant", BvMenuLoad, bv_menu_callback, app);
    submenu_add_item(app->menu, "Read Implant", BvMenuRead, bv_menu_callback, app);
    submenu_add_item(app->menu, "Seed Test Data", BvMenuSeed, bv_menu_callback, app);
    submenu_add_item(app->menu, "Save to Implant", BvMenuSave, bv_menu_callback, app);
    submenu_add_item(app->menu, "Zero Sector 1", BvMenuZero, bv_menu_callback, app);
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

    // Save to implant view
    app->save_view = view_alloc();
    view_allocate_model(app->save_view, ViewModelTypeLocking, sizeof(BvSaveModel));
    view_set_context(app->save_view, app);
    view_set_draw_callback(app->save_view, bv_save_draw);
    view_set_input_callback(app->save_view, bv_save_input);
    view_set_enter_callback(app->save_view, bv_save_enter);
    view_set_exit_callback(app->save_view, bv_save_exit);
    view_set_previous_callback(app->save_view, bv_save_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewSave, app->save_view);

    // Load from implant view
    app->load_view = view_alloc();
    view_allocate_model(app->load_view, ViewModelTypeLocking, sizeof(BvLoadModel));
    view_set_context(app->load_view, app);
    view_set_draw_callback(app->load_view, bv_load_draw);
    view_set_input_callback(app->load_view, bv_load_input);
    view_set_enter_callback(app->load_view, bv_load_enter);
    view_set_exit_callback(app->load_view, bv_load_exit);
    view_set_previous_callback(app->load_view, bv_load_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewLoad, app->load_view);

    // Vault browser (submenu, rebuilt on entry)
    app->browser = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->browser), bv_browser_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewBrowser, submenu_get_view(app->browser));

    // Entry detail view (scrollable text box, rebuilt per entry)
    app->detail = text_box_alloc();
    view_set_previous_callback(text_box_get_view(app->detail), bv_detail_previous);
    view_dispatcher_add_view(
        app->view_dispatcher, BvViewDetail, text_box_get_view(app->detail));

    // Add Entry keyboard view
    app->input = bv_text_input_alloc();
    view_set_previous_callback(bv_text_input_get_view(app->input), bv_input_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewInput, bv_text_input_get_view(app->input));

    // Zero Sector 1 view
    app->zero_view = view_alloc();
    view_allocate_model(app->zero_view, ViewModelTypeLocking, sizeof(BvZeroModel));
    view_set_context(app->zero_view, app);
    view_set_draw_callback(app->zero_view, bv_zero_draw);
    view_set_input_callback(app->zero_view, bv_zero_input);
    view_set_enter_callback(app->zero_view, bv_zero_enter);
    view_set_exit_callback(app->zero_view, bv_zero_exit);
    view_set_previous_callback(app->zero_view, bv_zero_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewZero, app->zero_view);

    // Diagnostics view
    app->diag = widget_alloc();
    bv_build_diag(app);
    view_set_previous_callback(widget_get_view(app->diag), bv_diag_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewDiag, widget_get_view(app->diag));

    // Register the `biovault` CLI command (stays registered until we delete it).
    // ParallelSafe so the CLI shell allows it to run while our app is open
    // (that's the whole point) — the vault mutex makes it safe.
    CliRegistry* cli = furi_record_open(RECORD_CLI);
    cli_registry_add_command(
        cli, "biovault", CliCommandFlagParallelSafe, bv_cli_callback, app);
    furi_record_close(RECORD_CLI);

    return app;
}

static void bv_free(BioVault* app) {
    CliRegistry* cli = furi_record_open(RECORD_CLI);
    cli_registry_delete_command(cli, "biovault");
    furi_record_close(RECORD_CLI);

    bv_poller_stop(app);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewRead);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewSave);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewLoad);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewDetail);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewInput);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewZero);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewDiag);
    submenu_free(app->menu);
    view_free(app->read_view);
    view_free(app->save_view);
    view_free(app->load_view);
    submenu_free(app->browser);
    text_box_free(app->detail);
    bv_text_input_free(app->input);
    view_free(app->zero_view);
    widget_free(app->diag);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    nfc_free(app->nfc);
    bv_records_clear(app->vault);
    free(app->vault);
    furi_mutex_free(app->vault_mutex);
    free(app);
}

int32_t biovault_app(void* p) {
    UNUSED(p);
    BioVault* app = bv_alloc();
    // Start by loading the vault from the implant, then drop into the menu. This
    // makes load-first the natural flow, so Save never overwrites the tag with a
    // vault that wasn't synced from it.
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewLoad);
    view_dispatcher_run(app->view_dispatcher);
    bv_free(app);
    return 0;
}
