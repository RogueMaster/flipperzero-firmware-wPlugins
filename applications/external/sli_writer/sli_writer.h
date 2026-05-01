#pragma once

/* ============================================================================
 *  SLI Writer — Header
 * ========================================================================== */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_nfc.h>

#include <nfc/nfc.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/loading.h>
#include <gui/scene_manager.h>

#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/bit_buffer.h>

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 *  Constants
 * ========================================================================== */

#define SLI_WRITER_FILE_EXTENSION ".nfc"
#define SLI_MAGIC_BLOCK_SIZE      4
#define SLI_MAGIC_MAX_BLOCKS      64

/* Path where the special (factory) UID is persisted on the SD card */
#define SLI_SPECIAL_UID_PATH "/ext/apps_data/sli_writer/special_uid.bin"

/* ============================================================================
 *  Write modes
 * ========================================================================== */

typedef enum {
    SliWriterModeNormal = 0, /* Standard Gen2 write (non-addressed)         */
    SliWriterModeSaveUid = 1, /* Inventory only — save detected UID as special */
    SliWriterModeSpecial = 2, /* Special write: restore factory UID first,
                                  write blocks addressed (flags=0x62),
                                  then set target UID                          */
} SliWriterMode;

/* ============================================================================
 *  Scenes / Views / Menu
 * ========================================================================== */

typedef enum {
    SliWriterSceneStart = 0,
    SliWriterSceneFileSelect,
    SliWriterSceneWrite,
    SliWriterSceneSuccess,
    SliWriterSceneError,
    SliWriterSceneNum,
} SliWriterScene;

typedef enum {
    SliWriterViewSubmenu = 0,
    SliWriterViewDialogEx,
    SliWriterViewLoading,
} SliWriterView;

typedef enum {
    SliWriterSubmenuIndexWrite = 0, /* "Write NFC File"       */
    SliWriterSubmenuIndexSaveUid = 1, /* "Save Special UID"     */
    SliWriterSubmenuIndexWriteSpec = 2, /* "Write Special"        */
    SliWriterSubmenuIndexAbout = 3, /* "About"                */
} SliWriterSubmenuIndex;

typedef enum {
    SliWriterCustomEventWriteSuccess = 100,
    SliWriterCustomEventWriteError,
    SliWriterCustomEventParseError,
    SliWriterCustomEventSaveUidSuccess,
} SliWriterCustomEvent;

/* ============================================================================
 *  Parsed .nfc data
 * ========================================================================== */

typedef struct {
    uint8_t uid[8]; /* MSB-first, as in the .nfc file               */
    uint8_t password_privacy[4];
    uint8_t data[SLI_MAGIC_MAX_BLOCKS * SLI_MAGIC_BLOCK_SIZE];
    uint8_t block_count;
    uint8_t block_size;
} SliWriterNfcData;

/* ============================================================================
 *  App context
 * ========================================================================== */

typedef struct {
    /* UI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    DialogEx* dialog_ex;
    Loading* loading;

    /* FS & dialogs */
    Storage* storage;
    DialogsApp* dialogs;

    /* Strings */
    FuriString* file_path;
    FuriString* error_message;

    /* NFC */
    Nfc* nfc;
    bool nfc_started;
    NfcPoller* poller;

    /* Write mode — set before starting the poller */
    SliWriterMode write_mode;

    /* State */
    bool have_uid;
    uint8_t detected_uid[8]; /* UID of the card currently in field  */

    /* Special (factory) UID — persisted to SD card */
    bool special_uid_saved;
    uint8_t special_uid[8];

    /* Data to write */
    SliWriterNfcData nfc_data;
} SliWriterApp;

/* ============================================================================
 *  API
 * ========================================================================== */

SliWriterApp* sli_writer_app_alloc(void);
void sli_writer_app_free(SliWriterApp* app);

bool sli_writer_parse_nfc_file(SliWriterApp* app, const char* file_path);
bool sli_writer_load_special_uid(SliWriterApp* app);
bool sli_writer_save_special_uid(SliWriterApp* app);

void sli_writer_submenu_callback(void* context, uint32_t index);
void sli_writer_dialog_ex_callback(DialogExResult result, void* context);
bool sli_writer_back_event_callback(void* context);
bool sli_writer_custom_event_callback(void* context, uint32_t event);

int32_t sli_writer_app(void* p);
