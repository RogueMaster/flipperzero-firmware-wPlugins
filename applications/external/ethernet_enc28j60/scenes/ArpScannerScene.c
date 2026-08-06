#include "../app_user.h"

typedef enum {
    SET_IP = 0,
    SET_RANGE,
    START_SCANNER,
    VIEW_RESULTS,
} ARP_MENU_OPTIONS;

typedef enum {
    ArpEventScanFinished = 1,
} ArpCustomEvent;

/**
 * This file contains the functions to display and work with the ARP SCANNER
 * It shows the IP list and saves it in a array
 */

// ip_start and range_ip now live in app->scan_params (F0.1)

/**
 * Menu To select the options and range of the Scanner
 */

uint32_t next_state;

// Callback for to enter when the variable item is pressed
void variable_list_enter_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    switch(index) {
    case SET_IP:
        next_state = ARP_STATE_SET_IP;
        break;

    case SET_RANGE:
        return;

    case START_SCANNER:
        next_state = ARP_STATE_START_SCAN;

        if(scene_manager_get_scene_state(app->scene_manager, app_scene_arp_scanner_menu_option) ==
           2) {
            next_state = ARP_STATE_SPOOF;
        }
        break;

    case VIEW_RESULTS:
        next_state = ARP_STATE_SHOW_LIST;
        break;

    default:
        return;
    }

    scene_manager_set_scene_state(app->scene_manager, app_scene_arp_scanner_option, next_state);

    scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_option);
}

// Callback for the variable item
void change_value_callback(VariableItem* item) {
    App* app = (App*)variable_item_get_context(item);
    uint32_t value = variable_item_get_current_value_index(item);

    if(value == 0) value = 255; // If the value is minor of 1
    if(value > 255) value = 1; // If the value is mayor of 255

    app->scan_params.range_ip = value;

    // Set the value in the Element
    variable_item_set_current_value_index(item, app->scan_params.range_ip);

    // Set the text
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "%u", app->scan_params.range_ip);

    // Set the text in the element
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));
}

void range_input_callback(void* context, int32_t value) {
    App* app = (App*)context;

    if(value < 1) value = 1;
    if(value > 255) value = 255;

    app->scan_params.range_ip = (uint8_t)value;

    scene_manager_previous_scene(app->scene_manager);
}

void set_range_value(App* app) {
    number_input_set_header_text(app->number_input, "Set Range");

    number_input_set_result_callback(
        app->number_input, range_input_callback, app, app->scan_params.range_ip, 1, 255);

    view_dispatcher_switch_to_view(app->view_dispatcher, NumberInputView);
}

void arp_menu_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    uint32_t next_state = 0;

    switch(index) {
    case SET_IP:
        next_state = ARP_STATE_SET_IP;
        break;

    case SET_RANGE:
        next_state = ARP_STATE_SET_RANGE;
        break;

    case START_SCANNER:
        next_state = ARP_STATE_START_SCAN;

        if(scene_manager_get_scene_state(app->scene_manager, app_scene_arp_scanner_menu_option) ==
           2) {
            next_state = ARP_STATE_SPOOF;
        }
        break;

    case VIEW_RESULTS:
        next_state = ARP_STATE_SHOW_LIST;
        break;

    default:
        return;
    }

    scene_manager_set_scene_state(app->scene_manager, app_scene_arp_scanner_option, next_state);

    scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_option);
}

// function for the arp menu scene on enter
void app_scene_arp_scanner_menu_on_enter(void* context) {
    App* app = (App*)context;

    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "SCAN HOSTS");

    // SET IP
    furi_string_reset(app->text);
    if(*(uint32_t*)app->scan_params.ip_start == 0)
        memcpy(app->scan_params.ip_start, app->ip_gateway, 4);
    furi_string_cat_printf(
        app->text,
        "Start IP [%u.%u.%u.%u]",
        app->scan_params.ip_start[0],
        app->scan_params.ip_start[1],
        app->scan_params.ip_start[2],
        app->scan_params.ip_start[3]);

    submenu_add_item(
        app->submenu, furi_string_get_cstr(app->text), SET_IP, arp_menu_callback, app);

    // SET RANGE
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "Range [%u]", app->scan_params.range_ip);

    submenu_add_item(
        app->submenu, furi_string_get_cstr(app->text), SET_RANGE, arp_menu_callback, app);

    // START
    submenu_add_item(app->submenu, "Start Scanning", START_SCANNER, arp_menu_callback, app);

    // VIEW RESULTS
    submenu_add_item(app->submenu, "View Scanned Hosts", VIEW_RESULTS, arp_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// function for the arp menu scene on event
bool app_scene_arp_scanner_menu_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// function for the arp menu scene on exit
void app_scene_arp_scanner_menu_on_exit(void* context) {
    App* app = (App*)context;
    UNUSED(app);
}

/**
 * Scene to show the list of the IP in the network
 */

// Function for the thread
int32_t arp_scanner_thread(void* context);

// Function to view the IP results
void build_ip_submenu(App* app, uint32_t selection);

// Function to set the thread and the view
void draw_the_arp_list(App* app) {
    enc28j60_t* ethernet = app->ethernet;

    bool start = app->enc28j60_connected;

    if(!start) {
        start = enc28j60_start(ethernet) != 0xff;
        app->enc28j60_connected = start;
    }

    if(!start) {
        draw_device_no_connected(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        return;
    }

    if(!is_link_up(ethernet)) {
        draw_network_not_connected(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        return;
    }

    app->thread_alternative =
        furi_thread_alloc_ex("ARP SCANNER", 10 * 1024, arp_scanner_thread, app);

    furi_thread_start(app->thread_alternative);

    view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);
}

// Function to draw to finished the thread
void finished_arp_thread(App* app) {
    furi_thread_join(app->thread_alternative);
    furi_thread_free(app->thread_alternative);
}

//  Callback for the Input
void settings_start_ip_address(void* context) {
    App* app = (App*)context;
    scene_manager_previous_scene(app->scene_manager);
}

// Function to set the IP address
void set_ip_address(App* app) {
    ip_assigner_reset(app->ip_assigner);
    ip_assigner_set_header(app->ip_assigner, "Set Ip Address");
    ip_assigner_callback(app->ip_assigner, settings_start_ip_address, app);
    ip_assigner_set_ip_array(app->ip_assigner, app->scan_params.ip_start);

    view_dispatcher_switch_to_view(
        app->view_dispatcher, IpAssignerView); // Switch to the input byte view
}

// Function to show the list of IP
void show_current_arp_list(App* app) {
    build_ip_submenu(app, ARP_STATE_START_SCAN);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// function on enter for the arp scanner scene
void app_scene_arp_scanner_on_enter(void* context) {
    App* app = (App*)context;

    switch(scene_manager_get_scene_state(app->scene_manager, app_scene_arp_scanner_option)) {
    case ARP_STATE_START_SCAN:
    case ARP_STATE_SPOOF:
        draw_the_arp_list(app);
        break;

    case ARP_STATE_SET_IP:
        set_ip_address(app);
        break;

    case ARP_STATE_SET_RANGE:
        set_range_value(app);
        break;

    case ARP_STATE_SHOW_LIST:
        show_current_arp_list(app);
        break;
    }
}

// function on event for the arp scanner scene
bool app_scene_arp_scanner_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);

        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ArpEventScanFinished) {
            finished_arp_thread(app);

            scene_manager_set_scene_state(
                app->scene_manager, app_scene_arp_scanner_option, ARP_STATE_SHOW_LIST);

            show_current_arp_list(app);

            return true;
        }
    }

    return false;
}

// function on exit for the arp scanner scene
void app_scene_arp_scanner_on_exit(void* context) {
    App* app = (App*)context;

    // If scene is in state 0 it finished the
    switch(scene_manager_get_scene_state(app->scene_manager, app_scene_arp_scanner_option)) {
    case ARP_STATE_START_SCAN:
    case ARP_STATE_SPOOF:

    default:
        break;
    }
}

/**
 * Callback for the options in the ip list
 */

// For only arp scanner
void ip_list_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    // Set the scene to get the index of the ip in the IP array list
    scene_manager_set_scene_state(app->scene_manager, app_scene_arp_ip_show_details_option, index);

    // Go to show details scene
    scene_manager_next_scene(app->scene_manager, app_scene_arp_ip_show_details_option);
}

// For the arp spoofing
void ip_list_spoofing_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    // copy the IP
    memcpy(app->ip_helper, app->ip_list[index].ip, 4);
    memcpy(app->mac_helper, app->ip_list[index].mac, 6);

    // Return to the last view that is the spoofing
    scene_manager_previous_scene(app->scene_manager);
}

/**
 * This scene is to show the MAC address from the IP
 */

// Function to get the IP list
void app_scene_arp_ip_show_details_on_enter(void* context) {
    App* app = (App*)context;

    // Change state of the previous scene to only show the Submenu view
    scene_manager_set_scene_state(
        app->scene_manager, app_scene_arp_scanner_option, ARP_STATE_SHOW_LIST);

    // Get the index in the arp ip list
    uint32_t index =
        scene_manager_get_scene_state(app->scene_manager, app_scene_arp_ip_show_details_option);

    // Reset furi string text
    furi_string_reset(app->text);

    // Point to the current ip indexed
    uint8_t* ip_showed = app->ip_list[index].ip;

    // Point to the current mac
    uint8_t* mac_showed = app->ip_list[index].mac;

    // Get if the IP is duplicated
    bool is_duplicated = is_duplicated_ip(ip_showed, app->ip_list, app->ip_counter);

    // Set the text to show IP address with it MAC address
    furi_string_printf(
        app->text, "IP: %u.%u.%u.%u ", ip_showed[0], ip_showed[1], ip_showed[2], ip_showed[3]);

    // add duplicated if the ip is
    if(is_duplicated) {
        furi_string_cat_printf(app->text, "\n(Duplicated)");
    }

    // Set MAC in the text
    furi_string_cat_printf(
        app->text,
        "\nMAC: %02x:%02x:%02x:%02x:%02x:%02x",
        mac_showed[0],
        mac_showed[1],
        mac_showed[2],
        mac_showed[3],
        mac_showed[4],
        mac_showed[5]);

    // reset Widget
    widget_reset(app->widget);

    // Set Header manually
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "IP ADDRESS DETAILS");

    // Set Text for the detailes of the IP
    widget_add_string_multiline_element(
        app->widget,
        10,
        30,
        AlignLeft,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));

    // Switch to widget view
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function to get the ip list
bool app_scene_arp_ip_show_details_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the ip list
void app_scene_arp_ip_show_details_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}

void build_ip_submenu(App* app, uint32_t selection) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "SCANNED HOSTS");

    for(uint8_t i = 0; i < app->ip_counter; i++) {
        furi_string_reset(app->text);
        furi_string_cat_printf(
            app->text,
            "%u.%u.%u.%u",
            app->ip_list[i].ip[0],
            app->ip_list[i].ip[1],
            app->ip_list[i].ip[2],
            app->ip_list[i].ip[3]);

        if(is_duplicated_ip(app->ip_list[i].ip, app->ip_list, app->ip_counter)) {
            furi_string_cat_printf(app->text, " (D)");
        }

        if(selection == ARP_STATE_START_SCAN) {
            submenu_add_item(
                app->submenu, furi_string_get_cstr(app->text), i, ip_list_callback, app);
        } else {
            submenu_add_item(
                app->submenu, furi_string_get_cstr(app->text), i, ip_list_spoofing_callback, app);
        }
    }
}

void ip_list_select_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    // Save IP selected in the helper global variable
    memcpy(app->ip_helper, app->ip_list[index].ip, 4);

    // Go to the previous scene
    scene_manager_previous_scene(app->scene_manager);
}

/**
 * Thread for the ARP Scanner
 */

int32_t arp_scanner_thread(void* context) {
    App* app = (App*)context;

    scanner_session_t scanner;
    scanner_session_init(&scanner, app);

    arp_scan_network(
        &scanner,
        app->ip_list,
        app->scan_params.ip_start,
        &app->ip_counter,
        app->scan_params.range_ip);

    scanner_session_deinit(&scanner);

    view_dispatcher_send_custom_event(app->view_dispatcher, ArpEventScanFinished);

    return 0;
}
