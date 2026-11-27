#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
//#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include "scenes/flipbip_scene.h"
#include "views/flipbip_scene_1.h"
#include "flipbip_coins.h"
#include "helpers/flipbip_file.h"

#define FLIPBIP_VERSION  "v2.1"
#define TEXT_BUFFER_SIZE 256

typedef struct {
    Gui* gui;
    // NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    SceneManager* scene_manager;
    VariableItemList* variable_item_list;
    TextInput* text_input;
    DialogEx* renew_dialog;
    DialogEx* encryption_dialog;
    FlipBipScene1* flipbip_scene_1;
    const char* mnemonic_menu_text;
    // Settings options
    int bip39_strength;
    int passphrase;
    FlipBipEncryption encryption; // used for the next save, matches the saved wallet
    // Secure enclave
    bool enclave_available; // false on clones: legacy encryption only
    FlipBipEncryption encryption_target; // for the Encryption scene
    bool suggest_enclave; // ask to switch once back at the menu
    // Main menu options
    int coin_type;
    int overwrite_saved_seed;
    int import_from_mnemonic;
    // Text input
    int input_state;
    char passphrase_text[TEXT_BUFFER_SIZE];
    char input_text[TEXT_BUFFER_SIZE];

    void (*wallet_create)(void* context);
} FlipBip;

typedef enum {
    FlipBipViewIdMenu,
    FlipBipViewIdScene1,
    FlipBipViewIdSettings,
    FlipBipViewIdTextInput,
    FlipBipViewRenewConfirm,
    FlipBipViewEncryptionConfirm,
} FlipBipViewId;

typedef enum {
    FlipBipStrength128,
    FlipBipStrength192,
    FlipBipStrength256,
} FlipBipStrengthState;

typedef enum {
    FlipBipPassphraseOff,
    FlipBipPassphraseOn,
} FlipBipPassphraseState;

typedef enum {
    FlipBipCustomEventSettingsEncryption = 100,
} FlipBipSettingsEvent;

typedef enum {
    FlipBipTextInputDefault,
    FlipBipTextInputPassphrase,
    FlipBipTextInputMnemonic
} FlipBipTextInputState;

typedef enum {
    FlipBipStatusSuccess = 0,
    FlipBipStatusReturn = 10,
    FlipBipStatusLoadError = 11,
    FlipBipStatusSaveError = 12,
    FlipBipStatusMnemonicCheckError = 13,
    FlipBipStatusEnclaveError = 14,
} FlipBipStatus;

// There's a scene ID for each coin, then these scenes are after so need to offset the first entry by at least NUM_COINS
typedef enum {
    SubmenuIndexScene1New = NUM_COINS + 1,
    SubmenuIndexScene1Renew,
    SubmenuIndexScene1Import,
    SubmenuIndexSettings,
    SubmenuIndexNOP,
} SubmenuIndex;

// After a wallet is generated or imported with legacy encryption on a Flipper
// that has a secure enclave, offer to switch once back at the menu.
static inline void flipbip_suggest_enclave(FlipBip* app) {
    app->suggest_enclave = app->enclave_available && app->encryption == FlipBipEncryptionLegacy;
}
