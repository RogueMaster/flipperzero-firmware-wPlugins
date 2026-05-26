#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include "VolorSavanna.h"

int is_choice = 0;

bool is_digit(char digit)
{
    return digit >= '0' && digit <= '9';
}

typedef struct {
    ViewDispatcher * view_dispatcher;
    SceneManager * scene_manager;
    TextInput * text_input;
    TextBox * text_box;

    char input_buffer[16];
    FuriString * display_text;
} App;

enum {
    AppViewTextInput, 
    AppViewTextBox, 
};

static void text_input_callback(void * context)
{
    App * app = context;

    if (is_choice == 0)
    {
        // Grab the players name and store it
        strcpy(name, app->input_buffer);

        // Stop game upon user request
        if (strcmp(app->input_buffer, "E") == 0)
        {
            view_dispatcher_stop(app->view_dispatcher);
        }

        // Clear input biffer
        for (int i = 0; i < 16; i++)
        {
            app->input_buffer[i] = '\0';
        }
        
        // Set the input field header
        text_input_set_header_text(app->text_input, "Make your choice:");

        is_choice = 1;
    }

    if (is_choice == 1)
    {
        // Stop game upon user request
        if (strcmp(app->input_buffer, "E") == 0)
        {
            view_dispatcher_stop(app->view_dispatcher);
        }

        // Clear input biffer
        for (int i = 0; i < 16; i++)
        {
            app->input_buffer[i] = '\0';
        }
        
        // Update display text
        furi_string_set(app->display_text, character_prompt);

        // Push text into text box
        text_box_set_text(app->text_box, furi_string_get_cstr(app->display_text));
        
        // Switch to display view
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewTextBox);
        
        is_choice = 2;
    }

    else
    {   if (is_choice == 2)
        {
            strcpy(character, app->input_buffer);

            // Stop game upon user request
            if (strcmp(app->input_buffer, "E") == 0)
            {
                view_dispatcher_stop(app->view_dispatcher);
            }

            // Clear input biffer
            for (int i = 0; i < 16; i++)
            {
                app->input_buffer[i] = '\0';
            }

            is_choice = 3;
        }

        if (is_choice == 3)
        {
            strcpy(choice, app->input_buffer);

            // Stop game upon user request
            if (strcmp(app->input_buffer, "E") == 0)
            {
                view_dispatcher_stop(app->view_dispatcher);
            }

            // Clear input biffer
            for (int i = 0; i < 16; i++)
            {
                app->input_buffer[i] = '\0';
            }

            // Update display text
            furi_string_set(app->display_text, VolorSavannaGame());

            // Push text into text box
            text_box_set_text(app->text_box, furi_string_get_cstr(app->display_text));
            
            // Switch to display view
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewTextBox);
        }
    }
}

bool text_box_back_callback(void * context)
{
    App * app = context;

    if (level == 0 && is_choice == 3)
    {
        text_input_set_header_text(app->text_input, name_prompt);
        is_choice = 0;
    }
    
    // Change the view
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewTextInput);

    return true;
}

int32_t VolorSavanna(void * p)
{
    UNUSED(p);

    // Allocating memory
    App * app = malloc(sizeof(App));
    app->display_text = furi_string_alloc();

    // Setup
    Gui * gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, text_box_back_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // Choice View
    app->text_input = text_input_alloc();
    text_input_set_header_text(app->text_input, name_prompt);
    text_input_set_result_callback(app->text_input, text_input_callback, app, app->input_buffer, sizeof(app->input_buffer), true);
    view_dispatcher_add_view(app->view_dispatcher, AppViewTextInput, text_input_get_view(app->text_input));

    // Adventure View
    app->text_box = text_box_alloc();
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    view_dispatcher_add_view(app->view_dispatcher, AppViewTextBox, text_box_get_view(app->text_box));

    // Start Game
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewTextInput);
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    view_dispatcher_remove_view(app->view_dispatcher, AppViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewTextInput);
    view_dispatcher_free(app->view_dispatcher);
    text_input_free(app->text_input);
    free(app);
    furi_record_close(RECORD_GUI);
    
    return 0;
}