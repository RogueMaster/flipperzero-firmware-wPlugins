#include "../app_user.h"

#define TARGET_TEXT "Target"
#define RANGE_TEXT  "Range"
#define PORT_TEXT   " Port:"

#define TARGET_PORT_TEXT TARGET_TEXT PORT_TEXT
#define RANGE_PORT_TEXT  RANGE_TEXT PORT_TEXT

// target_ip / target_port / range_port now live in app->scan_params (F0.1)
// F0.6 — target_port_bytes / range_port_bytes deleted (dead byte_input
// path; PORTS_SCANNER_SCENE_BYTE_INPUT state was never entered).

typedef enum {
    VIEW_IP_LIST,
    TARGET_IP,
    TARGET_PORT,
    SOURCE_PORT,
    PROTOCOL,
    START,
} PORTS_SCANNER_OPTIONS;

typedef enum {
    PORTS_SCANNER_TCP,
    PORTS_SCANNER_UDP,
} PORTS_SCANNER_PROTOCOLS;

typedef enum {
    PORTS_SCANNER_SCENE_MENU,
    PORTS_SCANNER_SCENE_BYTE_INPUT,
    PORTS_SCANNER_SCENE_IP_INPUT,
    PORTS_SCANNER_SCENE_WIDGET,
    PORTS_SCANNER_SCENE_SHOW_PORTS,
} PORTS_SCANNER_SCENE_STATES;

//const char* protocols[] = {"TCP", "UDP"};
const char* protocols[] = {"TCP"};
// protocols_index now lives in app->scan_params (F0.1)

void number_input_ports_callback(void* context, int32_t value) {
    App* app = context;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_ports_scanner_option);

    if(state == TARGET_PORT) {
        app->scan_params.target_port = value;
    } else if(state == SOURCE_PORT) {
        app->scan_params.range_port = value;
    }

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_MENU);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
    app_scene_ports_scanner_on_enter(app);
}

//  Callback for the Input
void settings_start_ip_address_ports_scanner(void* context) {
    App* app = (App*)context;
    //scene_manager_previous_scene(app->scene_manager);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
    app_scene_ports_scanner_on_enter(app);
}

// Function to set the IP address
void set_ip_address_ports_scanner(App* app) {
    ip_assigner_reset(app->ip_assigner);
    ip_assigner_set_header(app->ip_assigner, "Set Ip Address");
    ip_assigner_callback(app->ip_assigner, settings_start_ip_address_ports_scanner, app);
    ip_assigner_set_ip_array(app->ip_assigner, app->scan_params.target_ip);

    view_dispatcher_switch_to_view(
        app->view_dispatcher, IpAssignerView); // Switch to the input byte view
}

// F0.6 — byte_input_ports_scanner_callback / byte_change_ports_scanner
// deleted. Never registered with byte_input_set_result_callback; the
// active port input path is the number_input setter (lines 200-218).

int32_t ports_scanner_thread(void* context) {
    App* app = context;

    uint8_t value = PORT_CLOSED;

    switch(app->scan_params.protocols_index) {
    case PORTS_SCANNER_TCP:
        tcp_syn_scan(
            app,
            app->scan_params.target_ip,
            app->scan_params.target_port,
            app->scan_params.range_port);
        break;

    case PORTS_SCANNER_UDP:
        udp_port_scan(
            app,
            app->scan_params.target_ip,
            app->scan_params.target_port,
            app->scan_params.range_port);
        break;
    }

    return value;
}

void variable_list_ports_scanner_callback(void* context, uint32_t index) {
    App* app = context;

    switch(index) {
    case VIEW_IP_LIST:

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_arp_scanner_option, ARP_STATE_SHOW_LIST);

        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_option);

        break;

    case START:

        if(app->is_dora) {
            // F0.4c — no longer suspends app->thread; rx_dispatch keeps
            // running and ports_scanner_thread uses scanner_session.
            enc28j60_t* ethernet = app->ethernet;

            bool start = app->enc28j60_connected;

            if(!start) {
                start = enc28j60_start(ethernet) != 0xff;
                app->enc28j60_connected = start;
            }

            if(!start) {
                draw_device_no_connected(app);

                scene_manager_set_scene_state(
                    app->scene_manager,
                    app_scene_ports_scanner_option,
                    PORTS_SCANNER_SCENE_WIDGET);

                view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

                return;
            }

            if(!is_link_up(ethernet)) {
                draw_network_not_connected(app);

                scene_manager_set_scene_state(
                    app->scene_manager,
                    app_scene_ports_scanner_option,
                    PORTS_SCANNER_SCENE_WIDGET);

                view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

                return;
            }
            submenu_reset(app->submenu);
            submenu_set_header(app->submenu, "PORTS OPEN");

            app->thread_alternative =
                furi_thread_alloc_ex("Ports Sacanner", 5 * 1024, ports_scanner_thread, app);

            view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);

            //furi_thread_set_priority(app->thread_alternative, FuriThreadPriorityNormal);

            furi_thread_start(app->thread_alternative);

            //furi_thread_join(app->thread_alternative);

            /*uint32_t value = furi_thread_get_return_code(app->thread_alternative);
            if(value == PORT_OPEN)
                draw_port_open(app);
            else if(value == PORT_CLOSED)
                draw_port_not_open(app);
            */

            //furi_thread_free(app->thread_alternative);

            //furi_thread_resume(app->thread);

            scene_manager_set_scene_state(
                app->scene_manager,
                app_scene_ports_scanner_option,
                PORTS_SCANNER_SCENE_SHOW_PORTS);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

        } else {
            draw_dora_needed(app);

            scene_manager_set_scene_state(
                app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_WIDGET);
            view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        }

        //scene_manager_set_scene_state(
        //    app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_WIDGET);
        //view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

        break;

    case TARGET_IP:

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_IP_INPUT);
        set_ip_address_ports_scanner(app);

        break;

    case TARGET_PORT:
    case SOURCE_PORT: {
        int32_t current = (index == TARGET_PORT) ? app->scan_params.target_port :
                                                   app->scan_params.range_port;

        number_input_set_header_text(
            app->number_input, index == TARGET_PORT ? "Set Target Port" : "Set Range");

        number_input_set_result_callback(
            app->number_input,
            number_input_ports_callback,
            app,
            current,
            1, // min
            65535 // max
        );

        scene_manager_set_scene_state(app->scene_manager, app_scene_ports_scanner_option, index);

        view_dispatcher_switch_to_view(app->view_dispatcher, NumberInputView);
        break;
    }

    case PROTOCOL:
        /*app->scan_params.protocols_index =
            (app->scan_params.protocols_index == PORTS_SCANNER_TCP) ? PORTS_SCANNER_UDP :
                                                                      PORTS_SCANNER_TCP;*/

        app_scene_ports_scanner_on_enter(app);

        submenu_set_selected_item(app->submenu, PROTOCOL);

        break;
    }
}

void variable_item_change_protocol_callback(VariableItem* item) {
    App* app = (App*)variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, protocols[index]);
    app->scan_params.protocols_index = index;
}

void app_scene_ports_scanner_on_enter(void* context) {
    App* app = (App*)context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "SCAN PORTS");

    // VIEW IP LIST
    submenu_add_item(
        app->submenu,
        "View Scanned Hosts",
        VIEW_IP_LIST,
        variable_list_ports_scanner_callback,
        app);

    // TARGET IP
    if(*(uint32_t*)app->scan_params.target_ip == 0)
        memcpy(app->scan_params.target_ip, app->ip_gateway, 4);

    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text,
        "Target IP [%u.%u.%u.%u]",
        app->scan_params.target_ip[0],
        app->scan_params.target_ip[1],
        app->scan_params.target_ip[2],
        app->scan_params.target_ip[3]);

    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        TARGET_IP,
        variable_list_ports_scanner_callback,
        app);

    // TARGET PORT
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "Start Port [%u]", app->scan_params.target_port);

    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        TARGET_PORT,
        variable_list_ports_scanner_callback,
        app);

    // RANGE PORT
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "Range [%u]", app->scan_params.range_port);

    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        SOURCE_PORT,
        variable_list_ports_scanner_callback,
        app);

    // PROTOCOL
    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text, "Protocol [%s]", protocols[app->scan_params.protocols_index]);

    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        PROTOCOL,
        variable_list_ports_scanner_callback,
        app);

    // START
    submenu_add_item(
        app->submenu, "Start Scanning", START, variable_list_ports_scanner_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

bool app_scene_ports_scanner_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        switch(scene_manager_get_scene_state(app->scene_manager, app_scene_ports_scanner_option)) {
        case PORTS_SCANNER_SCENE_SHOW_PORTS:
            furi_thread_join(app->thread_alternative);
            furi_thread_free(app->thread_alternative);
            // F0.4c — no thread_resume; rx_dispatch + scanner_session
            // make app->thread no longer the chip owner.
            /* fall through */
        case PORTS_SCANNER_SCENE_BYTE_INPUT:
        case PORTS_SCANNER_SCENE_IP_INPUT:
        case PORTS_SCANNER_SCENE_WIDGET:

            scene_manager_set_scene_state(
                app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_MENU);
            app_scene_ports_scanner_on_enter(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

            consumed = true;

            break;
        }
    }
    return consumed;
}

void app_scene_ports_scanner_on_exit(void* context) {
    App* app = (App*)context;

    variable_item_list_reset(app->varList);
}
