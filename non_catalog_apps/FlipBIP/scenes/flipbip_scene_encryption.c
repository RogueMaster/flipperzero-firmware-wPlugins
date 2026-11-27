#include "../flipbip.h"
#include "../helpers/flipbip_file.h"

// Confirms switching the saved wallet to app->encryption_target, either picked
// in settings or suggested after a wallet was saved with legacy encryption.

typedef enum {
    EncryptionStateConfirm,
    EncryptionStateFailed,
} EncryptionState;

static void flipbip_scene_encryption_callback(DialogExResult result, void* context) {
    FlipBip* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static void flipbip_scene_encryption_show(FlipBip* app, EncryptionState state) {
    DialogEx* dialog = app->encryption_dialog;
    dialog_ex_reset(dialog);
    dialog_ex_set_context(dialog, app);
    dialog_ex_set_result_callback(dialog, flipbip_scene_encryption_callback);
    scene_manager_set_scene_state(app->scene_manager, FlipBipSceneEncryption, state);

    if(state == EncryptionStateFailed) {
        dialog_ex_set_header(dialog, "Switch failed", 64, 4, AlignCenter, AlignTop);
        dialog_ex_set_text(
            dialog, "The saved wallet\nwas not changed.", 64, 22, AlignCenter, AlignTop);
        dialog_ex_set_center_button_text(dialog, "OK");
    } else if(app->encryption_target == FlipBipEncryptionEnclave) {
        dialog_ex_set_header(
            dialog,
            app->suggest_enclave ? "Use Secure Enclave?" : "Switch to Enclave?",
            64,
            0,
            AlignCenter,
            AlignTop);
        dialog_ex_set_text(
            dialog,
            "Wallet will only open on\nthis Flipper. Keep your\nmnemonic backed up!",
            64,
            14,
            AlignCenter,
            AlignTop);
        dialog_ex_set_left_button_text(dialog, "No");
        dialog_ex_set_right_button_text(dialog, "Yes");
    } else {
        dialog_ex_set_header(dialog, "Switch to Legacy?", 64, 0, AlignCenter, AlignTop);
        dialog_ex_set_text(
            dialog,
            "Wallet will open on any\nFlipper with FlipBIP.\nLess secure!",
            64,
            14,
            AlignCenter,
            AlignTop);
        dialog_ex_set_left_button_text(dialog, "No");
        dialog_ex_set_right_button_text(dialog, "Yes");
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipBipViewEncryptionConfirm);
}

void flipbip_scene_encryption_on_enter(void* context) {
    FlipBip* app = context;
    flipbip_scene_encryption_show(app, EncryptionStateConfirm);
}

bool flipbip_scene_encryption_on_event(void* context, SceneManagerEvent event) {
    FlipBip* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == DialogExResultRight &&
       scene_manager_get_scene_state(app->scene_manager, FlipBipSceneEncryption) ==
           EncryptionStateConfirm) {
        if(!flipbip_switch_encryption(app->encryption_target)) {
            flipbip_scene_encryption_show(app, EncryptionStateFailed);
            return true;
        }
        app->encryption = app->encryption_target;
    }
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void flipbip_scene_encryption_on_exit(void* context) {
    FlipBip* app = context;
    app->suggest_enclave = false;
    dialog_ex_reset(app->encryption_dialog);
}
