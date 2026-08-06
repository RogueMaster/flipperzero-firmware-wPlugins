#include "app_user.h"
#include "libraries/protocol_tools/arp.h"
#include "libraries/protocol_tools/icmp.h"

// Just to set as initial MAC the user must to modify to have other MAC address
uint8_t MAC_INITIAL[6] = {0xba, 0x3f, 0x91, 0xc2, 0x7e, 0x5d};
uint8_t IP_DEFAULT[4] = {192, 168, 0, 2};

// Function to make paths
void make_paths(App* app) {
    furi_assert(app);

    if(!storage_simply_mkdir(app->storage, PATHAPPEXT)) {
        dialog_message_show_storage_error(app->dialogs, "Cannot create\napp folder");
    }
    if(!storage_simply_mkdir(app->storage, PATHPCAPS)) {
        dialog_message_show_storage_error(app->dialogs, "Cannot create\nlogs folder");
    }
}

static bool app_scene_costum_callback(void* context, uint32_t costum_event) {
    furi_assert(context);
    App* app = (App*)context;
    return scene_manager_handle_custom_event(app->scene_manager, costum_event);
}

static bool app_scene_back_event(void* context) {
    furi_assert(context);
    App* app = (App*)context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void app_tick_event(void* context) {
    furi_assert(context);
    App* app = (App*)context;
    UNUSED(app);
}

// F0.4a — auto-reply handlers migrated from app_worker.c.
// Predicate / handler pairs registered with rx_dispatch in app_alloc.

// F0.7 — both auto-reply predicates gate on is_static_ip. Pre-fix the
// auto-replies fired even before DORA, so the chip claimed ownership of
// its IP_DEFAULT (192.168.0.2) on whatever LAN it was plugged into —
// causing IP conflicts with whatever real host owned that address.
// is_static_ip is set true after DORA succeeds (app_worker.c) and on
// settings_load when the persisted flag was true.

static bool auto_arp_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(len);
    App* app = (App*)ctx;
    if(!app->is_static_ip) return false;
    return is_arp((uint8_t*)frame);
}

static void auto_arp_handler(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(len);
    App* app = (App*)ctx;
    arp_reply_requested(app->ethernet, (uint8_t*)frame, app->ethernet->ip_address);
}

static bool auto_icmp_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(len);
    App* app = (App*)ctx;
    if(!app->is_static_ip) return false;
    if(!is_icmp((uint8_t*)frame)) return false;
    // F0.5g — match ECHO REQUESTS only. Pre-fix the predicate matched
    // any ICMP, including ECHO REPLIES bound for our own ping loop.
    // The handler (ping_reply_to_request) ignored non-requests, but
    // the frame was already dequeued by rx_dispatch's receive_packet,
    // so PingScene's outstanding poll never saw the reply. Net effect:
    // pings looked dropped or intermittent on the same LAN where they
    // were actually arriving fine.
    icmp_header_t icmp = icmp_get_header((uint8_t*)frame);
    return icmp.type == ICMP_TYPE_ECHO_REQUEST;
}

static void auto_icmp_handler(const uint8_t* frame, uint16_t len, void* ctx) {
    App* app = (App*)ctx;
    ping_reply_to_request(app->ethernet, (uint8_t*)frame, len);
}

App* app_alloc() {
    // F0.5d-wave2 — calloc instead of malloc so every field starts
    // zero/NULL. Pre-fix `thread_alternative` could be uninitialized
    // garbage; if a scene checked it before assigning (e.g. GetIPScene
    // on_exit when the chip is disconnected and on_enter never set it),
    // furi_thread_join on a wild pointer would crash.
    App* app = (App*)calloc(1, sizeof(App));

    // F0.1 — initialize cross-scene scan parameters with sensible defaults.
    // Matches the prior file-static initial values:
    //   PortsScannerScene.c:11   target_port = 22
    //   PortsScannerScene.c:12   range_port = 1000
    //   PortsScannerScene.c:40   protocols_index = PORTS_SCANNER_TCP
    //   ArpScannerScene.c:17     range_ip = 30
    // Other arrays default to all zeros (already done by calloc/memset).
    app->scan_params.target_port = 22;
    app->scan_params.range_port = 1000;
    app->scan_params.protocols_index = 0; // PORTS_SCANNER_TCP
    app->scan_params.range_ip = 30;
    memset(app->scan_params.target_ip, 0, sizeof(app->scan_params.target_ip));
    memset(app->scan_params.ip_ping, 0, sizeof(app->scan_params.ip_ping));
    memset(app->scan_params.ip_start, 0, sizeof(app->scan_params.ip_start));

    // Alloc the scene manager and view dispatcher
    app->scene_manager = scene_manager_alloc(&app_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    // Set the navegation on the view dispatcher
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_scene_costum_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_scene_back_event);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, app_tick_event, 100);

    // Alloc the GUI Modules and add the view in the view dispatcher
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, WidgetView, widget_get_view(app->widget));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, SubmenuView, submenu_get_view(app->submenu));

    app->varList = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VarListView, variable_item_list_get_view(app->varList));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TextBoxView, text_box_get_view(app->text_box));

    app->input_byte_value = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, InputByteView, byte_input_get_view(app->input_byte_value));

    app->path = furi_string_alloc();

    app->file_browser = file_browser_alloc(app->path);
    view_dispatcher_add_view(
        app->view_dispatcher, FileBrowserView, file_browser_get_view(app->file_browser));

    app->ip_assigner = ip_assigner_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, IpAssignerView, ip_assigner_get_view(app->ip_assigner));

    app->loading = loading_alloc();
    view_dispatcher_add_view(app->view_dispatcher, LoadingView, loading_get_view(app->loading));

    app->text = furi_string_alloc();

    app->number_input = number_input_alloc();

    view_dispatcher_add_view(
        app->view_dispatcher, NumberInputView, number_input_get_view(app->number_input));

    // Init the storage
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    // Alloc the file storage
    app->file = storage_file_alloc(app->storage);

    // Alloc the memory for the enc28j60 instance
    app->ethernet = enc28j60_alloc(MAC_INITIAL, IP_DEFAULT);

    make_paths(app);

    enc28j60_soft_reset(app->ethernet); // Soft reset the enc28j60
    app->enc28j60_connected = enc28j60_start(app->ethernet) !=
                              0xff; // To know if the enc28j60 is connected

    memcpy(app->ip_helper, IP_DEFAULT, 4);

    app->is_static_ip = false;
    app->is_dora = false;

    // F0.2 — overlay persisted settings on top of the in-memory defaults.
    // Silent fallback to defaults if /ext/apps_data/ethernet/settings.cfg is
    // missing or malformed. MUST run BEFORE rx_dispatch_init below: settings
    // _load calls enc28j60_set_mac() which writes MAADR0..5 byte by byte;
    // if the dispatcher is already polling chip registers, the chip's bank
    // state races and MAADR ends up corrupted, which causes the chip's
    // UCEN filter to reject all unicast frames (including DHCP OFFER,
    // breaking DORA after F0.4a). Diagnosed via FURI_LOG inside DORA on
    // hardware: rx_calls=1860, rx_hits=0 — chip RX FIFO empty for the full
    // 3-second timeout while the laptop confirmed OFFER was sent.
    settings_load(app);

    // F0.4a — start RX dispatcher and register the two auto-reply handlers.
    // These previously lived inline in ethernet_thread (app_worker.c, deleted
    // in F0.4e).
    rx_dispatch_init(app);
    app->auto_arp_handle = rx_register(auto_arp_predicate, auto_arp_handler, app);
    app->auto_icmp_handle = rx_register(auto_icmp_predicate, auto_icmp_handler, app);

    return app;
}

void app_free(App* app) {
    // F0.2 — persist current settings before tearing down storage and the
    // ethernet instance. Errors are silent; a failed save must not block
    // app exit.
    settings_save(app);

    // F0.4a — stop dispatcher first so handlers don't race teardown.
    rx_unregister(app->auto_arp_handle);
    rx_unregister(app->auto_icmp_handle);
    rx_dispatch_deinit(app);

    // F0.4e — app->thread / ethernet_thread / app_worker.c are gone;
    // DORA runs in GetIPScene's alt thread now.

    //  Free all the views from the View Dispatcher
    view_dispatcher_remove_view(app->view_dispatcher, SubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, WidgetView);
    view_dispatcher_remove_view(app->view_dispatcher, TextBoxView);
    view_dispatcher_remove_view(app->view_dispatcher, NumberInputView);
    view_dispatcher_remove_view(app->view_dispatcher, VarListView);
    view_dispatcher_remove_view(app->view_dispatcher, InputByteView);
    view_dispatcher_remove_view(app->view_dispatcher, FileBrowserView);
    view_dispatcher_remove_view(app->view_dispatcher, IpAssignerView);
    view_dispatcher_remove_view(app->view_dispatcher, LoadingView);

    // Free memory of Scene Manager and View Dispatcher
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Free memory of GUI modules
    widget_free(app->widget);
    submenu_free(app->submenu);
    text_box_free(app->text_box);
    byte_input_free(app->input_byte_value);
    file_browser_free(app->file_browser);
    ip_assigner_free(app->ip_assigner);
    loading_free(app->loading);

    // Free memory of ENC
    free_enc28j60(app->ethernet);

    // Free memory of the text
    furi_string_free(app->text);
    furi_string_free(app->path);

    // Free the file storage
    storage_file_free(app->file);

    // Close records
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);

    free(app);
}

int enc28j60_app_main(void* p) {
    UNUSED(p);

    App* app = app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, app_scene_main_category_menu_option);

    view_dispatcher_run(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    app_free(app);

    return 0;
}
