#include "../../app_user.h"

static int32_t obdii_thread_on_work(void* context);

/*

  SHOW TYPICAL DATA ON PID CODES

*/

// Selector of option
void typical_menu_callback(void* context, uint32_t index) {
    App* app = context;

    app->obdii_aux_index = index;
    scene_manager_set_scene_state(app->scene_manager, app_scene_obdii_typical_codes_option, index);
    scene_manager_next_scene(
        app->scene_manager,
        app_scene_draw_obii_option); // It will go directly to the respective scene
}

// Scene on enter
void app_scene_obdii_typical_codes_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_add_item(app->submenu, "Engine Speed", 0, typical_menu_callback, app);
    submenu_add_item(app->submenu, "Vehicle Speed", 1, typical_menu_callback, app);
    submenu_add_item(app->submenu, "Calculated Engine Load", 2, typical_menu_callback, app);
    submenu_add_item(app->submenu, "Throttle Position", 3, typical_menu_callback, app);
    submenu_add_item(app->submenu, "Fuel Tank Input Level", 4, typical_menu_callback, app);
    submenu_add_item(app->submenu, "Throttle Relative Position", 5, typical_menu_callback, app);
    submenu_add_item(app->submenu, "Time Engine Running", 6, typical_menu_callback, app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, app_scene_obdii_typical_codes_option));
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
    scene_manager_set_scene_state(app->scene_manager, app_scene_obdii_typical_codes_option, 0);
}

// Scene on Event
bool app_scene_obdii_typical_codes_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    bool consumed = false;
    UNUSED(app);
    UNUSED(event);

    return consumed;
}

// Scene on exit
void app_scene_obdii_typical_codes_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

/*

  THIS SCENE WORKS TO DISPLAY THE RESPECTIVE DATA ON THE FLIPPER
  SHOWING THE TYPICAL DATA

*/

// Draw the unit medition
void draw_value(App* app, const char* text) {
    widget_add_string_element(app->widget, 65, 55, AlignCenter, AlignBottom, FontPrimary, text);
}

// Draws the data
void draw_scene(App* app, uint8_t selector, uint16_t variable) {
    FuriString* text_label = app->text;

    char* text = "";

    furi_string_reset(text_label);
    widget_reset(app->widget);

    if(selector == 0) { // Engine Speed
        widget_add_icon_element(app->widget, 50, 3, &I_RPM30x23);
        widget_add_string_element(
            app->widget, 65, 40, AlignCenter, AlignBottom, FontPrimary, "ENGINE SPEED");

        text = "RPM";

    } else if(selector == 1) { // Vehicle Speed
        widget_add_icon_element(app->widget, 37, 5, &I_SPD64x20);
        widget_add_string_element(
            app->widget, 65, 40, AlignCenter, AlignBottom, FontPrimary, "VEHICLE SPEED");

        text = "km/h";

    } else if(selector == 2) { // CALCULATED ENGINE LOAD
        widget_add_icon_element(app->widget, 50, 0, &I_LOAD28x20);
        widget_add_string_multiline_element(
            app->widget, 65, 43, AlignCenter, AlignBottom, FontPrimary, "CALCULATED\nENGINE LOAD");

        text = "%";

    } else if(selector == 3) { // Thortle Position
        widget_add_icon_element(app->widget, 46, 0, &I_THR36x24);
        widget_add_string_element(
            app->widget, 65, 40, AlignCenter, AlignBottom, FontPrimary, "THROTTLE POSITION");

        text = "%";

    } else if(selector == 4) { // Fuel Tank Input Level
        widget_add_icon_element(app->widget, 50, 0, &I_FTANK28x24);
        widget_add_string_multiline_element(
            app->widget, 65, 45, AlignCenter, AlignBottom, FontPrimary, "FUEL TANK\nINPUT LEVEL");

        text = "%";

    } else if(selector == 5) { // Thortle Relative Position
        widget_add_icon_element(app->widget, 46, 0, &I_THR36x24);
        widget_add_string_multiline_element(
            app->widget,
            65,
            45,
            AlignCenter,
            AlignBottom,
            FontPrimary,
            "THROTTLE\nRELATIVE POSITION");

        text = "%";
    } else if(selector == 6) {
        widget_add_icon_element(app->widget, 45, 0, &I_ENGTIME39x21);
        widget_add_string_multiline_element(
            app->widget, 65, 43, AlignCenter, AlignBottom, FontPrimary, "TIME SINCE\nENGINE START");

        text = "seg";
    }

    furi_string_cat_printf(text_label, "%u %s", variable, text);

    draw_value(app, furi_string_get_cstr(text_label));
}

// Draw the waiting data
void draw_waiting_data(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Waiting Data");
}

// Scene on enter
void app_scene_draw_obdii_on_enter(void* context) {
    App* app = context;

    app->thread = furi_thread_alloc_ex("GetDataPID", 1024, obdii_thread_on_work, app);
    furi_thread_start(app->thread);

    widget_reset(app->widget);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

// Scene on Event
bool app_scene_draw_obdii_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    bool consumed = false;
    UNUSED(app);
    UNUSED(event);

    return consumed;
}

// Scene on exit
void app_scene_draw_obdii_on_exit(void* context) {
    App* app = context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);

    widget_reset(app->widget);
}

/*
    Thread To Show DATA
*/

static int32_t obdii_thread_on_work(void* context) {
    App* app = context;
    OBDII scanner = app->obdii;

    scanner.bitrate = app->mcp_can->bitRate;

    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t request = 0;

    bool state;
    bool message;

    uint32_t option =
        scene_manager_get_scene_state(app->scene_manager, app_scene_obdii_typical_codes_option);

    switch(option) {
    case 0:
        request = ENGINE_SPEED;
        break;

    case 1:
        request = VEHICLE_SPEED;
        break;

    case 2:
        request = CALCULATED_ENGINE_LOAD;
        break;

    case 3:
        request = THROTTLE_POSITION;
        break;

    case 4:
        request = FUEL_INPUT_POSITION;
        break;

    case 5:
        request = THROTTLE_RELATIVE_POSITION;
        break;

    case 6:
        request = RUN_TIME_SINCE_ENGINE_START;
        break;

    default:
        break;
    }

    state = pid_init(&scanner);

    uint32_t time_delay = furi_get_tick();

    if(!state)
        draw_device_no_connected(app);
    else
        draw_waiting_data(app);

    while(state && furi_hal_gpio_read(&gpio_button_back)) {
        if((furi_get_tick() - time_delay) > 10) {
            message = pid_show_data(&scanner, request, data, 8);

            if(message) {
                switch(option) {
                case 0: // Engine Speed RPM
                    draw_scene(app, option, calculate_engine_speed(data[3], data[4]));
                    break;

                case 1: // Vehcile Speed
                    draw_scene(app, option, data[3]);
                    break;

                case 2: // Calculated Engine Load
                    draw_scene(app, option, calculate_engine_load(data[3]));
                    break;

                case 3: // Throttle Position
                    draw_scene(app, option, calculate_engine_load(data[3]));
                    break;

                case 4: // Fuel Tank Input Level
                    draw_scene(app, option, calculate_engine_load(data[3]));
                    break;

                case 5: // Thortle Relative Position
                    draw_scene(app, option, calculate_engine_load(data[3]));
                    break;

                case 6: // Time Engine Running
                    draw_scene(app, option, sum_value(data[3], data[4]));
                    break;

                case 7:

                default:
                    draw_in_development(app);
                    break;
                }
            }

            time_delay = furi_get_tick();
        }
    }
    pid_deinit(&scanner);
    return 0;
}
