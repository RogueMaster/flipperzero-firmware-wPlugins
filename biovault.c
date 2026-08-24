/*
 * BioVault - Flipper Zero FAP.
 * Menu-driven NFC vault for NTAG I2C Plus 2K implants.
 * Read path: activate -> GET_VERSION -> SECTOR SELECT 1 -> READ Sector 1.
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
#include "bv_pin.h"
#include "bv_records.h"
#include "bv_text_input.h"
#include "bv_hid.h"
#include "bv_settings.h"
#include "biovault_icons.h" // generated from images/ (fap_icon_assets)

#define TAG "BioVault"

// --- NTAG I2C Plus 2K command set ---
#define CMD_GET_VERSION     0x60
#define CMD_READ            0x30
#define CMD_WRITE           0xA2
#define CMD_PWD_AUTH        0x1B
#define CMD_SECTOR_SELECT_1 0xC2
#define CMD_SECTOR_SELECT_2 0xFF
#define NTAG_ACK            0x0A // 4-bit ACK from WRITE / sector-select packet 1

// Password & access config pages (Sector 0, NFC perspective; NT3H2211).
// Only written by the Settings provisioning action.
#define PG_AUTH0        0xE3 // byte 3 = AUTH0
#define PG_ACCESS       0xE4 // byte 0 = ACCESS (NFC_PROT b7, NFC_DIS_SEC1 b5, AUTHLIM b2-0)
#define PG_PWD          0xE5 // 4-byte password
#define PG_PACK         0xE6 // bytes 0-1 = PACK
#define PG_PT_I2C       0xE7 // byte 0 = PT_I2C (2K_PROT b3)
#define ACCESS_NFC_PROT 0x80 // read+write protection (vs write-only)
#define PT_I2C_2K_PROT  0x08 // password-protect all of Sector 1

// xSIID factory default password/PACK (Dangerous Things "DNGR"). Tag ships with
// AUTH0=0xE2, so config pages 0xE2-0xEB are write-protected by this password.
static const uint8_t XSIID_FACTORY_PWD[4] = {0x44, 0x4E, 0x47, 0x52}; // "DNGR"
static const uint8_t XSIID_FACTORY_PACK[2] = {0x00, 0x00};

static const uint8_t NTAG_I2C_PLUS_2K_VERSION[] = {0x00, 0x04, 0x04, 0x05, 0x02, 0x02, 0x15, 0x03};

#define SECTOR1            0x01
#define SECTOR1_PAGES      256u
#define SECTOR1_BYTES      (SECTOR1_PAGES * 4u) // 1024
#define READ_PAGES_PER_CMD 4u
#define READ_CMD_COUNT     (SECTOR1_PAGES / READ_PAGES_PER_CMD) // 64

#define FWT_NORMAL     60000u
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
    BvErrAuth,
    BvErrWrongImplant, // presented tag isn't the provisioned implant
    BvErrEnclave, // no device key: enclave unreachable / key store corrupt
    BvErrPinMissing, // keystore is PIN-wrapped but no PIN was entered
    BvErrKeystore, // keystore.bin unreadable; re-keying would orphan the tag
    BvErrKeystoreWrite, // could not persist a new keystore (SD gone/full)
    BvErrForeignVault, // vault decrypts to nothing: written by another Flipper
} BvError;

// Defined with the error strings, used by the NFC ops well above them.
static BvError bv_err_from_key(BvKeyStatus st);
static BvError bv_err_decrypt_failed(void);

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
    BvViewProvision,
    BvViewReveal,
    BvViewAuthWarn,
    BvViewAuthPick,
    BvViewPinWarn,
    BvViewPinMenu,
    BvViewPinEntry,
    BvViewAbout,
    BvViewDiag,
    BvViewNoKey, // startup gate when the device key is unavailable
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
    BvMenuAbout,
    BvMenuDiag,
} BvMenuIndex;

// On-device Add Entry field being edited.
typedef enum {
    BvAddLabel,
    BvAddUser,
    BvAddSecret,
} BvAddState;

// What the 6-digit PIN entry screen is collecting.
typedef enum {
    BvPinEntryUnlock, // session unlock (single entry)
    BvPinEntrySetNew, // set flow, first entry
    BvPinEntrySetConfirm, // set flow, confirmation
} BvPinEntryMode;

// What the PIN warning screen is gating.
typedef enum {
    BvPinWarnSetIntro, // consequences notice before the set-PIN flow
    BvPinWarnNeedLoad, // change/remove refused until a verified load
} BvPinWarnMode;

typedef enum {
    BvCustomEventPollerDone = 1,
    BvCustomEventHidDone, // HID send worker finished; reap on main thread
    // Posted from the CLI thread to drive GUI screens.
    BvCustomEventCliRead,
    BvCustomEventCliLoad,
    BvCustomEventCliSave,
    BvCustomEventCliZero,
    BvCustomEventCliReveal,
    BvCustomEventCliSettings, // CLI changed a setting; rebuild Settings list
    BvCustomEventCliProtect,
    BvCustomEventCliUnprotect,
} BvCustomEvent;

// Which NFC operation the shared poller is currently running.
typedef enum {
    BvOpRead,
    BvOpZero,
    BvOpSave,
    BvOpLoad,
    BvOpProvision, // write Sector 0 password/lock config
    BvOpReveal, // auth the implant, then display device-bound PWD/PACK
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
    char field[16]; // "Username" / "Password" / "Data" - what is being typed
} BvSendModel;

typedef enum {
    BvProvConfirm,
    BvProvWorking,
    BvProvDone,
    BvProvError,
} BvProvState;

typedef struct {
    BvProvState state;
    bool unprotect; // false = protect, true = unprotect
    bool protect_reads; // setting snapshot for confirm summary
    uint8_t authlim; // setting snapshot for confirm summary
    uint8_t pwd[4]; // shown after Protect (record for pm3)
    uint8_t pack[2];
    BvError error;
} BvProvModel;

typedef enum {
    BvRevealWorking, // waiting for the implant
    BvRevealShown,
    BvRevealError,
} BvRevealState;

typedef struct {
    BvRevealState state;
    uint8_t uid[10];
    size_t uid_len;
    uint8_t pwd[4];
    uint8_t pack[2];
    BvError error;
} BvRevealModel;

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

    // USB-HID send: field text snapshotted here so the worker thread never
    // touches the vault while typing. Sized for secret + optional newline.
    FuriThread* send_thread;
    char send_text[BV_SECRET_CAP + 2];
    BvHidResult send_result;
    View* zero_view;
    VariableItemList* settings_list;
    BvSettings settings;
    View* prov_view;
    bool prov_unprotect; // which provisioning op the poller runs
    View* reveal_view;
    View* auth_warn; // AUTHLIM warning gate
    Submenu* auth_pick; // AUTHLIM value picker
    Widget* about;
    Widget* diag;

    // On-device Add/Edit Entry state.
    BvAddState add_state;
    bool editing; // true: keyboard flow edits `edit_index` in place
    uint8_t edit_index; // entry being edited when `editing`
    char edit_label[BV_LABEL_CAP];
    char edit_user[BV_USER_CAP];
    char edit_secret[BV_SECRET_CAP];

    // Vault PIN session state.
    BvPinEntryMode pin_entry_mode;
    BvPinWarnMode pin_warn_mode;
    char pin_buf[BV_PIN_LEN + 1]; // entered PIN (set flow: first entry)
    char pin_buf2[BV_PIN_LEN + 1]; // set flow: confirmation entry
    bool unlock_to_settings; // re-enter flow returns to Settings, not Load
    bool dek_verified; // a GCM-authenticated load/save proved the session key
    Submenu* pin_menu;
    View* pin_warn;
    View* pin_entry;
    View* no_key; // startup gate when the device key is unavailable

    Nfc* nfc;
    NfcPoller* poller;
    bool poller_running;
    BvOp op;
    uint8_t op_fails; // consecutive partial-coupling failures this op
    uint8_t* save_blob; // sealed vault blob, prepared once per save op (heap)
    size_t save_blob_len;
    volatile uint32_t cli_sessions; // open `biovault` subshells (see bv_free)
    NotificationApp* notifications;

    BvVaultData* vault; // in-RAM vault (heap; ~8.3KB)
    FuriMutex* vault_mutex; // guards `vault` (GUI vs. CLI thread)
    uint8_t selected; // entry index shown in detail view
    bool vault_loaded; // true once synced with the tag
    FuriEventFlag* read_done; // signals a CLI `read` when the dump is ready

    bool enclave_ok;
    bool gcm_ok;
    bool dek_ok;
    bool vault_ok;
    bool records_ok;
    bool pin_ok;
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
// (<sector> 00 00 00) passively ACKs, so a poller timeout means success.
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

// No-tag retries forever; partial-coupling failures (sector select/read/write)
// get a retry budget, then surface as a real error.
#define BV_OP_MAX_FAILS 15

static bool bv_op_should_retry(BioVault* app, BvError e) {
    if(e == BvErrNoTag) return true;
    if(e == BvErrSectorSelect || e == BvErrRead || e == BvErrWrite) {
        return ++app->op_fails < BV_OP_MAX_FAILS;
    }
    return false;
}

static Iso14443_3aError
    bv_activate_uid(Iso14443_3aPoller* poller, uint8_t uid[10], size_t* uid_len);
static BvError
    bv_open_sector1(BioVault* app, Iso14443_3aPoller* poller, const uint8_t* uid, size_t uid_len);

// Full read sequence (NFC worker thread). Commits to the read model on success
// or terminal error; transient errors keep the "hold implant" screen up.
static BvError bv_do_read(BioVault* app, Iso14443_3aPoller* poller, bool* retry) {
    BvError err = BvErrNone;

    uint8_t uid[10];
    size_t uid_len = 0;
    Iso14443_3aError act = bv_activate_uid(poller, uid, &uid_len);
    if(act != Iso14443_3aErrorNone) {
        *retry = true;
        return BvErrNoTag;
    }

    uint8_t version[8] = {0};
    bool version_ok = bv_get_version(poller, version);
    bool version_match = version_ok &&
                         (memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) == 0);

    uint8_t buf[SECTOR1_BYTES];
    size_t got = 0;

    if(!version_ok) {
        err = BvErrNoTag;
    } else if(!version_match) {
        err = BvErrWrongTag;
    } else if((err = bv_open_sector1(app, poller, uid, uid_len)) != BvErrNone) {
        // err set to auth / sector-select failure
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
    // Wake any CLI `read` waiting to print the dump.
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

// PWD_AUTH (0x1B): authenticate the session for a protected Sector 1.
// Returns the tag's PACK in pack_out (may be NULL).
static bool bv_pwd_auth(Iso14443_3aPoller* poller, const uint8_t pwd[4], uint8_t pack_out[2]) {
    const uint8_t frame[5] = {CMD_PWD_AUTH, pwd[0], pwd[1], pwd[2], pwd[3]};
    BitBuffer* rx = bit_buffer_alloc(16);
    Iso14443_3aError e = bv_exchange(poller, frame, sizeof(frame), rx, FWT_NORMAL, true);
    bool ok = (e == Iso14443_3aErrorNone) && (bit_buffer_get_size_bytes(rx) >= 2);
    if(ok && pack_out) {
        pack_out[0] = bit_buffer_get_byte(rx, 0);
        pack_out[1] = bit_buffer_get_byte(rx, 1);
    }
    bit_buffer_free(rx);
    return ok;
}

// Activate the tag and extract its UID (for UID-diversified password
// derivation). Returns the activation error; uid_len is 0 if no UID read.
static Iso14443_3aError
    bv_activate_uid(Iso14443_3aPoller* poller, uint8_t uid[10], size_t* uid_len) {
    Iso14443_3aData* iso = iso14443_3a_alloc();
    Iso14443_3aError act = iso14443_3a_poller_activate(poller, iso);
    *uid_len = 0;
    if(act == Iso14443_3aErrorNone) {
        size_t n = 0;
        const uint8_t* u = iso14443_3a_get_uid(iso, &n);
        if(u && n <= 10) {
            memcpy(uid, u, n);
            *uid_len = n;
        }
    }
    iso14443_3a_free(iso);
    return act;
}

// Authenticate before Sector 1 access if the tag is provisioned. No-op
// otherwise. Password is device-bound (enclave DEK) and UID-diversified.
static BvError bv_auth_if_protected(
    BioVault* app,
    Iso14443_3aPoller* poller,
    const uint8_t* uid,
    size_t uid_len) {
    if(!app->settings.tag_protected) return BvErrNone;
    BvVaultKey key;
    BvKeyStatus ks = bv_vault_key_status(&key);
    if(ks != BvKeyOk) return bv_err_from_key(ks);
    uint8_t pwd[4], pack[2];
    bv_vault_tag_password(&key, uid, uid_len, pwd, pack);
    bv_vault_key_clear(&key);

    uint8_t got[2] = {0};
    bool ok = bv_pwd_auth(poller, pwd, got);
    // Verify PACK matches: a cloned tag can ACK the password but can't return
    // the right PACK (never exposed on READ).
    bool pack_ok = ok && (got[0] == pack[0]) && (got[1] == pack[1]);
    memset(pwd, 0, sizeof(pwd));
    memset(pack, 0, sizeof(pack));
    return pack_ok ? BvErrNone : BvErrAuth;
}

// Authenticate (if needed) then SECTOR SELECT 1 - common gate for every vault
// operation. Returns BvErrAuth/BvErrSectorSelect, a key error from
// bv_err_from_key(), or BvErrNone.
static BvError
    bv_open_sector1(BioVault* app, Iso14443_3aPoller* poller, const uint8_t* uid, size_t uid_len) {
    BvError err = bv_auth_if_protected(app, poller, uid, uid_len);
    if(err != BvErrNone) return err;
    if(!bv_select_sector(poller, SECTOR1)) return BvErrSectorSelect;
    return BvErrNone;
}

// Authenticate for a provisioning write. Try `primary`, then re-activate and
// try `fallback`. Re-activation is required: the NTAG rejects a second
// PWD_AUTH in the same activation after a wrong one.
static bool
    bv_prov_auth(Iso14443_3aPoller* poller, const uint8_t* primary, const uint8_t* fallback) {
    if(bv_pwd_auth(poller, primary, NULL)) return true;
    uint8_t uid[10];
    size_t uid_len = 0;
    if(bv_activate_uid(poller, uid, &uid_len) != Iso14443_3aErrorNone) return false;
    return bv_pwd_auth(poller, fallback, NULL);
}

// Zero all of Sector 1 user memory (NFC worker thread).
static BvError bv_do_zero(BioVault* app, Iso14443_3aPoller* poller, bool* retry) {
    BvError err = BvErrNone;
    uint16_t written = 0;

    uint8_t uid[10];
    size_t uid_len = 0;
    Iso14443_3aError act = bv_activate_uid(poller, uid, &uid_len);

    uint8_t version[8] = {0};
    bool version_ok = (act == Iso14443_3aErrorNone) && bv_get_version(poller, version);
    bool version_match = version_ok &&
                         (memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) == 0);

    if(act != Iso14443_3aErrorNone || !version_ok) {
        err = BvErrNoTag;
    } else if(!version_match) {
        err = BvErrWrongTag;
    } else if((err = bv_open_sector1(app, poller, uid, uid_len)) != BvErrNone) {
        // err set to auth / sector-select failure
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

    // On success, clear the in-RAM vault so RAM matches the blank tag.
    // vault_loaded stays true (empty + blank is synced). The wipe also
    // unblocks PIN remove/change: the ciphertext is gone, so there is
    // nothing a wrong-PIN rewrap could orphan.
    if(err == BvErrNone) {
        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
        bv_records_init(app->vault);
        furi_mutex_release(app->vault_mutex);
        app->vault_loaded = true;
        app->dek_verified = true;
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

// Serialize + seal the in-RAM vault into app->save_blob. GUI thread, once per
// save op, so poll-cycle retries never repeat the serialization/AEAD work.
static BvError bv_prepare_save_blob(BioVault* app) {
    // Whether the vault fits is only known post-compression, so seal first
    // into a worst-case buffer, then check against the tag's capacity.
    if(!app->save_blob) app->save_blob = malloc(BV_SERIALIZED_MAX + 1 + BV_BLOB_OVERHEAD);
    app->save_blob_len = 0;

    uint8_t* pt = malloc(BV_SERIALIZED_MAX); // too big for the stack
    size_t pt_len = 0;
    BvError err = BvErrNone;

    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    bool ser_ok = bv_records_serialize(app->vault, pt, BV_SERIALIZED_MAX, &pt_len);
    furi_mutex_release(app->vault_mutex);

    if(!ser_ok) {
        err = BvErrTooBig;
    } else {
        BvVaultKey key;
        BvKeyStatus ks = bv_vault_key_status(&key);
        if(ks == BvKeyOk) {
            if(!bv_vault_encrypt(&key, pt, pt_len, app->save_blob, &app->save_blob_len)) {
                err = BvErrCrypto;
            } else if(app->save_blob_len > SECTOR1_BYTES) {
                err = BvErrTooBig;
                app->save_blob_len = 0; // never leave an unwritable blob armed
            }
            bv_vault_key_clear(&key);
        } else {
            err = bv_err_from_key(ks);
        }
    }
    memset(pt, 0, BV_SERIALIZED_MAX);
    free(pt);
    return err;
}

// Write the prepared blob across Sector 1 pages (last page zero-padded).
// NFC worker thread.
static BvError bv_do_save(BioVault* app, Iso14443_3aPoller* poller, bool* retry) {
    BvError err = BvErrNone;
    uint16_t written = 0;

    uint8_t uid[10];
    size_t uid_len = 0;
    Iso14443_3aError act = bv_activate_uid(poller, uid, &uid_len);
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
    } else if((err = bv_open_sector1(app, poller, uid, uid_len)) != BvErrNone) {
        // err set to auth / sector-select failure
    } else {
        // Blank header page first, write real header (page 0) last: a torn save
        // reads back as an empty tag, not a valid header over stale ciphertext.
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

    // A completed save proves the session key: the tag now holds a blob
    // sealed under it.
    if(err == BvErrNone) app->dek_verified = true;

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
        TAG, "save complete: err=%d bytes=%u pages=%u", err, (unsigned)app->save_blob_len, written);
    return err;
}

// Read Sector 1 -> parse the vault blob -> decrypt -> populate the in-RAM vault.
static BvError bv_do_load(BioVault* app, Iso14443_3aPoller* poller, bool* retry) {
    BvError err = BvErrNone;
    uint8_t count = 0;

    uint8_t* buf = malloc(SECTOR1_BYTES);
    uint8_t* pt = malloc(BV_SERIALIZED_MAX); // v2 blobs decompress past 1KB

    uint8_t uid[10];
    size_t uid_len = 0;
    Iso14443_3aError act = bv_activate_uid(poller, uid, &uid_len);
    uint8_t version[8] = {0};
    bool vok = (act == Iso14443_3aErrorNone) && bv_get_version(poller, version);
    bool vmatch = vok && (memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) == 0);

    if(!vok) {
        err = BvErrNoTag;
    } else if(!vmatch) {
        err = BvErrWrongTag;
    } else if((err = bv_open_sector1(app, poller, uid, uid_len)) != BvErrNone) {
        // err set to auth / sector-select failure
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
                // Empty / non-vault tag: treated as a fresh empty vault. This
                // also unblocks PIN remove/change: with no ciphertext in
                // existence there is nothing a wrong-PIN rewrap could orphan.
                furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
                bv_records_init(app->vault);
                furi_mutex_release(app->vault_mutex);
                count = 0;
                app->dek_verified = true;
            } else {
                BvVaultKey key;
                BvKeyStatus ks = bv_vault_key_status(&key);
                if(ks != BvKeyOk) {
                    err = bv_err_from_key(ks);
                } else {
                    size_t pt_len = 0;
                    if(bv_vault_decrypt(&key, buf, blob_len, pt, BV_SERIALIZED_MAX, &pt_len)) {
                        furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
                        if(bv_records_parse(app->vault, pt, pt_len)) {
                            count = app->vault->count;
                            // GCM authenticated: this session's key (and PIN,
                            // if any) is proven correct.
                            app->dek_verified = true;
                        } else {
                            err = BvErrCrypto;
                        }
                        furi_mutex_release(app->vault_mutex);
                    } else {
                        // The tag holds a well-framed vault this key cannot
                        // open: a foreign Flipper's DEK, or the wrong PIN.
                        err = bv_err_decrypt_failed();
                    }
                    bv_vault_key_clear(&key);
                }
            }
        }
    }

    memset(pt, 0, BV_SERIALIZED_MAX);
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

// Write Sector 0 password/lock config (protect) or restore defaults
// (unprotect). NFC worker thread; the only path that writes Sector 0.
static BvError bv_do_provision(BioVault* app, Iso14443_3aPoller* poller, bool* retry) {
    bool unprotect = app->prov_unprotect;

    uint8_t uid[10];
    size_t uid_len = 0;
    Iso14443_3aError act = bv_activate_uid(poller, uid, &uid_len);
    uint8_t version[8] = {0};
    bool vok = (act == Iso14443_3aErrorNone) && bv_get_version(poller, version);
    if(!vok) {
        *retry = true; // wait for the implant before any write starts
        return BvErrNoTag;
    }
    *retry = false; // past here, failures are surfaced, never re-run
    if(memcmp(version, NTAG_I2C_PLUS_2K_VERSION, sizeof(version)) != 0) return BvErrWrongTag;

    BvVaultKey key;
    BvKeyStatus ks = bv_vault_key_status(&key);
    if(ks != BvKeyOk) return bv_err_from_key(ks);
    uint8_t pwd[4], pack[2];
    bv_vault_tag_password(&key, uid, uid_len, pwd, pack); // UID-diversified
    bv_vault_key_clear(&key);

    BvError err = BvErrNone;

    // Config pages are write-protected, so each provisioning write needs a prior
    // PWD_AUTH. Primary = the password the tag should hold (factory DNGR if not
    // provisioned, device-bound if provisioned); the other is the fallback.
    const uint8_t* primary = app->settings.tag_protected ? pwd : XSIID_FACTORY_PWD;
    const uint8_t* fallback = app->settings.tag_protected ? XSIID_FACTORY_PWD : pwd;
    if(!bv_prov_auth(poller, primary, fallback)) {
        err = BvErrAuth;
    }

    if(err == BvErrNone && !unprotect) {
        // PROTECT. Write PACK, ACCESS, PT_I2C first, change PWD last: the PWD
        // change could end the session, so no later write depends on it. A torn
        // write before PWD leaves the prior password, recoverable via two-try auth.
        uint8_t pg_pwd[4] = {pwd[0], pwd[1], pwd[2], pwd[3]};
        uint8_t pg_pack[4] = {pack[0], pack[1], 0, 0};
        uint8_t access = (uint8_t)((app->settings.protect_reads ? ACCESS_NFC_PROT : 0) |
                                   (app->settings.authlim & 0x07));
        uint8_t pg_access[4] = {access, 0, 0, 0};
        uint8_t pg_pt[4] = {PT_I2C_2K_PROT, 0, 0, 0};
        if(!bv_write_page(poller, PG_PACK, pg_pack) ||
           !bv_write_page(poller, PG_ACCESS, pg_access) ||
           !bv_write_page(poller, PG_PT_I2C, pg_pt) || !bv_write_page(poller, PG_PWD, pg_pwd)) {
            err = BvErrWrite;
        }
    } else if(err == BvErrNone && unprotect) {
        // UNPROTECT. Disable Sector 1 protection and clear access flags first,
        // then restore factory DNGR password/PACK (PWD last so a torn write
        // leaves the still-known device password).
        uint8_t zero[4] = {0, 0, 0, 0};
        uint8_t pg_pack[4] = {XSIID_FACTORY_PACK[0], XSIID_FACTORY_PACK[1], 0, 0};
        uint8_t pg_pwd[4] = {
            XSIID_FACTORY_PWD[0], XSIID_FACTORY_PWD[1], XSIID_FACTORY_PWD[2], XSIID_FACTORY_PWD[3]};
        if(!bv_write_page(poller, PG_PT_I2C, zero) || !bv_write_page(poller, PG_ACCESS, zero) ||
           !bv_write_page(poller, PG_PACK, pg_pack) || !bv_write_page(poller, PG_PWD, pg_pwd)) {
            err = BvErrWrite;
        }
    }

    if(err == BvErrNone) {
        app->settings.tag_protected = !unprotect;
        if(unprotect) {
            // Tag is back to factory; reset provisioning params to defaults.
            app->settings.protect_reads = false;
            app->settings.authlim = 0;
        }
        bv_settings_save(&app->settings);
    }

    with_view_model(
        app->prov_view,
        BvProvModel * m,
        {
            m->state = (err == BvErrNone) ? BvProvDone : BvProvError;
            m->error = err;
            if(err == BvErrNone && !unprotect) {
                memcpy(m->pwd, pwd, sizeof(pwd)); // display for pm3/recovery
                memcpy(m->pack, pack, sizeof(pack));
            }
        },
        true);
    memset(pwd, 0, sizeof(pwd)); // wipe after the display copy
    memset(pack, 0, sizeof(pack));
    FURI_LOG_I(TAG, "provision(%s) complete: err=%d", unprotect ? "off" : "on", err);
    return err;
}

// Reveal the device-bound PWD/PACK, gated on the implant being present.
// NFC worker thread.
static BvError bv_do_reveal(BioVault* app, Iso14443_3aPoller* poller, bool* retry) {
    uint8_t uid[10];
    size_t uid_len = 0;
    Iso14443_3aError act = bv_activate_uid(poller, uid, &uid_len);
    if(act != Iso14443_3aErrorNone || uid_len == 0) {
        *retry = true; // keep waiting for the implant
        return BvErrNoTag;
    }
    *retry = false;

    BvError err = BvErrNone;
    uint8_t pwd[4] = {0}, pack[2] = {0};

    if(!app->settings.tag_protected) {
        err = BvErrNoVault; // nothing provisioned, nothing to reveal
    } else {
        BvVaultKey key;
        BvKeyStatus ks = bv_vault_key_status(&key);
        if(ks != BvKeyOk) {
            err = bv_err_from_key(ks);
        } else {
            bv_vault_tag_password(&key, uid, uid_len, pwd, pack);
            bv_vault_key_clear(&key);
            // Auth against the presented tag: only the genuine implant holds the
            // enclave-derived PWD/PACK, so a UID spoof cannot pass.
            uint8_t got[2] = {0};
            bool authed = bv_pwd_auth(poller, pwd, got) && (got[0] == pack[0]) &&
                          (got[1] == pack[1]);
            if(!authed) err = BvErrWrongImplant;
        }
    }

    with_view_model(
        app->reveal_view,
        BvRevealModel * m,
        {
            if(err == BvErrNone) {
                memcpy(m->uid, uid, uid_len);
                m->uid_len = uid_len;
                memcpy(m->pwd, pwd, sizeof(pwd));
                memcpy(m->pack, pack, sizeof(pack));
                m->state = BvRevealShown;
            } else {
                m->state = BvRevealError;
                m->error = err;
            }
        },
        true);
    memset(pwd, 0, sizeof(pwd));
    memset(pack, 0, sizeof(pack));
    // Wake a CLI `reveal` waiting to print the result.
    furi_event_flag_set(app->read_done, BV_READ_DONE_FLAG);
    FURI_LOG_I(TAG, "reveal: err=%d", err);
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
        } else if(app->op == BvOpProvision) {
            err = bv_do_provision(app, poller, &retry);
        } else if(app->op == BvOpReveal) {
            err = bv_do_reveal(app, poller, &retry);
        } else {
            err = bv_do_read(app, poller, &retry);
        }
        if(retry) {
            // Tag not coupled yet - reset the field and keep polling.
            cmd = NfcCommandReset;
        } else {
            notification_message(
                app->notifications, err == BvErrNone ? &sequence_success : &sequence_error);
            cmd = NfcCommandStop;
        }
    }
    if(cmd == NfcCommandStop) {
        // Hand off to the main thread to reap the poller.
        view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventPollerDone);
    }
    return cmd;
}

static void bv_start_op(BioVault* app, BvOp op) {
    if(app->poller_running) return;
    app->op = op;
    app->op_fails = 0;
    // Cyan blink while reading, magenta while writing.
    notification_message(
        app->notifications,
        (op == BvOpRead || op == BvOpLoad || op == BvOpReveal) ? &sequence_blink_start_cyan :
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
    // Seal the vault first; an unsealable vault errors out without polling.
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

static void bv_start_provision(BioVault* app, bool unprotect) {
    if(app->poller_running) return;
    app->prov_unprotect = unprotect;
    with_view_model(
        app->prov_view,
        BvProvModel * m,
        {
            m->state = BvProvWorking;
            m->error = BvErrNone;
        },
        true);
    bv_start_op(app, BvOpProvision);
}

static void bv_start_reveal(BioVault* app) {
    if(app->poller_running) return;
    with_view_model(
        app->reveal_view,
        BvRevealModel * m,
        {
            m->state = BvRevealWorking;
            m->error = BvErrNone;
        },
        true);
    bv_start_op(app, BvOpReveal);
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
        return "Vault would not decrypt";
    case BvErrAuth:
        return "Auth failed (wrong device?)";
    case BvErrWrongImplant:
        return "Not the provisioned implant";
    case BvErrNoVault:
        return "No provisioned implant";
    case BvErrEnclave:
        return "No device key";
    case BvErrPinMissing:
        return "Vault PIN not entered";
    case BvErrKeystore:
        return "Keystore unreadable";
    case BvErrKeystoreWrite:
        return "Cannot save keystore";
    case BvErrForeignVault:
        return "Vault from another Flipper";
    default:
        return "Unknown error";
    }
}

// Key-related failures are the ones a user cannot diagnose from the error line
// alone, so each carries a short "what now". NULL-terminated, at most 3 lines
// of ~29 chars (FontSecondary at x=2). NULL when the error speaks for itself.
static const char* const* bv_error_hint(BvError e) {
    static const char* const enclave[] = {
        "Flipper's secure enclave is",
        "unreachable. Reboot; if it",
        "persists, update firmware.",
        NULL};
    static const char* const pin_missing[] = {
        "Settings > Vault PIN >", "Re-enter PIN, then retry.", NULL};
    static const char* const keystore[] = {
        "keystore.bin is damaged.", "Restore your backup copy;", "re-keying would lose it.", NULL};
    static const char* const keystore_write[] = {
        "Could not write to the SD", "card. Check it is inserted", "and has free space.", NULL};
    static const char* const foreign[] = {
        "Its key never left the", "Flipper that wrote it, so", "this one cannot unlock it.", NULL};
    static const char* const crypto[] = {
        "Wrong PIN, or the vault was", "written by another Flipper.", NULL};

    switch(e) {
    case BvErrEnclave:
        return enclave;
    case BvErrPinMissing:
        return pin_missing;
    case BvErrKeystore:
        return keystore;
    case BvErrKeystoreWrite:
        return keystore_write;
    case BvErrForeignVault:
        return foreign;
    case BvErrCrypto:
        return crypto;
    default:
        return NULL;
    }
}

// Map a keystore open failure to the error the user will see.
static BvError bv_err_from_key(BvKeyStatus st) {
    switch(st) {
    case BvKeyNoEnclave:
        return BvErrEnclave;
    case BvKeyPinRequired:
        return BvErrPinMissing;
    case BvKeyCorrupt:
        return BvErrKeystore;
    case BvKeyStoreFail:
        return BvErrKeystoreWrite;
    default:
        return BvErrCrypto;
    }
}

// A vault that fails GCM auth was either sealed with a different PIN or on a
// different Flipper. With no PIN set only the second is possible, so say so.
static BvError bv_err_decrypt_failed(void) {
    return bv_vault_pin_required() ? BvErrCrypto : BvErrForeignVault;
}

// Same explanation over the CLI, where there is no screen to draw it on.
static void bv_cli_print_hint(BvError e) {
    const char* const* hint = bv_error_hint(e);
    for(size_t i = 0; hint && hint[i]; i++)
        printf("  %s\r\n", hint[i]);
}

// Shared error body for the op views: the caller draws the title, this draws
// the error line, its hint, and the footer. The optional `extra` line is
// dropped when a hint is present; the hint is the more useful of the two.
static void bv_draw_error(Canvas* canvas, BvError e, const char* extra, const char* footer) {
    canvas_set_font(canvas, FontSecondary);
    const char* const* hint = bv_error_hint(e);
    uint8_t y = hint ? 22 : 28;
    canvas_draw_str(canvas, 2, y, bv_error_text(e));
    if(hint) {
        for(size_t i = 0; hint[i]; i++)
            canvas_draw_str(canvas, 2, (uint8_t)(y + 10 + i * 9), hint[i]);
    } else if(extra) {
        canvas_draw_str(canvas, 2, 40, extra);
    }
    canvas_draw_str(canvas, 2, 62, footer);
}

// Full-screen implant-coupling prompt: art on the left, "<verb> implant" centered.
static void bv_draw_hold(Canvas* canvas, const char* verb) {
    canvas_draw_icon(canvas, 0, 8, &I_NFC_manual_60x50);
    const uint8_t cx = 94; // center of region right of the art
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
        bv_draw_error(canvas, m->error, NULL, "OK: retry  Back: menu");
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
        snprintf(line, sizeof(line), "Wrote %u pages", m->pages_written);
        bv_draw_error(canvas, m->error, line, "OK: retry   Back: menu");
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
        bv_draw_error(canvas, m->error, NULL, "OK: retry   Back: menu");
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
        bv_draw_error(canvas, m->error, NULL, "OK: retry   Back: skip");
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

// Build the scrollable detail text for the selected entry.
static void bv_build_detail(BioVault* app) {
    text_box_reset(app->detail);
    furi_mutex_acquire(app->vault_mutex, FuriWaitForever);
    if(app->selected >= app->vault->count) {
        app->detail_text[0] = '\0';
    } else {
        const BvEntry* e = &app->vault->entries[app->selected];
        if(e->type == BvEntryNote) {
            snprintf(
                app->detail_text, sizeof(app->detail_text), "%s\nData: %s", e->label, e->secret);
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

// Worker thread: switch USB to HID, type the snapshotted field, restore USB,
// then hand the thread back to the main loop to be reaped.
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

    // Optionally press Enter after a credential field (notes left as-is).
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

// Build and show the "what to send" picker for the selected entry.
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
static void bv_build_settings(BioVault* app);
static void bv_input_result(void* context);
static uint32_t bv_input_previous(void* context);

// --- Vault PIN entry screen (6 digit wheels) ---

typedef struct {
    uint8_t digits[BV_PIN_LEN];
    uint8_t pos;
    const char* header;
    bool busy; // derive in progress: GUI thread redraws while dispatcher blocks
} BvPinEntryModel;

static uint32_t bv_pin_entry_previous_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE; // Back during startup unlock exits the app
}

static uint32_t bv_pin_entry_previous_settings(void* context) {
    UNUSED(context);
    return BvViewSettings;
}

static void bv_pin_wipe_bufs(BioVault* app) {
    memset(app->pin_buf, 0, sizeof(app->pin_buf));
    memset(app->pin_buf2, 0, sizeof(app->pin_buf2));
}

static void bv_pin_entry_draw(Canvas* canvas, void* model) {
    BvPinEntryModel* m = model;
    canvas_clear(canvas);
    if(m->busy) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignBottom, "Deriving key...");
        return;
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, m->header);

    // Six digit cells, selected one inverted.
    const uint8_t cw = 14, ch = 21, gap = 4;
    const uint8_t x0 = (128 - (BV_PIN_LEN * cw + (BV_PIN_LEN - 1) * gap)) / 2;
    const uint8_t y0 = 20;
    canvas_set_font(canvas, FontBigNumbers);
    for(uint8_t i = 0; i < BV_PIN_LEN; i++) {
        uint8_t x = x0 + i * (cw + gap);
        char d[2] = {(char)('0' + m->digits[i]), 0};
        if(i == m->pos) {
            canvas_draw_rbox(canvas, x, y0, cw, ch, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, x + cw / 2, y0 + ch - 3, AlignCenter, AlignBottom, d);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_rframe(canvas, x, y0, cw, ch, 2);
            canvas_draw_str_aligned(canvas, x + cw / 2, y0 + ch - 3, AlignCenter, AlignBottom, d);
        }
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 62, AlignCenter, AlignBottom, "Up/Down: digit   OK: confirm");
}

static void bv_pin_entry_confirm(BioVault* app);

static bool bv_pin_entry_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyBack) return false; // previous callback decides

    bool confirm = false;
    with_view_model(
        app->pin_entry,
        BvPinEntryModel * m,
        {
            switch(event->key) {
            case InputKeyUp:
                m->digits[m->pos] = (m->digits[m->pos] + 1) % 10;
                break;
            case InputKeyDown:
                m->digits[m->pos] = (m->digits[m->pos] + 9) % 10;
                break;
            case InputKeyLeft:
                m->pos = (m->pos + BV_PIN_LEN - 1) % BV_PIN_LEN;
                break;
            case InputKeyRight:
                m->pos = (m->pos + 1) % BV_PIN_LEN;
                break;
            case InputKeyOk:
                confirm = (event->type == InputTypeShort);
                break;
            default:
                break;
            }
        },
        true);
    if(confirm) bv_pin_entry_confirm(app);
    return true;
}

// Show the PIN screen. Digits reset to 000000; Back exits the app during the
// startup unlock, otherwise returns to Settings.
static void bv_pin_entry_open(BioVault* app, BvPinEntryMode mode, const char* header) {
    app->pin_entry_mode = mode;
    with_view_model(
        app->pin_entry,
        BvPinEntryModel * m,
        {
            memset(m->digits, 0, sizeof(m->digits));
            m->pos = 0;
            m->header = header;
            m->busy = false;
        },
        true);
    bool startup_unlock = (mode == BvPinEntryUnlock) && !app->unlock_to_settings;
    view_set_previous_callback(
        app->pin_entry,
        startup_unlock ? bv_pin_entry_previous_exit : bv_pin_entry_previous_settings);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewPinEntry);
}

// Derive the unlock key from the entered PIN (blocks ~3s by design). The GUI
// thread keeps drawing, so flip the screen to its busy state first.
static void bv_pin_unlock_finish(BioVault* app) {
    with_view_model(app->pin_entry, BvPinEntryModel * m, { m->busy = true; }, true);
    uint8_t salt[BV_PIN_SALT_SIZE];
    uint32_t sw = 0, hw = 0;
    uint8_t key[BV_PIN_KEY_SIZE];
    bool ok = bv_vault_pin_params(salt, &sw, &hw) &&
              bv_pin_derive(app->pin_buf, salt, sw, hw, key);
    if(ok) bv_vault_unlock_key_set(key);
    memset(key, 0, sizeof(key));
    bv_pin_wipe_bufs(app);
    if(!ok) notification_message(app->notifications, &sequence_error);
    if(app->unlock_to_settings) {
        app->unlock_to_settings = false;
        bv_build_settings(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewSettings);
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewLoad);
    }
}

static void bv_pin_set_finish(BioVault* app) {
    with_view_model(app->pin_entry, BvPinEntryModel * m, { m->busy = true; }, true);
    bool ok = bv_vault_pin_enable(app->pin_buf);
    bv_pin_wipe_bufs(app);
    notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
    FURI_LOG_I(TAG, "PIN set: %s", ok ? "ok" : "failed");
    bv_build_settings(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewSettings);
}

static void bv_pin_entry_confirm(BioVault* app) {
    char pin[BV_PIN_LEN + 1] = {0};
    with_view_model(
        app->pin_entry,
        BvPinEntryModel * m,
        {
            for(uint8_t i = 0; i < BV_PIN_LEN; i++)
                pin[i] = (char)('0' + m->digits[i]);
            memset(m->digits, 0, sizeof(m->digits));
        },
        false);

    switch(app->pin_entry_mode) {
    case BvPinEntryUnlock:
        strlcpy(app->pin_buf, pin, sizeof(app->pin_buf));
        bv_pin_unlock_finish(app);
        break;
    case BvPinEntrySetNew:
        strlcpy(app->pin_buf, pin, sizeof(app->pin_buf));
        bv_pin_entry_open(app, BvPinEntrySetConfirm, "Repeat new PIN");
        break;
    case BvPinEntrySetConfirm:
        strlcpy(app->pin_buf2, pin, sizeof(app->pin_buf2));
        if(strcmp(app->pin_buf, app->pin_buf2) != 0) {
            notification_message(app->notifications, &sequence_error);
            bv_pin_wipe_bufs(app);
            bv_pin_entry_open(app, BvPinEntrySetNew, "Mismatch - new PIN");
        } else {
            bv_pin_set_finish(app);
        }
        break;
    }
    memset(pin, 0, sizeof(pin));
}

// On keyboard confirm: advance label -> user -> secret, then commit the entry.
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
            ok = (app->edit_index < app->vault->count) && bv_records_set(
                                                              app->vault,
                                                              app->edit_index,
                                                              type,
                                                              app->edit_label,
                                                              app->edit_user,
                                                              app->edit_secret);
        } else {
            ok = bv_records_add(
                app->vault, type, app->edit_label, app->edit_user, app->edit_secret);
        }
        furi_mutex_release(app->vault_mutex);
        // Signal a dropped entry (full vault or vanished edit target).
        if(!ok) notification_message(app->notifications, &sequence_error);
        FURI_LOG_I(
            TAG,
            "%s %s '%s': %s",
            app->editing ? "edit" : "add",
            type == BvEntryNote ? "note" : "cred",
            app->edit_label,
            ok ? "ok" : "failed");
        app->editing = false;
        bv_build_browser(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewBrowser);
        break;
    }
    }
}

static void bv_configure_input(BioVault* app) {
    // When editing, keep prefilled buffers so the user modifies rather than retypes.
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

// Start the keyboard flow prefilled with an entry's fields; updates in place.
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
static const char* const bv_authlim_text[] = {"OFF", "2", "4", "8", "16", "32", "64", "128"};

// Settings row order (indices used by the enter callback for the action rows).
typedef enum {
    BvSetAutoEnter,
    BvSetReadProt,
    BvSetAuthLim,
    BvSetProtect,
    BvSetUnprotect,
    BvSetReveal,
    BvSetPin,
} BvSettingsRow;

static void bv_settings_newline_changed(VariableItem* item) {
    BioVault* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, bv_toggle_text[idx]);
    app->settings.send_newline = (idx != 0);
    bv_settings_save(&app->settings);
}

static void bv_settings_readprot_changed(VariableItem* item) {
    BioVault* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, bv_toggle_text[idx]);
    app->settings.protect_reads = (idx != 0);
    bv_settings_save(&app->settings);
}

// AUTHLIM is destructive (a low value can brick the tag): OK opens a warning,
// then a picker, not a plain toggle.
static void bv_prov_open(BioVault* app, bool unprotect); // defined below

static void bv_build_pin_menu(BioVault* app);
static void bv_pin_warn_open(BioVault* app, BvPinWarnMode mode);

static void bv_settings_enter(void* context, uint32_t index) {
    BioVault* app = context;
    if(index == BvSetProtect) {
        bv_prov_open(app, false);
    } else if(index == BvSetUnprotect) {
        bv_prov_open(app, true);
    } else if(index == BvSetReveal) {
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewReveal);
    } else if(index == BvSetAuthLim) {
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewAuthWarn);
    } else if(index == BvSetPin) {
        if(!bv_vault_pin_required()) {
            bv_pin_warn_open(app, BvPinWarnSetIntro);
        } else {
            bv_build_pin_menu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, BvViewPinMenu);
        }
    }
}

// --- Vault PIN warning gate + menu ---

static void bv_pin_warn_draw(Canvas* canvas, void* model) {
    BvPinWarnMode* mode = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    if(*mode == BvPinWarnSetIntro) {
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "! Vault PIN !");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 22, "A 6-digit PIN becomes part");
        canvas_draw_str(canvas, 2, 32, "of the vault key. A wrong PIN");
        canvas_draw_str(canvas, 2, 42, "is undetectable, a forgotten");
        canvas_draw_str(canvas, 2, 52, "PIN = vault LOST. No reset.");
        canvas_draw_str(canvas, 2, 62, "OK: set PIN  Back: cancel");
    } else {
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "PIN not verified");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "Load your vault first so this");
        canvas_draw_str(canvas, 2, 34, "session's PIN is proven, then");
        canvas_draw_str(canvas, 2, 44, "remove the PIN.");
        canvas_draw_str(canvas, 2, 62, "OK / Back: return");
    }
}

// Set the warning mode (app state + view model) and show the gate.
static void bv_pin_warn_open(BioVault* app, BvPinWarnMode mode) {
    app->pin_warn_mode = mode;
    with_view_model(app->pin_warn, BvPinWarnMode * m, { *m = mode; }, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewPinWarn);
}

static bool bv_pin_warn_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort) return false;
    if(app->pin_warn_mode == BvPinWarnSetIntro && event->key == InputKeyOk) {
        bv_pin_entry_open(app, BvPinEntrySetNew, "New vault PIN");
        return true;
    }
    if(event->key == InputKeyOk) {
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewSettings);
        return true;
    }
    return false; // Back falls through to the previous callback (Settings)
}

typedef enum {
    BvPinMenuReenter,
    BvPinMenuRemove,
} BvPinMenuIndex;

static void bv_pin_menu_callback(void* context, uint32_t index) {
    BioVault* app = context;
    if(index == BvPinMenuReenter) {
        app->unlock_to_settings = true;
        bv_pin_entry_open(app, BvPinEntryUnlock, "Vault PIN");
    } else if(index == BvPinMenuRemove) {
        if(!app->dek_verified) {
            bv_pin_warn_open(app, BvPinWarnNeedLoad);
            return;
        }
        bool ok = bv_vault_pin_disable();
        notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
        FURI_LOG_I(TAG, "PIN remove: %s", ok ? "ok" : "failed");
        bv_build_settings(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewSettings);
    }
}

static void bv_build_pin_menu(BioVault* app) {
    submenu_reset(app->pin_menu);
    submenu_set_header(app->pin_menu, "Vault PIN");
    submenu_add_item(app->pin_menu, "Re-enter PIN", BvPinMenuReenter, bv_pin_menu_callback, app);
    submenu_add_item(app->pin_menu, "Remove PIN", BvPinMenuRemove, bv_pin_menu_callback, app);
}

static void bv_build_settings(BioVault* app) {
    variable_item_list_reset(app->settings_list);
    VariableItem* item;
    uint8_t idx;

    item = variable_item_list_add(
        app->settings_list, "USB Auto-Return", 2, bv_settings_newline_changed, app);
    idx = app->settings.send_newline ? 1 : 0;
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, bv_toggle_text[idx]);

    item = variable_item_list_add(
        app->settings_list, "Read protect", 2, bv_settings_readprot_changed, app);
    idx = app->settings.protect_reads ? 1 : 0;
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, bv_toggle_text[idx]);

    // Auth limit is OK-gated (warning -> picker); shows only its current value.
    item = variable_item_list_add(app->settings_list, "Auth limit", 1, NULL, app);
    idx = app->settings.authlim < COUNT_OF(bv_authlim_text) ? app->settings.authlim : 0;
    variable_item_set_current_value_text(item, bv_authlim_text[idx]);

    // Action rows (OK-handled by the enter callback).
    item = variable_item_list_add(app->settings_list, "Protect implant", 1, NULL, app);
    variable_item_set_current_value_text(item, app->settings.tag_protected ? "ON" : "OFF");
    item = variable_item_list_add(app->settings_list, "Unprotect implant", 1, NULL, app);
    variable_item_set_current_value_text(item, "");
    item = variable_item_list_add(app->settings_list, "Reveal password", 1, NULL, app);
    variable_item_set_current_value_text(item, "");
    item = variable_item_list_add(app->settings_list, "Vault PIN", 1, NULL, app);
    variable_item_set_current_value_text(item, bv_vault_pin_required() ? "SET" : "OFF");
}

static uint32_t bv_settings_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
}

// --- Auth limit warning + picker ---

static void bv_auth_pick_callback(void* context, uint32_t index) {
    BioVault* app = context;
    app->settings.authlim = (uint8_t)index; // 0 = off, else 2^index
    bv_settings_save(&app->settings);
    bv_build_settings(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewSettings);
}

static void bv_build_auth_pick(BioVault* app) {
    submenu_reset(app->auth_pick);
    submenu_set_header(app->auth_pick, "Max failed unlocks");
    for(uint8_t i = 0; i < COUNT_OF(bv_authlim_text); i++) {
        submenu_add_item(app->auth_pick, bv_authlim_text[i], i, bv_auth_pick_callback, app);
    }
    submenu_set_selected_item(
        app->auth_pick,
        app->settings.authlim < COUNT_OF(bv_authlim_text) ? app->settings.authlim : 0);
}

static void bv_auth_warn_draw(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "! Auth Limit !");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, "Failed unlocks past the limit");
    canvas_draw_str(canvas, 2, 32, "PERMANENTLY lock the vault");
    canvas_draw_str(canvas, 2, 42, "+ config pages. No undo.");
    canvas_draw_str(canvas, 2, 52, "Recommend a high value.");
    canvas_draw_str(canvas, 2, 62, "OK: choose  Back: cancel");
}

static bool bv_auth_warn_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyOk) {
        bv_build_auth_pick(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewAuthPick);
        return true;
    }
    return false; // Back -> previous callback (settings)
}

static uint32_t bv_auth_return_settings(void* context) {
    UNUSED(context);
    return BvViewSettings;
}

// --- Provisioning confirm/progress view ---

static void bv_prov_draw(Canvas* canvas, void* model) {
    BvProvModel* m = model;
    canvas_clear(canvas);
    char line[44];
    switch(m->state) {
    case BvProvConfirm:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, m->unprotect ? "Unprotect Implant" : "Protect Implant");
        canvas_set_font(canvas, FontSecondary);
        if(m->unprotect) {
            canvas_draw_str(canvas, 2, 24, "Remove tag password?");
            canvas_draw_str(canvas, 2, 35, "Restores open access.");
        } else {
            snprintf(
                line,
                sizeof(line),
                "%s, authlim %s",
                m->protect_reads ? "read+write" : "write-only",
                m->authlim ? "on" : "off");
            canvas_draw_str(canvas, 2, 24, "Set device-bound PWD:");
            canvas_draw_str(canvas, 2, 35, line);
            canvas_draw_str(canvas, 2, 46, "Irreversible w/o this Flipper!");
        }
        canvas_draw_str(canvas, 2, 60, "OK: apply   Back: cancel");
        break;
    case BvProvWorking:
        bv_draw_hold(canvas, m->unprotect ? "Unprotecting" : "Protecting");
        break;
    case BvProvDone:
        if(m->unprotect) {
            bv_draw_done(canvas, "Unprotected", "Tag password cleared", "Back: settings");
        } else {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 10, "Protected!");
            canvas_set_font(canvas, FontSecondary);
            snprintf(
                line,
                sizeof(line),
                "PWD  %02X %02X %02X %02X",
                m->pwd[0],
                m->pwd[1],
                m->pwd[2],
                m->pwd[3]);
            canvas_draw_str(canvas, 2, 25, line);
            snprintf(line, sizeof(line), "PACK %02X %02X", m->pack[0], m->pack[1]);
            canvas_draw_str(canvas, 2, 37, line);
            canvas_draw_str(canvas, 2, 50, "Record it (pm3/recovery)");
            canvas_draw_str(canvas, 2, 62, "Back: settings");
        }
        break;
    case BvProvError:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Provisioning failed");
        bv_draw_error(canvas, m->error, NULL, "OK: retry   Back: settings");
        break;
    }
}

static bool bv_prov_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort) return false;
    BvProvState st;
    bool unprotect;
    with_view_model(
        app->prov_view,
        BvProvModel * m,
        {
            st = m->state;
            unprotect = m->unprotect;
        },
        false);
    if(st == BvProvWorking) return true; // block navigation mid-write
    if(event->key == InputKeyOk) {
        if(st == BvProvConfirm || st == BvProvError) {
            if(!app->poller_running) bv_start_provision(app, unprotect);
        }
        return true;
    }
    return false; // Back -> previous callback (settings)
}

static uint32_t bv_prov_previous(void* context) {
    UNUSED(context);
    return BvViewSettings;
}

static void bv_prov_exit(void* context) {
    BioVault* app = context;
    bv_poller_stop(app);
    // Clear the shown password from the model on exit.
    with_view_model(
        app->prov_view,
        BvProvModel * m,
        {
            memset(m->pwd, 0, sizeof(m->pwd));
            memset(m->pack, 0, sizeof(m->pack));
        },
        false);
}

// Open the provisioning confirm screen, snapshotting settings for the summary.
static void bv_prov_open(BioVault* app, bool unprotect) {
    with_view_model(
        app->prov_view,
        BvProvModel * m,
        {
            m->state = BvProvConfirm;
            m->unprotect = unprotect;
            m->protect_reads = app->settings.protect_reads;
            m->authlim = app->settings.authlim;
            m->error = BvErrNone;
        },
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, BvViewProvision);
}

// --- Reveal tag password view ---

static void bv_reveal_draw(Canvas* canvas, void* model) {
    BvRevealModel* m = model;
    canvas_clear(canvas);
    char line[40];
    switch(m->state) {
    case BvRevealWorking:
        bv_draw_hold(canvas, "Reading");
        break;
    case BvRevealShown: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Tag password");
        canvas_set_font(canvas, FontSecondary);
        int n = snprintf(line, sizeof(line), "UID ");
        for(size_t i = 0; i < m->uid_len && i < 7; i++) {
            n += snprintf(line + n, sizeof(line) - n, "%02X", m->uid[i]);
        }
        canvas_draw_str(canvas, 2, 24, line);
        snprintf(
            line,
            sizeof(line),
            "PWD  %02X %02X %02X %02X",
            m->pwd[0],
            m->pwd[1],
            m->pwd[2],
            m->pwd[3]);
        canvas_draw_str(canvas, 2, 37, line);
        snprintf(line, sizeof(line), "PACK %02X %02X", m->pack[0], m->pack[1]);
        canvas_draw_str(canvas, 2, 49, line);
        canvas_draw_str(canvas, 2, 62, "Back: hide");
        break;
    }
    case BvRevealError:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Reveal failed");
        bv_draw_error(canvas, m->error, NULL, "OK: retry   Back: settings");
        break;
    }
}

static bool bv_reveal_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort) return false;
    BvRevealState st;
    with_view_model(app->reveal_view, BvRevealModel * m, { st = m->state; }, false);
    if(event->key == InputKeyOk && st == BvRevealError) {
        if(!app->poller_running) bv_start_reveal(app);
        return true;
    }
    return false; // Back -> previous callback (settings); exit wipes the secret
}

static void bv_reveal_enter(void* context) {
    bv_start_reveal((BioVault*)context);
}

static void bv_reveal_exit(void* context) {
    BioVault* app = context;
    bv_poller_stop(app);
    // Clear the shown password from the view model on exit.
    with_view_model(
        app->reveal_view,
        BvRevealModel * m,
        {
            memset(m->pwd, 0, sizeof(m->pwd));
            memset(m->pack, 0, sizeof(m->pack));
            memset(m->uid, 0, sizeof(m->uid));
            m->uid_len = 0;
            m->state = BvRevealWorking;
        },
        true);
}

static uint32_t bv_reveal_previous(void* context) {
    UNUSED(context);
    return BvViewSettings;
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
        // Never overwrite the tag with an un-synced vault.
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
    case BvMenuAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewAbout);
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

// --- About / usage ---

static void bv_build_about(BioVault* app) {
    widget_reset(app->about);
    widget_add_text_scroll_element(
        app->about,
        0,
        0,
        128,
        64,
        "\e#BioVault v0.1\n"
        "by Shain Lakin\n"
        "\n"
        "Enclave-encrypted vault on an implantable NFC tag: the ciphertext lives "
        "on the tag, the AES-256-GCM key in this Flipper's secure enclave. Neither "
        "alone reveals the vault.\n"
        "\n"
        "The vault lives in Sector 1 only. Sector 0 user data is left untouched, "
        "so the tag still works as a normal NFC tag/NDEF.\n"
        "\n"
        "An optional vault PIN (Settings) wraps the key with PIN-derived "
        "material stretched through the enclave: not a bypassable check, and "
        "brute-forcing it needs this Flipper, slowly. Wrong PINs are "
        "undetectable by design; a forgotten PIN loses the vault.\n"
        "\n"
        "\e#Hardware\n"
        "Dangerous Things xSIID implant, or any NTAG I2C Plus 2K (NXP NT3H2211).\n"
        "\n"
        "\e#Usage\n"
        "Load reads the vault from the tag; edits stay in RAM until Save writes "
        "them back. Vault browses entries (view / send / edit / remove). Send types "
        "a field to a host over USB-HID. Settings holds USB auto-return and "
        "optional tag password protection. All commands are also exposed through "
        "the 'biovault' CLI.\n"
        "\n"
        "\e#Repo\n"
        "github.com/flamebarke/biovault-flipper\n");
}

static uint32_t bv_about_previous(void* context) {
    UNUSED(context);
    return BvViewMenu;
}

// --- No-device-key gate ---
// Shown at launch when the enclave self-test failed. Without the KEK nothing
// can be unwrapped, so every vault operation would fail with the same opaque
// error; say why once, up front, instead of at the end of each attempt.

static void bv_no_key_draw(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "! No device key !");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, "The secure enclave on this");
    canvas_draw_str(canvas, 2, 32, "Flipper is unreachable, so");
    canvas_draw_str(canvas, 2, 42, "no vault key exists. Reboot");
    canvas_draw_str(canvas, 2, 52, "or reinstall the firmware.");
    canvas_draw_str(canvas, 2, 62, "OK: continue  Back: exit");
}

static void bv_start_flow(BioVault* app);

static bool bv_no_key_input(InputEvent* event, void* context) {
    BioVault* app = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyOk) {
        bv_start_flow(app); // let the user reach Diagnostics/About anyway
        return true;
    }
    return false; // Back exits the app
}

static bool bv_custom_event_callback(void* context, uint32_t event) {
    BioVault* app = context;
    switch(event) {
    case BvCustomEventPollerDone:
        // Provisioning changed settings; refresh the Settings list.
        if(app->op == BvOpProvision) bv_build_settings(app);
        bv_poller_stop(app); // reap the finished poller on the main thread
        return true;
    case BvCustomEventHidDone:
        if(app->send_thread) {
            furi_thread_join(app->send_thread);
            furi_thread_free(app->send_thread);
            app->send_thread = NULL;
        }
        memset(app->send_text, 0, sizeof(app->send_text)); // wipe the secret
        notification_message(
            app->notifications, app->send_result == BvHidOk ? &sequence_success : &sequence_error);
        return true;
    // CLI-driven screen switches.
    case BvCustomEventCliRead:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewRead);
        return true;
    case BvCustomEventCliLoad:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewLoad);
        return true;
    case BvCustomEventCliSave:
        // Same guard as the menu: never save an un-synced vault.
        view_dispatcher_switch_to_view(
            app->view_dispatcher, app->vault_loaded ? BvViewSave : BvViewLoad);
        return true;
    case BvCustomEventCliZero:
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewZero);
        return true;
    case BvCustomEventCliReveal:
        // Opens the reveal screen; the UID/auth gate still applies.
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewReveal);
        return true;
    case BvCustomEventCliSettings:
        bv_build_settings(app); // reflect a CLI settings change on the GUI list
        return true;
    case BvCustomEventCliProtect:
        // Confirm screen; OK on the device commits the write.
        bv_prov_open(app, false);
        return true;
    case BvCustomEventCliUnprotect:
        bv_prov_open(app, true);
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

    const struct {
        const char* label;
        bool ok;
    } rows[] = {
        {"Enclave KEK", app->enclave_ok},
        {"AES-GCM KAT", app->gcm_ok},
        {"KEK/DEK wrap", app->dek_ok},
        {"Vault codec", app->vault_ok},
        {"Records", app->records_ok},
        {"PIN KDF", app->pin_ok},
    };

    // A bare "FAIL" tells the user nothing they can act on, so a failing run
    // scrolls the results with an explanation appended. Everything passing
    // fits the screen, so keep that case as a plain fixed list.
    if(app->enclave_ok && app->gcm_ok && app->dek_ok && app->vault_ok && app->records_ok &&
       app->pin_ok) {
        widget_add_string_element(
            app->diag, 64, 2, AlignCenter, AlignTop, FontPrimary, "Diagnostics");
        char line[40];
        for(size_t i = 0; i < COUNT_OF(rows); i++) {
            snprintf(line, sizeof(line), "%s: OK", rows[i].label);
            widget_add_string_element(
                app->diag, 2, 13 + i * 8, AlignLeft, AlignTop, FontSecondary, line);
        }
        return;
    }

    char text[1024]; // fits the rows plus both explanation blocks
    size_t n = snprintf(text, sizeof(text), "\e#Diagnostics\n");
    for(size_t i = 0; i < COUNT_OF(rows) && n < sizeof(text); i++) {
        n += snprintf(
            text + n, sizeof(text) - n, "%s: %s\n", rows[i].label, rows[i].ok ? "OK" : "FAIL");
    }
    if(!app->enclave_ok && n < sizeof(text)) {
        n += snprintf(
            text + n,
            sizeof(text) - n,
            "\n\e#Enclave KEK\n"
            "The vault key is wrapped by a device-unique key held in this "
            "Flipper's secure enclave. The enclave is unreachable, so that key "
            "cannot be created or read and no vault can be opened on this "
            "device.\n"
            "\n"
            "The key store lives on the radio core. Reboot the Flipper; if the "
            "check still fails, reinstall the firmware (including the radio "
            "stack) from qFlipper.\n"
            "\n"
            "This is not caused by a missing PIN or an unprovisioned tag, and "
            "no other app needs to run first.\n");
    }
    if(n < sizeof(text) &&
       (!app->gcm_ok || !app->dek_ok || !app->vault_ok || !app->records_ok || !app->pin_ok)) {
        snprintf(
            text + n,
            sizeof(text) - n,
            "\n\e#Self-test failed\n"
            "A crypto or codec self-test did not pass. Do not trust this build "
            "with a vault; reinstall BioVault and report the failing row.\n");
    }
    widget_add_text_scroll_element(app->diag, 0, 0, 128, 64, text);
}

// --- CLI command (registered while the app is running) ---
// `biovault` drives the same in-RAM vault as the GUI over USB. Interactive
// prompts keep secrets out of shell history (but not terminal scrollback).

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

// Extract the label argument: the whole line remainder, trimmed (allows spaces).
static void bv_cli_arg(FuriString* args, char* out, size_t size) {
    FuriString* s = furi_string_alloc_set(args);
    furi_string_trim(s);
    strlcpy(out, furi_string_get_cstr(s), size);
    furi_string_free(s);
}

// --- Subshell subcommands (context = BioVault*) ---

// list/get snapshot the vault under the lock and print after releasing it, so a
// stalled host terminal can't block the GUI/NFC threads on vault_mutex.
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
    printf(
        ok ? "Removed '%s'. Run 'save' to persist to the implant.\r\n" : "Not found: %s\r\n",
        label);
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

// Edit an entry in place. A blank line keeps the current value.
static void bv_cli_edit(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    BioVault* app = context;
    char label[BV_LABEL_CAP];
    bv_cli_arg(args, label, sizeof(label));
    if(!strlen(label)) {
        printf("Usage: edit <label>\r\n");
        return;
    }

    // Snapshot the entry under the lock, then prompt without holding it.
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
    // Re-find in case the vault changed while prompting.
    idx = bv_records_find(app->vault, cur_label);
    bool ok = (idx >= 0) &&
              bv_records_set(app->vault, (uint8_t)idx, type, cur_label, user, secret);
    furi_mutex_release(app->vault_mutex);
    printf(
        ok ? "Updated '%s'. Run 'save' to persist to the implant.\r\n" :
             "Edit failed (entry gone or field too long).\r\n",
        cur_label);
}

// Render a hex + ASCII dump of a raw Sector 1 read.
static void bv_cli_print_dump(const uint8_t* buf, size_t len) {
    for(size_t off = 0; off < len; off += 16) {
        printf("%04X  ", (unsigned)off);
        for(size_t b = 0; b < 16; b++) {
            if(off + b < len) {
                printf("%02X ", buf[off + b]);
            } else {
                printf("   ");
            }
            if(b == 7) printf(" "); // gutter between 8-byte halves
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

    // Snapshot the read model, render outside the model lock.
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
        bv_cli_print_hint(error);
        free(buf);
        return;
    }

    printf("\r\nSector 1: %u bytes", (unsigned)len);
    if(version_valid) {
        printf("   version ");
        for(size_t i = 0; i < sizeof(version); i++)
            printf("%02X", version[i]);
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
        printf("Vault not synced yet - opened the Load screen first (load before\r\n"
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

// --- Settings / reveal over CLI ---

static const char* const bv_cli_authlim[] = {"off", "2", "4", "8", "16", "32", "64", "128"};

static bool bv_cli_onoff(const char* v, bool* out) {
    if(strcmp(v, "on") == 0 || strcmp(v, "1") == 0) {
        *out = true;
        return true;
    }
    if(strcmp(v, "off") == 0 || strcmp(v, "0") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static void bv_cli_settings(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    BioVault* app = context;
    char key[16] = {0};
    char val[16] = {0};
    sscanf(furi_string_get_cstr(args), "%15s %15s", key, val);

    if(!strlen(key)) {
        printf("Settings:\r\n");
        printf("  autoreturn   %s\r\n", app->settings.send_newline ? "on" : "off");
        printf("  readprotect  %s\r\n", app->settings.protect_reads ? "on" : "off");
        printf(
            "  authlim      %s\r\n",
            bv_cli_authlim[app->settings.authlim < 8 ? app->settings.authlim : 0]);
        printf("  protected    %s (status)\r\n", app->settings.tag_protected ? "yes" : "no");
        printf("  pin          %s (set on device)\r\n", bv_vault_pin_required() ? "set" : "off");
        printf(
            "Self-test: enclave=%d gcm=%d dek=%d vault=%d records=%d pin=%d\r\n",
            app->enclave_ok,
            app->gcm_ok,
            app->dek_ok,
            app->vault_ok,
            app->records_ok,
            app->pin_ok);
        printf("Set: settings <autoreturn|readprotect> <on|off>\r\n");
        printf("     settings authlim <off|2|4|8|16|32|64|128>\r\n");
        return;
    }
    if(!strlen(val)) {
        printf("Usage: settings %s <value>\r\n", key);
        return;
    }

    bool bv = false;
    if(strcmp(key, "autoreturn") == 0 && bv_cli_onoff(val, &bv)) {
        app->settings.send_newline = bv;
    } else if(strcmp(key, "readprotect") == 0 && bv_cli_onoff(val, &bv)) {
        app->settings.protect_reads = bv;
    } else if(strcmp(key, "authlim") == 0) {
        int idx = -1;
        for(int i = 0; i < 8; i++)
            if(strcmp(val, bv_cli_authlim[i]) == 0) idx = i;
        if(idx < 0) {
            printf("Bad authlim '%s' (off|2|4|8|16|32|64|128)\r\n", val);
            return;
        }
        app->settings.authlim = (uint8_t)idx;
    } else {
        printf("Bad setting/value. Try 'settings' for usage.\r\n");
        return;
    }
    bv_settings_save(&app->settings);
    view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventCliSettings);
    printf("OK: %s = %s\r\n", key, val);
}

// Open the on-device Protect / Unprotect confirm screen. Write is committed
// with OK on the Flipper (irreversible Sector 0 write), then hold the implant.
static void bv_cli_protect(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    BioVault* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventCliProtect);
    printf("Protect screen open on device. Review 'settings', press OK on the\r\n"
           "Flipper to confirm, then hold the implant. PWD/PACK show on-device\r\n"
           "(or run 'reveal' after).\r\n");
}

static void bv_cli_unprotect(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    BioVault* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventCliUnprotect);
    printf("Unprotect screen open on device. Press OK on the Flipper to confirm,\r\n"
           "then hold the implant.\r\n");
}

// Reveal the implant's PWD/PACK to the terminal. Same gate as the on-device
// reveal: the provisioned implant must authenticate first.
static void bv_cli_reveal(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    BioVault* app = context;
    if(!app->settings.tag_protected) {
        printf("No provisioned implant. Protect one first.\r\n");
        return;
    }
    furi_event_flag_clear(app->read_done, BV_READ_DONE_FLAG);
    view_dispatcher_send_custom_event(app->view_dispatcher, BvCustomEventCliReveal);
    printf("Hold the provisioned implant to the Flipper...\r\n");
    fflush(stdout);

    uint32_t flags =
        furi_event_flag_wait(app->read_done, BV_READ_DONE_FLAG, FuriFlagWaitAny, 30000);
    if(flags & FuriFlagError) {
        printf("Timed out.\r\n");
        return;
    }

    BvRevealState st = BvRevealError;
    BvError err = BvErrNone;
    uint8_t pwd[4] = {0};
    uint8_t pack[2] = {0};
    with_view_model(
        app->reveal_view,
        BvRevealModel * m,
        {
            st = m->state;
            err = m->error;
            memcpy(pwd, m->pwd, sizeof(pwd));
            memcpy(pack, m->pack, sizeof(pack));
        },
        false);
    if(st == BvRevealShown) {
        printf("PWD  %02X%02X%02X%02X\r\n", pwd[0], pwd[1], pwd[2], pwd[3]);
        printf("PACK %02X%02X\r\n", pack[0], pack[1]);
        printf("pm3: hf mfu dump -k %02X%02X%02X%02X\r\n", pwd[0], pwd[1], pwd[2], pwd[3]);
    } else {
        printf("Reveal failed: %s\r\n", bv_error_text(err));
        bv_cli_print_hint(err);
    }
    memset(pwd, 0, sizeof(pwd));
    memset(pack, 0, sizeof(pack));
}

// Vault PIN over CLI. Set/enter prompt twice: a wrong PIN is undetectable by
// design (no verifier), so a clean double entry is the only typo guard.
static void bv_cli_pin(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    BioVault* app = context;
    char sub[16] = {0};
    sscanf(furi_string_get_cstr(args), "%15s", sub);
    bool set = bv_vault_pin_required();

    if(!strlen(sub)) {
        printf("Vault PIN: %s\r\n", set ? "set" : "off");
        printf("Usage: pin <set|remove|enter>\r\n");
        return;
    }

    if(strcmp(sub, "set") == 0) {
        if(set) {
            printf("PIN already set. Remove it first (needs a verified load).\r\n");
            return;
        }
        printf("The PIN becomes part of the vault key. A wrong PIN cannot be\r\n"
               "detected, and a forgotten PIN means the vault is LOST.\r\n");
        char p1[64] = {0};
        char p2[64] = {0};
        printf("New PIN (%d digits): ", BV_PIN_LEN);
        fflush(stdout);
        bv_cli_read_line(p1, sizeof(p1));
        bool valid = strlen(p1) == BV_PIN_LEN;
        for(size_t i = 0; valid && i < BV_PIN_LEN; i++) {
            valid = (p1[i] >= '0') && (p1[i] <= '9');
        }
        if(!valid) {
            printf("PIN must be exactly %d digits; unchanged.\r\n", BV_PIN_LEN);
            memset(p1, 0, sizeof(p1));
            return;
        }
        printf("Repeat PIN: ");
        fflush(stdout);
        bv_cli_read_line(p2, sizeof(p2));
        if(strcmp(p1, p2) != 0) {
            printf("Mismatch; PIN unchanged.\r\n");
        } else {
            uint32_t t0 = furi_get_tick();
            bool ok = bv_vault_pin_enable(p1);
            if(ok) {
                printf(
                    "PIN set. Unlock derive takes ~%lums.\r\n",
                    (unsigned long)(furi_get_tick() - t0));
            } else {
                printf("Failed; PIN unchanged.\r\n");
            }
        }
        memset(p1, 0, sizeof(p1));
        memset(p2, 0, sizeof(p2));
    } else if(strcmp(sub, "remove") == 0) {
        if(!set) {
            printf("No PIN set.\r\n");
            return;
        }
        if(!app->dek_verified) {
            printf("Refused: load your vault first so this session's PIN is proven\r\n"
                   "correct, then remove it. (Protects against a typo'd session\r\n"
                   "rewrapping the key.)\r\n");
            return;
        }
        printf(bv_vault_pin_disable() ? "PIN removed.\r\n" : "Failed; PIN unchanged.\r\n");
    } else if(strcmp(sub, "enter") == 0) {
        if(!set) {
            printf("No PIN set.\r\n");
            return;
        }
        char p1[64] = {0};
        printf("PIN: ");
        fflush(stdout);
        bv_cli_read_line(p1, sizeof(p1));
        uint8_t salt[BV_PIN_SALT_SIZE];
        uint32_t sw = 0, hw = 0;
        uint8_t key[BV_PIN_KEY_SIZE];
        uint32_t t0 = furi_get_tick();
        bool ok = bv_vault_pin_params(salt, &sw, &hw) && bv_pin_derive(p1, salt, sw, hw, key);
        if(ok) bv_vault_unlock_key_set(key);
        memset(key, 0, sizeof(key));
        if(ok) {
            printf(
                "Unlock key set (derive %lums). Load to verify it.\r\n",
                (unsigned long)(furi_get_tick() - t0));
        } else {
            printf("Derive failed.\r\n");
        }
        memset(p1, 0, sizeof(p1));
    } else {
        printf("Usage: pin <set|remove|enter>\r\n");
    }
}

static void bv_cli_motd(void* context) {
    UNUSED(context);
    printf("\r\n  \e[33m\xe2\x98\xa3\e[0m \e[1;36mBioVault\e[0m \e[36mv0.1\e[0m\r\n"
           "  enclave-encrypted implant vault\r\n\r\n");
    printf("\e[36mVault:\e[0m  list, get <label>, add <label>, edit <label>, remove <label>\r\n");
    printf("\e[36mDevice:\e[0m read, load, save, wipe, reveal  (drive the on-device screens)\r\n");
    printf("\e[36mProtect:\e[0m protect, unprotect  (confirm on the Flipper)\r\n");
    printf("\e[36mConfig:\e[0m settings [<key> <value>], pin <set|remove|enter>\r\n");
    printf("\e[36mShell:\e[0m  exit\r\n");
    printf("Vault edits are in RAM; run 'save' to persist them to the implant.\r\n");
}

// The `biovault` command opens a subshell with the vault commands.
static void bv_cli_callback(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(args);
    BioVault* app = context;

    // Track open subshells: bv_free must not free `app` while one is live.
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
    cli_registry_add_command(registry, "reveal", CliCommandFlagParallelSafe, bv_cli_reveal, app);
    cli_registry_add_command(registry, "protect", CliCommandFlagParallelSafe, bv_cli_protect, app);
    cli_registry_add_command(
        registry, "unprotect", CliCommandFlagParallelSafe, bv_cli_unprotect, app);
    cli_registry_add_command(
        registry, "settings", CliCommandFlagParallelSafe, bv_cli_settings, app);
    cli_registry_add_command(registry, "pin", CliCommandFlagParallelSafe, bv_cli_pin, app);

    CliShell* shell = cli_shell_alloc(bv_cli_motd, app, pipe, registry, NULL);
    cli_shell_set_prompt(shell, "biovault");
    cli_shell_start(shell);
    cli_shell_join(shell); // blocks until `exit` or disconnect
    cli_shell_free(shell);
    cli_registry_free(registry);

    __atomic_sub_fetch(&app->cli_sessions, 1, __ATOMIC_SEQ_CST);
}

// --- App lifecycle ---

static BioVault* bv_alloc(void) {
    BioVault* app = malloc(sizeof(BioVault));
    memset(app, 0, sizeof(BioVault));

    // Crypto/data self-tests; results shown in Diagnostics.
    app->enclave_ok = bv_crypto_enclave_selftest();
    app->gcm_ok = bv_crypto_gcm_kat();
    app->dek_ok = bv_crypto_dek_selftest();
    app->vault_ok = bv_vault_selftest();
    app->records_ok = bv_records_selftest();
    app->pin_ok = bv_pin_selftest();
    FURI_LOG_I(
        TAG,
        "self-test: enclave=%d gcm=%d dek=%d vault=%d records=%d pin=%d",
        app->enclave_ok,
        app->gcm_ok,
        app->dek_ok,
        app->vault_ok,
        app->records_ok,
        app->pin_ok);

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
    submenu_add_item(app->menu, "About", BvMenuAbout, bv_menu_callback, app);
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
    view_dispatcher_add_view(app->view_dispatcher, BvViewDetail, text_box_get_view(app->detail));

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
    view_dispatcher_add_view(
        app->view_dispatcher, BvViewInput, bv_text_input_get_view(app->input));

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
    variable_item_list_set_enter_callback(app->settings_list, bv_settings_enter, app);
    view_set_previous_callback(
        variable_item_list_get_view(app->settings_list), bv_settings_previous);
    view_dispatcher_add_view(
        app->view_dispatcher, BvViewSettings, variable_item_list_get_view(app->settings_list));

    // Provisioning confirm/progress view
    app->prov_view = view_alloc();
    view_allocate_model(app->prov_view, ViewModelTypeLocking, sizeof(BvProvModel));
    view_set_context(app->prov_view, app);
    view_set_draw_callback(app->prov_view, bv_prov_draw);
    view_set_input_callback(app->prov_view, bv_prov_input);
    view_set_exit_callback(app->prov_view, bv_prov_exit);
    view_set_previous_callback(app->prov_view, bv_prov_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewProvision, app->prov_view);

    // Reveal tag password view (UID-gated)
    app->reveal_view = view_alloc();
    view_allocate_model(app->reveal_view, ViewModelTypeLocking, sizeof(BvRevealModel));
    view_set_context(app->reveal_view, app);
    view_set_draw_callback(app->reveal_view, bv_reveal_draw);
    view_set_input_callback(app->reveal_view, bv_reveal_input);
    view_set_enter_callback(app->reveal_view, bv_reveal_enter);
    view_set_exit_callback(app->reveal_view, bv_reveal_exit);
    view_set_previous_callback(app->reveal_view, bv_reveal_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewReveal, app->reveal_view);

    // Auth limit warning gate + picker
    app->auth_warn = view_alloc();
    view_set_context(app->auth_warn, app);
    view_set_draw_callback(app->auth_warn, bv_auth_warn_draw);
    view_set_input_callback(app->auth_warn, bv_auth_warn_input);
    view_set_previous_callback(app->auth_warn, bv_auth_return_settings);
    view_dispatcher_add_view(app->view_dispatcher, BvViewAuthWarn, app->auth_warn);

    app->auth_pick = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->auth_pick), bv_auth_return_settings);
    view_dispatcher_add_view(
        app->view_dispatcher, BvViewAuthPick, submenu_get_view(app->auth_pick));

    // Vault PIN warning gate + menu
    app->pin_warn = view_alloc();
    view_allocate_model(app->pin_warn, ViewModelTypeLockFree, sizeof(BvPinWarnMode));
    view_set_context(app->pin_warn, app);
    view_set_draw_callback(app->pin_warn, bv_pin_warn_draw);
    view_set_input_callback(app->pin_warn, bv_pin_warn_input);
    view_set_previous_callback(app->pin_warn, bv_auth_return_settings);
    view_dispatcher_add_view(app->view_dispatcher, BvViewPinWarn, app->pin_warn);

    app->pin_menu = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->pin_menu), bv_auth_return_settings);
    view_dispatcher_add_view(app->view_dispatcher, BvViewPinMenu, submenu_get_view(app->pin_menu));

    // 6-digit PIN entry screen
    app->pin_entry = view_alloc();
    view_allocate_model(app->pin_entry, ViewModelTypeLocking, sizeof(BvPinEntryModel));
    view_set_context(app->pin_entry, app);
    view_set_draw_callback(app->pin_entry, bv_pin_entry_draw);
    view_set_input_callback(app->pin_entry, bv_pin_entry_input);
    view_dispatcher_add_view(app->view_dispatcher, BvViewPinEntry, app->pin_entry);

    // About / usage
    app->about = widget_alloc();
    bv_build_about(app);
    view_set_previous_callback(widget_get_view(app->about), bv_about_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewAbout, widget_get_view(app->about));

    // No-device-key gate
    app->no_key = view_alloc();
    view_set_context(app->no_key, app);
    view_set_draw_callback(app->no_key, bv_no_key_draw);
    view_set_input_callback(app->no_key, bv_no_key_input);
    view_dispatcher_add_view(app->view_dispatcher, BvViewNoKey, app->no_key);

    // Diagnostics view
    app->diag = widget_alloc();
    bv_build_diag(app);
    view_set_previous_callback(widget_get_view(app->diag), bv_diag_previous);
    view_dispatcher_add_view(app->view_dispatcher, BvViewDiag, widget_get_view(app->diag));

    // Register the `biovault` CLI command. ParallelSafe so it runs while the
    // app is open; the vault mutex makes it safe.
    CliRegistry* cli = furi_record_open(RECORD_CLI);
    cli_registry_add_command(cli, "biovault", CliCommandFlagParallelSafe, bv_cli_callback, app);
    furi_record_close(RECORD_CLI);

    return app;
}

static void bv_free(BioVault* app) {
    CliRegistry* cli = furi_record_open(RECORD_CLI);
    cli_registry_delete_command(cli, "biovault");
    furi_record_close(RECORD_CLI);

    // A `biovault` subshell already open keeps running (no stop API) and holds
    // `app`. Wait it out before freeing to avoid a dangling vault/mutex.
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
    view_dispatcher_remove_view(app->view_dispatcher, BvViewProvision);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewReveal);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewAuthWarn);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewAuthPick);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewPinWarn);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewPinMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewPinEntry);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewDiag);
    view_dispatcher_remove_view(app->view_dispatcher, BvViewNoKey);
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
    view_free(app->prov_view);
    view_free(app->reveal_view);
    view_free(app->auth_warn);
    submenu_free(app->auth_pick);
    view_free(app->pin_warn);
    submenu_free(app->pin_menu);
    view_free(app->pin_entry);
    view_free(app->no_key);
    widget_free(app->about);
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
        memset(app->save_blob, 0, BV_SERIALIZED_MAX + 1 + BV_BLOB_OVERHEAD);
        free(app->save_blob);
    }
    bv_vault_unlock_key_clear();
    // Wipe secrets in app-owned buffers before returning the allocation.
    memset(app, 0, sizeof(*app));
    free(app);
}

static void bv_start_flow(BioVault* app) {
    if(bv_vault_pin_required()) {
        bv_pin_entry_open(app, BvPinEntryUnlock, "Vault PIN");
    } else {
        // Load-first flow so Save never overwrites the tag with an un-synced vault.
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewLoad);
    }
}

int32_t biovault_app(void* p) {
    UNUSED(p);
    BioVault* app = bv_alloc();
    if(!app->enclave_ok) {
        // No KEK: explain once here rather than failing every operation later.
        view_dispatcher_switch_to_view(app->view_dispatcher, BvViewNoKey);
    } else {
        bv_start_flow(app);
    }
    view_dispatcher_run(app->view_dispatcher);
    bv_free(app);
    return 0;
}
