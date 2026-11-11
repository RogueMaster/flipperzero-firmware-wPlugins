#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>

#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <toolbox/path.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/nfc_protocol.h>

#include "scenes/badge_wallet_scene.h"

/* Where the user's saved cards live, and where our sidecar metadata lives.
 * The .nfc files themselves are treated as strictly read-only. */
#define BADGE_WALLET_NFC_DIR  EXT_PATH("nfc")
#define BADGE_WALLET_NFC_EXT  ".nfc"
#define BADGE_WALLET_DATA_DIR EXT_PATH("apps_data/badge_wallet")
#define BADGE_WALLET_SIDECAR  EXT_PATH("apps_data/badge_wallet/wallet.txt")

#define BADGE_WALLET_MAX_CARDS   128U
#define BADGE_WALLET_LABEL_MAX   64U
#define BADGE_WALLET_UID_MAX_LEN 10U
#define BADGE_WALLET_NAME_MAX    128U

typedef enum {
    BadgeWalletViewSubmenu,
    BadgeWalletViewPopup,
    BadgeWalletViewTextInput,
    BadgeWalletViewWidget,
} BadgeWalletView;

typedef enum {
    BadgeWalletListModeFavourites,
    BadgeWalletListModeAll,
} BadgeWalletListMode;

/* One saved badge. `path` is the full path to the .nfc file (the identity key
 * in the sidecar); `label` and `favorite` are user metadata held only in
 * memory and persisted to the sidecar. */
typedef struct {
    FuriString* path;
    FuriString* label;
    bool favorite;
} BadgeCard;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    Storage* storage;

    Submenu* submenu;
    Popup* popup;
    TextInput* text_input;
    Widget* widget;

    BadgeCard cards[BADGE_WALLET_MAX_CARDS];
    size_t card_count;

    BadgeWalletListMode list_mode;
    size_t selected_index; /**< index into cards[] for the card being acted on */

    char label_buf[BADGE_WALLET_LABEL_MAX];
    /* Popup keeps the pointer we hand it (no copy), so this must outlive the
     * Emulate scene's popup view. */
    char emulate_text[BADGE_WALLET_LABEL_MAX + 24];

    /* NFC emulation resources. Owned by the Emulate scene: allocated on enter,
     * fully torn down on exit, so the NFC HAL is only held while emulating. */
    Nfc* nfc;
    NfcDevice* emulate_device;
    NfcListener* listener;
} BadgeWalletApp;

/** Scan BADGE_WALLET_NFC_DIR for *.nfc files and load sidecar metadata onto
 * them. Rebuilds the cards[] array from scratch. */
void badge_wallet_load_cards(BadgeWalletApp* app);

/** Persist the current cards[] metadata (favourite + label) to the sidecar. */
void badge_wallet_save_sidecar(BadgeWalletApp* app);

/** Free the FuriStrings held by every loaded card and reset the count. */
void badge_wallet_free_cards(BadgeWalletApp* app);

/** Number of cards currently flagged as favourite. */
size_t badge_wallet_favourite_count(BadgeWalletApp* app);
