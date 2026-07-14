#include <furi.h>
#include <furi_hal.h>

#include <../applications/services/notification/notification_messages.h> //Tohle by melo byt pak pro blikani

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/submenu.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <dialogs/dialogs.h>
#include <furi/core/string.h>

#include <storage/storage.h>

#include <flipper_format/flipper_format.h> //Potreba?

#include <gui/icon_i.h>

#include "scenes/scenes.h"

#define TAG                 "MfDesApp"
#define NFC_CARDS_PATH      "/ext/nfc"
#define INITIAL_VECTOR_SIZE 8
#define KEY_SIZE            24

// File saving
#define SAVE_PATH         APP_DATA_PATH("mfdesfire_auth.save")
#define SAVE_FILE_HEADER  "Flipper DESFire Auth Settings"
#define SAVE_FILE_VERSION 1

// View index
typedef enum {
    MfDesAppViewBrowser,
    MfDesAppViewSubmenu,
    MfDesAppViewByteInput,
    MfDesAppViewPopupAuth,
} MfDesAppView;

typedef enum {
    MfDesAppByteInputIV,
    MfDesAppByteInputKey,
} MfDesAppByteInputType;

// Save index
typedef enum {
    MfDesSaveIV,
    MfDesSaveKey,
} MfDesSaveIndex;

typedef enum {
    MfDesAppCustomExit,
} MfDesAppCustomEvent;

/**
 * @brief Struktura pro uchování stavu aplikace.
 */
typedef struct {
    Gui* gui;

    NotificationApp* notifications;

    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    // Views
    FileBrowser* file_browser;
    Submenu* submenu;
    ByteInput* byte_input;
    Popup* popup;
    Widget* widget;

    FuriString* selected_card_path;

    uint8_t initial_vector[INITIAL_VECTOR_SIZE];
    uint8_t key[KEY_SIZE];

    // MfDesAppView current_view; // Track current view manually
} MfDesApp;

bool mfdesfire_auth_save_settings(MfDesApp* app, uint32_t index);
bool mfdesfire_auth_load_settings(MfDesApp* instance);
