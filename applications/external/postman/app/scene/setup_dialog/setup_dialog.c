#include "../../app.h"
#include "../../uart/uart.h"
#include "../../version.h"
#include <gui/modules/dialog_ex.h>
#include <gui/view_dispatcher.h>
#include <postmanflipx_icons.h>

#define WELCOME_DURATION_MS 1500

static void setup_dialog_callback(DialogExResult result, void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static void setup_dialog_update(
    App* app,
    const char* message,
    const Icon* icon,
    const char* center_button_text,
    const char* left_button_text,
    int text_x,
    int text_y,
    Align text_align_h,
    Align text_align_v) {
    DialogEx* dialog_ex = app->dialog;

    // Set the text with specified position and alignment
    dialog_ex_set_text(dialog_ex, message, text_x, text_y, text_align_h, text_align_v);
    dialog_ex_set_icon(dialog_ex, 1, 1, icon);

    // Set the center button text only if it is not NULL
    if(center_button_text) {
        dialog_ex_set_center_button_text(dialog_ex, center_button_text);
    }

    if(left_button_text) {
        dialog_ex_set_left_button_text(dialog_ex, left_button_text);
    }
    dialog_ex_set_result_callback(dialog_ex, setup_dialog_callback);
    dialog_ex_set_context(dialog_ex, app);
}

static void uart_check_task(App* app) {
    FURI_LOG_T(TAG, "Updating dialog: Checking the board ...");

    setup_dialog_update(
        app, "esp32 check", &I_WifiCheck_128_64, NULL, NULL, 72, 58, AlignLeft, AlignCenter);

    bool version_ok = uart_terminal_uart_send_version_command(app->uart);

    // Update the dialog with the current message

    if(version_ok) {
        // Update dialog to show success result
        FURI_LOG_T(TAG, "Updating dialog: Board checked");
        app->status = BOARD_CONNECTED_WIFI_OFF;

        bool connected = uart_terminal_uart_check_status(app->uart);

        if(connected) {
            app->status = BOARD_CONNECTED_WIFI_ON;

            setup_dialog_update(
                app,
                "connection check",
                &I_WifiCheck_128_64,
                NULL,
                NULL,
                52,
                58,
                AlignLeft,
                AlignCenter);

            if(activeWifiCommand(app->uart, NULL)) {
                FURI_LOG_T(TAG, "Wifi is connected");
            } else {
                FURI_LOG_T(TAG, "Wifi is not connected");
            }
        }

        setup_dialog_update(
            app,
            "esp32 is ready",
            &I_esp32Success,
            "continue",
            "exit",
            63,
            5,
            AlignLeft,
            AlignCenter);
    } else {
        // Update dialog to show failure result
        FURI_LOG_T(TAG, "Updating dialog: No board or wrong version");
        app->status = BOARD_ERROR;
        setup_dialog_update(
            app,
            "Oh no !! Board is missing or incorrect firmware",
            &I_error,
            "retry",
            "exit",
            65,
            25,
            AlignLeft,
            AlignCenter);
    }
}

static void setup_dialog_timer_callback(void* context) {
    App* app = context;

    switch(app->dialog_state) {
    case SetupDialogStateWelcome:
        // Move to the next state
        app->dialog_state = SetupDialogStateChecking;
        // Update dialog to show checking board state and start UART check task
        uart_check_task(app);
        break;
    case SetupDialogStateChecking:
    case SetupDialogStateResult:
        // No action needed
        break;
    }
}

void scene_on_enter_setup_dialog(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_setup_dialog");
    App* app = context;

    // Initialize the dialog state
    app->dialog_state = SetupDialogStateWelcome;

    // Initialize the timer
    app->timer = furi_timer_alloc(setup_dialog_timer_callback, FuriTimerTypeOnce, app);

    // Update dialog to show welcome message
    char welcome_message[64];
    snprintf(welcome_message, sizeof(welcome_message), "v%s \ninitializing...", VERSION);
    setup_dialog_update(
        app, welcome_message, &I_Postman_128_64, NULL, NULL, 73, 60, AlignLeft, AlignBottom);

    // Start the timer for the welcome message
    furi_timer_start(app->timer, WELCOME_DURATION_MS);

    // Switch to the dialog view
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_SetupDialog);
}

bool scene_on_event_setup_dialog(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DialogExResultCenter:

            switch(app->status) {
            case BOARD_CONNECTED_WIFI_OFF:
                // Switch to the main menu view
                scene_manager_next_scene(app->scene_manager, MainMenu);
                consumed = true;
                break;
            case BOARD_CONNECTED_WIFI_ON:
                // Switch to the main menu view
                scene_manager_next_scene(app->scene_manager, MainMenu);
                consumed = true;
                break;
            case BOARD_ERROR:
                // Restart the UART check task
                app->dialog_state = SetupDialogStateChecking;
                uart_check_task(app);
                consumed = true;
                break;
            }

            consumed = true;
            break;
        case DialogExResultLeft:
            scene_manager_stop(app->scene_manager);
            view_dispatcher_stop(app->view_dispatcher);
            consumed = true;
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void scene_on_exit_setup_dialog(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_setup_dialog");
    App* app = context;

    dialog_ex_reset(app->dialog);

    // Stop and free the timer
    furi_timer_stop(app->timer);
}
