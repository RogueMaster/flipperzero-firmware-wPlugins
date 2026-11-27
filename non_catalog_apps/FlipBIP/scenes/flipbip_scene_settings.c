#include "../flipbip.h"
#include "../helpers/flipbip_file.h"
#include <lib/toolbox/value_index.h>
// From: lib/crypto
#include <memzero.h>

#define TEXT_LABEL_ON  "ON"
#define TEXT_LABEL_OFF "OFF"

const char* const bip39_strength_text[3] = {
    "12",
    "18",
    "24",
};
const uint32_t bip39_strength_value[3] = {
    FlipBipStrength128,
    FlipBipStrength192,
    FlipBipStrength256,
};

const char* const passphrase_text[2] = {
    TEXT_LABEL_OFF,
    TEXT_LABEL_ON,
};
const uint32_t passphrase_value[2] = {
    FlipBipPassphraseOff,
    FlipBipPassphraseOn,
};

const char* const encryption_text[2] = {
    "Legacy",
    "Enclave",
};
const uint32_t encryption_value[2] = {
    FlipBipEncryptionLegacy,
    FlipBipEncryptionEnclave,
};

static void flipbip_scene_settings_set_bip39_strength(VariableItem* item) {
    FlipBip* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, bip39_strength_text[index]);
    app->bip39_strength = bip39_strength_value[index];
}

static void flipbip_scene_settings_set_passphrase(VariableItem* item) {
    FlipBip* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, passphrase_text[index]);
    app->passphrase = passphrase_value[index];

    if(app->passphrase == FlipBipPassphraseOn) {
        app->input_state = FlipBipTextInputPassphrase;
        text_input_set_header_text(app->text_input, "Enter BIP39 passphrase");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipBipViewIdTextInput);
    } else {
        memzero(app->passphrase_text, TEXT_BUFFER_SIZE);
    }
}

static void flipbip_scene_settings_set_encryption(VariableItem* item) {
    FlipBip* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(flipbip_wallet_format() == FlipBipWalletNone) {
        // Nothing saved yet: only changes how the next wallet is saved
        variable_item_set_current_value_text(item, encryption_text[index]);
        app->encryption = encryption_value[index];
    } else {
        // Re-encrypting the saved wallet needs confirming first; the list is
        // rebuilt from app->encryption when coming back to this scene
        app->encryption_target = encryption_value[index];
        view_dispatcher_send_custom_event(
            app->view_dispatcher, FlipBipCustomEventSettingsEncryption);
    }
}

void flipbip_scene_settings_submenu_callback(void* context, uint32_t index) {
    FlipBip* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flipbip_scene_settings_on_enter(void* context) {
    FlipBip* app = context;
    VariableItem* item;
    uint8_t value_index;

    // BIP39 strength
    item = variable_item_list_add(
        app->variable_item_list, "BIP39 Words:", 3, flipbip_scene_settings_set_bip39_strength, app);
    value_index = value_index_uint32(app->bip39_strength, bip39_strength_value, 3);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, bip39_strength_text[value_index]);

    // Passphrase
    item = variable_item_list_add(
        app->variable_item_list,
        "BIP39 Passphrase:",
        2,
        flipbip_scene_settings_set_passphrase,
        app);
    value_index = value_index_uint32(app->passphrase, passphrase_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, passphrase_text[value_index]);

    // Encryption, only legacy without a secure enclave (Flipper clones)
    item = variable_item_list_add(
        app->variable_item_list,
        "Encryption:",
        app->enclave_available ? 2 : 1,
        flipbip_scene_settings_set_encryption,
        app);
    value_index = value_index_uint32(app->encryption, encryption_value, 2);
    variable_item_set_current_value_index(item, app->enclave_available ? value_index : 0);
    variable_item_set_current_value_text(item, encryption_text[value_index]);

    variable_item_list_set_selected_item(
        app->variable_item_list,
        scene_manager_get_scene_state(app->scene_manager, FlipBipSceneSettings));

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipBipViewIdSettings);
}

bool flipbip_scene_settings_on_event(void* context, SceneManagerEvent event) {
    FlipBip* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == FlipBipCustomEventSettingsEncryption) {
        scene_manager_set_scene_state(app->scene_manager, FlipBipSceneSettings, 2);
        scene_manager_next_scene(app->scene_manager, FlipBipSceneEncryption);
        consumed = true;
    }
    return consumed;
}

void flipbip_scene_settings_on_exit(void* context) {
    FlipBip* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    variable_item_list_reset(app->variable_item_list);
}
