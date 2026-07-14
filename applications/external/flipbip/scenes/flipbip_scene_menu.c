#include "../flipbip.h"
#include "../helpers/flipbip_file.h"

// #define FLIPBIP_SUBMENU_TEXT "** FlipBIP wallet " FLIPBIP_VERSION " **"

void flipbip_scene_menu_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    FlipBip* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flipbip_scene_menu_on_enter(void* context) {
    FlipBip* app = context;

    // // FlipBIP header with version
    // submenu_add_item(
    //     app->submenu,
    //     FLIPBIP_SUBMENU_TEXT,
    //     SubmenuIndexNOP,
    //     flipbip_scene_menu_submenu_callback,
    //     app);

    if(flipbip_has_file(FlipBipFileKey, NULL, false) &&
       flipbip_has_file(FlipBipFileDat, NULL, false)) {
        for(uint32_t coin_type = 0; coin_type < NUM_COINS; coin_type++) {
            char wallet_menu_item[17] = "View      wallet";
            strncpy(
                wallet_menu_item + 5,
                COIN_TEXT_ARRAY[coin_type][COIN_TEXT_LABEL],
                strlen(COIN_TEXT_ARRAY[coin_type][COIN_TEXT_LABEL]));
            submenu_add_item(
                app->submenu,
                wallet_menu_item,
                coin_type,
                flipbip_scene_menu_submenu_callback,
                app);
        }

        submenu_add_item(
            app->submenu,
            "Regenerate wallet",
            SubmenuIndexScene1Renew,
            flipbip_scene_menu_submenu_callback,
            app);
    } else {
        submenu_add_item(
            app->submenu,
            "Generate new wallet",
            SubmenuIndexScene1New,
            flipbip_scene_menu_submenu_callback,
            app);
    }
    submenu_add_item(
        app->submenu,
        app->mnemonic_menu_text,
        SubmenuIndexScene1Import,
        flipbip_scene_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu, "Settings", SubmenuIndexSettings, flipbip_scene_menu_submenu_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, FlipBipSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipBipViewIdMenu);
}

bool flipbip_scene_menu_on_event(void* context, SceneManagerEvent event) {
    FlipBip* app = context;
    //UNUSED(app);
    if(event.type == SceneManagerEventTypeBack) {
        //exit app
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < SubmenuIndexScene1New) {
            app->overwrite_saved_seed = 0;
            app->import_from_mnemonic = 0;
            app->coin_type = event.event; // CoinType
            scene_manager_set_scene_state(
                app->scene_manager, FlipBipSceneMenu, event.event); // CoinType
            scene_manager_next_scene(app->scene_manager, FlipBipSceneScene_1);
            return true;
        } else if(event.event == SubmenuIndexScene1New) {
            app->overwrite_saved_seed = 1;
            app->import_from_mnemonic = 0;
            app->wallet_create(app);
            return true;
        } else if(event.event == SubmenuIndexScene1Renew) {
            app->overwrite_saved_seed = 1;
            app->import_from_mnemonic = 0;
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipBipViewRenewConfirm);
            return true;
        } else if(event.event == SubmenuIndexScene1Import) {
            app->import_from_mnemonic = 1;
            scene_manager_set_scene_state(
                app->scene_manager, FlipBipSceneMenu, SubmenuIndexScene1Import);
            scene_manager_next_scene(app->scene_manager, FlipBipSceneScene_1);
            return true;
        } else if(event.event == SubmenuIndexSettings) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipBipSceneMenu, SubmenuIndexSettings);
            scene_manager_next_scene(app->scene_manager, FlipBipSceneSettings);
            return true;
        } else if(event.event == SubmenuIndexNOP) {
            return true;
        }
    }
    return false;
}

void flipbip_scene_menu_on_exit(void* context) {
    FlipBip* app = context;
    submenu_reset(app->submenu);
}
