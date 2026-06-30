#include "ami_tool_i.h"

/* Local indexes for submenu items */
typedef enum {
    AmiToolMainMenuIndexRead,
    AmiToolMainMenuIndexGenerate,
    AmiToolMainMenuIndexAmiiboLink,
    AmiToolMainMenuIndexSaved,
    AmiToolMainMenuIndexBluetooth,
    AmiToolMainMenuIndexExit,
} AmiToolMainMenuIndex;

static void ami_tool_scene_main_menu_show_error(AmiToolApp* app, const char* message) {
    furi_assert(app);
    if(!message) {
        message = "Unknown error";
    }
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, message);
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
    app->main_menu_error_visible = true;
}

static bool ami_tool_scene_main_menu_require_key(AmiToolApp* app) {
    furi_assert(app);

    if(ami_tool_has_retail_key(app)) {
        return true;
    }

    AmiToolRetailKeyStatus status = ami_tool_load_retail_key(app);
    switch(status) {
    case AmiToolRetailKeyStatusOk:
        return true;
    case AmiToolRetailKeyStatusNotFound:
        ami_tool_scene_main_menu_show_error(app, "key_retail.bin file does not exist");
        break;
    case AmiToolRetailKeyStatusInvalidSize:
        ami_tool_scene_main_menu_show_error(app, "key_retail.bin file data error");
        break;
    default:
        ami_tool_scene_main_menu_show_error(app, "key_retail.bin file read error");
        break;
    }
    return false;
}

static void ami_tool_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    AmiToolApp* app = context;

    switch(index) {
    case AmiToolMainMenuIndexRead:
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventMainMenuRead);
        break;
    case AmiToolMainMenuIndexGenerate:
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventMainMenuGenerate);
        break;
    case AmiToolMainMenuIndexAmiiboLink:
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventMainMenuAmiiboLink);
        break;
    case AmiToolMainMenuIndexSaved:
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventMainMenuSaved);
        break;
    case AmiToolMainMenuIndexBluetooth:
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventMainMenuBluetooth);
        break;
    case AmiToolMainMenuIndexExit:
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventMainMenuExit);
        break;
    default:
        break;
    }
}

void ami_tool_scene_main_menu_on_enter(void* context) {
    AmiToolApp* app = context;

    app->main_menu_error_visible = false;
    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu,
        "Read",
        AmiToolMainMenuIndexRead,
        ami_tool_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Generate",
        AmiToolMainMenuIndexGenerate,
        ami_tool_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Blank Tag Emulation",
        AmiToolMainMenuIndexAmiiboLink,
        ami_tool_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Saved",
        AmiToolMainMenuIndexSaved,
        ami_tool_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Bluetooth Connection",
        AmiToolMainMenuIndexBluetooth,
        ami_tool_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Exit",
        AmiToolMainMenuIndexExit,
        ami_tool_scene_main_menu_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
}

bool ami_tool_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case AmiToolEventMainMenuRead:
            scene_manager_next_scene(app->scene_manager, AmiToolSceneRead);
            return true;
        case AmiToolEventMainMenuGenerate:
            if(ami_tool_scene_main_menu_require_key(app)) {
                scene_manager_next_scene(app->scene_manager, AmiToolSceneGenerate);
            }
            return true;
        case AmiToolEventMainMenuSaved:
            scene_manager_next_scene(app->scene_manager, AmiToolSceneSaved);
            return true;
        case AmiToolEventMainMenuAmiiboLink:
            scene_manager_next_scene(app->scene_manager, AmiToolSceneAmiiboLink);
            return true;
        case AmiToolEventMainMenuBluetooth:
            scene_manager_next_scene(app->scene_manager, AmiToolSceneBluetooth);
            return true;
        case AmiToolEventMainMenuExit:
            scene_manager_stop(app->scene_manager);
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(app->main_menu_error_visible) {
            app->main_menu_error_visible = false;
            view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
            return true;
        }
        /* Back from main menu exits the app */
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    return false;
}

void ami_tool_scene_main_menu_on_exit(void* context) {
    UNUSED(context);
    /* Nothing to clean up */
}
