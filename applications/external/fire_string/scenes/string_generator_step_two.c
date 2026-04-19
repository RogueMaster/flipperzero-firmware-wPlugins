#include "../fire_string.h"

/** indicies for menu items*/
typedef enum {
    FireStringStepTwoSelection_USB,
    FireStringStepTwoSelection_BT,
    // FireStringStepTwoSelection_NFC,      // TODO?
    // FireStringStepTwoSelection_RFID,     // TODO?
    // FireStringStepTwoSelection_SUBGHZ    // TODO?
    // FireStringStepTwoSelection_Infrared  // TODO?
    FireStringStepTwoSelection_Save,
    FireStringStepTwoSelection_Restart,
    FireStringStepTwoSelection_Exit
} FireStringStepTwoSelection;

void fire_string_menu_callback_step_two_menu(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "fire_string_menu_callback_step_two_menu");
    furi_check(context);

    FireString* app = context;

    switch(index) {
    case FireStringStepTwoSelection_USB:
        scene_manager_next_scene(app->scene_manager, FireStringScene_LoadingUSB);
        break;
    case FireStringStepTwoSelection_BT:
        scene_manager_next_scene(app->scene_manager, FireStringScene_Bluetooth);
        break;
    case FireStringStepTwoSelection_Save:
        scene_manager_next_scene(app->scene_manager, FireStringScene_SaveString);
        break;
    case FireStringStepTwoSelection_Restart:
        furi_string_reset(app->fire_string);
        app->settings->file_loaded = false;
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FireStringScene_MainMenu);
        break;
    case FireStringStepTwoSelection_Exit:
        view_dispatcher_stop(app->view_dispatcher);
        break;
    }
}

void fire_string_scene_on_enter_step_two_menu(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_step_two_menu");
    furi_check(context);

    FireString* app = context;

    submenu_add_item(
        app->submenu,
        "USB Export",
        FireStringStepTwoSelection_USB,
        fire_string_menu_callback_step_two_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Bluetooth Export",
        FireStringStepTwoSelection_BT,
        fire_string_menu_callback_step_two_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Save to internal storage",
        FireStringStepTwoSelection_Save,
        fire_string_menu_callback_step_two_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Restart",
        FireStringStepTwoSelection_Restart,
        fire_string_menu_callback_step_two_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Exit",
        FireStringStepTwoSelection_Exit,
        fire_string_menu_callback_step_two_menu,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_SubMenu);
}

/** main menu event handler - switches scene based on the event */
bool fire_string_scene_on_event_step_two_menu(void* context, SceneManagerEvent event) {
    // FURI_LOG_T(TAG, "fire_string_scene_on_event_step_two_menu");
    furi_check(context);

    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void fire_string_scene_on_exit_step_two_menu(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_step_two_menu");
    furi_check(context);

    FireString* app = context;
    submenu_reset(app->submenu);
}
