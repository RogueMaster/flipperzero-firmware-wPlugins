#include "bunnyconnect_i.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>

static bool bunnyconnect_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BunnyConnectApp* app = context;

    switch(event) {
    case BunnyConnectCustomEventConnect:
        FURI_LOG_D(TAG, "Connect event received");
        app->state = BunnyConnectStateConnected;
        return true;
    case BunnyConnectCustomEventDisconnect:
        FURI_LOG_D(TAG, "Disconnect event received");
        app->state = BunnyConnectStateDisconnected;
        return true;
    default:
        FURI_LOG_D(TAG, "Unknown event: %lu", event);
        return false;
    }
}

static bool bunnyconnect_app_back_event_callback(void* context) {
    furi_assert(context);
    BunnyConnectApp* app = context;

    // Use tracked current view instead of querying view dispatcher
    switch(app->current_view) {
    case BunnyConnectViewMainMenu:
        // Allow exit from main menu
        return false;

    case BunnyConnectViewTerminal:
    case BunnyConnectViewConfig:
    case BunnyConnectViewCustomKeyboard:
    case BunnyConnectViewPopup:
        // Return to main menu from any submenu/view
        app->current_view = BunnyConnectViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewMainMenu);
        return true; // Consume the back event

    default:
        // For any unknown view, return to main menu
        app->current_view = BunnyConnectViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewMainMenu);
        return true;
    }
}

BunnyConnectApp* bunnyconnect_app_alloc(void) {
    BunnyConnectApp* app = malloc(sizeof(BunnyConnectApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app");
        return NULL;
    }

    memset(app, 0, sizeof(BunnyConnectApp));

    // Initialize config with defaults
    strncpy(app->config.device_name, "BunnyConnect", sizeof(app->config.device_name) - 1);
    app->config.baud_rate = 115200;
    app->config.auto_connect = false;
    app->config.flow_control = false;
    app->config.data_bits = 8;
    app->config.stop_bits = 1;
    app->config.parity = 0;
    app->state = BunnyConnectStateDisconnected;
    app->current_view = BunnyConnectViewMainMenu;
    app->is_running = true;

    // Open records
    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) {
        FURI_LOG_E(TAG, "Failed to open GUI record");
        bunnyconnect_app_free(app);
        return NULL;
    }

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    if(!app->notifications) {
        FURI_LOG_E(TAG, "Failed to open notification record");
        bunnyconnect_app_free(app);
        return NULL;
    }

    // Allocate view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        FURI_LOG_E(TAG, "Failed to allocate view dispatcher");
        bunnyconnect_app_free(app);
        return NULL;
    }

    // Allocate mutex for thread safety
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!app->mutex) {
        FURI_LOG_E(TAG, "Failed to allocate mutex");
        bunnyconnect_app_free(app);
        return NULL;
    }

    // Allocate event queue
    app->event_queue = furi_message_queue_alloc(32, sizeof(uint32_t));
    if(!app->event_queue) {
        FURI_LOG_E(TAG, "Failed to allocate event queue");
        bunnyconnect_app_free(app);
        return NULL;
    }

    // Allocate text string
    app->text_string = furi_string_alloc();
    if(!app->text_string) {
        FURI_LOG_E(TAG, "Failed to allocate text string");
        bunnyconnect_app_free(app);
        return NULL;
    }

    // Allocate terminal buffer
    app->terminal_buffer = malloc(TERMINAL_BUFFER_SIZE);
    if(!app->terminal_buffer) {
        FURI_LOG_E(TAG, "Failed to allocate terminal buffer");
        bunnyconnect_app_free(app);
        return NULL;
    }
    memset(app->terminal_buffer, 0, TERMINAL_BUFFER_SIZE);
    app->terminal_buffer_position = 0;

    // Set up view dispatcher callbacks
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bunnyconnect_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bunnyconnect_app_back_event_callback);

    // Attach to GUI
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    FURI_LOG_I(TAG, "App allocated successfully");
    return app;
}

void bunnyconnect_app_free(BunnyConnectApp* app) {
    if(!app) return;

    FURI_LOG_I(TAG, "Freeing app resources");

    // Stop worker thread if running
    if(app->worker_thread) {
        app->is_running = false;
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
    }

    // Close serial handle
    if(app->serial_handle) {
        furi_hal_serial_control_release(app->serial_handle);
    }

    // Free GUI elements - remove views first, then free them
    if(app->view_dispatcher) {
        if(app->main_menu) {
            view_dispatcher_remove_view(app->view_dispatcher, BunnyConnectViewMainMenu);
            submenu_free(app->main_menu);
        }
        if(app->text_box) {
            view_dispatcher_remove_view(app->view_dispatcher, BunnyConnectViewTerminal);
            text_box_free(app->text_box);
        }
        if(app->config_menu) {
            view_dispatcher_remove_view(app->view_dispatcher, BunnyConnectViewConfig);
            submenu_free(app->config_menu);
        }
        if(app->custom_keyboard) {
            view_dispatcher_remove_view(app->view_dispatcher, BunnyConnectViewCustomKeyboard);
            bunnyconnect_keyboard_free(app->custom_keyboard);
        }
        if(app->popup) {
            view_dispatcher_remove_view(app->view_dispatcher, BunnyConnectViewPopup);
            popup_free(app->popup);
        }
        view_dispatcher_free(app->view_dispatcher);
    }

    // Free terminal buffer
    if(app->terminal_buffer) {
        free(app->terminal_buffer);
    }

    // Free mutex
    if(app->mutex) {
        furi_mutex_free(app->mutex);
    }

    // Free message queue
    if(app->event_queue) {
        furi_message_queue_free(app->event_queue);
    }

    // Free strings
    if(app->text_string) {
        furi_string_free(app->text_string);
    }

    // Close records
    if(app->gui) {
        furi_record_close(RECORD_GUI);
    }
    if(app->notifications) {
        furi_record_close(RECORD_NOTIFICATION);
    }

    free(app);
}

void bunnyconnect_show_error_popup(BunnyConnectApp* app, const char* message) {
    if(!app || !message || !app->popup) return;

    FURI_LOG_E(TAG, "Error: %s", message);

    popup_set_context(app->popup, app);
    popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, message, 64, 32, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 3000);

    if(app->view_dispatcher) {
        view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewPopup);
    }
}
