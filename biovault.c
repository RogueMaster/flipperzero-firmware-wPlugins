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
#include <gui/modules/variable_item_list.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/helpers/iso14443_crc.h>

#include <cli/cli.h>
#include <toolbox/cli/shell/cli_shell.h>
#include <stdio.h>

#include "bv_crypto.h"
#include "bv_vault.h"
#include "bv_records.h"
#include "bv_text_input.h"
#include "bv_hid.h"
#include "bv_settings.h"
#include "biovault_icons.h" // generated from images/ (fap_icon_assets)

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

#define BV_READ_DONE_FLAG (1u << 0) // set on read_done when a read terminates

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
    BvViewEntryMenu,
    BvViewDetail,
    BvViewSendPick,
    BvViewSendDo,
    BvViewInput,
    BvViewZero,
    BvViewSettings,
    BvViewDiag,
} BvViewId;

// Per-entry action menu (opened by selecting an entry in the browser).
typedef enum {
    BvEntryActView,
    BvEntryActSend,
    BvEntryActEdit,
    BvEntryActRemove,
} BvEntryAction;

// Which field the Send picker types over USB HID.
typedef enum {
    BvSendUser,
    BvSendSecret,
} BvSendField;

typedef enum {
    BvMenuVault,
    BvMenuAdd,
    BvMenuLoad,
    BvMenuRead,
    BvMenuSave,
    BvMenuZero,
    BvMenuSettings,
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
    BvCustomEventHidDone, // HID send worker finished; reap it on the main thread
    // Posted from the CLI thread to drive the GUI screens remotely.
    BvCustomEventCliRead,
    BvCustomEventCliLoad,
    BvCustomEventCliSave,
    BvCustomEventCliZero,
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

typedef enum {
    BvSendSending,
    BvSendDone,
    BvSendNoUsb,
    BvSendBusy,
} BvSendState;

typedef struct {
    BvSendState state;
    char field[16]; // "Username" / "Password" / "Data" — what is being typed
} BvSendModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* menu;
    View* read_view;
    View* save_view;
    View* load_view;
    Submenu* browser;
    Submenu* entry_menu;
    Submenu* send_pick;
    View* send_view;
    TextBox* detail;
    char detail_text[BV_LABEL_CAP + BV_USER_CAP + BV_SECRET_CAP + 32];
    BvTextInput* input;

    // USB-HID send: the field text is snapshotted here so the worker thread
    // never touches the vault while typing. Sized for the secret plus an
    // optional trailing newline (see BvSettings.send_newline).
    FuriThread* send_thread;
    char send_text[BV_SECRET_CAP + 2];
    BvHidResult send_result;
    View* zero_view;
    VariableItemList* settings_list;
    BvSettings settings;
    Widget* diag;

    // On-device Add/Edit Entry state.
    BvAddState add_state;
    bool editing; // true: keyboard flow updates entry `edit_index` in place
    uint8_t edit_index; // entry being edited when `editing`
    char edit_label[BV_LABEL_CAP];
    char edit_user[BV_USER_CAP];
    char edit_secret[BV_SECRET_CAP];

    Nfc* nfc;
    NfcPoller* poller;
    bool poller_running;
    BvOp op;
    uint8_t op_fails; // consecutive partial-coupling failures for the current op
    uint8_t* save_blob; // sealed vault blob, prepared once per save op (heap)
    size_t save_blob_len;
    volatile uint32_t cli_sessions; // open `biovault` subshells (see bv_free)
    NotificationApp* notifications;

    BvVaultData* vault; // in-RAM vault (heap; ~3.8KB, never on the stack)
    FuriMutex* vault_mutex; // guards `vault` (GUI thread vs. CLI thread)
    uint8_t selected; // entry index shown in the detail view
    bool vault_loaded; // true once synced with the tag (or a known-empty tag)
    FuriEventFlag* read_done; // signals a CLI `read` when the dump is ready

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

// Transient tag-communication failures keep polling silently (the tag may just
// be misaligned — common with an implant). No-tag stays transient forever, but
// partial-coupling failures (sector select / read / write) get a retry budget:
// a fault that persists across this many attempts is a real error and must be
// surfaced, not retried silently forever.
#define BV_OP_MAX_FAILS 15

static bool bv_op_should_retry(BioVault* app, BvError e) {
    if(e == BvErrNoTag) return true;
    if(e == BvErrSectorSelect || e == BvErrRead || e == BvErrWrite) {
        return ++app->op_fails < BV_OP_MAX_FAILS;
    }
    return false;
}

// Full read sequence (runs on the NFC worker thread). Commits into the read
// model on success or terminal error; transient errors are not committed so the
// "hold implant" screen stays up while polling continues.
static BvError bv_do_read(BioVault* app, Iso14443_3aPoller* poller, bool* retry) {
    BvError err = BvErrNone;

    Iso14443_3aData* iso_data = iso14443_3a_alloc();
    Iso14443_3aError act = iso14443_3a_poller_activate(poller, iso_data);
    iso14443_3a_free(iso_data);
    if(act != Iso14443_3aErrorNone) {
        *retry = true;
        return BvErrNoTag;
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

    *retry = bv_op_should_retry(app, err);
    if(*retry) return err;

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
    // Wake any CLI `read` waiting to print the dump (harmless for GUI reads).
    furi_event_flag_set(app->read_done, BV_READ_DONE_FLAG);
    FURI_LOG_I(TAG, "read complete: err=%d bytes=%u", err, (unsigned)got);
    return err;
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
static BvError bv_do_zero(BioVault* app, Iso14443_3aPoller* poller, bool* retry) {
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

    *retry = bv_op_should_retry(app, err);
    if(*retry) return err;

    // Wipe is a full reset: on success, clear the in-RAM vault too so RAM matches
    // the now-blank tag. vault_loaded stays true (empty + blank is a synced state)
    // so a later save keeps the tag empty instead of rewriting stale entries.
    if(err == BvErrNone) {
        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
        bv_records_init(app->vault);
        furi_mutex_release(app->vault_mutex);
        app->vault_loaded = true;
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
    FURI_LOG_I(TAG, "wipe complete: err=%d pages=%u", err, written);
    return err;
}

// Serialize + seal the in-RAM vault into app->save_blob. Runs on the GUI thread
// once per save op (bv_start_save), so the poll-cycle retries in bv_do_save
// never repeat the serialization/enclave/AEAD work.
static BvError bv_prepare_save_blob(BioVault* app) {
    if(!app->save_blob) app->save_blob = malloc(1088);
    app->save_blob_len = 0;

    uint8_t* pt = malloc(4096); // too big for the stack
    size_t pt_len = 0;
    BvError err = BvErrNone;

    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    bool ser_ok = bv_records_serialize(app->vault, pt, 4096, &pt_len);
    furi_mutex_release(app->vault_mutex);

    if(!ser_ok || pt_len + BV_BLOB_OVERHEAD > SECTOR1_BYTES) {
        err = BvErrTooBig;
    } else {
        BvVaultKey key;
        if(bv_vault_key_open(&key)) {
            if(!bv_vault_encrypt(&key, pt, pt_len, app->save_blob, &app->save_blob_len)) {
                err = BvErrCrypto;
            }
            bv_vault_key_clear(&key);
        } else {
            err = BvErrCrypto;
        }
    }
    memset(pt, 0, 4096);
    free(pt);
    return err;
}

// Write the prepared blob (app->save_blob) across Sector 1 pages (last page
// zero-padded). Runs on the NFC worker thread.
static BvError bv_do_save(BioVault* app, Iso14443_3aPoller* poller, bool* retry) {
    BvError err = BvErrNone;
    uint16_t written = 0;

    Iso14443_3aData* iso_data = iso14443_3a_alloc();
    Iso14443_3aError act = iso14443_3a_poller_activate(poller, iso_data);
    iso14443_3a_free(iso_data);
    uint8_t version[8] = {0};
    bool vok = (act == Iso14443_3aErrorNone) && bv_get_version(poller, version);
    if(!vok) {
        *retry = true;
        return BvErrNoTag;
    }
    bool vmatch = memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) == 0;

    if(!vmatch) {
        err = BvErrWrongTag;
    } else if(app->save_blob_len == 0) {
        err = BvErrCrypto; // no prepared blob (bv_start_save should prevent this)
    } else if(!bv_select_sector(poller, SECTOR1)) {
        err = BvErrSectorSelect;
    } else {
        // Blank the header page first and write the real header (page 0) last:
        // a save torn mid-way then reads back as an empty tag on Load, instead
        // of a valid-looking header framing stale ciphertext.
        static const uint8_t blank[4] = {0, 0, 0, 0};
        uint32_t pages = (app->save_blob_len + 3) / 4;
        if(!bv_write_page(poller, 0, blank)) {
            err = BvErrWrite;
        } else {
            for(uint32_t i = 0; i < pages; i++) {
                uint32_t p = (i + 1 < pages) ? (i + 1) : 0; // 1..N-1, then 0
                uint8_t pd[4] = {0, 0, 0, 0};
                size_t off = p * 4;
                size_t n = (app->save_blob_len - off < 4) ? (app->save_blob_len - off) : 4;
                memcpy(pd, app->save_blob + off, n);
                if(!bv_write_page(poller, (uint8_t)p, pd)) {
                    err = BvErrWrite;
                    break;
                }
                written++;
            }
        }
    }

    *retry = bv_op_should_retry(app, err);
    if(*retry) return err;

    with_view_model(
        app->save_view,
        BvSaveModel * m,
        {
            m->bytes = (uint16_t)app->save_blob_len;
            m->pages_written = written;
            m->state = (err == BvErrNone) ? BvZeroDone : BvZeroError;
            m->error = err;
        },
        true);
    FURI_LOG_I(
        TAG,
        "save complete: err=%d bytes=%u pages=%u",
        err,
        (unsigned)app->save_blob_len,
        written);
    return err;
}

// Read Sector 1 -> parse the vault blob -> decrypt -> populate the in-RAM vault.
static BvError bv_do_load(BioVault* app, Iso14443_3aPoller* poller, bool* retry) {
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

    *retry = bv_op_should_retry(app, err);
    if(*retry) return err;

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
    return err;
}

static NfcCommand bv_poller_callback(NfcGenericEventEx event, void* context) {
    BioVault* app = context;
    Iso14443_3aPoller* poller = event.poller;
    const NfcEvent* nfc_event = event.parent_event_data;
    NfcCommand cmd = NfcCommandContinue;

    if(nfc_event->type == NfcEventTypePollerReady) {
        BvError err;
        bool retry = false;
        if(app->op == BvOpZero) {
            err = bv_do_zero(app, poller, &retry);
        } else if(app->op == BvOpSave) {
            err = bv_do_save(app, poller, &retry);
        } else if(app->op == BvOpLoad) {
            err = bv_do_load(app, poller, &retry);
        } else {
            err = bv_do_read(app, poller, &retry);
        }
        if(retry) {
            // Tag not (well) coupled yet — reset the field and keep polling.
            cmd = NfcCommandReset;
        } else {
            notification_message(
                app->notifications, err == BvErrNone ? &sequence_success : &sequence_error);
            cmd = NfcCommandStop;
        }
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
    app->op_fails = 0;
    // Blink like the stock NFC app: cyan while reading, magenta while writing.
    notification_message(
        app->notifications,
        (op == BvOpRead || op == BvOpLoad) ? &sequence_blink_start_cyan :
                                             &sequence_blink_start_magenta);
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
    notification_message(app->notifications, &sequence_blink_stop);
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
    // Seal the vault before touching the field; a vault that can't be sealed
    // (too big, keystore/enclave fault) errors out here without ever polling.
    BvError prep_err = bv_prepare_save_blob(app);
    with_view_model(
        app->save_view,
        BvSaveModel * m,
        {
            m->state = (prep_err == BvErrNone) ? BvZeroWriting : BvZeroError;
            m->pages_written = 0;
            m->bytes = 0;
            m->error = prep_err;
        },
        true);
    if(prep_err != BvErrNone) {
        notification_message(app->notifications, &sequence_error);
        return;
    }
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

// Full-screen implant-coupling prompt, in the style of the stock NFC app: art
// on the left, "<verb> implant" centered in the space to its right.
static void bv_draw_hold(Canvas* canvas, const char* verb) {
    canvas_draw_icon(canvas, 0, 8, &I_NFC_manual_60x50);
    const uint8_t cx = 94; // center of the region right of the 60px-wide art
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, cx, 30, AlignCenter, AlignCenter, verb);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, cx, 43, AlignCenter, AlignCenter, "implant");
}

// Success screen: big check mark + up to two result lines + footer hint.
static void bv_draw_done(Canvas* canvas, const char* l1, const char* l2, const char* footer) {
    canvas_draw_icon(canvas, 6, 16, &I_check_big_20x17);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 34, 24, l1);
    canvas_set_font(canvas, FontSecondary);
    if(l2) canvas_draw_str(canvas, 34, 36, l2);
    canvas_draw_str(canvas, 2, 60, footer);
}

static void bv_read_draw(Canvas* canvas, void* model) {
    BvReadModel* m = model;
    canvas_clear(canvas);

    char line[40];
    switch(m->state) {
    case BvStateReading:
        bv_draw_hold(canvas, "Reading");
        break;
    case BvStateError:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Read Implant");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, bv_error_text(m->error));
        canvas_draw_str(canvas, 2, 52, "OK: retry  Back: menu");
        break;
    case BvStateDone: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Read Implant");
        canvas_set_font(canvas, FontSecondary);
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
        bv_draw_hold(canvas, "Reading");
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

    char line[40];
    switch(m->state) {
    case BvZeroConfirm:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Wipe Implant");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 26, "Erase tag AND vault?");
        canvas_draw_str(canvas, 2, 37, "Clears Sector 1 + RAM.");
        canvas_draw_str(canvas, 2, 54, "OK: wipe   Back: cancel");
        break;
    case BvZeroWriting:
        bv_draw_hold(canvas, "Erasing");
        break;
    case BvZeroDone:
        snprintf(line, sizeof(line), "%u pages cleared", m->pages_written);
        bv_draw_done(canvas, "Wiped!", line, "Back: menu");
        break;
    case BvZeroError:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Wipe Implant");
        canvas_set_font(canvas, FontSecondary);
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

    char line[40];
    switch(m->state) {
    case BvZeroConfirm:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Save to Implant");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 26, "Write vault to Sector 1?");
        canvas_draw_str(canvas, 2, 37, "Overwrites its contents.");
        canvas_draw_str(canvas, 2, 54, "OK: save   Back: cancel");
        break;
    case BvZeroWriting:
        bv_draw_hold(canvas, "Writing");
        break;
    case BvZeroDone:
        snprintf(line, sizeof(line), "%u bytes, %u pages", m->bytes, m->pages_written);
        bv_draw_done(canvas, "Saved!", line, "Back: menu");
        break;
    case BvZeroError:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Save to Implant");
        canvas_set_font(canvas, FontSecondary);
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

    char line[40];
    switch(m->state) {
    case BvStateDone:
        snprintf(line, sizeof(line), "%u entries", m->count);
        bv_draw_done(canvas, "Vault loaded!", line, "OK: continue");
        break;
    case BvStateError:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Load from Implant");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, bv_error_text(m->error));
        canvas_draw_str(canvas, 2, 54, "OK: retry   Back: skip");
        break;
    default:
        bv_draw_hold(canvas, "Reading");
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

// --- Vault browser + entry-action menu + detail view UI ---

static void bv_edit_start(BioVault* app, uint8_t index);
static void bv_entry_menu_callback(void* context, uint32_t index);
static void bv_send_open_picker(BioVault* app);

// Build the per-entry action menu (View / Send / Edit / Remove) for the entry.
static void bv_build_entry_menu(BioVault* app) {
    submenu_reset(app->entry_menu);
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    const char* label =
        (app->selected < app->vault->count) ? app->vault->entries[app->selected].label : "Entry";
    submenu_set_header(app->entry_menu, label);
    furi_mutex_release(app->vault_mutex);
    submenu_add_item(app->entry_menu, "View", BvEntryActView, bv_entry_menu_callback, app);
    submenu_add_item(app->entry_menu, "Send (USB)", BvEntryActSend, bv_entry_menu_callback, app);
    submenu_add_item(app->entry_menu, "Edit", BvEntryActEdit, bv_entry_menu_callback, app);
    submenu_add_item(app->entry_menu, "Remove", BvEntryActRemove, bv_entry_menu_callback, app);
}

static void bv_browser_item_callback(void* context, uint32_t index) {
    BioVault* app = context;
    app->selected = (uint8_t)index;
    bv_build_entry_menu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewEntryMenu);
}

static void bv_entry_menu_callback(void* context, uint32_t index) {
    BioVault* app = context;
    switch(index) {
    case BvEntryActView:
        bv_build_detail(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewDetail);
        break;
    case BvEntryActSend:
        bv_send_open_picker(app);
        break;
    case BvEntryActEdit:
        bv_edit_start(app, app->selected);
        break;
    case BvEntryActRemove:
        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
        bv_records_remove(app->vault, app->selected);
        furi_mutex_release(app->vault_mutex);
        bv_build_browser(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewBrowser);
        break;
    default:
        break;
    }
}

static uint32_t bv_browser_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
}

static uint32_t bv_entry_menu_previous(void* context) {
    UNUSED(context);
    return BvViewBrowser;
}

// Build the scrollable detail text for the selected entry. Compact layout (no
// blank lines) to fit the small screen: label on top, then each field inline.
// The TextBox still wraps and scrolls long values.
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
                "%s\nData: %s",
                e->label,
                e->secret);
        } else {
            snprintf(
                app->detail_text,
                sizeof(app->detail_text),
                "%s\nUser: %s\nPass: %s",
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
    return BvViewEntryMenu;
}

// --- Send field over USB HID ---

// Runs on a short-lived worker thread: switch USB to HID, type the snapshotted
// field, restore USB, then hand the thread back to the main loop to be reaped.
static int32_t bv_send_worker(void* context) {
    BioVault* app = context;
    app->send_result = bv_hid_type(app->send_text);
    BvSendState st = (app->send_result == BvHidOk)   ? BvSendDone :
                     (app->send_result == BvHidBusy) ? BvSendBusy :
                                                       BvSendNoUsb;
    with_view_model(app->send_view, BvSendModel * m, { m->state = st; }, true);
    view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventHidDone);
    return 0;
}

static void bv_send_pick_callback(void* context, uint32_t index) {
    BioVault* app = context;
    if(app->send_thread) return; // a send is already in flight

    BvSendField which = (BvSendField)index;
    const char* field_name;
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    if(app->selected >= app->vault->count) {
        furi_mutex_release(app->vault_mutex);
        return;
    }
    const BvEntry* e = &app->vault->entries[app->selected];
    bool is_note = e->type == BvEntryNote;
    if(which == BvSendUser) {
        strlcpy(app->send_text, e->user, sizeof(app->send_text));
        field_name = "Username";
    } else {
        strlcpy(app->send_text, e->secret, sizeof(app->send_text));
        field_name = is_note ? "Data" : "Password";
    }
    furi_mutex_release(app->vault_mutex);

    // Optionally press Enter after a credential field (HID maps '\n' -> Return);
    // notes are left as-is so a data dump isn't auto-submitted.
    if(app->settings.send_newline && !is_note) {
        strlcat(app->send_text, "\n", sizeof(app->send_text));
    }

    with_view_model(
        app->send_view,
        BvSendModel * m,
        {
            m->state = BvSendSending;
            strlcpy(m->field, field_name, sizeof(m->field));
        },
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewSendDo);

    app->send_thread = furi_thread_alloc_ex("BvHidSend", 2048, bv_send_worker, app);
    furi_thread_start(app->send_thread);
}

// Build the "what to send" picker for the selected entry (Username/Password for
// a credential, Data for a note) and show it.
static void bv_send_open_picker(BioVault* app) {
    submenu_reset(app->send_pick);
    submenu_set_header(app->send_pick, "Send over USB");
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    if(app->selected >= app->vault->count) {
        furi_mutex_release(app->vault_mutex);
        return;
    }
    bool is_note = app->vault->entries[app->selected].type == BvEntryNote;
    furi_mutex_release(app->vault_mutex);
    if(is_note) {
        submenu_add_item(app->send_pick, "Data", BvSendSecret, bv_send_pick_callback, app);
    } else {
        submenu_add_item(app->send_pick, "Username", BvSendUser, bv_send_pick_callback, app);
        submenu_add_item(app->send_pick, "Password", BvSendSecret, bv_send_pick_callback, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewSendPick);
}

static uint32_t bv_send_pick_previous(void* context) {
    UNUSED(context);
    return BvViewEntryMenu;
}

static void bv_send_draw(Canvas* canvas, void* model) {
    BvSendModel* m = model;
    canvas_clear(canvas);
    char line[40];
    switch(m->state) {
    case BvSendSending:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Sending over USB");
        canvas_set_font(canvas, FontSecondary);
        snprintf(line, sizeof(line), "Typing %s...", m->field);
        canvas_draw_str(canvas, 2, 30, line);
        canvas_draw_str(canvas, 2, 46, "Focus the target field.");
        break;
    case BvSendDone:
        snprintf(line, sizeof(line), "%s typed", m->field);
        bv_draw_done(canvas, "Sent!", line, "Back: entry");
        break;
    case BvSendNoUsb:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "USB not connected");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "Plug into a host, then");
        canvas_draw_str(canvas, 2, 42, "retry the send.");
        canvas_draw_str(canvas, 2, 60, "Back: entry");
        break;
    case BvSendBusy:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "USB busy");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "USB mode is locked by");
        canvas_draw_str(canvas, 2, 42, "another app.");
        canvas_draw_str(canvas, 2, 60, "Back: entry");
        break;
    }
}

static bool bv_send_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort) return false;
    BvSendState st;
    with_view_model(app->send_view, BvSendModel * m, { st = m->state; }, false);
    if(st == BvSendSending) return true; // block navigation while typing
    if(event->key == InputKeyOk) {
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewEntryMenu);
        return true;
    }
    return false; // Back -> previous callback (entry menu)
}

static uint32_t bv_send_previous(void* context) {
    UNUSED(context);
    return BvViewEntryMenu;
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
        bool ok;
        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
        if(app->editing) {
            ok = (app->edit_index < app->vault->count) &&
                 bv_records_set(
                     app->vault, app->edit_index, type, app->edit_label, app->edit_user,
                     app->edit_secret);
        } else {
            ok = bv_records_add(app->vault, type, app->edit_label, app->edit_user,
                app->edit_secret);
        }
        furi_mutex_release(app->vault_mutex);
        // A full vault (or a vanished edit target) would otherwise drop the
        // typed entry with no sign anything went wrong.
        if(!ok) notification_message(app->notifications, &sequence_error);
        FURI_LOG_I(TAG, "%s %s '%s': %s", app->editing ? "edit" : "add",
            type == BvEntryNote ? "note" : "cred", app->edit_label, ok ? "ok" : "failed");
        app->editing = false;
        bv_build_browser(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewBrowser);
        break;
    }
    }
}

static void bv_configure_input(BioVault* app) {
    // When editing, the buffers are prefilled with the current values; keep them
    // (clear_default_text = false) so the user modifies rather than retypes.
    bool keep = app->editing;
    switch(app->add_state) {
    case BvAddLabel:
        bv_text_input_set_header_text(app->input, "Label (site or name)");
        bv_text_input_set_minimum_length(app->input, 1);
        bv_text_input_set_result_callback(
            app->input, bv_input_result, app, app->edit_label, sizeof(app->edit_label), !keep);
        break;
    case BvAddUser:
        bv_text_input_set_header_text(app->input, "Username (optional)");
        bv_text_input_set_minimum_length(app->input, 0);
        bv_text_input_set_result_callback(
            app->input, bv_input_result, app, app->edit_user, sizeof(app->edit_user), !keep);
        break;
    case BvAddSecret:
        bv_text_input_set_header_text(app->input, "Password / secret");
        bv_text_input_set_minimum_length(app->input, 0);
        bv_text_input_set_result_callback(
            app->input, bv_input_result, app, app->edit_secret, sizeof(app->edit_secret), !keep);
        break;
    }
}

static void bv_add_start(BioVault* app) {
    app->editing = false;
    memset(app->edit_label, 0, sizeof(app->edit_label));
    memset(app->edit_user, 0, sizeof(app->edit_user));
    memset(app->edit_secret, 0, sizeof(app->edit_secret));
    app->add_state = BvAddLabel;
    bv_configure_input(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewInput);
}

// Start the keyboard flow prefilled with an existing entry's fields; on the
// final step it updates that entry in place instead of appending a new one.
static void bv_edit_start(BioVault* app, uint8_t index) {
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    if(index >= app->vault->count) {
        furi_mutex_release(app->vault_mutex);
        return;
    }
    const BvEntry* e = &app->vault->entries[index];
    strlcpy(app->edit_label, e->label, sizeof(app->edit_label));
    strlcpy(app->edit_user, e->user, sizeof(app->edit_user));
    strlcpy(app->edit_secret, e->secret, sizeof(app->edit_secret));
    furi_mutex_release(app->vault_mutex);

    app->editing = true;
    app->edit_index = index;
    app->add_state = BvAddLabel;
    bv_configure_input(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewInput);
}

static uint32_t bv_input_previous(void* context) {
    UNUSED(context);
    return BvViewMenu; // Back cancels the add flow
}

// --- Settings ---

static const char* const bv_toggle_text[] = {"OFF", "ON"};

static void bv_settings_newline_changed(VariableItem* item) {
    BioVault* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, bv_toggle_text[idx]);
    app->settings.send_newline = (idx != 0);
    bv_settings_save(&app->settings);
}

static void bv_build_settings(BioVault* app) {
    variable_item_list_reset(app->settings_list);
    VariableItem* item = variable_item_list_add(
        app->settings_list, "Auto-Enter", 2, bv_settings_newline_changed, app);
    uint8_t idx = app->settings.send_newline ? 1 : 0;
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, bv_toggle_text[idx]);
}

static uint32_t bv_settings_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
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
    case BvMenuSettings:
        bv_build_settings(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewSettings);
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
    switch(event) {
    case BvCustomEventPollerDone:
        bv_poller_stop(app); // reap the finished poller on the main thread
        return true;
    case BvCustomEventHidDone:
        if(app->send_thread) {
            furi_thread_join(app->send_thread);
            furi_thread_free(app->send_thread);
            app->send_thread = NULL;
        }
        memset(app->send_text, 0, sizeof(app->send_text)); // don't keep the secret
        notification_message(
            app->notifications,
            app->send_result == BvHidOk ? &sequence_success : &sequence_error);
        return true;
    // CLI-driven screen switches. Read/Load auto-start via their enter callbacks;
    // Save/Zero land on their confirm screen (OK on the device commits the write).
    case BvCustomEventCliRead:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewRead);
        return true;
    case BvCustomEventCliLoad:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewLoad);
        return true;
    case BvCustomEventCliSave:
        // Same guard as the menu: never save a vault that wasn't synced first.
        view_dispatcher_switch_to_view(
            app->view_dispatcher, app->vault_loaded ? BvViewSave : BvViewLoad);
        return true;
    case BvCustomEventCliZero:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewZero);
        return true;
    default:
        return false;
    }
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

// Extract the label argument: the entire remainder of the line, trimmed, so
// labels containing spaces (enterable on the GUI keyboard) stay addressable.
static void bv_cli_arg(FuriString* args, char* out, size_t size) {
    FuriString* s = furi_string_alloc_set(args);
    furi_string_trim(s);
    strlcpy(out, furi_string_get_cstr(s), size);
    furi_string_free(s);
}

// --- Subshell subcommands (context = BioVault*) ---

// list/get snapshot the vault under the lock and print after releasing it, so a
// stalled host terminal can never block the GUI/NFC threads on vault_mutex.
static void bv_cli_list(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    BioVault* app = context;
    BvVaultData* snap = malloc(sizeof(BvVaultData));
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    *snap = *app->vault;
    furi_mutex_release(app->vault_mutex);
    printf("Vault: %u entries\r\n", snap->count);
    for(uint8_t i = 0; i < snap->count; i++) {
        const BvEntry* e = &snap->entries[i];
        printf("  [%u] %s%s\r\n", i, e->label, e->type == BvEntryNote ? "  (note)" : "");
    }
    bv_records_clear(snap);
    free(snap);
}

static void bv_cli_get(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    BioVault* app = context;
    char label[BV_LABEL_CAP];
    bv_cli_arg(args, label, sizeof(label));
    if(!strlen(label)) {
        printf("Usage: get <label>\r\n");
        return;
    }
    BvEntry e = {0};
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    int idx = bv_records_find(app->vault, label);
    if(idx >= 0) e = app->vault->entries[idx];
    furi_mutex_release(app->vault_mutex);
    if(idx < 0) {
        printf("Not found: %s\r\n", label);
        return;
    }
    printf("%s\r\n", e.label);
    if(e.type == BvEntryNote) {
        printf("  data: %s\r\n", e.secret);
    } else {
        printf("  user: %s\r\n  pass: %s\r\n", e.user, e.secret);
    }
    memset(&e, 0, sizeof(e));
}

static void bv_cli_remove(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    BioVault* app = context;
    char label[BV_LABEL_CAP];
    bv_cli_arg(args, label, sizeof(label));
    if(!strlen(label)) {
        printf("Usage: remove <label>\r\n");
        return;
    }
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    int idx = bv_records_find(app->vault, label);
    bool ok = (idx >= 0) && bv_records_remove(app->vault, (uint8_t)idx);
    furi_mutex_release(app->vault_mutex);
    printf(ok ? "Removed '%s'. Run 'save' to persist to the implant.\r\n" : "Not found: %s\r\n", label);
}

static void bv_cli_add(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    BioVault* app = context;
    char label[BV_LABEL_CAP];
    bv_cli_arg(args, label, sizeof(label));
    if(!strlen(label)) {
        printf("Usage: add <label>\r\n");
        return;
    }
    char user[BV_USER_CAP] = {0};
    char secret[BV_SECRET_CAP] = {0};
    printf("Username (blank = note): ");
    fflush(stdout);
    bv_cli_read_line(user, sizeof(user));
    printf("Secret: ");
    fflush(stdout);
    bv_cli_read_line(secret, sizeof(secret));

    BvEntryType type = (strlen(user) > 0) ? BvEntryCred : BvEntryNote;
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    bool ok = bv_records_add(app->vault, type, label, user, secret);
    furi_mutex_release(app->vault_mutex);
    printf(
        ok ? "Added '%s'. Run 'save' to persist to the implant.\r\n" :
             "Add failed (vault full or field too long).\r\n",
        label);
}

// Edit an existing entry in place. Shows current values; a blank line keeps the
// current value, so updating just the password is: Enter, then type the new one.
static void bv_cli_edit(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    BioVault* app = context;
    char label[BV_LABEL_CAP];
    bv_cli_arg(args, label, sizeof(label));
    if(!strlen(label)) {
        printf("Usage: edit <label>\r\n");
        return;
    }

    // Snapshot the current entry under the lock, then prompt without holding it.
    char user[BV_USER_CAP] = {0};
    char secret[BV_SECRET_CAP] = {0};
    char cur_label[BV_LABEL_CAP] = {0};
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    int idx = bv_records_find(app->vault, label);
    if(idx >= 0) {
        const BvEntry* e = &app->vault->entries[idx];
        strlcpy(cur_label, e->label, sizeof(cur_label));
        strlcpy(user, e->user, sizeof(user));
        strlcpy(secret, e->secret, sizeof(secret));
    }
    furi_mutex_release(app->vault_mutex);
    if(idx < 0) {
        printf("Not found: %s\r\n", label);
        return;
    }

    char in_user[BV_USER_CAP] = {0};
    char in_secret[BV_SECRET_CAP] = {0};
    printf("Editing '%s' (blank line = keep current)\r\n", cur_label);
    printf("Username [%s]: ", user);
    fflush(stdout);
    bv_cli_read_line(in_user, sizeof(in_user));
    printf("Secret [%s]: ", strlen(secret) ? "********" : "");
    fflush(stdout);
    bv_cli_read_line(in_secret, sizeof(in_secret));

    if(strlen(in_user)) strlcpy(user, in_user, sizeof(user));
    if(strlen(in_secret)) strlcpy(secret, in_secret, sizeof(secret));
    BvEntryType type = (strlen(user) > 0) ? BvEntryCred : BvEntryNote;

    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    // Re-find in case the vault changed while we were prompting.
    idx = bv_records_find(app->vault, cur_label);
    bool ok = (idx >= 0) &&
              bv_records_set(app->vault, (uint8_t)idx, type, cur_label, user, secret);
    furi_mutex_release(app->vault_mutex);
    printf(
        ok ? "Updated '%s'. Run 'save' to persist to the implant.\r\n" :
             "Edit failed (entry gone or field too long).\r\n",
        cur_label);
}

// ANSI-colored, compact banner. Accents only (\e[33m yellow, \e[1;36m bold cyan,
// \e[36m cyan; \e[0m reset); body text stays default-foreground so it's readable
// on any terminal background. Biohazard glyph is UTF-8 U+2623 (\xe2\x98\xa3).
// Screen-driving commands: post a custom event so the GUI thread switches to the
// matching view. These return immediately — the operation runs on the device
// (hold the implant to the Flipper); watch the Flipper screen for progress.
// Render a classic hex + ASCII dump of a raw Sector 1 read.
static void bv_cli_print_dump(const uint8_t* buf, size_t len) {
    for(size_t off = 0; off < len; off += 16) {
        printf("%04X  ", (unsigned)off);
        for(size_t b = 0; b < 16; b++) {
            if(off + b < len) {
                printf("%02X ", buf[off + b]);
            } else {
                printf("   ");
            }
            if(b == 7) printf(" "); // gutter between the two 8-byte halves
        }
        printf(" |");
        for(size_t b = 0; b < 16 && off + b < len; b++) {
            uint8_t c = buf[off + b];
            putchar((c >= 0x20 && c < 0x7f) ? c : '.');
        }
        printf("|\r\n");
    }
}

static void bv_cli_read(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    BioVault* app = context;

    furi_event_flag_clear(app->read_done, BV_READ_DONE_FLAG);
    view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventCliRead);
    printf("Reading implant... hold it to the Flipper.\r\n");
    fflush(stdout);

    uint32_t flags =
        furi_event_flag_wait(app->read_done, BV_READ_DONE_FLAG, FuriFlagWaitAny, 30000);
    if(flags & FuriFlagError) {
        printf("Timed out waiting for a read.\r\n");
        return;
    }

    // Snapshot the read model, then render outside the model lock.
    uint8_t* buf = malloc(SECTOR1_BYTES);
    size_t len = 0;
    uint8_t version[8] = {0};
    bool version_valid = false;
    BvState state = BvStateError;
    BvError error = BvErrNone;
    with_view_model(
        app->read_view,
        BvReadModel * m,
        {
            state = m->state;
            error = m->error;
            version_valid = m->version_valid;
            memcpy(version, m->version, sizeof(version));
            len = (m->sector1_len > SECTOR1_BYTES) ? SECTOR1_BYTES : m->sector1_len;
            memcpy(buf, m->sector1, len);
        },
        false);

    if(state != BvStateDone) {
        printf("Read failed: %s\r\n", bv_error_text(error));
        free(buf);
        return;
    }

    printf("\r\nSector 1: %u bytes", (unsigned)len);
    if(version_valid) {
        printf("   version ");
        for(size_t i = 0; i < sizeof(version); i++) printf("%02X", version[i]);
    }
    printf("\r\n");
    size_t blob_len = 0;
    if(bv_vault_framed_len(buf, len, &blob_len)) {
        printf("BioVault blob detected: %u bytes framed\r\n\r\n", (unsigned)blob_len);
    } else {
        printf("No BioVault blob (blank or foreign tag)\r\n\r\n");
    }
    bv_cli_print_dump(buf, len);
    free(buf);
}

static void bv_cli_load(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    BioVault* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventCliLoad);
    printf("Load screen open on device. Hold the implant to the Flipper,\r\n"
           "then 'list' here to see the loaded entries.\r\n");
}

static void bv_cli_save(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    BioVault* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventCliSave);
    if(app->vault_loaded) {
        printf("Save screen open on device. Press OK on the Flipper to write,\r\n"
               "then hold the implant to the Flipper.\r\n");
    } else {
        printf("Vault not synced yet — opened the Load screen first (load before\r\n"
               "save so the tag is never overwritten with un-synced data).\r\n");
    }
}

static void bv_cli_wipe(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    BioVault* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventCliZero);
    printf("Wipe screen open on device. Press OK on the Flipper to confirm, then\r\n"
           "hold the implant. Erases the tag AND clears the in-RAM vault.\r\n");
}

static void bv_cli_motd(void* context) {
    UNUSED(context);
    printf("\r\n  \e[33m\xe2\x98\xa3\e[0m \e[1;36mBioVault\e[0m \e[36mv0.1\e[0m\r\n"
           "  enclave-encrypted implant vault\r\n\r\n");
    printf("\e[36mVault:\e[0m  list, get <label>, add <label>, edit <label>, remove <label>\r\n");
    printf("\e[36mDevice:\e[0m read, load, save, wipe  (drive the on-device screens)\r\n");
    printf("\e[36mShell:\e[0m  exit\r\n");
    printf("Vault edits are in RAM; run 'save' to persist them to the implant.\r\n");
}

// The `biovault` command opens a subshell (biovault>:) with the vault commands,
// so entries can be managed without re-typing `biovault` each time.
static void bv_cli_callback(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(args);
    BioVault* app = context;

    // Track open subshells: bv_free must not tear down `app` while one is live.
    __atomic_add_fetch(&app->cli_sessions, 1, __ATOMIC_SEQ_CST);

    CliRegistry* registry = cli_registry_alloc();
    cli_registry_add_command(registry, "list", CliCommandFlagParallelSafe, bv_cli_list, app);
    cli_registry_add_command(registry, "get", CliCommandFlagParallelSafe, bv_cli_get, app);
    cli_registry_add_command(registry, "add", CliCommandFlagParallelSafe, bv_cli_add, app);
    cli_registry_add_command(registry, "edit", CliCommandFlagParallelSafe, bv_cli_edit, app);
    cli_registry_add_command(registry, "remove", CliCommandFlagParallelSafe, bv_cli_remove, app);
    cli_registry_add_command(registry, "read", CliCommandFlagParallelSafe, bv_cli_read, app);
    cli_registry_add_command(registry, "load", CliCommandFlagParallelSafe, bv_cli_load, app);
    cli_registry_add_command(registry, "save", CliCommandFlagParallelSafe, bv_cli_save, app);
    cli_registry_add_command(registry, "wipe", CliCommandFlagParallelSafe, bv_cli_wipe, app);

    CliShell* shell = cli_shell_alloc(bv_cli_motd, app, pipe, registry, NULL);
    cli_shell_set_prompt(shell, "biovault");
    cli_shell_start(shell);
    cli_shell_join(shell); // blocks until the user types `exit` or disconnects
    cli_shell_free(shell);
    cli_registry_free(registry);

    __atomic_sub_fetch(&app->cli_sessions, 1, __ATOMIC_SEQ_CST);
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
    app->read_done = furi_event_flag_alloc();

    app->nfc = nfc_alloc();
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
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
    submenu_add_item(app->menu, "Save to Implant", BvMenuSave, bv_menu_callback, app);
    submenu_add_item(app->menu, "Wipe Implant", BvMenuZero, bv_menu_callback, app);
    submenu_add_item(app->menu, "Settings", BvMenuSettings, bv_menu_callback, app);
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

    // Per-entry action menu (View / Edit / Remove, rebuilt per selection)
    app->entry_menu = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->entry_menu), bv_entry_menu_previous);
    view_dispatcher_add_view(
        app->view_dispatcher, BvViewEntryMenu, submenu_get_view(app->entry_menu));

    // Entry detail view (scrollable text box, rebuilt per entry)
    app->detail = text_box_alloc();
    view_set_previous_callback(text_box_get_view(app->detail), bv_detail_previous);
    view_dispatcher_add_view(
        app->view_dispatcher, BvViewDetail, text_box_get_view(app->detail));

    // Send-field picker (submenu, rebuilt per entry)
    app->send_pick = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->send_pick), bv_send_pick_previous);
    view_dispatcher_add_view(
        app->view_dispatcher, BvViewSendPick, submenu_get_view(app->send_pick));

    // Send progress/result view
    app->send_view = view_alloc();
    view_allocate_model(app->send_view, ViewModelTypeLocking, sizeof(BvSendModel));
    view_set_context(app->send_view, app);
    view_set_draw_callback(app->send_view, bv_send_draw);
    view_set_input_callback(app->send_view, bv_send_input);
    view_set_previous_callback(app->send_view, bv_send_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewSendDo, app->send_view);

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

    // Settings view (persisted preferences)
    bv_settings_load(&app->settings);
    app->settings_list = variable_item_list_alloc();
    view_set_previous_callback(
        variable_item_list_get_view(app->settings_list), bv_settings_previous);
    view_dispatcher_add_view(
        app->view_dispatcher, BvViewSettings, variable_item_list_get_view(app->settings_list));

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

    // Deleting the registry entry does not stop a `biovault` subshell that is
    // already open (there is no cli_shell stop API — it runs until the user
    // types `exit`), and that subshell holds `app`. Wait it out; freeing now
    // would leave the CLI thread on a dangling vault/mutex.
    while(__atomic_load_n(&app->cli_sessions, __ATOMIC_SEQ_CST) > 0) {
        furi_delay_ms(50);
    }

    // Wait out any in-flight HID send before tearing down its view/context.
    if(app->send_thread) {
        furi_thread_join(app->send_thread);
        furi_thread_free(app->send_thread);
        app->send_thread = NULL;
    }

    bv_poller_stop(app);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewRead);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewSave);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewLoad);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewEntryMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewDetail);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewSendPick);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewSendDo);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewInput);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewZero);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewDiag);
    submenu_free(app->menu);
    view_free(app->read_view);
    view_free(app->save_view);
    view_free(app->load_view);
    submenu_free(app->browser);
    submenu_free(app->entry_menu);
    submenu_free(app->send_pick);
    view_free(app->send_view);
    text_box_free(app->detail);
    bv_text_input_free(app->input);
    view_free(app->zero_view);
    variable_item_list_free(app->settings_list);
    widget_free(app->diag);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    nfc_free(app->nfc);
    bv_records_clear(app->vault);
    free(app->vault);
    furi_mutex_free(app->vault_mutex);
    furi_event_flag_free(app->read_done);
    if(app->save_blob) {
        memset(app->save_blob, 0, 1088);
        free(app->save_blob);
    }
    // Wipes the secrets still sitting in app-owned buffers (edit_* keyboard
    // buffers, detail_text) before the allocation is returned to the heap.
    memset(app, 0, sizeof(*app));
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
