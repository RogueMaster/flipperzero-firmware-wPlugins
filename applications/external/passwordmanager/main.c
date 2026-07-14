#include "main.h"
#include "views/saved_passwords.h"
#include "views/delete_password.h"
#include "textInput/textInput.h"
#include "passwordStorage/passwordStorage.h"

static void render_main_menu(Canvas* canvas, void* model) {
    if(!model) {
        FURI_LOG_E("Password Manager", "render_main_menu: model is NULL!");
        return;
    }

    AppContext** model_ = model;
    AppContext* app = *model_;

    FURI_LOG_I("Password Manager", "Context in render: %p", app);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    // Draw header
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 20, 10, "Password Manager");
    canvas_draw_line(canvas, 0, 12, 128, 12);

    for(size_t i = 0; i < MENU_ITEMS; i++) {
        int y = 25 + i * 12;

        if(i == app->selected) {
            // Highlight selected item with black box
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 10, 128, 12);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_str(canvas, 5, y, app->items[i]);
    }
}

static bool handle_main_menu_input(InputEvent* event, void* context) {
    AppContext* app = context;

    if(!context) {
        FURI_LOG_E("Password Manager", "handle_main_menu_input: context is NULL!");
        return false;
    }

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyUp) {
            if(app->selected > 0) app->selected--;
            return true;
        } else if(event->key == InputKeyDown) {
            if(app->selected + 1 < MENU_ITEMS) app->selected++;
            return true;
        } else if(event->key == InputKeyBack) {
            app->running = false;
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        } else if(event->key == InputKeyOk) {
            if(app->selected == 0) {
                // Show stored credentials
                app->credentials_number =
                    read_passwords_from_file(PASS_SAVE_PATH, app->credentials);
                view_dispatcher_switch_to_view(app->view_dispatcher, ViewSavedPasswords);
                return true;
            } else if(app->selected == 1) {
                app->credentials_number =
                    read_passwords_from_file(PASS_SAVE_PATH, app->credentials);
                // Add password flow
                view_dispatcher_switch_to_view(app->view_dispatcher, ViewTextInputCredentialName);
                app->selected = 0;
                app->scroll_offset = 0;
                return true;
            } else if(app->selected == 2) {
                app->credentials_number =
                    read_passwords_from_file(PASS_SAVE_PATH, app->credentials);
                // Delete password view
                view_dispatcher_switch_to_view(app->view_dispatcher, ViewDeletePassword);
                app->selected = 0;
                app->scroll_offset = 0;
                return true;
            }
        }
    }

    return false;
}

// Custom callback for the view dispatcher
static bool view_dispatcher_navigation_event_callback(void* context) {
    AppContext* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMainMenu);
    return true;
}

int32_t password_manager_app(void* p) {
    UNUSED(p);

    // Initialize the app context
    AppContext* app = malloc(sizeof(AppContext));

    if(!app) {
        // Handle allocation failure
        return -1;
    }

    // Initialize app context fields to avoid nullptr issues
    memset(app, 0, sizeof(AppContext));

    app->selected = 0;
    app->scroll_offset = 0;
    app->running = true;
    app->confirm_delete = false;

    app->items[0] = "Saved passwords";
    app->items[1] = "Add new password";
    app->items[2] = "Delete password";

    app->credentials_number =
        read_passwords_from_file("/ext/passwordManager.txt", app->credentials);

    // Initialize GUI and View Dispatcher
    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) {
        free(app);
        return -2;
    }

    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        furi_record_close(RECORD_GUI);
        free(app);
        return -3;
    }

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, view_dispatcher_navigation_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize Main Menu View
    app->main_menu_view = view_alloc();
    if(!app->main_menu_view) {
        view_dispatcher_free(app->view_dispatcher);
        furi_record_close(RECORD_GUI);
        free(app);
        return -4;
    }

    view_set_context(app->main_menu_view, app);
    view_allocate_model(app->main_menu_view, ViewModelTypeLockFree, sizeof(AppContext*));
    AppContext** app_view = view_get_model(app->main_menu_view);
    *app_view = app;

    view_set_draw_callback(app->main_menu_view, render_main_menu);
    view_set_input_callback(app->main_menu_view, handle_main_menu_input);

    view_dispatcher_add_view(app->view_dispatcher, ViewMainMenu, app->main_menu_view);

    app->saved_passwords_view = saved_passwords_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, ViewSavedPasswords, app->saved_passwords_view);

    app->delete_password_view = delete_password_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, ViewDeletePassword, app->delete_password_view);

    app->textInput_credential_name = credential_name_TextInput_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        ViewTextInputCredentialName,
        text_input_get_view(app->textInput_credential_name));

    app->textInput_username = credential_username_TextInput_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewTextInputUsername, text_input_get_view(app->textInput_username));

    app->textInput_password = credential_password_TextInput_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewTextInputPassword, text_input_get_view(app->textInput_password));

    // Start with main menu
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMainMenu);

    // Main loop - just run the view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Free all resources when app exits
    view_dispatcher_remove_view(app->view_dispatcher, ViewMainMenu);
    view_free(app->main_menu_view);
    view_dispatcher_remove_view(app->view_dispatcher, ViewSavedPasswords);
    view_free(app->saved_passwords_view);
    view_dispatcher_remove_view(app->view_dispatcher, ViewDeletePassword);
    view_free(app->delete_password_view);
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputCredentialName);
    view_free(text_input_get_view(app->textInput_credential_name));
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputUsername);
    view_free(text_input_get_view(app->textInput_username));
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputPassword);
    view_free(text_input_get_view(app->textInput_password));

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);

    return 0;
}
