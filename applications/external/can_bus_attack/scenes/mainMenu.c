#include "../app_user.h"

// This variable works to get the value of the selector and set it when the user
// enter at the Scene
static uint8_t menu_selector = 0;

// Function to display init
void draw_start(App* app) {
    widget_reset(app->widget);

    widget_add_icon_element(app->widget, 26, 1, &I_Icons_etsi_uma);
    widget_add_string_element(
        app->widget, 64, 40, AlignCenter, AlignCenter, FontPrimary, "CANBUS ATTACK APP");
    widget_add_string_element(
        app->widget,
        64,
        55,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        "TFG - Jorge Pimentel Naranjo");

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

void basic_scenes_menu_callback(void* context, uint32_t index) {
    App* app = context;

    menu_selector = index;

    switch(index) {
    case SniffingTestOption:
        scene_manager_handle_custom_event(app->scene_manager, SniffingOptionEvent);
        break;

    case AttackDoSOption:
        scene_manager_handle_custom_event(app->scene_manager, AttackDoSOptionEvent);
        break;

    case ReplayAttackOption:
        scene_manager_handle_custom_event(app->scene_manager, ReplayAttackOptionEvent);
        break;

    case ModifyAttackOption:
        scene_manager_handle_custom_event(app->scene_manager, ModifyAttackOptionEvent);
        break;

    case AboutUsOption:
        scene_manager_handle_custom_event(app->scene_manager, AboutUsEvent);
        break;

    default:
        break;
    }
}

void app_scene_menu_on_enter(void* context) {
    App* app = context;

    uint32_t state = scene_manager_get_scene_state(app->scene_manager, app_scene_main_menu);

    if(state == 0) {
        draw_start(app);
        furi_delay_ms(START_TIME);
        scene_manager_set_scene_state(app->scene_manager, app_scene_main_menu, 1);
    }

    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "MENU ATTACKS");

    submenu_add_item(
        app->submenu, "Sniffing", SniffingTestOption, basic_scenes_menu_callback, app);

    submenu_add_item(app->submenu, "DoS Attack", AttackDoSOption, basic_scenes_menu_callback, app);

    submenu_add_item(
        app->submenu, "Replay Attack", ReplayAttackOption, basic_scenes_menu_callback, app);

    submenu_add_item(
        app->submenu, "Modify Attack", ModifyAttackOption, basic_scenes_menu_callback, app);

    submenu_add_item(app->submenu, "About Us", AboutUsOption, basic_scenes_menu_callback, app);

    submenu_set_selected_item(app->submenu, menu_selector);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

    app->obdii_aux_index = 0;
}

bool app_scene_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case SniffingOptionEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_sniffing_option);
            consumed = true;
            break;

        case AttackDoSOptionEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_attack_dos_option);
            consumed = true;
            break;

        case ReplayAttackOptionEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_replay_attack_option);
            consumed = true;
            break;

        case ModifyAttackOptionEvent:
            if(app->modify_frame->canId == 0) {
                memcpy(app->modify_frame, &app->replay_frame, sizeof(CANFRAME));
            }
            scene_manager_next_scene(app->scene_manager, app_scene_attack_modify_option);
            consumed = true;
            break;

        case AboutUsEvent:
            scene_manager_next_scene(app->scene_manager, app_scene_about_us);
            consumed = true;
            break;

        default:
            break;
        }
        break;
    default:
        break;
    }
    return consumed;
}

void app_scene_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
