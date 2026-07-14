#include "../../app_user.h"

// Thread
static int32_t obdii_thread_getting_pid_supported_on_work(void* context);

// Callback for the supported menu
void supported_pid_callback(void* context, uint32_t index) {
    App* app = context;

    UNUSED(app);

    switch(index) {
    case 0x100:
        app->flags = BLOCK_A;
        scene_manager_next_scene(app->scene_manager, app_scene_list_supported_pid_option);
        break;

    case 0x101:
        app->flags = BLOCK_B;
        scene_manager_next_scene(app->scene_manager, app_scene_list_supported_pid_option);
        break;

    case 0x102:
        app->flags = BLOCK_C;
        scene_manager_next_scene(app->scene_manager, app_scene_list_supported_pid_option);
        break;

    case 0x103:
        app->flags = BLOCK_D;
        scene_manager_next_scene(app->scene_manager, app_scene_list_supported_pid_option);
        break;

    default:
        break;
    }
}

// Scene on enter
void app_scene_menu_supported_pid_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Get Supported PID");
    submenu_add_item(app->submenu, "Block A", 0x100, supported_pid_callback, app);
    submenu_add_item(app->submenu, "Block B", 0x101, supported_pid_callback, app);
    submenu_add_item(app->submenu, "Block C", 0x102, supported_pid_callback, app);
    submenu_add_item(app->submenu, "Block D", 0x103, supported_pid_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Scene on Event
bool app_scene_menu_supported_pid_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Scene on exit
void app_scene_menu_supported_pid_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

/*
    Scene of the pid list
*/

// Scene on enter
void app_scene_list_supported_pid_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);

    if(scene_manager_get_scene_state(app->scene_manager, app_scene_list_supported_pid_option) ==
       1) {
        scene_manager_set_scene_state(app->scene_manager, app_scene_list_supported_pid_option, 0);
        scene_manager_previous_scene(app->scene_manager);
    } else {
        app->thread = furi_thread_alloc_ex(
            "GetSupportedPID", 1024, obdii_thread_getting_pid_supported_on_work, app);
        furi_thread_start(app->thread);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Scene on event
bool app_scene_list_supported_pid_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    if(event.event == DEVICE_NO_CONNECTED) {
        consumed = true;

        // Go to the scene
        scene_manager_set_scene_state(app->scene_manager, app_scene_list_supported_pid_option, 1);

        // Set the scene to know the error
        scene_manager_set_scene_state(app->scene_manager, app_scene_obdii_warning_scenes, 0);
        scene_manager_next_scene(app->scene_manager, app_scene_obdii_warning_scenes);
    }
    if(event.event == MESSAGE_ERROR) {
        scene_manager_set_scene_state(app->scene_manager, app_scene_list_supported_pid_option, 0);

        // Set the scene to know the error
        scene_manager_set_scene_state(app->scene_manager, app_scene_list_supported_pid_option, 1);
        scene_manager_set_scene_state(app->scene_manager, app_scene_obdii_warning_scenes, 1);
        scene_manager_next_scene(app->scene_manager, app_scene_obdii_warning_scenes);
    }
    return consumed;
}

// Scene on exit
void app_scene_list_supported_pid_on_exit(void* context) {
    App* app = context;
    if(scene_manager_get_scene_state(app->scene_manager, app_scene_list_supported_pid_option) ==
       0) {
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
    }

    submenu_reset(app->submenu);
}

/*
    Thread to get the supported PID
*/

static int32_t obdii_thread_getting_pid_supported_on_work(void* context) {
    App* app = context;

    OBDII scanner;

    scanner.bitrate = app->mcp_can->bitRate;

    bool run = pid_init(&scanner);

    bool draw_list = false;

    FuriString* text = app->text;

    // if the device is not detected the thread send an custom event
    if(!run) {
        scene_manager_set_scene_state(app->scene_manager, app_scene_supported_pid_option, 1);
        view_dispatcher_send_custom_event(app->view_dispatcher, DEVICE_NO_CONNECTED);
    }

    // if it runs
    if(run) {
        // Time delay added to initialize
        furi_delay_ms(500);

        uint32_t flag = app->flags;

        if(pid_get_supported_pid(&scanner, flag)) {
            draw_list = true;

        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, MESSAGE_ERROR);
        }

        if(draw_list) {
            for(uint8_t i = 1; i <= 32; i++) {
                furi_string_reset(text);

                if(scanner.codes[i + flag].is_supported) {
                    furi_string_cat_printf(text, "0x%lx Supported", i + flag);
                }

                if(!scanner.codes[i + flag].is_supported) {
                    furi_string_cat_printf(text, "0x%lx Not Supported", i + flag);
                }

                submenu_add_item(
                    app->submenu, furi_string_get_cstr(text), i, supported_pid_callback, app);
            }
        }
    }

    pid_deinit(&scanner);

    return 0;
}
