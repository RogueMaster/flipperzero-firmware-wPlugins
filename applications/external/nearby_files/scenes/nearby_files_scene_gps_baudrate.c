#include "nearby_files_scene.h"
#include "../nearby_files.h"

typedef enum {
    NearbyFilesGpsBaudrateMenuItem4800,
    NearbyFilesGpsBaudrateMenuItem9600,
    NearbyFilesGpsBaudrateMenuItem19200,
    NearbyFilesGpsBaudrateMenuItem38400,
    NearbyFilesGpsBaudrateMenuItem57600,
    NearbyFilesGpsBaudrateMenuItem115200,
} NearbyFilesGpsBaudrateMenuItem;

static void nearby_files_scene_gps_baudrate_show_result(DialogsApp* dialogs, uint32_t baudrate) {
    DialogMessage* message = dialog_message_alloc();
    FuriString* text = furi_string_alloc_printf("Applied %lu baud", (unsigned long)baudrate);

    dialog_message_set_header(message, "GPS Baudrate", 64, 4, AlignCenter, AlignTop);
    dialog_message_set_text(message, furi_string_get_cstr(text), 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "OK", NULL, NULL);
    dialog_message_show(dialogs, message);

    furi_string_free(text);
    dialog_message_free(message);
}

static void nearby_files_scene_gps_baudrate_submenu_callback(void* context, uint32_t index) {
    NearbyFilesApp* app = context;

    switch(index) {
    case NearbyFilesGpsBaudrateMenuItem4800:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NearbyFilesCustomEventSetBaudrate4800);
        break;
    case NearbyFilesGpsBaudrateMenuItem9600:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NearbyFilesCustomEventSetBaudrate9600);
        break;
    case NearbyFilesGpsBaudrateMenuItem19200:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NearbyFilesCustomEventSetBaudrate19200);
        break;
    case NearbyFilesGpsBaudrateMenuItem38400:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NearbyFilesCustomEventSetBaudrate38400);
        break;
    case NearbyFilesGpsBaudrateMenuItem57600:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NearbyFilesCustomEventSetBaudrate57600);
        break;
    case NearbyFilesGpsBaudrateMenuItem115200:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NearbyFilesCustomEventSetBaudrate115200);
        break;
    }
}

void nearby_files_scene_gps_baudrate_on_enter(void* context) {
    NearbyFilesApp* app = context;
    const uint32_t current_baudrate = gps_reader_get_baudrate(app->gps_reader);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select GPS Baudrate");

    submenu_add_item(
        app->submenu,
        "4800",
        NearbyFilesGpsBaudrateMenuItem4800,
        nearby_files_scene_gps_baudrate_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "9600",
        NearbyFilesGpsBaudrateMenuItem9600,
        nearby_files_scene_gps_baudrate_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "19200",
        NearbyFilesGpsBaudrateMenuItem19200,
        nearby_files_scene_gps_baudrate_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "38400",
        NearbyFilesGpsBaudrateMenuItem38400,
        nearby_files_scene_gps_baudrate_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "57600",
        NearbyFilesGpsBaudrateMenuItem57600,
        nearby_files_scene_gps_baudrate_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "115200",
        NearbyFilesGpsBaudrateMenuItem115200,
        nearby_files_scene_gps_baudrate_submenu_callback,
        app);

    switch(current_baudrate) {
    case 4800:
        submenu_set_selected_item(app->submenu, NearbyFilesGpsBaudrateMenuItem4800);
        break;
    case 9600:
        submenu_set_selected_item(app->submenu, NearbyFilesGpsBaudrateMenuItem9600);
        break;
    case 19200:
        submenu_set_selected_item(app->submenu, NearbyFilesGpsBaudrateMenuItem19200);
        break;
    case 38400:
        submenu_set_selected_item(app->submenu, NearbyFilesGpsBaudrateMenuItem38400);
        break;
    case 57600:
        submenu_set_selected_item(app->submenu, NearbyFilesGpsBaudrateMenuItem57600);
        break;
    case 115200:
        submenu_set_selected_item(app->submenu, NearbyFilesGpsBaudrateMenuItem115200);
        break;
    default:
        break;
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, NearbyFilesViewSubmenu);
}

bool nearby_files_scene_gps_baudrate_on_event(void* context, SceneManagerEvent event) {
    NearbyFilesApp* app = context;
    bool consumed = false;
    uint32_t new_baudrate = 0;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case NearbyFilesCustomEventSetBaudrate4800:
            new_baudrate = 4800;
            break;
        case NearbyFilesCustomEventSetBaudrate9600:
            new_baudrate = 9600;
            break;
        case NearbyFilesCustomEventSetBaudrate19200:
            new_baudrate = 19200;
            break;
        case NearbyFilesCustomEventSetBaudrate38400:
            new_baudrate = 38400;
            break;
        case NearbyFilesCustomEventSetBaudrate57600:
            new_baudrate = 57600;
            break;
        case NearbyFilesCustomEventSetBaudrate115200:
            new_baudrate = 115200;
            break;
        }

        if(new_baudrate != 0) {
            if(gps_reader_set_baudrate(app->gps_reader, new_baudrate)) {
                nearby_files_save_config_baudrate(new_baudrate);
                nearby_files_scene_gps_baudrate_show_result(app->dialogs, new_baudrate);
            }
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nearby_files_scene_gps_baudrate_on_exit(void* context) {
    NearbyFilesApp* app = context;
    submenu_reset(app->submenu);
}
