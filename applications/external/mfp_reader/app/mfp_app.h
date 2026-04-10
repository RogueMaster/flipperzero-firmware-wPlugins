#pragma once

#include "mfp_poller.h"
#include "mfp_storage.h"
#include "mfp_keys.h"
#include "mfp_emulate_view.h"
#include "mfp_result_view.h"

typedef struct MfpDumpView MfpDumpView;

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/text_input.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/text_box.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

typedef enum {
    MfpViewSubmenu,
    MfpViewPopup,
    MfpViewWidget,
    MfpViewDialogEx,
    MfpViewByteInput,
    MfpViewFileBrowser,
    MfpViewEmulate,
    MfpViewDump,
    MfpViewResult,
    MfpViewTextBox,
    MfpViewTextInput,
} MfpView;

/* ---- Per-sector scan results ---- */

typedef enum {
    MfpSectorNone = 0,
    MfpSectorOk,
    MfpSectorAuthFail,
    MfpSectorReadFail,
} MfpSectorStatus;

typedef struct {
    MfpSectorStatus status;
    MfpKey          key_a;
    MfpKey          key_b;
    bool            key_a_found;
    bool            key_b_found;
    uint8_t         blocks_read;
} MfpSectorResult;

typedef struct MfpApp {
    /* NFC */
    Nfc*       nfc;
    NfcPoller* poller;

    /* Card identification (Phase 1 — ISO14443-4A + GetVersion) */
    MfpVersion version;       /* uid, uid_len, sl, size from GetVersion */
    uint8_t    sak;           /* from ISO14443-3A selection */
    uint8_t    atqa[2];
    uint8_t    ats_bytes[32]; /* ATS historical bytes */
    uint8_t    ats_len;
    bool       card_identified;

    /* Card auth + data (Phase 2) */
    MfpSession session;
    MfpBlock   blocks[MFP_MAX_BLOCKS];
    uint8_t    blocks_read;
    MfpError   last_error;
    int        last_iso_error; /* raw Iso14443_4aError from last send_block call */
    uint8_t    dbg_resp[16];  /* first 16 bytes of last GetVersion response */
    uint8_t    dbg_resp_len;  /* actual length of that response */
    bool       card_authed;
    bool       loaded_from_file;

    /* Key used for auth */
    MfpKey     key;
    MfpKeyType key_type;
    uint8_t    target_sector;

    /* Dictionary attack */
    FuriString* dict_path;     /* empty = manual key */
    uint8_t*   dict_buf;       /* flat: dict_key_count * MFP_AES_KEY_SIZE bytes */
    uint32_t   dict_key_count;

    /* Read-all scan state */
    MfpSectorResult sector_results[MFP_SECTORS_4K];
    uint8_t    scan_total_sectors;
    uint8_t    scan_sectors_done;
    uint8_t    scan_sectors_ok;
    bool       read_all_mode;
    volatile bool scan_cancel_requested;
    FuriString* scan_status_text;

    /* Write block state */
    uint8_t    write_block_offset;           /* 0..3 (block within sector) */
    uint8_t    write_block_data[MFP_BLOCK_SIZE];
    uint8_t    edit_byte_idx;                /* current byte being edited */

    /* Emulation state */
    void*      emulator;                     /* MfpListener* */
    bool       allow_overwrite;              /* if true, emulation writes overwrite the dump file */
    bool       modified_saved;               /* true if emulation produced a saved modified dump */
    char       modified_save_path[128];      /* path written to on modification save */

    /* Storage */
    Storage* storage;
    char     save_path[128];

    /* Saved cards list (populated by saved scene) */
    char     saved_names[MFP_MAX_SAVED][MFP_NAME_LEN];
    uint32_t saved_count;

    /* Notifications / LED */
    NotificationApp* notifications;

    /* GUI */
    ViewDispatcher* view_dispatcher;
    SceneManager*   scene_manager;
    Submenu*        submenu;
    Popup*          popup;
    Widget*         widget;
    DialogEx*       dialog_ex;
    ByteInput*      byte_input;
    FileBrowser*    file_browser;
    FuriString*     file_browser_result;
    MfpEmulateView* emulate_view;
    MfpDumpView*    dump_view;
    MfpResultView*  result_view;
    TextBox*        text_box;
    FuriString*     text_box_store;
    TextInput*      text_input;
    char            text_store[64];
} MfpApp;

/** Application entry point — registered in application.fam */
int32_t mfp_app_entry(void* p);
