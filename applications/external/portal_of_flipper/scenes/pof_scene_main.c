#include "../portal_of_flipper_i.h"
#include "../pof_token.h"

enum SubmenuIndex {
    SubmenuIndexLoad = POF_TOKEN_LIMIT,
};

void pof_scene_main_submenu_callback(void* context, uint32_t index) {
    PoFApp* pof = context;
    view_dispatcher_send_custom_event(pof->view_dispatcher, index);
}

void pof_scene_main_on_update(void* context) {
    PoFApp* pof = context;
    VirtualPortal* virtual_portal = pof->virtual_portal;

    Submenu* submenu = pof->submenu;
    submenu_reset(pof->submenu);
    FuriString* token_name = furi_string_alloc();

    if(pof->pof_usb) {
        int count = 0;
        for(int i = 0; i < POF_TOKEN_LIMIT; i++) {
            if(virtual_portal->tokens[i]->loaded) {
                PoFToken* pof_token = virtual_portal->tokens[i];
                furi_string_reset(token_name);
                //TODO: only do if debug mode
                furi_string_cat_printf(token_name, "%d: %s", i, pof_token->dev_name);

                // Unload figure
                submenu_add_item(
                    submenu,
                    furi_string_get_cstr(token_name),
                    i,
                    pof_scene_main_submenu_callback,
                    pof);
                count++;
            }
        }

        if(count < POF_TOKEN_LIMIT) {
            submenu_add_item(
                submenu, "<Load figure>", SubmenuIndexLoad, pof_scene_main_submenu_callback, pof);
        }

        submenu_set_selected_item(
            submenu, scene_manager_get_scene_state(pof->scene_manager, PoFSceneMain));
    } else {
        submenu_add_item(
            submenu, "Failed to start", SubmenuIndexLoad, pof_scene_main_submenu_callback, pof);
    }
    furi_string_free(token_name);
    view_dispatcher_switch_to_view(pof->view_dispatcher, PoFViewSubmenu);
}

void pof_scene_main_on_enter(void* context) {
    pof_scene_main_on_update(context);
}

bool pof_scene_main_on_event(void* context, SceneManagerEvent event) {
    PoFApp* pof = context;
    VirtualPortal* virtual_portal = pof->virtual_portal;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexLoad) {
            if(pof->pof_usb) {
                // Explicitly save state so that the correct item is
                // reselected if the user cancels loading a file.
                scene_manager_set_scene_state(pof->scene_manager, PoFSceneMain, SubmenuIndexLoad);
                scene_manager_next_scene(pof->scene_manager, PoFSceneFileSelect);
            }
            consumed = true;
        } else {
            scene_manager_set_scene_state(pof->scene_manager, PoFSceneMain, event.event);
            pof_token_clear(virtual_portal->tokens[event.event], true);
            pof_scene_main_on_update(context);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(pof->scene_manager);
        view_dispatcher_stop(pof->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void pof_scene_main_on_exit(void* context) {
    PoFApp* pof = context;
    submenu_reset(pof->submenu);
}
