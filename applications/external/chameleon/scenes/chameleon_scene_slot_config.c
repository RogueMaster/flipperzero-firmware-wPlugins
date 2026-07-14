#include "../chameleon_app_i.h"

typedef enum {
    SubmenuIndexActivate,
    SubmenuIndexRename,
    SubmenuIndexChangeType,
    SubmenuIndexBack,
} SubmenuIndex;

static void chameleon_scene_slot_config_submenu_callback(void* context, uint32_t index) {
    ChameleonApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void chameleon_scene_slot_config_on_enter(void* context) {
    ChameleonApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);

    char header[32];
    snprintf(header, sizeof(header), "Slot %d Configuration", app->active_slot);

    submenu_add_item(
        submenu,
        "Activate Slot",
        SubmenuIndexActivate,
        chameleon_scene_slot_config_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Rename Slot",
        SubmenuIndexRename,
        chameleon_scene_slot_config_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Change Tag Type",
        SubmenuIndexChangeType,
        chameleon_scene_slot_config_submenu_callback,
        app);

    submenu_set_header(submenu, header);

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
}

bool chameleon_scene_slot_config_on_event(void* context, SceneManagerEvent event) {
    ChameleonApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubmenuIndexActivate:
            if(chameleon_app_set_active_slot(app, app->active_slot)) {
                popup_set_header(app->popup, "Success", 64, 10, AlignCenter, AlignTop);
                snprintf(
                    app->text_buffer,
                    sizeof(app->text_buffer),
                    "Slot %d activated",
                    app->active_slot);
                popup_set_text(app->popup, app->text_buffer, 64, 32, AlignCenter, AlignCenter);
            } else {
                popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(app->popup, "Failed to activate", 64, 32, AlignCenter, AlignCenter);
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);
            furi_delay_ms(1500);
            view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
            consumed = true;
            break;

        case SubmenuIndexRename:
            scene_manager_next_scene(app->scene_manager, ChameleonSceneSlotRename);
            consumed = true;
            break;

        case SubmenuIndexChangeType:
            popup_set_header(app->popup, "Coming Soon", 64, 10, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "Tag type change\nnot yet implemented",
                64,
                32,
                AlignCenter,
                AlignCenter);
            view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);
            furi_delay_ms(1500);
            view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void chameleon_scene_slot_config_on_exit(void* context) {
    ChameleonApp* app = context;
    submenu_reset(app->submenu);
    popup_reset(app->popup);
}
