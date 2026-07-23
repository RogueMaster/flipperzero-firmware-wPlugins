#include "ir_share_app.h"
#include <stream/stream.h>
#include <stream/buffered_file_stream.h>

#define TAG "IrShare"

// About screen title bar (\e#/\e! markup: bold/inverted, see widget.h)
#define ISH_ABOUT_NAME      "\e#\e!       Flipper Share - IR       \e!\n"
#define ISH_ABOUT_BLANK_INV "\e#\e!                                                      \e!\n"

// Callback when a file is selected in the file browser
static void file_browser_select_callback(void* context) {
    if(!context) return;
    IrShareApp* app = context;

    // Get the selected file path from result_path
    const char* file_path = furi_string_get_cstr(app->result_path);
    if(file_path && file_path[0]) {
        FURI_LOG_I(TAG, "file selected: '%s'", file_path);
        strncpy(app->selected_file_path, file_path, sizeof(app->selected_file_path) - 1);
        app->selected_file_path[sizeof(app->selected_file_path) - 1] = '\0';
        app->file_info_loaded = false;
        app->selected_file_size = 0;

        // Send file selection event
        if(app->view_dispatcher) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 1);
        }
    }
}

// Callback for internal use, not used in the app
_Bool file_browser_callback(
    FuriString* path,
    void* context,
    unsigned char** icon,
    FuriString* name) {
    UNUSED(icon);
    UNUSED(name);
    UNUSED(path);
    UNUSED(context);
    return false;
}

// Set the callback after creating the file_browser (in ir_share_alloc):
// file_browser_set_result_callback(app->file_browser, file_browser_callback, app);

void show_file_info_scene(IrShareApp* app) {
    furi_assert(app);
    dialog_ex_set_header(
        app->dialog_show_file, "File Info", 64, SCENE_HEADER_POSITION_Y, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_show_file, app->selected_file_path, 64, 32, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, IrShareViewIdShowFile);
}

bool ir_share_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    IrShareApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool ir_share_back_event_callback(void* context) {
    furi_assert(context);
    IrShareApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    IrShareApp* app = context;

    if(index == 0) { // Send - open file browser
        scene_manager_next_scene(app->scene_manager, IrShareSceneFileBrowser);
    } else if(index == 1) { // Receive
        scene_manager_next_scene(app->scene_manager, IrShareSceneReceive);
    } else if(index == 2) { // About
        // Inverted title bar + scrollable body, same layout as official apps
        // (see good-faps spi_mem_manager about scene).
        widget_reset(app->widget_about);
        widget_add_text_box_element(
            app->widget_about, 0, 0, 128, 14, AlignCenter, AlignBottom, ISH_ABOUT_BLANK_INV, false);
        widget_add_text_box_element(
            app->widget_about, 0, 2, 128, 14, AlignCenter, AlignBottom, ISH_ABOUT_NAME, false);

        FuriString* about_text = furi_string_alloc();
        // furi_string_printf(about_text, "\e#%s\n", "Information");
        // FAP_VERSION is a string literal injected by fbt/ufbt from
        // application.fam's fap_version — single source of truth, no duplication.
        furi_string_cat_printf(about_text, "Version: %s\n", FAP_VERSION);
        furi_string_cat_printf(about_text, "Developed by: %s\n", "@lomalkin");
        furi_string_cat_printf(about_text, "Github: %s\n\n", "github.com/lomalkin");
        furi_string_cat_printf(about_text, "\e#%s\n", "Description");
        furi_string_cat_printf(about_text, "%s\n\n", "File transfer over Infrared.");
        furi_string_cat_printf(
            about_text,
            "See also: Flipper Share (classic) to faster transfer speeds via Sub-GHz radio.\n");
        widget_add_text_scroll_element(
            app->widget_about, 0, 16, 128, 50, furi_string_get_cstr(about_text));
        furi_string_free(about_text);

        view_dispatcher_switch_to_view(app->view_dispatcher, IrShareViewIdAbout);
    }
}

// Return to main menu when pressing Back on About
static uint32_t ir_share_about_previous(void* context) {
    UNUSED(context);
    return IrShareViewIdMenu;
}

// TODO: check / cleanup

// Function to read file information (size, etc.) - commented out for debugging
/*
static bool read_file_info(IrShareApp* app) {
    if(!app || !app->selected_file_path[0]) {
        return false;
    }
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        return false;
    }
    
    FileInfo file_info;
    bool success = false;
    
    if(storage_common_stat(storage, app->selected_file_path, &file_info) == FSE_OK) {
        app->selected_file_size = file_info.size;
        app->file_info_loaded = true;
        success = true;
    } else {
        app->file_info_loaded = false;
    }
    
    furi_record_close(RECORD_STORAGE);
    return success;
}
*/

// Example function for reading file content (commented out for now)
// static bool read_file_content(IrShareApp* app, char* buffer, size_t buffer_size) {
//     Storage* storage = furi_record_open(RECORD_STORAGE);
//     File* file = storage_file_alloc(storage);
//     bool success = false;
//
//     if(storage_file_open(file, app->selected_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
//         size_t bytes_read = storage_file_read(file, buffer, buffer_size - 1);
//         size_t bytes_read = storage_file_read(file, buffer, buffer_size - 1);
//         buffer[bytes_read] = '\0'; // Null terminate
//         success = true;
//         storage_file_close(file);
//     }
//
//     storage_file_free(file);
//     furi_record_close(RECORD_STORAGE);
//     return success;
// }

// Temporarily disabled callback to debug crash
/*
static void file_browser_callback(void* context) {
    if(!context) {
        return;
    }
    
    IrShareApp* app = context;
    
    // Set the selected file path
    strcpy(app->selected_file_path, "/ext/test_file.txt");

    // Don't read file info for now - just set default values
    app->selected_file_size = 0;
    app->file_info_loaded = false;

    // Send custom event to handle file selection in the scene
    // This is safer than calling scene manager directly from callback
    if(app->view_dispatcher) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 1);
    }
}
*/

static IrShareApp* ir_share_alloc() {
    IrShareApp* app = malloc(sizeof(IrShareApp));
    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();

    app->scene_manager = scene_manager_alloc(&ir_share_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, ir_share_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ir_share_back_event_callback);

    // Create submenu for main menu
    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Send via IR", 0, submenu_callback, app);
    submenu_add_item(app->submenu, "Receive via IR", 1, submenu_callback, app);
    submenu_add_item(app->submenu, "About", 2, submenu_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, IrShareViewIdMenu, submenu_get_view(app->submenu));

    // Create file browser with result_path for selected file retrieval.
    // Seeded with the base path; then it keeps the last selection for the
    // whole session, so the browser reopens where the user left off.
    FuriString* result_path = furi_string_alloc_set(SCENE_FILE_BROWSER_BASE_PATH);
    // Allocate file browser once
    app->file_browser = file_browser_alloc(result_path);

    // Configure file browser
    file_browser_configure(
        app->file_browser,
        "*", // all extensions
        SCENE_FILE_BROWSER_BASE_PATH, // initial path - where files are located
        false, // do not skip assets
        false, // do not hide dot files
        NULL, // default file icon
        false // do not hide extensions
    );

    // Set callback for file selection
    file_browser_set_callback(app->file_browser, file_browser_select_callback, app);

    // Store result_path for later use
    app->result_path = result_path;
    view_dispatcher_add_view(
        app->view_dispatcher, IrShareViewIdFileBrowser, file_browser_get_view(app->file_browser));

    // Create dialog to show file path/info
    app->dialog_show_file = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, IrShareViewIdShowFile, dialog_ex_get_view(app->dialog_show_file));

    // Create dialog for Receive
    app->dialog_receive = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, IrShareViewIdReceive, dialog_ex_get_view(app->dialog_receive));

    app->selected_file_path[0] = '\0'; // Explicitly initialize with empty string
    app->selected_file_size = 0;
    app->file_info_loaded = false;

    // Initialize fields for file reading scene
    app->file_reading_state = NULL;
    app->timer = NULL;

    // Create widget for About (scrollable text)
    app->widget_about = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, IrShareViewIdAbout, widget_get_view(app->widget_about));
    // Ensure Back from About returns to Menu
    view_set_previous_callback(widget_get_view(app->widget_about), ir_share_about_previous);

    return app;
}

static void ir_share_free(IrShareApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, IrShareViewIdReceive);
    view_dispatcher_remove_view(app->view_dispatcher, IrShareViewIdShowFile);
    view_dispatcher_remove_view(app->view_dispatcher, IrShareViewIdFileBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, IrShareViewIdMenu);
    // Also remove the About view to avoid leaving a dangling view pointer
    view_dispatcher_remove_view(app->view_dispatcher, IrShareViewIdAbout);

    dialog_ex_free(app->dialog_show_file);
    dialog_ex_free(app->dialog_receive);
    widget_free(app->widget_about);

    file_browser_free(app->file_browser);
    if(app->result_path) {
        furi_string_free(app->result_path);
    }
    submenu_free(app->submenu);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    free(app);
}

int32_t ir_share_app(void* p) {
    UNUSED(p);
    IrShareApp* app = ir_share_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, IrShareSceneMenu);

    view_dispatcher_run(app->view_dispatcher);

    ir_share_free(app);

    return 0;
}
