#include "../nfc_dict_manager.h"
#include "../nfc_dict_manager_utils.h"

void nfc_dict_manager_scene_merge_on_enter(void* context) {
    NfcDictManager* app = context;

    nfc_dict_manager_create_directories(app->storage);

    submenu_reset(app->submenu);

    FuriString* dict_a_display = furi_string_alloc();
    FuriString* dict_b_display = furi_string_alloc();

    if(furi_string_empty(app->selected_dict_a)) {
        furi_string_set(dict_a_display, "Dict A: Not selected");
    } else {
        FuriString* filename_a = furi_string_alloc();
        path_extract_filename(app->selected_dict_a, filename_a, false);
        furi_string_printf(dict_a_display, "Dict A: %s", furi_string_get_cstr(filename_a));
        furi_string_free(filename_a);
    }

    if(furi_string_empty(app->selected_dict_b)) {
        furi_string_set(dict_b_display, "Dict B: Not selected");
    } else {
        FuriString* filename_b = furi_string_alloc();
        path_extract_filename(app->selected_dict_b, filename_b, false);
        furi_string_printf(dict_b_display, "Dict B: %s", furi_string_get_cstr(filename_b));
        furi_string_free(filename_b);
    }

    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(dict_a_display),
        0,
        nfc_dict_manager_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(dict_b_display),
        1,
        nfc_dict_manager_submenu_callback,
        app);

    if(!furi_string_empty(app->selected_dict_a) && !furi_string_empty(app->selected_dict_b)) {
        submenu_add_item(app->submenu, "Start Merge", 2, nfc_dict_manager_submenu_callback, app);
    }

    furi_string_free(dict_a_display);
    furi_string_free(dict_b_display);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewSubmenu);
}

bool nfc_dict_manager_scene_merge_on_event(void* context, SceneManagerEvent event) {
    NfcDictManager* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case 0: // dict A
            scene_manager_next_scene(app->scene_manager, NfcDictManagerSceneMergeSelectA);
            consumed = true;
            break;
        case 1: // dict B
            scene_manager_next_scene(app->scene_manager, NfcDictManagerSceneMergeSelectB);
            consumed = true;
            break;
        case 2: // merge
            if(!furi_string_empty(app->selected_dict_a) &&
               !furi_string_empty(app->selected_dict_b)) {
                app->merge_stage = 0;

                popup_set_header(app->popup, "Processing...", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup,
                    "Merging dictionaries\nand validating keys...",
                    64,
                    32,
                    AlignCenter,
                    AlignCenter);
                popup_set_timeout(app->popup, 0);
                popup_set_context(app->popup, app);
                popup_set_callback(app->popup, NULL);

                furi_timer_start(app->merge_timer, 1500);

                view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewPopup);
            }
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_dict_manager_scene_merge_on_exit(void* context) {
    NfcDictManager* app = context;
    submenu_reset(app->submenu);

    if(scene_manager_get_scene_state(app->scene_manager, NfcDictManagerSceneStart)) {
        furi_string_reset(app->selected_dict_a);
        furi_string_reset(app->selected_dict_b);
    }
}
