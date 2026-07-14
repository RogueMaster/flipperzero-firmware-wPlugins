#include <font/font.h>
#include <flipper_http/flipper_http.h> // we're using FlipperHTTP since UART is already set up
#include <easy_flipper/easy_flipper.h>

#define TAG         "FlipRPI"
#define VERSION_TAG TAG " v1.0"

#define MAX_COMMANDS 54

// Define the submenu items for our FlipRPI application
typedef enum {
    FlipRPISubmenuIndexRun, // Click to view the RPI output
    FlipRPISubmenuIndexSend, // Click to send a command to the RPI
    FlipRPISubmenuIndexAbout,
    //    FlipRPISubmenuIndexSettings,
    FlipRPISubmenuIndexCommandStart = 10,

} FlipRPISubmenuIndex;

// Define a single view for our FlipRPI application
typedef enum {
    FlipRPIViewMain = 3, // The main screen
    FlipRPIViewSubmenu = 4, // The main submenu
    FlipRPIViewAbout = 5, // The about screen
        // FlipRPIViewSettings = 6,  // The settings screen
    FlipRPIViewTextInput = 7, // The text input screen
    //
    FlipRPIViewSubmenuCommands, // The submenu for commands
} FlipRPIView;

typedef enum {
    FlipRPICustomEventUART
} FlipRPICustomEvent;

// Each screen will have its own view
typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    Submenu* submenu; // The submenu
    Submenu* submenu_commands; // The submenu for commands
    Widget* widget; // The widget
    TextBox* textbox; // The text box
    FlipperHTTP* fhttp; // FlipperHTTP (UART, simplified loading, etc.)
    UART_TextInput* uart_text_input; // The text input
    char* uart_text_input_buffer; // Buffer for the text input
    char* uart_text_input_temp_buffer; // Temporary buffer for the text input
    uint32_t uart_text_input_buffer_size; // Size of the text input buffer
    //
    FuriTimer* timer; // timer to redraw the UART data as it comes in
} FlipRPIApp;

// sudo apt install libssl-dev zlib1g-dev libpocl2 ocl-icd-libopencl1 opencl-headers clinfo ocl-icd-dev ocl-icd-opencl-dev llvm-dev libclang-dev

const char* commands[MAX_COMMANDS] = {
    "[CUSTOM]",
    "cat",
    "cd",
    "clear",
    "cmake",
    "cmake .."
    "cp",
    //"echo",
    "git",
    "git clone",
    "git clone https://github.com/",
    "git pull",
    "git push",
    //"grep",
    "htop",
    "ifconfig",
    "ip",
    "ls",
    "make"
    "mkdir",
    "mv",
    "pip",
    "pip3",
    "pip install",
    "pip3 install",
    "ps -aux",
    "pwd",
    "python3",
    "python3 -m venv",
    "rm",
    "sudo",
    // "sudo raspi-config",
    "sudo apt",
    "sudo apt update",
    "sudo apt upgrade",
    "sudo apt autoremove",
    "sudo apt-get",
    "sudo apt-get update",
    "sudo apt-get upgrade",
    "sudo apt full-upgrade",
    "sudo apt install build-essential",
    "sudo apt install cmake clang",
    "sudo apt install git",
    "sudo apt install python3-full python3-pip",
    "sudo apt install python3-venv",
    "sudo apt install vim",
    "sudo make install",
    "sudo systemctl start",
    "sudo systemctl stop",
    "sudo systemctl restart",
    "sudo systemctl status",
    "sudo reboot",
    "sudo shutdown -h now",
    "touch",
    "uname -a",
    "whereis",
    "df -h",
};

static void callback_submenu_choices(void* context, uint32_t index);

static uint32_t callback_to_submenu(void* context) {
    UNUSED(context);
    return FlipRPIViewSubmenu;
}

static uint32_t callback_exit_app(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}
static size_t last_response_len = 0;
static void update_text_box(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");
    furi_check(app->textbox, "Text_box is NULL");
    if(!app->fhttp->last_response_str || furi_string_size(app->fhttp->last_response_str) == 0) {
        text_box_reset(app->textbox);
        text_box_set_focus(app->textbox, TextBoxFocusEnd);
        text_box_set_font(app->textbox, TextBoxFontText);
        text_box_set_text(app->textbox, "Awaiting data...");
    } else if(furi_string_size(app->fhttp->last_response_str) != last_response_len) {
        text_box_reset(app->textbox);
        text_box_set_focus(app->textbox, TextBoxFocusEnd);
        text_box_set_font(app->textbox, TextBoxFontText);
        last_response_len = furi_string_size(app->fhttp->last_response_str);
        text_box_set_text(app->textbox, furi_string_get_cstr(app->fhttp->last_response_str));
    }
}

static void text_updated(void* context) {
    FlipRPIApp* app = (FlipRPIApp*)context;
    furi_check(app, "FlipRPIApp is NULL");
    furi_check(app->fhttp, "FlipperHTTP is NULL");

    // store the entered text
    strncpy(
        app->uart_text_input_buffer,
        app->uart_text_input_temp_buffer,
        app->uart_text_input_buffer_size);

    // add \n to the end of the command (without using strcat)
    app->uart_text_input_buffer[app->uart_text_input_buffer_size - 2] = '\n';

    // Ensure null-termination
    app->uart_text_input_buffer[app->uart_text_input_buffer_size - 1] = '\0';

    // send command to UART
    if(!flipper_http_send_data(app->fhttp, app->uart_text_input_buffer)) {
        FURI_LOG_E(TAG, "Failed to send data over UART");
        easy_flipper_dialog("Error", "Failed to send data over UART");
    }
    update_text_box(app);
    // switch to the main view to see the response from RPI
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipRPIViewMain);
}

static bool alloc_text_input(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");

    // Allocate the text input buffer
    app->uart_text_input_buffer_size = 128;
    if(!easy_flipper_set_buffer(&app->uart_text_input_buffer, app->uart_text_input_buffer_size)) {
        return false;
    }
    if(!easy_flipper_set_buffer(
           &app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size)) {
        return false;
    }

    // Allocate the text input
    if(!easy_flipper_set_uart_text_input(
           &app->uart_text_input,
           FlipRPIViewTextInput,
           "Enter command",
           app->uart_text_input_temp_buffer,
           app->uart_text_input_buffer_size,
           text_updated,
           callback_to_submenu,
           &app->view_dispatcher,
           app)) {
        return false;
    }

    return app->uart_text_input != NULL;
}

static void free_text_input(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");

    // Free the text input
    if(app->uart_text_input) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipRPIViewTextInput);
        uart_text_input_free(app->uart_text_input);
        app->uart_text_input = NULL;
    }

    // Free the text input buffer
    if(app->uart_text_input_buffer) {
        free(app->uart_text_input_buffer);
        app->uart_text_input_buffer = NULL;
    }
    if(app->uart_text_input_temp_buffer) {
        free(app->uart_text_input_temp_buffer);
        app->uart_text_input_temp_buffer = NULL;
    }
}

static bool alloc_widget(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");
    furi_check(app->fhttp, "FlipperHTTP is NULL");

    if(app->widget) {
        FURI_LOG_E(TAG, "Widget already allocated");
        return false;
    }
    return easy_flipper_set_widget(
        &app->widget,
        FlipRPIViewAbout,
        "FlipRPI v1.0\n------ ------ ------\nControl your Raspberry Pi\nwith your Flipper.",
        callback_to_submenu,
        &app->view_dispatcher);
}
static void free_widget(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");

    // Free the widget
    if(app->widget) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipRPIViewAbout);
        widget_free(app->widget);
        app->widget = NULL;
    }
}

static void update_submenu(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");
    for(uint32_t i = 0; i < MAX_COMMANDS; i++) {
        if(commands[i] != NULL) {
            submenu_add_item(
                app->submenu_commands,
                commands[i],
                FlipRPISubmenuIndexCommandStart + i,
                callback_submenu_choices,
                app);
        }
    }
}

static bool alloc_submenu_command(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");

    if(app->submenu_commands) {
        FURI_LOG_E(TAG, "Submenu already allocated");
        return false;
    }

    // Allocate the submenu for commands
    if(!easy_flipper_set_submenu(
           &app->submenu_commands,
           FlipRPIViewSubmenuCommands,
           "Commands",
           callback_to_submenu,
           &app->view_dispatcher)) {
        return false;
    }
    update_submenu(app);
    return app->submenu_commands != NULL;
}

static void free_submenu_command(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");

    // Free the submenu for commands
    if(app->submenu_commands) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipRPIViewSubmenuCommands);
        submenu_free(app->submenu_commands);
        app->submenu_commands = NULL;
    }
}
static void flip_rpi_timer_callback(void* context) {
    furi_check(context, "flip_rpi_timer_callback: Context is NULL");
    FlipRPIApp* app = (FlipRPIApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipRPICustomEventUART);
}

static bool alloc_text_box(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");
    if(app->textbox) {
        FURI_LOG_E(TAG, "Text box already allocated");
        return false;
    }
    if(!easy_flipper_set_text_box(
           &app->textbox,
           FlipRPIViewMain,
           app->fhttp->last_response_str && furi_string_size(app->fhttp->last_response_str) > 0 ?
               (char*)furi_string_get_cstr(app->fhttp->last_response_str) :
               "Awaiting data...",
           false,
           callback_to_submenu,
           &app->view_dispatcher)) {
        FURI_LOG_E(TAG, "Failed to allocate text box");
        return false;
    }
    if(app->timer == NULL) {
        app->timer = furi_timer_alloc(flip_rpi_timer_callback, FuriTimerTypePeriodic, app);
    }
    furi_timer_start(app->timer, 250);
    return app->textbox != NULL;
}

static void free_text_box(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");

    // Free the text box
    if(app->textbox) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipRPIViewMain);
        text_box_free(app->textbox);
        app->textbox = NULL;
    }
    // Free the timer
    if(app->timer) {
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
}

static void flip_rpi_loader_process_callback(void* context) {
    FlipRPIApp* app = (FlipRPIApp*)context;
    furi_check(app, "FlipRPIApp is NULL");
    furi_check(app->fhttp, "FlipperHTTP is NULL");

    // Update the text box
    update_text_box(app);
}

bool flip_rpi_custom_event_callback(void* context, uint32_t index) {
    furi_check(context, "flip_rpi_custom_event_callback: Context is NULL");
    switch(index) {
    case FlipRPICustomEventUART:
        flip_rpi_loader_process_callback(context);
        return true;
    default:
        FURI_LOG_E(TAG, "flip_rpi_custom_event_callback. Unknown index: %ld", index);
        return false;
    }
}
static void callback_submenu_choices(void* context, uint32_t index) {
    FlipRPIApp* app = (FlipRPIApp*)context;
    furi_check(app, "FlipRPIApp is NULL");
    if(!app->fhttp) {
        app->fhttp = flipper_http_alloc();
        if(!app->fhttp) {
            FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
            easy_flipper_dialog(
                "Error",
                "Failed to start UART.\nUART is likely busy or an RPI\nis not connected.");
            return;
        }
    }
    switch(index) {
    case FlipRPISubmenuIndexRun:
        free_text_box(app);
        if(!alloc_text_box(app)) {
            FURI_LOG_E(TAG, "Failed to allocate text box");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipRPIViewMain);
        break;
    case FlipRPISubmenuIndexSend:
        free_submenu_command(app);
        if(!alloc_submenu_command(app)) {
            FURI_LOG_E(TAG, "Failed to allocate submenu");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipRPIViewSubmenuCommands);
        break;
    case FlipRPISubmenuIndexAbout:
        free_widget(app);
        if(!alloc_widget(app)) {
            FURI_LOG_E(TAG, "Failed to allocate widget");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipRPIViewAbout);
        break;
    // case FlipRPISubmenuIndexSettings:
    //     view_dispatcher_switch_to_view(app->view_dispatcher, FlipRPIViewSettings);
    //     break;
    default:
        // Handle the commands
        if(index >= FlipRPISubmenuIndexCommandStart) {
            // Get the command index
            uint32_t command_index = index - FlipRPISubmenuIndexCommandStart;

            // Get the command
            const char* command = commands[command_index];

            // alloc text input
            free_text_input(app);
            if(!alloc_text_input(app)) {
                FURI_LOG_E(TAG, "Failed to allocate text input");
                return;
            }
            // alloc text_box (viewed after text input)
            free_text_box(app);
            if(!alloc_text_box(app)) {
                FURI_LOG_E(TAG, "Failed to allocate text box");
                return;
            }
            // send to text input if command is custom
            if(command_index != 0) {
                // edit the text input buffer so it starts with the command
                strncpy(
                    app->uart_text_input_temp_buffer, command, app->uart_text_input_buffer_size);
                app->uart_text_input_temp_buffer[app->uart_text_input_buffer_size - 1] = '\0';
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipRPIViewTextInput);
        }
    }
}

// Function to allocate resources for the FlipRPIApp
static FlipRPIApp* flip_rpi_app_alloc() {
    FlipRPIApp* app = (FlipRPIApp*)malloc(sizeof(FlipRPIApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    // Allocate ViewDispatcher
    if(!easy_flipper_set_view_dispatcher(&app->view_dispatcher, gui, app)) {
        return NULL;
    }
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flip_rpi_custom_event_callback);
    // Submenu
    if(!easy_flipper_set_submenu(
           &app->submenu,
           FlipRPIViewSubmenu,
           VERSION_TAG,
           callback_exit_app,
           &app->view_dispatcher)) {
        return NULL;
    }
    submenu_add_item(app->submenu, "View", FlipRPISubmenuIndexRun, callback_submenu_choices, app);
    submenu_add_item(app->submenu, "Send", FlipRPISubmenuIndexSend, callback_submenu_choices, app);
    submenu_add_item(
        app->submenu, "About", FlipRPISubmenuIndexAbout, callback_submenu_choices, app);
    // submenu_add_item(app->submenu, "Settings", FlipRPISubmenuIndexSettings, callback_submenu_choices, app);

    // Switch to the main view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipRPIViewSubmenu);

    return app;
}

// Function to free the resources used by FlipRPIApp
static void flip_rpi_app_free(FlipRPIApp* app) {
    furi_check(app, "FlipRPIApp is NULL");

    // Free Submenu(s)
    if(app->submenu) {
        view_dispatcher_remove_view(app->view_dispatcher, FlipRPIViewSubmenu);
        submenu_free(app->submenu);
    }

    free_widget(app);
    free_text_input(app);
    free_submenu_command(app);
    free_text_box(app);

    // free the FlipperHTTP
    if(app->fhttp) {
        flipper_http_free(app->fhttp);
        app->fhttp = NULL;
    }

    // free the view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // close the gui
    furi_record_close(RECORD_GUI);

    // free the app
    free(app);
}

// Entry point for the FlipRPI application
int32_t main_flip_rpi(void* p) {
    // Suppress unused parameter warning
    UNUSED(p);

    // Initialize the FlipRPI application
    FlipRPIApp* app = flip_rpi_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate FlipRPIApp");
        return -1;
    }

    // Run the view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Free the resources used by the FlipRPI application
    flip_rpi_app_free(app);

    // Return 0 to indicate success
    return 0;
}
