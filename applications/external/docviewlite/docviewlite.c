#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <flipper_format/flipper_format.h>

#define TAG "docviewlite"

// Change this to BACKLIGHT_AUTO if you don't want the backlight to be continuously on.
#define BACKLIGHT_ON 1

// Maximum file content size - adjust based on available memory
#define MAX_FILE_SIZE   8192
#define MAX_LINES       100
#define MAX_LINE_LENGTH 128

// File extension filter
#define FILE_EXTENSION_FILTER ".txt"

// Our application menu items
typedef enum {
    DocViewSubmenuIndexBrowse,
    DocViewSubmenuIndexSettings,
    DocViewSubmenuIndexAbout,
} DocViewSubmenuIndex;

// Each view is a screen we show the user
typedef enum {
    DocViewViewSubmenu, // The main menu
    DocViewViewFileBrowser, // File browser screen
    DocViewViewTextViewer, // Document viewing screen
    DocViewViewSettings, // Settings screen
    DocViewViewAbout, // About screen
    DocViewViewTextInput, // For search or other text input
} DocViewView;

// Custom events
typedef enum {
    DocViewEventIdRedraw = 0,
    DocViewEventIdLoadFile,
    DocViewEventIdNextPage,
    DocViewEventIdPrevPage,
} DocViewEventId;

// Settings for the document viewer
typedef struct {
    uint8_t font_size; // Font size setting (small, medium, large)
    bool word_wrap; // Word wrap enabled or not
    uint8_t scroll_speed; // Auto-scroll speed if implemented
} DocViewSettings;

// Model for document content
typedef struct {
    FuriString* file_path; // Current file path
    FuriString* content; // File content as string
    size_t content_length; // Length of content
    uint16_t current_line; // Current line being viewed
    uint16_t total_lines; // Total lines in document
    uint16_t lines_per_screen; // How many lines fit on screen
    uint16_t line_offsets[MAX_LINES]; // Offset of each line in content
    DocViewSettings settings; // Viewer settings
    bool is_file_loaded; // Flag to indicate if file has been loaded successfully
    // Title scrolling variables
    int16_t title_scroll_offset; // Current horizontal scroll position of title
    uint8_t title_scroll_direction; // 0 = left to right, 1 = right to left
    bool title_needs_scrolling; // Whether the title is wider than the display
    uint16_t title_width; // Width of the title in pixels
} DocViewModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    Widget* widget_about;
    View* text_viewer;
    VariableItemList* settings_menu;
    DialogsApp* dialogs;
    Storage* storage;
    FuriString* file_path;
    char* file_buffer;

    DocViewModel* doc_model; // Document content and state
    FuriTimer* timer; // For any timed operations
} DocViewApp;

// Forward declaration of app free function to fix ordering issues
static void docview_app_free(DocViewApp* app);

/**
 * Navigation callbacks
 */
static uint32_t docview_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

static uint32_t docview_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return DocViewViewSubmenu;
}

/**
 * Button callback for error dialog
 */
static void docview_error_ok_callback(GuiButtonType button, InputType type, void* context) {
    DocViewApp* app = context;
    if(type == InputTypeShort && button == GuiButtonTypeCenter && app != NULL) {
        view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewSubmenu);
    }
}

/**
 * Display an error message as a widget
 */
static void docview_show_error(DocViewApp* app, const char* error_text) {
    if(app == NULL) {
        return;
    }

    // Create widget to display error
    Widget* widget = widget_alloc();
    widget_add_string_element(widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, error_text);
    widget_add_button_element(widget, GuiButtonTypeCenter, "OK", docview_error_ok_callback, app);

    // Show error dialog
    view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewAbout);
    // Free the old about widget and replace it with our error widget
    view_dispatcher_remove_view(app->view_dispatcher, DocViewViewAbout);
    if(app->widget_about != NULL) {
        widget_free(app->widget_about);
    }
    app->widget_about = widget;
    view_dispatcher_add_view(app->view_dispatcher, DocViewViewAbout, widget_get_view(widget));
    view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewAbout);
}

/**
 * Submenu callbacks
 */
static void docview_submenu_callback(void* context, uint32_t index) {
    DocViewApp* app = (DocViewApp*)context;

    if(app == NULL) {
        FURI_LOG_E(TAG, "App context is NULL in submenu callback");
        return;
    }

    switch(index) {
    case DocViewSubmenuIndexBrowse:
        // Open file browser dialog
        {
            DialogsFileBrowserOptions browser_options;
            dialog_file_browser_set_basic_options(&browser_options, FILE_EXTENSION_FILTER, NULL);

            // Make sure file path is initialized
            if(app->file_path == NULL) {
                app->file_path = furi_string_alloc_set("/ext");
            }

            bool result = dialog_file_browser_show(
                app->dialogs, app->file_path, app->file_path, &browser_options);

            if(result) {
                // Make sure doc_model and file_path are initialized
                if(app->doc_model != NULL && app->doc_model->file_path != NULL) {
                    // Load selected file using set instead of duplicate
                    furi_string_set(app->doc_model->file_path, app->file_path);
                    app->doc_model->is_file_loaded = false; // Reset file loaded flag

                    // Switch to text viewer and load the file
                    view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewTextViewer);
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, DocViewEventIdLoadFile);
                } else {
                    FURI_LOG_E(TAG, "Doc model or file path is NULL");
                    docview_show_error(app, "Internal error: NULL model");
                }
            }
        }
        break;
    case DocViewSubmenuIndexSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewSettings);
        break;
    case DocViewSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewAbout);
        break;
    default:
        break;
    }
}

/**
 * Timer callback to update title scroll position
 */
static void docview_scroll_timer_callback(void* context) {
    DocViewApp* app = context;
    if(app == NULL || app->doc_model == NULL || app->text_viewer == NULL) {
        return;
    }

    // Only update if we have a file loaded and title needs scrolling
    if(app->doc_model->is_file_loaded && app->doc_model->title_needs_scrolling) {
        // Update the scroll position based on direction
        if(app->doc_model->title_scroll_direction == 0) {
            // Scrolling from left to right
            app->doc_model->title_scroll_offset++;
            // If we've scrolled to the end, change direction
            if(app->doc_model->title_scroll_offset >= 40) {
                app->doc_model->title_scroll_direction = 1;
            }
        } else {
            // Scrolling from right to left
            app->doc_model->title_scroll_offset--;
            // If we've scrolled back to start, change direction
            if(app->doc_model->title_scroll_offset <= -app->doc_model->title_width + 88) {
                app->doc_model->title_scroll_direction = 0;
            }
        }

        // Update the view to show the new scroll position
        with_view_model(
            app->text_viewer,
            DocViewModel * model,
            { model->title_scroll_offset = app->doc_model->title_scroll_offset; },
            true);
    }
}

/**
 * Load and parse a text file
 */
static bool docview_load_file(DocViewApp* app) {
    bool success = false;

    // Basic error checking
    if(app == NULL || app->doc_model == NULL || app->doc_model->file_path == NULL) {
        FURI_LOG_E(TAG, "Invalid app or doc model pointers");
        return false;
    }

    // Reset scrolling parameters when loading a new file
    app->doc_model->title_scroll_offset = 0;
    app->doc_model->title_scroll_direction = 0;
    app->doc_model->title_needs_scrolling = false;
    app->doc_model->title_width = 0;

    // Get file path
    const char* file_path = furi_string_get_cstr(app->doc_model->file_path);
    if(file_path == NULL || strlen(file_path) == 0) {
        FURI_LOG_E(TAG, "Invalid file path");
        return false;
    }

    FURI_LOG_I(TAG, "Loading file: %s", file_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage == NULL) {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return false;
    }

    File* file = storage_file_alloc(storage);
    if(file == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate file");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    if(storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Get file size
        uint64_t file_size = storage_file_size(file);
        if(file_size > 0) {
            if(file_size > MAX_FILE_SIZE) {
                file_size = MAX_FILE_SIZE; // Limit file size
                FURI_LOG_W(TAG, "File truncated to %d bytes", MAX_FILE_SIZE);
            }

            // Free old buffer if it exists
            if(app->file_buffer) {
                free(app->file_buffer);
                app->file_buffer = NULL;
            }

            // Allocate new buffer
            app->file_buffer = malloc(file_size + 1);
            if(app->file_buffer) {
                // Read file content
                uint16_t bytes_read = storage_file_read(file, app->file_buffer, file_size);
                if(bytes_read > 0) {
                    app->file_buffer[bytes_read] = '\0';

                    // Set content and length
                    furi_string_set(app->doc_model->content, app->file_buffer);
                    app->doc_model->content_length = bytes_read;

                    // Parse line positions
                    uint16_t line_count = 0;
                    app->doc_model->line_offsets[line_count++] = 0;

                    for(uint16_t i = 0; i < bytes_read && line_count < MAX_LINES; i++) {
                        if(app->file_buffer[i] == '\n') {
                            app->doc_model->line_offsets[line_count++] = i + 1;
                        }
                    }

                    // Ensure we have at least one line
                    if(line_count == 0) {
                        line_count = 1;
                    }

                    app->doc_model->total_lines = line_count;
                    app->doc_model->current_line = 0;

                    // Lines per screen based on current font size
                    switch(app->doc_model->settings.font_size) {
                    case 1: // Medium
                        app->doc_model->lines_per_screen = 6;
                        break;
                    case 2: // Large
                        app->doc_model->lines_per_screen = 4;
                        break;
                    default: // Small
                        app->doc_model->lines_per_screen = 8;
                        break;
                    }

                    app->doc_model->is_file_loaded = true;
                    success = true;
                    FURI_LOG_I(
                        TAG,
                        "File loaded successfully: %d bytes, %d lines",
                        bytes_read,
                        line_count);
                } else {
                    FURI_LOG_E(TAG, "Failed to read file or file is empty");
                }
            } else {
                FURI_LOG_E(TAG, "Failed to allocate memory for file content");
            }
        } else {
            FURI_LOG_E(TAG, "File is empty");
        }
    } else {
        FURI_LOG_E(TAG, "Failed to open file: %s", file_path);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

/**
 * Draw document text on screen
 */
static void docview_text_viewer_draw_callback(Canvas* canvas, void* model) {
    DocViewModel* doc_model = (DocViewModel*)model;
    if(doc_model == NULL) {
        FURI_LOG_E(TAG, "DocViewModel is NULL in draw callback");
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Error: NULL model");
        return;
    }

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // If file is not loaded, show message
    if(!doc_model->is_file_loaded) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Loading file...");
        return;
    }

    // Check if content exists
    if(doc_model->content == NULL || furi_string_size(doc_model->content) == 0 ||
       doc_model->total_lines == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "No content to display");
        return;
    }

    // Header with file name and position indicator
    canvas_set_font(canvas, FontPrimary);
    FuriString* header = furi_string_alloc();

    // Extract filename from path
    const char* path = furi_string_get_cstr(doc_model->file_path);
    const char* filename = strrchr(path, '/');
    if(filename) {
        filename++; // Skip the slash
    } else {
        filename = path;
    }

    // Calculate total pages (ensure we don't divide by zero)
    uint16_t total_pages = 1;
    uint16_t current_page = 1;
    if(doc_model->lines_per_screen > 0) {
        total_pages = (doc_model->total_lines + doc_model->lines_per_screen - 1) /
                      doc_model->lines_per_screen;
        current_page = doc_model->current_line / doc_model->lines_per_screen + 1;
    }

    // Show filename and position in file
    furi_string_printf(header, "%s (%d/%d)", filename, current_page, total_pages);

    // Get the title's width to determine if scrolling is needed
    canvas_set_font(canvas, FontPrimary);
    uint16_t title_width = canvas_string_width(canvas, furi_string_get_cstr(header));

    // Set title width for scrolling logic
    doc_model->title_width = title_width;

    // Determine if title needs scrolling (wider than ~100 pixels to leave room for scrollbar)
    doc_model->title_needs_scrolling = (title_width > 100);

    // Draw the title with horizontal scrolling if needed
    if(doc_model->title_needs_scrolling) {
        canvas_draw_str(
            canvas, 2 + doc_model->title_scroll_offset, 10, furi_string_get_cstr(header));
    } else {
        canvas_draw_str(canvas, 2, 10, furi_string_get_cstr(header));
    }

    furi_string_free(header);

    // Draw separator line
    canvas_draw_line(canvas, 0, 13, 128, 13);

    // Display text content
    canvas_set_font(canvas, FontSecondary);
    uint8_t y_offset = 24;
    uint8_t line_height = 10;

    const char* content = furi_string_get_cstr(doc_model->content);
    if(content == NULL) {
        FURI_LOG_E(TAG, "Content string is NULL");
        return;
    }

    for(uint16_t i = 0; i < doc_model->lines_per_screen; i++) {
        uint16_t line_idx = doc_model->current_line + i;
        if(line_idx >= doc_model->total_lines) {
            break;
        }

        // Get line content with bounds checking
        uint16_t start = 0;
        if(line_idx < doc_model->total_lines) {
            start = doc_model->line_offsets[line_idx];
        }

        uint16_t end = doc_model->content_length;
        if(line_idx + 1 < doc_model->total_lines) {
            end = doc_model->line_offsets[line_idx + 1] - 1; // -1 to exclude newline
        }

        // Ensure end is not beyond content length
        if(end > doc_model->content_length) {
            end = doc_model->content_length;
        }

        // Ensure end is greater than start
        if(end <= start) {
            continue;
        }

        // Copy line content to temporary buffer with bounds checking
        if((end - start) < MAX_LINE_LENGTH) {
            char line_buf[MAX_LINE_LENGTH];
            size_t line_len = end - start;

            if(line_len > 0 && start < doc_model->content_length) {
                // Safely copy content to line buffer
                size_t copy_len = (line_len < MAX_LINE_LENGTH - 1) ? line_len :
                                                                     MAX_LINE_LENGTH - 1;
                memcpy(line_buf, content + start, copy_len);
                line_buf[copy_len] = '\0';

                // Remove CR if present
                if(copy_len > 0 && line_buf[copy_len - 1] == '\r') {
                    line_buf[copy_len - 1] = '\0';
                }

                canvas_draw_str(canvas, 2, y_offset, line_buf);
            }
        }

        y_offset += line_height;
    }

    // Draw scrollbar if document has more than one page
    if(doc_model->total_lines > doc_model->lines_per_screen) {
        uint8_t scrollbar_x = 124;
        uint8_t scrollbar_height = 48;
        uint8_t scrollbar_y = 15;

        // Draw scrollbar background
        canvas_draw_line(
            canvas, scrollbar_x, scrollbar_y, scrollbar_x, scrollbar_y + scrollbar_height);

        // Calculate position ratio safely
        float position_ratio = 0.0f;
        if(doc_model->total_lines > doc_model->lines_per_screen) {
            position_ratio = (float)doc_model->current_line /
                             (float)(doc_model->total_lines - doc_model->lines_per_screen);
            if(position_ratio > 1.0f) position_ratio = 1.0f;
        }

        uint8_t indicator_pos = scrollbar_y + (position_ratio * scrollbar_height);
        uint8_t indicator_height = 5;
        canvas_draw_box(canvas, scrollbar_x - 1, indicator_pos, 3, indicator_height);
    }
}

/**
 * Handle input events for text viewer
 */
static bool docview_text_viewer_input_callback(InputEvent* event, void* context) {
    DocViewApp* app = (DocViewApp*)context;
    bool consumed = false;

    if(app == NULL || app->doc_model == NULL) {
        FURI_LOG_E(TAG, "App or doc_model is NULL in input callback");
        return false;
    }

    // Don't process inputs if file isn't loaded
    if(!app->doc_model->is_file_loaded) {
        return false;
    }

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            // Scroll up
            if(app->doc_model->current_line > 0) {
                app->doc_model->current_line--;
                with_view_model(
                    app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
            }
            consumed = true;
        } else if(event->key == InputKeyDown) {
            // Scroll down
            if(app->doc_model->current_line + app->doc_model->lines_per_screen <
               app->doc_model->total_lines) {
                app->doc_model->current_line++;
                with_view_model(
                    app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
            }
            consumed = true;
        } else if(event->key == InputKeyLeft) {
            // Page up
            if(app->doc_model->current_line > 0) {
                if(app->doc_model->current_line >= app->doc_model->lines_per_screen) {
                    app->doc_model->current_line -= app->doc_model->lines_per_screen;
                } else {
                    app->doc_model->current_line = 0;
                }
                with_view_model(
                    app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
            }
            consumed = true;
        } else if(event->key == InputKeyRight) {
            // Page down
            if(app->doc_model->current_line + app->doc_model->lines_per_screen <
               app->doc_model->total_lines) {
                app->doc_model->current_line += app->doc_model->lines_per_screen;
                if(app->doc_model->current_line + app->doc_model->lines_per_screen >
                   app->doc_model->total_lines) {
                    app->doc_model->current_line =
                        app->doc_model->total_lines - app->doc_model->lines_per_screen;
                }
                with_view_model(
                    app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
            }
            consumed = true;
        }
    }

    return consumed;
}

/**
 * Handle custom events for text viewer
 */
static bool docview_text_viewer_custom_event_callback(uint32_t event, void* context) {
    DocViewApp* app = (DocViewApp*)context;
    bool handled = false;

    if(app == NULL || app->doc_model == NULL || app->text_viewer == NULL) {
        FURI_LOG_E(TAG, "App, doc_model or text_viewer is NULL in custom event callback");
        return false;
    }

    switch(event) {
    case DocViewEventIdLoadFile: {
        bool success = docview_load_file(app);
        if(success) {
            with_view_model(
                app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
            handled = true;
        } else {
            // Show error if loading failed
            docview_show_error(app, "Failed to load file");
            view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewAbout);
        }
    } break;

    case DocViewEventIdNextPage:
        if(app->doc_model->is_file_loaded &&
           app->doc_model->current_line + app->doc_model->lines_per_screen <
               app->doc_model->total_lines) {
            app->doc_model->current_line += app->doc_model->lines_per_screen;
            if(app->doc_model->current_line + app->doc_model->lines_per_screen >
               app->doc_model->total_lines) {
                app->doc_model->current_line =
                    app->doc_model->total_lines - app->doc_model->lines_per_screen;
                if(app->doc_model->current_line >= app->doc_model->total_lines) {
                    app->doc_model->current_line = app->doc_model->total_lines - 1;
                }
            }
            with_view_model(
                app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
        }
        handled = true;
        break;

    case DocViewEventIdPrevPage:
        if(app->doc_model->is_file_loaded && app->doc_model->current_line > 0) {
            if(app->doc_model->current_line >= app->doc_model->lines_per_screen) {
                app->doc_model->current_line -= app->doc_model->lines_per_screen;
            } else {
                app->doc_model->current_line = 0;
            }
            with_view_model(
                app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
        }
        handled = true;
        break;
    }

    return handled;
}

// Settings for font sizes
static const char* const font_size_names[] = {"Small", "Medium", "Large"};
static void docview_font_size_changed(VariableItem* item) {
    DocViewApp* app = variable_item_get_context(item);
    if(app == NULL || app->doc_model == NULL) {
        FURI_LOG_E(TAG, "App or doc_model is NULL in font size callback");
        return;
    }

    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, font_size_names[index]);

    app->doc_model->settings.font_size = index;

    // Adjust lines per screen based on font size
    switch(index) {
    case 0: // Small
        app->doc_model->lines_per_screen = 8;
        break;
    case 1: // Medium
        app->doc_model->lines_per_screen = 6;
        break;
    case 2: // Large
        app->doc_model->lines_per_screen = 4;
        break;
    }
}

// Settings for word wrap
static const char* const word_wrap_names[] = {"Off", "On"};
static void docview_word_wrap_changed(VariableItem* item) {
    DocViewApp* app = variable_item_get_context(item);
    if(app == NULL || app->doc_model == NULL) {
        FURI_LOG_E(TAG, "App or doc_model is NULL in word wrap callback");
        return;
    }

    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, word_wrap_names[index]);

    app->doc_model->settings.word_wrap = (index == 1);
}

/**
 * Allocate and initialize the application
 */
static DocViewApp* docview_app_alloc() {
    DocViewApp* app = malloc(sizeof(DocViewApp));
    if(app == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate app memory");
        return NULL;
    }

    // Initialize with NULL to avoid undefined behavior if allocation fails
    app->view_dispatcher = NULL;
    app->notifications = NULL;
    app->submenu = NULL;
    app->widget_about = NULL;
    app->text_viewer = NULL;
    app->settings_menu = NULL;
    app->dialogs = NULL;
    app->storage = NULL;
    app->file_path = NULL;
    app->file_buffer = NULL;
    app->doc_model = NULL;
    app->timer = NULL;

    // Open required records
    Gui* gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);

    // Initialize the view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    // Allocate file path string
    app->file_path = furi_string_alloc_set("/ext");
    app->file_buffer = NULL;

    // Initialize document model
    app->doc_model = malloc(sizeof(DocViewModel));
    if(app->doc_model == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate doc_model memory");
        docview_app_free(app);
        return NULL;
    }

    app->doc_model->file_path = furi_string_alloc();
    app->doc_model->content = furi_string_alloc();
    app->doc_model->content_length = 0;
    app->doc_model->current_line = 0;
    app->doc_model->total_lines = 0;
    app->doc_model->lines_per_screen = 8;
    app->doc_model->settings.font_size = 0;
    app->doc_model->settings.word_wrap = false;
    app->doc_model->settings.scroll_speed = 0;
    app->doc_model->is_file_loaded = false; // Initialize as not loaded
    // Initialize title scrolling variables
    app->doc_model->title_scroll_offset = 0;
    app->doc_model->title_scroll_direction = 0;
    app->doc_model->title_needs_scrolling = false;
    app->doc_model->title_width = 0;

    // Initialize main menu
    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Browse Files", DocViewSubmenuIndexBrowse, docview_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Settings", DocViewSubmenuIndexSettings, docview_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", DocViewSubmenuIndexAbout, docview_submenu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), docview_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, DocViewViewSubmenu, submenu_get_view(app->submenu));

    // Initialize text viewer - View* type
    app->text_viewer = view_alloc();
    view_set_context(app->text_viewer, app);
    view_set_draw_callback(app->text_viewer, docview_text_viewer_draw_callback);
    view_set_input_callback(app->text_viewer, docview_text_viewer_input_callback);
    view_set_previous_callback(app->text_viewer, docview_navigation_submenu_callback);
    view_set_custom_callback(app->text_viewer, docview_text_viewer_custom_event_callback);
    view_allocate_model(app->text_viewer, ViewModelTypeLocking, sizeof(DocViewModel));

    // Initialize the model safely
    with_view_model(
        app->text_viewer,
        DocViewModel * model,
        {
            model->file_path = furi_string_alloc();
            model->content = furi_string_alloc();
            model->content_length = 0;
            model->current_line = 0;
            model->total_lines = 0;
            model->lines_per_screen = 8;
            model->settings.font_size = 0;
            model->settings.word_wrap = false;
            model->settings.scroll_speed = 0;
            model->is_file_loaded = false;
            // Initialize title scrolling variables
            model->title_scroll_offset = 0;
            model->title_scroll_direction = 0;
            model->title_needs_scrolling = false;
            model->title_width = 0;
        },
        true);

    view_dispatcher_add_view(app->view_dispatcher, DocViewViewTextViewer, app->text_viewer);

    // Initialize settings menu
    app->settings_menu = variable_item_list_alloc();

    // Font size setting
    VariableItem* item = variable_item_list_add(
        app->settings_menu,
        "Font Size",
        3, // small, medium, large
        docview_font_size_changed,
        app);
    variable_item_set_current_value_index(item, app->doc_model->settings.font_size);
    variable_item_set_current_value_text(
        item, font_size_names[app->doc_model->settings.font_size]);

    // Word wrap setting
    item = variable_item_list_add(
        app->settings_menu,
        "Word Wrap",
        2, // on/off
        docview_word_wrap_changed,
        app);
    variable_item_set_current_value_index(item, app->doc_model->settings.word_wrap ? 1 : 0);
    variable_item_set_current_value_text(
        item, word_wrap_names[app->doc_model->settings.word_wrap ? 1 : 0]);

    view_set_previous_callback(
        variable_item_list_get_view(app->settings_menu), docview_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        DocViewViewSettings,
        variable_item_list_get_view(app->settings_menu));

    // Initialize about screen
    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "Doc Viewer Lite\n"
        "Version 0.1\n"
        "--Made by: C0d3-5t3w--\n"
        "A simple document viewer\n"
        "for Flipper Zero.\n\n"
        "Controls:\n"
        "Up/Down: Scroll line by line\n"
        "Left/Right: Page up/down\n"
        "Back: Return to menu");
    view_set_previous_callback(
        widget_get_view(app->widget_about), docview_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, DocViewViewAbout, widget_get_view(app->widget_about));

    // Open and configure notifications
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
#endif

    // Create timer for title scrolling
    app->timer = furi_timer_alloc(docview_scroll_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 200); // Update every 200ms

    // Start with the main menu
    view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewSubmenu);

    return app;
}

/**
 * Free application resources
 */
static void docview_app_free(DocViewApp* app) {
    if(app == NULL) {
        return;
    }

#ifdef BACKLIGHT_ON
    if(app->notifications) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
        furi_record_close(RECORD_NOTIFICATION);
    }
#endif

    // Stop and free the timer
    if(app->timer != NULL) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }

    // Free views if they exist
    if(app->view_dispatcher) {
        if(app->submenu) {
            view_dispatcher_remove_view(app->view_dispatcher, DocViewViewSubmenu);
            submenu_free(app->submenu);
        }

        if(app->text_viewer) {
            view_dispatcher_remove_view(app->view_dispatcher, DocViewViewTextViewer);
            view_free(app->text_viewer);
        }

        if(app->settings_menu) {
            view_dispatcher_remove_view(app->view_dispatcher, DocViewViewSettings);
            variable_item_list_free(app->settings_menu);
        }

        if(app->widget_about) {
            view_dispatcher_remove_view(app->view_dispatcher, DocViewViewAbout);
            widget_free(app->widget_about);
        }
    }

    // Free file resources
    if(app->file_buffer) {
        free(app->file_buffer);
    }

    // Free document model
    if(app->doc_model) {
        if(app->doc_model->file_path) {
            furi_string_free(app->doc_model->file_path);
        }
        if(app->doc_model->content) {
            furi_string_free(app->doc_model->content);
        }
        free(app->doc_model);
    }

    // Free file path
    if(app->file_path) {
        furi_string_free(app->file_path);
    }

    // Free view dispatcher
    if(app->view_dispatcher) {
        view_dispatcher_free(app->view_dispatcher);
    }

    // Close records
    if(app->storage) {
        furi_record_close(RECORD_STORAGE);
    }
    if(app->dialogs) {
        furi_record_close(RECORD_DIALOGS);
    }
    furi_record_close(RECORD_GUI);

    // Free the app
    free(app);
}

/**
 * Application entry point
 */
int32_t docviewlite_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "Doc Viewer Lite starting");

    DocViewApp* app = docview_app_alloc();
    if(app == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate app");
        return -1;
    }

    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "Doc Viewer Lite cleaning up");
    docview_app_free(app);

    return 0;
}
