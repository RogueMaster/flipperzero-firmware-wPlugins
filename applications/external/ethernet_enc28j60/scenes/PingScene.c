#include "../app_user.h"
#include "../modules/ping_module.h"
#include "../modules/arp_module.h"

// F0.4g — predicate context for the ping reply match. Lives only on
// the ping_thread stack; predicate runs in rx_dispatch and writes
// `received=true` as a side effect when it sees an echo reply for our
// ping target.
typedef struct {
    uint8_t* target_ip;
    bool received;
} ping_reply_match_ctx_t;

typedef enum {
    PingEventDeviceNotConnected = 0,
    PingEventNetworkNotConnected,
    PingEventDoraFailed,
    PingEventDoraNeeded,
    PingEventShowLoading,
    PingEventUpdateCounter,
} PingCustomEvent;

static bool ping_reply_match(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(len);
    ping_reply_match_ctx_t* c = (ping_reply_match_ctx_t*)ctx;
    if(ping_packet_replied((uint8_t*)frame, c->target_ip)) {
        c->received = true;
        return true;
    }
    return false;
}

/**
 * Still on development, at this moment this file it can't be seen by the user
 * but if you want to see it you only need to add an option in Main menu to switch at any of these scenes
 * The ping is missing some valid values to send, for examle the ping to google only receive 8 ping replies
 * But if you do ping to a linux device some times it replies 8 times, but others replies 35 times.
 * This problem will be solved but at the moment will be in development
 */

// ip_ping now lives in app->scan_params (F0.1)

// counter for messages sent
uint16_t messages_sent = 0;

// counter for ping responses
uint16_t ping_responses = 0;

// Function for the thread
int32_t ping_thread(void* context);

/**
 * Ping Menu Scene
 * This scene is to set the IP for the ping option
 * It will show a menu with the options to set the IP
 * and then it will switch to the ping scene
 */

void menu_ping_options_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    if(index == 0) {
        // Switch to the ping scene
        scene_manager_next_scene(app->scene_manager, app_scene_ping_option);
    }

    if(index == 1) {
        // Switch to the ping set IP scene
        scene_manager_next_scene(app->scene_manager, app_scene_ping_set_ip_option);
    }

    if(index == 2) {
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_arp_scanner_option, ARP_STATE_SHOW_LIST);

        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_option);
    }
}

// Function for the testing scene on enter
void app_scene_ping_menu_scene_on_enter(void* context) {
    App* app = (App*)context;

    // reset submenu and switch view
    submenu_reset(app->submenu);

    furi_string_reset(app->text);

    if(*(uint32_t*)app->scan_params.ip_ping == 0)
        memcpy(app->scan_params.ip_ping, app->ip_gateway, 4);

    furi_string_cat_printf(app->text, "PING HOST");

    submenu_set_header(app->submenu, furi_string_get_cstr(app->text));

    furi_string_reset(app->text);

    submenu_add_item(app->submenu, "View Scanned Hosts", 2, menu_ping_options_callback, app);

    furi_string_cat_printf(
        app->text,
        "Target IP [%u:%u:%u:%u]",
        app->scan_params.ip_ping[0],
        app->scan_params.ip_ping[1],
        app->scan_params.ip_ping[2],
        app->scan_params.ip_ping[3]);

    submenu_add_item(
        app->submenu, furi_string_get_cstr(app->text), 1, menu_ping_options_callback, app);

    submenu_add_item(app->submenu, "Start Ping", 0, menu_ping_options_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Function for the testing scene on event
bool app_scene_ping_menu_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the testing scene on exit
void app_scene_ping_menu_scene_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}

/**
 * Ping Set IP Scene
 * This scene is to set the IP for the ping option
 * It will show a byte input view here you will set the IP
 */

void draw_ping_packet_count(App* app) {
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Ping to");

    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text,
        "%u:%u:%u:%u",
        app->scan_params.ip_ping[0],
        app->scan_params.ip_ping[1],
        app->scan_params.ip_ping[2],
        app->scan_params.ip_ping[3]);

    widget_add_string_element(
        app->widget,
        64,
        30,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));

    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "Responses %u of %u", ping_responses, messages_sent);

    widget_add_string_element(
        app->widget,
        64,
        50,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

// Callback for the input byte callback in the ping scene
void input_bytes_for_ip_to_ping_callback(void* context) {
    App* app = (App*)context;
    scene_manager_previous_scene(app->scene_manager);
}

// Function for the testing scene on enter
void app_scene_ping_set_ip_scene_on_enter(void* context) {
    App* app = (App*)context;

    ip_assigner_reset(app->ip_assigner);

    ip_assigner_set_header(app->ip_assigner, "IP TO PING");

    ip_assigner_set_ip_array(app->ip_assigner, app->scan_params.ip_ping);

    ip_assigner_callback(app->ip_assigner, input_bytes_for_ip_to_ping_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, IpAssignerView);
}

// Function for ping scene on event
bool app_scene_ping_set_ip_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for ping scene on exit
void app_scene_ping_set_ip_scene_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}

/**
 * DO THE PING
 * This scene is to test the ping module and the enc28j60
 * It will ping to a default IP (google) and it will show the result
 * of the ping in the screen
 */

// Function for ping scene on enter
void app_scene_ping_scene_on_enter(void* context) {
    App* app = (App*)context;

    // F0.4c — no thread_suspend; ping_thread uses scanner_session.
    // Allocate and start the thread
    app->thread_alternative = furi_thread_alloc_ex("PING", 10 * 1024, ping_thread, app);
    furi_thread_start(app->thread_alternative);

    // Reset the widget and switch view
    widget_reset(app->widget);
}

// Function for  ping scene on event
bool app_scene_ping_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case 0:
            // Draw the device is not connected
            draw_device_no_connected(app);
            break;
        case 1:
            // Draw the device is connected
            draw_network_not_connected(app);
            break;

        case 2:
            // Draw the device is connected but the process Dora failed
            draw_dora_failed(app);
            break;

        case 3:
            draw_dora_needed(app);
            break;

        case 5:
            // Draw the ping packet count
            draw_ping_packet_count(app);
            break;

        default:
            break;
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
    return consumed;
}

// Function for ping scene on exit
void app_scene_ping_scene_on_exit(void* context) {
    App* app = (App*)context;

    // Join and free the thread
    furi_thread_join(app->thread_alternative);
    furi_thread_free(app->thread_alternative);
    // F0.4c — no thread_resume.
}

/**
 * Thread for the ping scene
 */

int32_t ping_thread(void* context) {
    App* app = (App*)context;

    // Message to send
    char* ping_data = "hello from flipper";

    // data lenght for the ping data
    uint16_t data_len = strlen(ping_data);

    enc28j60_t* ethernet = app->ethernet;
    uint8_t* packet_to_send = ethernet->tx_buffer;
    uint16_t packet_size = 0;

    uint8_t* packet_to_receive = ethernet->rx_buffer;
    uint16_t packet_receive_len = 0;

    bool is_connected = app->enc28j60_connected;

    // Variable to start the process
    bool start_ping = false;

    // Array to get the MAC for the next hop (target if on-subnet, else gateway).
    uint8_t mac_to_send[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    // F0.3a — scanner session (subnet-aware MAC resolve + cache).
    scanner_session_t scanner;
    scanner_session_init(&scanner, app);

    // reset the counters
    messages_sent = 0;
    ping_responses = 0;

    // sequence
    uint16_t sequence = 1;

    // To know if the enc28j60 is connected
    if(!is_connected) {
        is_connected = enc28j60_start(ethernet) != 0xff; // Start the enc28j60
        app->enc28j60_connected = is_connected; // Update the connection status
    }

    // Get time
    uint32_t last_time_ping = furi_get_tick();

    // Change view to disconnected device
    if(!is_connected) {
        view_dispatcher_send_custom_event(app->view_dispatcher, PingEventDeviceNotConnected);
        goto finalize;
    }

    // Get link up to the LAN
    start_ping = is_link_up(ethernet);

    // Change view to network not connected
    if(!start_ping) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 1);
        goto finalize;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, PingEventShowLoading);

    // Do process Dora to get the IP gateway, and set our IP if we didnt have the IP
    if(!app->is_dora) {
        start_ping = false;
    }

    // If the process Dora failed, we will not continue
    if(!start_ping) {
        view_dispatcher_send_custom_event(app->view_dispatcher, PingEventDoraNeeded);
        goto finalize;
    }

    // F0.3a — resolve next-hop MAC for the ping target.
    // scanner_resolve_next_hop handles subnet check + ARP + cache, and
    // updates app->mac_gateway when the resolved hop IS the gateway.
    if(start_ping && is_connected &&
       !scanner_resolve_next_hop(&scanner, app->scan_params.ip_ping, mac_to_send)) {
        start_ping = false;
    }

    // F0.4g — replaces the inline send + receive_packet poll. The poll
    // raced rx_dispatch (which always wins on INT) and lost echo
    // replies. ARP requests during the wait are handled by the
    // already-registered auto_arp handler (app_user.c), so we no
    // longer call arp_reply_requested here.
    UNUSED(packet_to_receive);
    UNUSED(packet_receive_len);
    UNUSED(last_time_ping);

    while(start_ping && is_connected && !scanner_cancel_requested(&scanner)) {
        uint32_t loop_start = furi_get_tick();

        packet_size = create_flipper_ping_packet(
            packet_to_send,
            ethernet->mac_address,
            mac_to_send,
            app->ethernet->ip_address,
            app->scan_params.ip_ping,
            1,
            sequence,
            (uint8_t*)ping_data,
            data_len);

        if(sequence == 0xffff) sequence = 0;
        sequence++;
        messages_sent++;
        view_dispatcher_send_custom_event(app->view_dispatcher, PingEventUpdateCounter);

        ping_reply_match_ctx_t pred_ctx = {
            .target_ip = app->scan_params.ip_ping,
            .received = false,
        };
        scanner_send_trigger_ctx_t trigger_ctx = {
            .eth = ethernet,
            .buf = packet_to_send,
            .len = packet_size,
        };
        uint16_t got = 0;
        if(scanner_wait_for_packet(
               &scanner,
               ping_reply_match,
               &pred_ctx,
               scanner_send_packet_trigger,
               &trigger_ctx,
               &got,
               1000) &&
           pred_ctx.received) {
            ping_responses++;
            view_dispatcher_send_custom_event(app->view_dispatcher, PingEventUpdateCounter);
        }

        // Pace at ~1 pps. If a reply came in early, sleep the remaining
        // window so the user sees the same cadence as a stock ping.
        while((furi_get_tick() - loop_start) < 1000) {
            if(scanner_cancel_requested(&scanner)) {
                break;
            }

            furi_delay_ms(10);
        }
    }

finalize:
    scanner_session_deinit(&scanner);

    return 0;
}
