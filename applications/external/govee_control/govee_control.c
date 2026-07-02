#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <lib/toolbox/path.h>

#define TAG "GoveeControl"

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Popup* popup;
    VariableItemList* variable_item_list;
    NotificationApp* notifications;
    FuriMessageQueue* event_queue;
    FuriThread* worker_thread;
    bool running;
} GoveeApp;

typedef enum {
    GoveeViewMenu,
    GoveeViewScanning,
    GoveeViewDeviceControl,
} GoveeView;

// Removed unused callbacks - will implement when needed

static int32_t govee_worker(void* context) {
    GoveeApp* app = context;
    FURI_LOG_I(TAG, "Worker thread started");

    while(app->running) {
        // TODO: Implement BLE scanning and device management
        furi_delay_ms(100);
    }

    FURI_LOG_I(TAG, "Worker thread stopped");
    return 0;
}

static uint32_t govee_exit_callback(void* context) {
    UNUSED(context);
    return GoveeViewMenu;
}

static void govee_popup_callback(void* context) {
    GoveeApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, GoveeViewMenu);
}

static void govee_submenu_callback(void* context, uint32_t index) {
    GoveeApp* app = context;

    switch(index) {
    case 0:
        // Start scanning
        FURI_LOG_I(TAG, "Starting BLE scan");
        popup_set_text(
            app->popup,
            "Scanning for devices...\nPress Back to cancel",
            64,
            32,
            AlignCenter,
            AlignCenter);
        view_dispatcher_switch_to_view(app->view_dispatcher, GoveeViewScanning);
        break;
    case 1:
        // Manual entry
        popup_set_text(
            app->popup,
            "Manual mode\nNot implemented yet\nPress Back",
            64,
            32,
            AlignCenter,
            AlignCenter);
        view_dispatcher_switch_to_view(app->view_dispatcher, GoveeViewScanning);
        break;
    case 2:
        // Device list
        view_dispatcher_switch_to_view(app->view_dispatcher, GoveeViewDeviceControl);
        break;
    default:
        break;
    }
}

static GoveeApp* govee_app_alloc() {
    GoveeApp* app = malloc(sizeof(GoveeApp));
    if(!app) {
        return NULL;
    }

    // Initialize GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_NOTIFICATION);
        free(app);
        return NULL;
    }
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Submenu
    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Scan for Devices", 0, govee_submenu_callback, app);
    submenu_add_item(app->submenu, "Manual Entry", 1, govee_submenu_callback, app);
    submenu_add_item(app->submenu, "Saved Devices", 2, govee_submenu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), govee_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, GoveeViewMenu, submenu_get_view(app->submenu));

    // Popup for scanning
    app->popup = popup_alloc();
    popup_set_header(app->popup, "Scanning", 64, 10, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Looking for H6006...\nPress Back to cancel", 64, 32, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 10000); // 10 second timeout
    popup_set_callback(app->popup, govee_popup_callback);
    popup_set_context(app->popup, app);
    view_set_previous_callback(popup_get_view(app->popup), govee_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, GoveeViewScanning, popup_get_view(app->popup));

    // Variable item list for device control
    app->variable_item_list = variable_item_list_alloc();
    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list), govee_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        GoveeViewDeviceControl,
        variable_item_list_get_view(app->variable_item_list));

    // Event queue
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Worker thread
    app->running = true;
    app->worker_thread = furi_thread_alloc();
    furi_thread_set_name(app->worker_thread, "GoveeWorker");
    furi_thread_set_stack_size(app->worker_thread, 2048);
    furi_thread_set_context(app->worker_thread, app);
    furi_thread_set_callback(app->worker_thread, govee_worker);

    return app;
}

static void govee_app_free(GoveeApp* app) {
    furi_assert(app);

    // Stop worker thread
    app->running = false;
    furi_thread_join(app->worker_thread);
    furi_thread_free(app->worker_thread);

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, GoveeViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, GoveeViewScanning);
    view_dispatcher_remove_view(app->view_dispatcher, GoveeViewDeviceControl);

    submenu_free(app->submenu);
    popup_free(app->popup);
    variable_item_list_free(app->variable_item_list);

    view_dispatcher_free(app->view_dispatcher);

    furi_message_queue_free(app->event_queue);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

int32_t govee_control_app(void* p) {
    UNUSED(p);
    GoveeApp* app = govee_app_alloc();

    // Start worker thread
    furi_thread_start(app->worker_thread);

    // Set initial view
    view_dispatcher_switch_to_view(app->view_dispatcher, GoveeViewMenu);

    // Main loop
    view_dispatcher_run(app->view_dispatcher);

    govee_app_free(app);
    return 0;
}
