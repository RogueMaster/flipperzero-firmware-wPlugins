#include "../breach_map_i.h"

/* index 0 is reserved for "add asset"; assets start at index 1. */
#define ASSET_LIST_ADD_INDEX 0
#define ASSET_LIST_OFFSET    1

static void breach_map_scene_asset_list_callback(void* context, uint32_t index) {
    BreachMapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void breach_map_scene_asset_list_on_enter(void* context) {
    BreachMapApp* app = context;
    Submenu* submenu = app->submenu;
    Session* session = app->session;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Assets");
    submenu_add_item(
        submenu, "[+] Add asset", ASSET_LIST_ADD_INDEX, breach_map_scene_asset_list_callback, app);

    FuriString* label = furi_string_alloc();
    for(uint16_t i = 0; i < session->asset_count; i++) {
        const Asset* a = &session->assets[i];
        furi_string_printf(label, "%s  r%u", a->name, a->risk);
        submenu_add_item(
            submenu,
            furi_string_get_cstr(label),
            i + ASSET_LIST_OFFSET,
            breach_map_scene_asset_list_callback,
            app);
    }
    furi_string_free(label);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, BreachMapSceneAssetList));

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool breach_map_scene_asset_list_on_event(void* context, SceneManagerEvent event) {
    BreachMapApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, BreachMapSceneAssetList, event.event);
        consumed = true;
        if(event.event == ASSET_LIST_ADD_INDEX) {
            uint16_t index = asset_manager_add(app->session);
            if(index == RECON_INVALID_INDEX) {
                app->message_mode = ReconMessageInfo;
                furi_string_set(app->message_text, "Asset limit reached");
                scene_manager_next_scene(app->scene_manager, BreachMapSceneMessage);
            } else {
                app->selected_asset = index;
                scene_manager_next_scene(app->scene_manager, BreachMapSceneAssetEdit);
            }
        } else {
            app->selected_asset = event.event - ASSET_LIST_OFFSET;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneAssetEdit);
        }
    }
    return consumed;
}

void breach_map_scene_asset_list_on_exit(void* context) {
    BreachMapApp* app = context;
    submenu_reset(app->submenu);
}
