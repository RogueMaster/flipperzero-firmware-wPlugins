#include "bunnyconnect_i.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_usb_hid.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>

typedef enum {
    BunnyConnectSubmenuIndexConnect,
    BunnyConnectSubmenuIndexTerminal,
    BunnyConnectSubmenuIndexKeyboard,
    BunnyConnectSubmenuIndexConfig,
    BunnyConnectSubmenuInfo,
    BunnyConnectSubmenuIndexExit,
} BunnyConnectSubmenuIndex;

static void bunnyconnect_submenu_callback(void* context, uint32_t index) {
    BunnyConnectApp* app = context;
    if(!app || !app->view_dispatcher) return;

    switch(index) {
    case BunnyConnectSubmenuIndexConnect:
        view_dispatcher_send_custom_event(app->view_dispatcher, BunnyConnectCustomEventConnect);
        break;
    case BunnyConnectSubmenuIndexTerminal:
        if(app->text_box) {
            app->current_view = BunnyConnectViewTerminal;
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewTerminal);
        }
        break;
    case BunnyConnectSubmenuIndexKeyboard:
        if(app->custom_keyboard) {
            app->current_view = BunnyConnectViewCustomKeyboard;
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewCustomKeyboard);
        }
        break;
    case BunnyConnectSubmenuIndexConfig:
        if(app->config_menu) {
            app->current_view = BunnyConnectViewConfig;
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewConfig);
        }
        break;
    case BunnyConnectSubmenuInfo:
        if(app->info_widget) {
            app->current_view = BunnyConnectViewInfo;
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewInfo);
        }
        break;
    case BunnyConnectSubmenuIndexExit:
        view_dispatcher_stop(app->view_dispatcher);
        break;
    }
}

static void bunnyconnect_keyboard_callback(void* context) {
    BunnyConnectApp* app = context;
    if(!app || !app->view_dispatcher) return;

    view_dispatcher_send_custom_event(app->view_dispatcher, BunnyConnectCustomEventKeyboardDone);
}

int32_t bunnyconnect_worker_thread(void* context) {
    BunnyConnectApp* app = context;

    while(app->is_running) {
        if(app->state == BunnyConnectStateConnected && app->usb_cdc_connected) {
            // Check for incoming USB CDC data using correct API
            size_t bytes_received = furi_hal_cdc_receive(
                app->usb_cdc_port, (uint8_t*)app->rx_buffer, RX_BUFFER_SIZE - 1);

            if(bytes_received > 0) {
                // Null terminate received data
                app->rx_buffer[bytes_received] = '\0';

                // Update terminal buffer
                if(furi_mutex_acquire(app->mutex, 100) == FuriStatusOk) {
                    // Use safe string append instead of strncat
                    if(!safe_append_string(
                           app->terminal_buffer, app->rx_buffer, TERMINAL_BUFFER_SIZE)) {
                        // Buffer full, clear some space by shifting content
                        size_t current_len = strlen(app->terminal_buffer);
                        if(current_len > TERMINAL_BUFFER_SIZE / 2) {
                            // Move second half to beginning
                            size_t move_start = current_len / 2;
                            memmove(
                                app->terminal_buffer,
                                app->terminal_buffer + move_start,
                                current_len - move_start + 1);
                            // Try append again
                            safe_append_string(
                                app->terminal_buffer, app->rx_buffer, TERMINAL_BUFFER_SIZE);
                        }
                    }

                    // Trigger UI update
                    uint32_t event = BunnyConnectCustomEventRefreshScreen;
                    furi_message_queue_put(app->event_queue, &event, 0);

                    furi_mutex_release(app->mutex);
                }
            }
        }

        furi_delay_ms(10);
    }

    return 0;
}

static bool bunnyconnect_serial_init(BunnyConnectApp* app) {
    if(!app) return false;

    // Initialize USB power and CDC
    bunnyconnect_power_init();

    // Enable USB power if configured
    if(app->config.usb_power_enabled) {
        if(!bunnyconnect_power_set_usb_enabled(true)) {
            FURI_LOG_E(TAG, "Failed to enable USB power");
            bunnyconnect_power_deinit();
            return false;
        }

        // Wait for USB enumeration and device detection
        furi_delay_ms(2000);

        // Check if USB device is connected
        if(!bunnyconnect_power_is_usb_connected()) {
            FURI_LOG_W(TAG, "USB device not detected, but continuing anyway");
        }
    }

    // Initialize CDC communication
    app->usb_cdc_port = 0; // Use first CDC port
    app->usb_cdc_connected = true;

    FURI_LOG_I(
        TAG,
        "USB CDC connection established (baud: %lu, power: %s)",
        app->config.baud_rate,
        app->config.usb_power_enabled ? "ON" : "OFF");

    return true;
}

static void bunnyconnect_serial_deinit(BunnyConnectApp* app) {
    if(!app) return;

    app->usb_cdc_connected = false;

    // Turn off USB power
    bunnyconnect_power_deinit();

    FURI_LOG_I(TAG, "USB CDC connection and power disabled");
}

bool bunnyconnect_custom_event_callback(void* context, uint32_t event) {
    BunnyConnectApp* app = context;
    if(!app) return false;

    switch(event) {
    case BunnyConnectCustomEventConnect:
        FURI_LOG_I(TAG, "Connect event");
        if(bunnyconnect_serial_init(app)) {
            app->state = BunnyConnectStateConnected;

            // Start worker thread
            app->worker_thread = furi_thread_alloc();
            furi_thread_set_name(app->worker_thread, "BunnyWorker");
            furi_thread_set_stack_size(app->worker_thread, 1024);
            furi_thread_set_context(app->worker_thread, app);
            furi_thread_set_callback(app->worker_thread, bunnyconnect_worker_thread);
            furi_thread_start(app->worker_thread);

            notification_message(app->notifications, &sequence_success);
        } else {
            bunnyconnect_show_error_popup(app, "Failed to connect");
        }

        if(app->text_box) {
            app->current_view = BunnyConnectViewTerminal;
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewTerminal);
        }
        return true;

    case BunnyConnectCustomEventDisconnect:
        FURI_LOG_I(TAG, "Disconnect event");
        app->state = BunnyConnectStateDisconnected;

        // Stop worker thread
        if(app->worker_thread) {
            app->is_running = false;
            furi_thread_join(app->worker_thread);
            furi_thread_free(app->worker_thread);
            app->worker_thread = NULL;
            app->is_running = true;
        }

        bunnyconnect_serial_deinit(app);
        return true;

    case BunnyConnectCustomEventKeyboardDone:
        FURI_LOG_I(TAG, "Keyboard done event");
        if(app->input_buffer[0] != '\0') {
            // Send the text via USB CDC if connected
            if(app->usb_cdc_connected) {
                size_t len = strlen(app->input_buffer);
                furi_hal_cdc_send(app->usb_cdc_port, (uint8_t*)app->input_buffer, len);

                // Add newline
                uint8_t newline = '\n';
                furi_hal_cdc_send(app->usb_cdc_port, &newline, 1);

                // Also send via HID for computer input
                for(size_t i = 0; i < len; i++) {
                    uint16_t key = HID_ASCII_TO_KEY(app->input_buffer[i]);
                    if(key != HID_KEYBOARD_NONE) {
                        furi_hal_hid_kb_press(key);
                        furi_delay_ms(10);
                        furi_hal_hid_kb_release(key);
                        furi_delay_ms(10);
                    }
                }
                // Send enter key via HID
                furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
                furi_delay_ms(10);
                furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            }

            // Clear buffer
            memset(app->input_buffer, 0, INPUT_BUFFER_SIZE);
        }

        if(app->main_menu) {
            app->current_view = BunnyConnectViewMainMenu;
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewMainMenu);
        }
        return true;

    case BunnyConnectCustomEventRefreshScreen:
        if(app->text_box && app->terminal_buffer) {
            if(furi_mutex_acquire(app->mutex, 100) == FuriStatusOk) {
                text_box_set_text(app->text_box, app->terminal_buffer);
                text_box_set_focus(app->text_box, TextBoxFocusEnd);
                furi_mutex_release(app->mutex);
            }
        }
        return true;

    default:
        return false;
    }
}

bool bunnyconnect_navigation_callback(void* context) {
    BunnyConnectApp* app = context;
    if(!app || !app->view_dispatcher) return false;

    // Use tracked current view instead of querying view dispatcher
    switch(app->current_view) {
    case BunnyConnectViewMainMenu:
        // Allow exit from main menu
        return false;

    case BunnyConnectViewTerminal:
    case BunnyConnectViewConfig:
    case BunnyConnectViewCustomKeyboard:
    case BunnyConnectViewPopup:
    case BunnyConnectViewInfo:
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

static bool bunnyconnect_setup_views(BunnyConnectApp* app) {
    if(!app || !app->view_dispatcher) return false;

    // Initialize with default USB settings
    app->config.usb_power_enabled = true; // Enable USB power by default
    app->config.auto_enumerate = true; // Auto-enumerate as CDC device

    // Initialize terminal buffer with welcome message
    const char* welcome_msg = "BunnyConnect Terminal\nReady for connection...\n";
    size_t msg_len = strlen(welcome_msg);
    if(msg_len < TERMINAL_BUFFER_SIZE) {
        memcpy(app->terminal_buffer, welcome_msg, msg_len + 1);
    }

    // Main menu
    app->main_menu = submenu_alloc();
    if(!app->main_menu) {
        FURI_LOG_E(TAG, "Failed to allocate main menu");
        return false;
    }

    submenu_add_item(
        app->main_menu,
        "Connect",
        BunnyConnectSubmenuIndexConnect,
        bunnyconnect_submenu_callback,
        app);
    submenu_add_item(
        app->main_menu,
        "Terminal",
        BunnyConnectSubmenuIndexTerminal,
        bunnyconnect_submenu_callback,
        app);
    submenu_add_item(
        app->main_menu,
        "Keyboard",
        BunnyConnectSubmenuIndexKeyboard,
        bunnyconnect_submenu_callback,
        app);
    submenu_add_item(
        app->main_menu,
        "Config",
        BunnyConnectSubmenuIndexConfig,
        bunnyconnect_submenu_callback,
        app);
    submenu_add_item(
        app->main_menu, "Info", BunnyConnectSubmenuInfo, bunnyconnect_submenu_callback, app);
    submenu_add_item(
        app->main_menu, "Exit", BunnyConnectSubmenuIndexExit, bunnyconnect_submenu_callback, app);

    view_dispatcher_add_view(
        app->view_dispatcher, BunnyConnectViewMainMenu, submenu_get_view(app->main_menu));

    // Terminal
    app->text_box = text_box_alloc();
    if(!app->text_box) {
        FURI_LOG_E(TAG, "Failed to allocate text box");
        return false;
    }
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->terminal_buffer);
    view_dispatcher_add_view(
        app->view_dispatcher, BunnyConnectViewTerminal, text_box_get_view(app->text_box));

    // Config menu
    app->config_menu = submenu_alloc();
    if(!app->config_menu) {
        FURI_LOG_E(TAG, "Failed to allocate config menu");
        return false;
    }
    submenu_add_item(app->config_menu, "Baud Rate: 115200", 0, NULL, app);
    submenu_add_item(app->config_menu, "Flow Control: Off", 1, NULL, app);
    submenu_add_item(app->config_menu, "USB Power: ON", 2, NULL, app);
    submenu_add_item(app->config_menu, "Auto Enumerate: ON", 3, NULL, app);
    view_dispatcher_add_view(
        app->view_dispatcher, BunnyConnectViewConfig, submenu_get_view(app->config_menu));

    // Info widget
    app->info_widget = widget_alloc();
    if(!app->info_widget) {
        FURI_LOG_E(TAG, "Failed to allocate info widget");
        return false;
    }
    widget_add_text_scroll_element(
        app->info_widget,
        0,
        0,
        128,
        64,
        "BunnyConnect v1.0\n\n"
        "USB CDC Terminal App\n"
        "Made by C0d3-5t3w\n\n"
        "Features:\n"
        "- USB CDC Communication\n"
        "- Terminal Interface\n"
        "- Custom Keyboard\n"
        "- USB Power Control\n\n"
        "Use Connect to establish\n"
        "USB CDC connection with\n"
        "external devices.\n\n"
        "Press Back to return.");
    view_dispatcher_add_view(
        app->view_dispatcher, BunnyConnectViewInfo, widget_get_view(app->info_widget));

    // Custom keyboard
    app->custom_keyboard = bunnyconnect_keyboard_alloc();
    if(!app->custom_keyboard) {
        FURI_LOG_E(TAG, "Failed to allocate custom keyboard");
        return false;
    }
    bunnyconnect_keyboard_set_header_text(app->custom_keyboard, "Enter command:");
    bunnyconnect_keyboard_set_result_callback(
        app->custom_keyboard,
        bunnyconnect_keyboard_callback,
        app,
        app->input_buffer,
        INPUT_BUFFER_SIZE,
        true);
    view_dispatcher_add_view(
        app->view_dispatcher,
        BunnyConnectViewCustomKeyboard,
        bunnyconnect_keyboard_get_view(app->custom_keyboard));

    // Popup
    app->popup = popup_alloc();
    if(!app->popup) {
        FURI_LOG_E(TAG, "Failed to allocate popup");
        return false;
    }
    view_dispatcher_add_view(
        app->view_dispatcher, BunnyConnectViewPopup, popup_get_view(app->popup));

    view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewMainMenu);
    app->current_view = BunnyConnectViewMainMenu;

    FURI_LOG_I(TAG, "Views setup successfully");
    return true;
}

int32_t bunnyconnect_app_run(BunnyConnectApp* app) {
    if(!app) {
        FURI_LOG_E(TAG, "App is NULL");
        return -1;
    }

    FURI_LOG_I(TAG, "Starting app");

    if(!bunnyconnect_setup_views(app)) {
        FURI_LOG_E(TAG, "Failed to setup views");
        return -1;
    }

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bunnyconnect_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bunnyconnect_navigation_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewMainMenu);

    FURI_LOG_I(TAG, "Running view dispatcher");
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup critical resources once the view dispatcher is stopped
    FURI_LOG_I(TAG, "View dispatcher stopped, cleaning up critical app resources");

    // Stop and free the worker thread
    if(app->worker_thread) {
        app->is_running = false; // Signal the thread to exit its loop
        furi_thread_join(app->worker_thread); // Wait for the thread to finish
        furi_thread_free(app->worker_thread); // Free the thread object
        app->worker_thread = NULL;
    }

    // Deinitialize serial communication and related power management
    // This should be safe to call even if a connection was not fully established
    bunnyconnect_serial_deinit(app);

    FURI_LOG_I(TAG, "App finished");
    return 0;
}

int32_t bunnyconnect_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "BunnyConnect starting");

    BunnyConnectApp* app = bunnyconnect_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app");
        return -1;
    }

    int32_t result = bunnyconnect_app_run(app);
    bunnyconnect_app_free(app);

    FURI_LOG_I(TAG, "BunnyConnect finished with result: %ld", result);
    return result;
}
