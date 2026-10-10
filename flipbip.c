#include "flipbip.h"
#include "helpers/flipbip_file.h"
// From: lib/crypto
#include <memzero.h>
#include <bip39.h>

#define MNEMONIC_MENU_DEFAULT "Import mnemonic seed"
#define MNEMONIC_MENU_SUCCESS "Import seed (success)"
#define MNEMONIC_MENU_FAILURE "Import seed (failed!)"

bool flipbip_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FlipBip* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

//leave app if back button pressed
bool flipbip_navigation_event_callback(void* context) {
    furi_assert(context);
    FlipBip* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void text_input_callback(void* context) {
    furi_assert(context);
    FlipBip* app = context;
    const bool has_text = strlen(app->input_text) > 0;

    if(app->input_state == FlipBipTextInputPassphrase) {
        if(has_text && app->passphrase == FlipBipPassphraseOn) {
            strcpy(app->passphrase_text, app->input_text);
        }
    } else if(app->input_state == FlipBipTextInputMnemonic) {
        if(has_text && app->import_from_mnemonic == 1) {
            // Validate and save straight from the input buffer
            if(mnemonic_check(app->input_text) != 0 &&
               flipbip_save_file_secure(app->input_text)) {
                app->mnemonic_menu_text = MNEMONIC_MENU_SUCCESS;
            } else {
                app->mnemonic_menu_text = MNEMONIC_MENU_FAILURE;
            }
        }
    }

    const FlipBipTextInputState state = app->input_state;
    memzero(app->input_text, TEXT_BUFFER_SIZE);
    app->input_state = FlipBipTextInputDefault;

    if(state == FlipBipTextInputMnemonic) {
        // Leave the scene 1 instance used for text input, back to the menu
        scene_manager_previous_scene(app->scene_manager);
    } else if(state == FlipBipTextInputPassphrase) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipBipViewIdSettings);
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipBipViewIdMenu);
    }
}

static void flipbip_scene_renew_dialog_callback(DialogExResult result, void* context) {
    FlipBip* app = context;
    if(result == DialogExResultRight) {
        app->wallet_create(app);
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipBipViewIdMenu);
    }
}

static void flipbip_wallet_create(void* context) {
    FlipBip* app = context;
    furi_assert(app);
    scene_manager_set_scene_state(app->scene_manager, FlipBipSceneMenu, SubmenuIndexScene1New);
    scene_manager_next_scene(app->scene_manager, FlipBipSceneScene_1);
}

FlipBip* flipbip_app_alloc() {
    FlipBip* app = malloc(sizeof(FlipBip));
    app->gui = furi_record_open(RECORD_GUI);
    //app->notification = furi_record_open(RECORD_NOTIFICATION);

    // Turn backlight on, believe me this makes testing your app easier
    //notification_message(app->notification, &sequence_display_backlight_on);

    // Scene additions
    app->view_dispatcher = view_dispatcher_alloc();

    app->scene_manager = scene_manager_alloc(&flipbip_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flipbip_navigation_event_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, flipbip_custom_event_callback);
    app->submenu = submenu_alloc();

    // Settings
    app->bip39_strength = FlipBipStrength256; // 256 bits (24 words)
    app->passphrase = FlipBipPassphraseOff;

    // Main menu
    app->coin_type = CoinTypeBTC0; // 0 (BTC)
    app->overwrite_saved_seed = 0;
    app->import_from_mnemonic = 0;
    app->mnemonic_menu_text = MNEMONIC_MENU_DEFAULT;

    // Text input
    app->input_state = FlipBipTextInputDefault;

    view_dispatcher_add_view(
        app->view_dispatcher, FlipBipViewIdMenu, submenu_get_view(app->submenu));
    app->flipbip_scene_1 = flipbip_scene_1_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipBipViewIdScene1, flipbip_scene_1_get_view(app->flipbip_scene_1));
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipBipViewIdSettings,
        variable_item_list_get_view(app->variable_item_list));

    app->text_input = text_input_alloc();
    text_input_set_result_callback(
        app->text_input,
        text_input_callback,
        (void*)app,
        app->input_text,
        TEXT_BUFFER_SIZE,
        // clear default text
        true);
    //text_input_set_header_text(app->text_input, "Input");
    view_dispatcher_add_view(
        app->view_dispatcher, FlipBipViewIdTextInput, text_input_get_view(app->text_input));

    app->wallet_create = flipbip_wallet_create;
    app->renew_dialog = dialog_ex_alloc();
    dialog_ex_set_result_callback(app->renew_dialog, flipbip_scene_renew_dialog_callback);
    dialog_ex_set_context(app->renew_dialog, app);
    dialog_ex_set_left_button_text(app->renew_dialog, "No");
    dialog_ex_set_right_button_text(app->renew_dialog, "Yes");
    dialog_ex_set_header(
        app->renew_dialog,
        "Current wallet\nwill be deleted!\nProceed?",
        16,
        12,
        AlignLeft,
        AlignTop);
    view_dispatcher_add_view(
        app->view_dispatcher, FlipBipViewRenewConfirm, dialog_ex_get_view(app->renew_dialog));

    // End Scene Additions

    return app;
}

void flipbip_app_free(FlipBip* app) {
    furi_assert(app);

    // Scene manager
    scene_manager_free(app->scene_manager);

    // Views must be removed from the dispatcher before they are freed
    view_dispatcher_remove_view(app->view_dispatcher, FlipBipViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlipBipViewIdScene1);
    view_dispatcher_remove_view(app->view_dispatcher, FlipBipViewIdSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FlipBipViewIdTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, FlipBipViewRenewConfirm);

    submenu_free(app->submenu);
    flipbip_scene_1_free(app->flipbip_scene_1);
    variable_item_list_free(app->variable_item_list);
    text_input_free(app->text_input);
    dialog_ex_free(app->renew_dialog);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    // Wipe passphrase / input buffers
    memzero(app, sizeof(FlipBip));
    free(app);
}

int32_t flipbip_app(void* p) {
    UNUSED(p);
    FlipBip* app = flipbip_app_alloc();

    // Disabled because causes exit on custom firmwares such as RM
    /*if(!furi_hal_region_is_provisioned()) {
        flipbip_app_free(app);
        return 1;
    }*/

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, FlipBipSceneMenu); //Start with menu

    furi_hal_power_suppress_charge_enter();

    view_dispatcher_run(app->view_dispatcher);

    furi_hal_power_suppress_charge_exit();
    flipbip_app_free(app);

    return 0;
}
