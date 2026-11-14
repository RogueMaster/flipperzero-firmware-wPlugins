#include "../nfc_magic_app_i.h"
#include "nfc_magic_scene.h"

enum SubmenuIndex {
    SubmenuIndexIso15693Write, // clone a saved .nfc onto the card (like the other magic types)
    SubmenuIndexIso15693Wipe, // zero every data block, 56/57/62/63 included
    SubmenuIndexIso15693WriteUid, // enter a UID by hand (magic-only bonus)
    SubmenuIndexIso15693Info, // read + show the card in front of you
};

void nfc_magic_scene_iso15693_submenu_callback(void* context, uint32_t index) {
    NfcMagicApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void nfc_magic_scene_iso15693_on_enter(void* context) {
    NfcMagicApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu,
        "Write",
        SubmenuIndexIso15693Write,
        nfc_magic_scene_iso15693_submenu_callback,
        app);

    submenu_add_item(
        submenu, "Wipe", SubmenuIndexIso15693Wipe, nfc_magic_scene_iso15693_submenu_callback, app);

    submenu_add_item(
        submenu,
        "Write UID",
        SubmenuIndexIso15693WriteUid,
        nfc_magic_scene_iso15693_submenu_callback,
        app);

    submenu_add_item(
        submenu, "Info", SubmenuIndexIso15693Info, nfc_magic_scene_iso15693_submenu_callback, app);

    submenu_set_header(submenu, "ISO15693 / NfcV");

    // Restore the cursor, as every other menu in the app does. This scene is the return target for
    // the whole feature -- Info, the write-fail screens and the gen1 opt-in all come back here --
    // and on_exit's submenu_reset drops the cursor to index 0, which is Write. Without this, reading
    // a card's Info and pressing Back leaves the selection on the destructive item.
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, NfcMagicSceneIso15693));

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcMagicAppViewMenu);
}

bool nfc_magic_scene_iso15693_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, NfcMagicSceneIso15693, event.event);
        if(event.event == SubmenuIndexIso15693Write) {
            // Clone a saved ISO15693 .nfc onto the magic card, via the shared file-select + write
            // flow (same as Gen1/Gen2/USCUID-UL).
            app->iso15693_mode = NfcMagicIso15693ModeClone;
            app->iso15693_force_gen1 = false; // a fresh clone always tries gen2 first
            scene_manager_next_scene(app->scene_manager, NfcMagicSceneFileSelect);
            consumed = true;
        } else if(event.event == SubmenuIndexIso15693Wipe) {
            // Zero every data block, 56/57/62/63 included (no source file).
            app->iso15693_mode = NfcMagicIso15693ModeWipe;
            scene_manager_next_scene(app->scene_manager, NfcMagicSceneWriteConfirm);
            consumed = true;
        } else if(event.event == SubmenuIndexIso15693WriteUid) {
            app->iso15693_mode = NfcMagicIso15693ModeWriteUid;
            app->iso15693_force_gen1 = false; // a fresh Write-UID always tries gen2 first
            scene_manager_next_scene(app->scene_manager, NfcMagicSceneIso15693WriteInput);
            consumed = true;
        } else if(event.event == SubmenuIndexIso15693Info) {
            scene_manager_next_scene(app->scene_manager, NfcMagicSceneIso15693GetInfo);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, NfcMagicSceneStart);
    }

    return consumed;
}

void nfc_magic_scene_iso15693_on_exit(void* context) {
    NfcMagicApp* app = context;
    submenu_reset(app->submenu);
}
