// fast_js.c
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <js_thread.h>
#include <toolbox/path.h>
#include "js_app_i.h"
#include <cli/cli.h>
#include "js_modules.h"

#define TAG "FastJS"

// Define custom event IDs
#define VIEW_EVENT_ADD_SCRIPT 1

// Define the submenu items for our FastJS application
typedef enum {
    FastJSSubmenuIndexRun, // Execute scripts in playlist
    FastJSSubmenuIndexAbout, // Show the about view
    FastJSSubmenuIndexConfig, // Open the configuration view
} FastJSSubmenuIndex;

// Define views for our FastJS application
typedef enum {
    FastJSViewMain, // The main screen
    FastJSViewSubmenu, // The menu when the app starts
    FastJSViewAbout, // The about screen
    FastJSViewConfigure, // The configuration screen (now shows the playlist)
    FastJSViewConsole, // Console output view
} FastJSView;

// Define a structure to hold settings, including the playlist
#define MAX_PLAYLIST_SIZE      10
#define MAX_SCRIPT_PATH_LENGTH 256

typedef struct {
    char script_path[MAX_SCRIPT_PATH_LENGTH];
    size_t playlist_count;
    char playlist[MAX_PLAYLIST_SIZE][MAX_SCRIPT_PATH_LENGTH];
} SettingsData;

// Forward declaration of FastJSApp for use in callbacks
typedef struct FastJSApp FastJSApp;

// Define the ScriptPlaylist structure
typedef struct {
    char scripts[MAX_PLAYLIST_SIZE][MAX_SCRIPT_PATH_LENGTH];
    size_t count;
} ScriptPlaylist;

// Define the application structure
struct FastJSApp {
    ViewDispatcher* view_dispatcher; // Switches between our views
    Submenu* submenu; // The application submenu
    Widget* widget_about; // The about screen
    Submenu* config_view; // The configuration screen (now a submenu)
    JsConsoleView* console_view; // Console output view

    char* selected_javascript_file; // Store the selected script file path
    char* temp_buffer; // Temporary buffer
    uint32_t temp_buffer_size; // Size of the temporary buffer

    ScriptPlaylist playlist; // Script playlist

    JsThread* js_thread; // JavaScript execution thread
    Gui* gui; // GUI reference
};

// Path to save the settings
#define REMEMBERED_SCRIPT_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/fast_js_app/settings.bin"

// Function to save settings, including the playlist
static void save_settings(const char* script_path, const ScriptPlaylist* playlist) {
    // Create the directory for saving settings
    char directory_path[256];
    snprintf(
        directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/fast_js_app");

    // Create the directory
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);

    // Open the settings file
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, REMEMBERED_SCRIPT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to open settings file for writing: %s", REMEMBERED_SCRIPT_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    // Write the length of script_path
    size_t script_path_length = strlen(script_path) + 1; // Include null terminator
    if(storage_file_write(file, &script_path_length, sizeof(size_t)) != sizeof(size_t)) {
        FURI_LOG_E(TAG, "Failed to write script_path_length");
    }

    // Write the script_path
    if(storage_file_write(file, script_path, script_path_length) != script_path_length) {
        FURI_LOG_E(TAG, "Failed to write script_path");
    }

    // Write the playlist count
    if(storage_file_write(file, &playlist->count, sizeof(size_t)) != sizeof(size_t)) {
        FURI_LOG_E(TAG, "Failed to write playlist count");
    }

    // Write each playlist script with its length
    for(size_t i = 0; i < playlist->count; ++i) {
        size_t script_length = strlen(playlist->scripts[i]) + 1; // Include null terminator
        if(storage_file_write(file, &script_length, sizeof(size_t)) != sizeof(size_t)) {
            FURI_LOG_E(TAG, "Failed to write script length for script %zu", i);
        }
        if(storage_file_write(file, playlist->scripts[i], script_length) != script_length) {
            FURI_LOG_E(TAG, "Failed to write playlist script %zu", i);
        }
    }

    FURI_LOG_I(
        TAG, "Settings saved: script_path=%s, playlist_count=%zu", script_path, playlist->count);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Function to load settings, including the playlist
static bool load_settings(char* buffer, size_t buffer_size, ScriptPlaylist* playlist) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, REMEMBERED_SCRIPT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open settings file for reading: %s", REMEMBERED_SCRIPT_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false; // Return false if the file does not exist
    }

    // Read the length of script_path
    size_t script_path_length;
    if(storage_file_read(file, &script_path_length, sizeof(size_t)) != sizeof(size_t)) {
        FURI_LOG_E(TAG, "Failed to read script_path_length");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Check if script_path_length fits in buffer_size
    if(script_path_length > buffer_size) {
        FURI_LOG_E(TAG, "script_path_length exceeds buffer size");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Read the script_path
    if(storage_file_read(file, buffer, script_path_length) != script_path_length) {
        FURI_LOG_E(TAG, "Failed to read script_path");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Ensure null-termination
    buffer[script_path_length - 1] = '\0';

    // Read the playlist count
    if(storage_file_read(file, &playlist->count, sizeof(size_t)) != sizeof(size_t)) {
        FURI_LOG_E(TAG, "Failed to read playlist count");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Ensure the playlist count does not exceed maximum
    if(playlist->count > MAX_PLAYLIST_SIZE) {
        FURI_LOG_E(
            TAG, "Playlist count exceeds maximum allowed. Truncating to %d.", MAX_PLAYLIST_SIZE);
        playlist->count = MAX_PLAYLIST_SIZE;
    }

    // Read each playlist script with its length
    for(size_t i = 0; i < playlist->count; ++i) {
        size_t script_length;
        if(storage_file_read(file, &script_length, sizeof(size_t)) != sizeof(size_t)) {
            FURI_LOG_E(TAG, "Failed to read script length for script %zu", i);
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return false;
        }

        if(script_length > MAX_SCRIPT_PATH_LENGTH) {
            FURI_LOG_E(TAG, "Script length exceeds maximum allowed for script %zu", i);
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return false;
        }

        if(storage_file_read(file, playlist->scripts[i], script_length) != script_length) {
            FURI_LOG_E(TAG, "Failed to read playlist script %zu", i);
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return false;
        }

        // Ensure null-termination
        playlist->scripts[i][script_length - 1] = '\0';
    }

    FURI_LOG_I(
        TAG, "Settings loaded: script_path=%s, playlist_count=%zu", buffer, playlist->count);

    // Log all loaded scripts for verification
    for(size_t i = 0; i < playlist->count; ++i) {
        FURI_LOG_I(TAG, "Loaded script[%zu]: %s", i, playlist->scripts[i]);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

// Navigation callbacks
static uint32_t fast_js_navigation_configure_callback(void* context) {
    UNUSED(context);
    return FastJSViewSubmenu;
}

static uint32_t fast_js_navigation_about_callback(void* context) {
    UNUSED(context);
    return FastJSViewSubmenu;
}

static uint32_t fast_js_submenu_exit_callback(void* context) {
    // Exit the application
    UNUSED(context);
    return VIEW_NONE; // Return VIEW_NONE to exit the app
}

// Callback function for JS thread events
static void js_callback(JsThreadEvent event, const char* msg, void* context) {
    FastJSApp* app = (FastJSApp*)context;
    furi_assert(app);

    if(event == JsThreadEventDone) {
        FURI_LOG_I(TAG, "Script done");
        console_view_print(app->console_view, "--- DONE ---");
    } else if(event == JsThreadEventPrint) {
        console_view_print(app->console_view, msg);
    } else if(event == JsThreadEventError) {
        console_view_print(app->console_view, "--- ERROR ---");
        console_view_print(app->console_view, msg);
    } else if(event == JsThreadEventErrorTrace) {
        FuriString* compact_trace = furi_string_alloc_set_str(msg);
        // Compact the trace message
        // Keep only first line
        size_t line_end = furi_string_search_char(compact_trace, '\n');
        if(line_end > 0) {
            furi_string_left(compact_trace, line_end);
        }

        // Remove full path
        FuriString* file_name = furi_string_alloc();
        size_t filename_start = furi_string_search_rchar(compact_trace, '/');
        if(filename_start > 0) {
            filename_start++;
            furi_string_set_n(
                file_name,
                compact_trace,
                filename_start,
                furi_string_size(compact_trace) - filename_start);
            furi_string_printf(compact_trace, "at %s", furi_string_get_cstr(file_name));
        }

        furi_string_free(file_name);

        console_view_print(app->console_view, furi_string_get_cstr(compact_trace));
        furi_string_free(compact_trace);
        console_view_print(app->console_view, "See logs for full trace");
    }
}

// Navigation callback for the Console view to go back to the submenu
static uint32_t fast_js_navigation_console_callback(void* context) {
    FastJSApp* app = (FastJSApp*)context;
    // Stop the JS thread if it's still running
    if(app->js_thread) {
        js_thread_stop(app->js_thread);
        app->js_thread = NULL;
    }
    return FastJSViewSubmenu;
}

// Callback function for configuration item selection
static void playlist_item_callback(void* context, uint32_t index) {
    FastJSApp* app = (FastJSApp*)context;
    if(index < app->playlist.count) {
        // Remove the script from the playlist
        for(size_t i = index; i < app->playlist.count - 1; ++i) {
            strcpy(app->playlist.scripts[i], app->playlist.scripts[i + 1]);
        }
        app->playlist.count--;

        // Update the config view
        submenu_reset(app->config_view);

        // Re-add the playlist items
        for(size_t i = 0; i < app->playlist.count; ++i) {
            submenu_add_item(
                app->config_view, app->playlist.scripts[i], i, playlist_item_callback, app);
        }

        // Re-add the "Add Script" option
        submenu_add_item(
            app->config_view, "Add Script", MAX_PLAYLIST_SIZE, playlist_item_callback, app);

        // Save settings
        save_settings(app->selected_javascript_file, &app->playlist);

        console_view_print(app->console_view, "Script removed from playlist.");
    } else if(index == MAX_PLAYLIST_SIZE) {
        // "Add Script" selected
        view_dispatcher_send_custom_event(app->view_dispatcher, VIEW_EVENT_ADD_SCRIPT);
    }
}

// Custom event callback to handle file browser dialog for adding scripts
static bool fast_js_custom_event_callback(void* context, uint32_t event) {
    FastJSApp* app = (FastJSApp*)context;

    if(event == VIEW_EVENT_ADD_SCRIPT) {
        // Open file browser to select a script to add to the playlist

        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
        if(!dialogs) {
            return false;
        }

        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, ".js", NULL);

        browser_options.extension = "js";
        browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;
        browser_options.skip_assets = true;
        browser_options.hide_dot_files = true;
        browser_options.icon = NULL;
        browser_options.hide_ext = false;

        FuriString* javascript_file_path = furi_string_alloc_set_str("/ext/apps/Scripts");
        if(!javascript_file_path) {
            furi_record_close(RECORD_DIALOGS);
            return false;
        }

        if(dialog_file_browser_show(
               dialogs, javascript_file_path, javascript_file_path, &browser_options)) {
            // Store the selected script file path
            const char* file_path = furi_string_get_cstr(javascript_file_path);

            // Check if playlist is full
            if(app->playlist.count >= MAX_PLAYLIST_SIZE) {
                console_view_print(app->console_view, "Playlist is full.");
            } else {
                // Add the script to the playlist
                strncpy(
                    app->playlist.scripts[app->playlist.count], file_path, MAX_SCRIPT_PATH_LENGTH);
                app->playlist.scripts[app->playlist.count][MAX_SCRIPT_PATH_LENGTH - 1] =
                    '\0'; // Ensure null-termination
                app->playlist.count++;

                // Update the config view
                submenu_reset(app->config_view);

                // Re-add the playlist items, showing only the filename
                for(size_t i = 0; i < app->playlist.count; ++i) {
                    FuriString* filename = furi_string_alloc_set(app->playlist.scripts[i]);
                    path_extract_filename(filename, filename, false); // Extract filename from path
                    submenu_add_item(
                        app->config_view,
                        furi_string_get_cstr(filename), // Use only the filename
                        i,
                        playlist_item_callback,
                        app);
                    furi_string_free(filename); // Free the filename string after use
                }

                // Re-add the "Add Script" option
                submenu_add_item(
                    app->config_view, "Add Script", MAX_PLAYLIST_SIZE, playlist_item_callback, app);

                // Save the updated playlist
                save_settings(app->selected_javascript_file, &app->playlist);

                console_view_print(app->console_view, "Script added to playlist.");
            }
        }

        furi_string_free(javascript_file_path);
        furi_record_close(RECORD_DIALOGS);

        // Return to the configuration view
        view_dispatcher_switch_to_view(app->view_dispatcher, FastJSViewConfigure);

        return true; // Event handled
    }

    return false; // Event not handled
}

// Function to execute a single script
static void execute_script(FastJSApp* app, const char* script_path) {
    FuriString* name = furi_string_alloc();
    FuriString* script_path_str = furi_string_alloc_set(script_path);
    path_extract_filename(script_path_str, name, false);
    FuriString* start_text = furi_string_alloc_printf("Running %s", furi_string_get_cstr(name));
    console_view_print(app->console_view, furi_string_get_cstr(start_text));
    console_view_print(app->console_view, "------------");
    furi_string_free(name);
    furi_string_free(start_text);

    app->js_thread = js_thread_run(furi_string_get_cstr(script_path_str), js_callback, app);
    furi_string_free(script_path_str);
}

// Handle submenu item selection
static void fast_js_submenu_callback(void* context, uint32_t index) {
    FastJSApp* app = (FastJSApp*)context;
    switch(index) {
    case FastJSSubmenuIndexRun:
        // Execute all scripts in the playlist
        if(app->playlist.count == 0) {
            console_view_print(app->console_view, "No scripts in the playlist.");
        } else {
            // Switch to the console view
            view_dispatcher_switch_to_view(app->view_dispatcher, FastJSViewConsole);

            // Execute each script sequentially
            for(size_t i = 0; i < app->playlist.count; ++i) {
                execute_script(app, app->playlist.scripts[i]);
            }
        }
        break;
    case FastJSSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, FastJSViewAbout);
        break;
    case FastJSSubmenuIndexConfig:
        view_dispatcher_switch_to_view(app->view_dispatcher, FastJSViewConfigure);
        break;
    default:
        break;
    }
}
// Function to allocate and initialize the FastJS application
static FastJSApp* fast_js_app_alloc() {
    FastJSApp* app = (FastJSApp*)malloc(sizeof(FastJSApp));
    if(!app) {
        return NULL;
    }

    app->temp_buffer_size = MAX_SCRIPT_PATH_LENGTH; // Buffer size for paths
    app->temp_buffer = (char*)malloc(app->temp_buffer_size);
    app->selected_javascript_file = (char*)malloc(app->temp_buffer_size);

    if(!app->temp_buffer || !app->selected_javascript_file) {
        free(app->temp_buffer);
        free(app->selected_javascript_file);
        free(app);
        return NULL;
    }

    // Initialize buffers with empty strings
    app->temp_buffer[0] = '\0';
    app->selected_javascript_file[0] = '\0';

    // Initialize the playlist
    app->playlist.count = 0;

    // Try to load the remembered settings
    if(load_settings(app->selected_javascript_file, app->temp_buffer_size, &app->playlist)) {
        FURI_LOG_I(
            TAG,
            "Settings loaded: script_path=%s, playlist_count=%zu",
            app->selected_javascript_file,
            app->playlist.count);
    } else {
        FURI_LOG_I(TAG, "No saved settings found; using defaults");
    }

    // Allocate and set up the view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    if(!app->view_dispatcher) {
        free(app->temp_buffer);
        free(app->selected_javascript_file);
        free(app);
        return NULL;
    }

    // Open the GUI and attach the view dispatcher
    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    // Set the custom event callback for file browsing
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, fast_js_custom_event_callback);

    // Initialize the console view
    app->console_view = console_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FastJSViewConsole, console_view_get_view(app->console_view));
    view_set_previous_callback(
        console_view_get_view(app->console_view), fast_js_navigation_console_callback);
    view_set_context(console_view_get_view(app->console_view), app);

    // Initialize the submenu view
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "FastJS");
    submenu_add_item(
        app->submenu, "Run Playlist", FastJSSubmenuIndexRun, fast_js_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", FastJSSubmenuIndexAbout, fast_js_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Config", FastJSSubmenuIndexConfig, fast_js_submenu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), fast_js_submenu_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, FastJSViewSubmenu, submenu_get_view(app->submenu));

    // Initialize the configuration view
    app->config_view = submenu_alloc();
    submenu_set_header(app->config_view, "Playlist");
    // Add existing scripts to the submenu, showing only the filename
    for(size_t i = 0; i < app->playlist.count; ++i) {
        FuriString* filename = furi_string_alloc_set(app->playlist.scripts[i]);
        path_extract_filename(filename, filename, false); // Extract filename from path
        submenu_add_item(
            app->config_view,
            furi_string_get_cstr(filename), // Use only the filename
            i,
            playlist_item_callback,
            app);
        furi_string_free(filename); // Free the filename string after use
    }

    // Add "Add Script" option
    submenu_add_item(
        app->config_view, "Add Script", MAX_PLAYLIST_SIZE, playlist_item_callback, app);

    view_set_previous_callback(
        submenu_get_view(app->config_view), fast_js_navigation_configure_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, FastJSViewConfigure, submenu_get_view(app->config_view));

    // Initialize the about view
    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "FastJS App\n---\nExecute your scripts\nseamlessly.\n---\nManage your\nplaylist in the config menu.\nYou will click 'Add Script'\nto add a script, or click\nthe script to remove it.\n---\nPress BACK to return.");
    view_set_previous_callback(
        widget_get_view(app->widget_about), fast_js_navigation_about_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, FastJSViewAbout, widget_get_view(app->widget_about));

    // Start with the submenu view
    view_dispatcher_switch_to_view(app->view_dispatcher, FastJSViewSubmenu);

    return app;
}

// Function to free the resources used by FastJSApp
static void fast_js_app_free(FastJSApp* app) {
    if(!app) return;

    // Stop JS thread if running
    if(app->js_thread) {
        js_thread_stop(app->js_thread);
        app->js_thread = NULL;
    }

    // Free console view
    view_dispatcher_remove_view(app->view_dispatcher, FastJSViewConsole);
    console_view_free(app->console_view);

    // Free submenu view
    view_dispatcher_remove_view(app->view_dispatcher, FastJSViewSubmenu);
    submenu_free(app->submenu);

    // Free configuration view
    view_dispatcher_remove_view(app->view_dispatcher, FastJSViewConfigure);
    submenu_free(app->config_view);

    // Free about view
    view_dispatcher_remove_view(app->view_dispatcher, FastJSViewAbout);
    widget_free(app->widget_about);

    // Free buffers
    free(app->temp_buffer);
    free(app->selected_javascript_file);

    // Free dispatcher
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    // Free the app structure
    free(app);
}

// Entry point for the FastJS application
int32_t fast_js_app(void* p) {
    UNUSED(p);

    // Initialize the FastJS application
    FastJSApp* app = fast_js_app_alloc();
    if(!app) {
        // Allocation failed
        return -1; // Indicate failure
    }

    // Run the view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Free the resources used by the FastJS application
    fast_js_app_free(app);

    // Return 0 to indicate success
    return 0;
}
