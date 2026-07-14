/* Thanks to jamisonderek for all his tutorials, videos and specially for the
 * Basic scenes tutorial:
 * https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/ui/basic_scenes
 * Because its basically this entire app with a few tweaks
 *
 * Thanks to instantiator.dev for his tutorial to create applications for the
 * flipper zero: https://instantiator.dev/post/flipper-zero-app-tutorial-01/
 *
 * Thanks to Ztuu for the rot13 code:
 * https://gist.github.com/Ztuu/e9106e9095422a7d7266653f1e156366
 *
 * Created by: @nothingbutlucas
 */
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Diffrent app scenes
typedef enum {
    MainMenuScene,
    Rot13InputScene,
    Rot13CipherMessageScene,
    ReadmeScene,
    Rot13SceneCount,
} Rot13Scene;

// View references
typedef enum {
    Rot13SubmenuView,
    Rot13WidgetView,
    Rot13TextInputView,
    Rot13TextBoxView,
} Rot13View;

// App object

typedef struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    TextBox* text_box;
    char* rot13_text;
    uint8_t rot13_text_size;
} App;

// Refence to item menus. Avoid magic numbers
typedef enum {
    MainMenuSceneCipherToRot13,
    MainMenuSceneReadme,
} MainMenuSceneIndex;

// Reference to custom events. Avoid magic numbers
typedef enum {
    MainMenuSceneCipherToRot13Event,
    MainMenuSceneReadmeEvent,
} Rot13MainMenuEvent;

typedef enum {
    Rot13CipherInputSceneSaveEvent,
} Rot13CipherInputEvent;

// ROT13 Function in C
// Borrowed from https://gist.github.com/Ztuu/e9106e9095422a7d7266653f1e156366

char* rot13(const char* src) {
    if(src == NULL) {
        return NULL;
    }

    char* result = malloc(strlen(src) + 1);

    if(result != NULL) {
        strcpy(result, src);
        char* current_char = result;

        while(*current_char != '\0') {
            // Only increment alphabet characters
            if((*current_char >= 97 && *current_char <= 122) ||
               (*current_char >= 65 && *current_char <= 90)) {
                if(*current_char > 109 || (*current_char > 77 && *current_char < 91)) {
                    // Characters that wrap around to the start of the alphabet
                    *current_char -= 13;
                } else {
                    // Characters that can be safely incremented
                    *current_char += 13;
                }
            }
            current_char++;
        }
    }
    return result;
}

// Function for stub menu
void rot13_menu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
    case MainMenuSceneReadme:
        scene_manager_handle_custom_event(app->scene_manager, MainMenuSceneReadmeEvent);
        break;
    case MainMenuSceneCipherToRot13:
        scene_manager_handle_custom_event(app->scene_manager, MainMenuSceneCipherToRot13Event);
        break;
    }
}

// Functions for every scene
// Every scene must have on_enter, on_event and on_exit
void rot13_main_menu_scene_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ROT13 Cipher");
    submenu_add_item(
        app->submenu, "Cipher with ROT13", MainMenuSceneCipherToRot13, rot13_menu_callback, app);
    submenu_add_item(app->submenu, "Readme", MainMenuSceneReadme, rot13_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, Rot13SubmenuView);
}

bool rot13_main_menu_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case MainMenuSceneReadmeEvent:
            scene_manager_next_scene(app->scene_manager, ReadmeScene);
            consumed = true;
            break;
        case MainMenuSceneCipherToRot13Event:
            scene_manager_next_scene(app->scene_manager, Rot13InputScene);
            consumed = true;
            break;
        }
        break;
    default:
        break;
    }
    return consumed;
}

void main_menu_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

void text_input_callback(void* context) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, Rot13CipherInputSceneSaveEvent);
}
void rot13_input_scene_on_enter(void* context) {
    App* app = context;
    bool clear_text = true;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter text to cipher");
    text_input_set_result_callback(
        app->text_input,
        text_input_callback,
        app,
        app->rot13_text,
        app->rot13_text_size,
        clear_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, Rot13TextInputView);
}
bool rot13_greeting_input_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == Rot13CipherInputSceneSaveEvent) {
            scene_manager_next_scene(app->scene_manager, Rot13CipherMessageScene);
            consumed = true;
        }
    }
    return consumed;
}
void rot13_greeting_input_scene_on_exit(void* context) {
    UNUSED(context);
}

void transform_rot13_on_enter(void* context) {
    // Aca deberiamos transformar el texto a rot13
    App* app = context;
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, rot13(app->rot13_text));
    view_dispatcher_switch_to_view(app->view_dispatcher, Rot13TextBoxView);
}

bool rot13_greeting_message_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; // event not handled.
}
void rot13_greeting_message_scene_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

void readme_scene_on_enter(void* context) {
    App* app = context;
    text_box_reset(app->text_box);
    text_box_set_text(
        app->text_box,
        "ROT13 is a simple letter substitution cipher that replaces a letter "
        "with the 13th "
        "letter after it in the Latin alphabet.\n"
        "For example:\n"
        "HELLO = URYYB = HELLO\n"
        "So ROT13 works for \"cipher\" and \"decipher\" at the same time.\n"
        "Source: Wikipedia\nThis application was developed by "
        "@nothingbutlucas.\nSee the repo on codeberg:\n"
        "codeberg.org/nothingbutlucas/flipperzero_rot13");
    view_dispatcher_switch_to_view(app->view_dispatcher, Rot13TextBoxView);
}

bool readme_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; // event not handled.
}

void readme_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

// Arrays for the handlers

void (*const rot13_scene_on_enter_handlers[])(void*) = {
    rot13_main_menu_scene_on_enter,
    rot13_input_scene_on_enter,
    transform_rot13_on_enter,
    readme_scene_on_enter,
};

bool (*const rot13_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    rot13_main_menu_scene_on_event,
    rot13_greeting_input_scene_on_event,
    rot13_greeting_message_scene_on_event,
    readme_scene_on_event,
};

void (*const rot13_scene_on_exit_handlers[])(void*) = {
    main_menu_scene_on_exit,
    rot13_greeting_input_scene_on_exit,
    rot13_greeting_message_scene_on_exit,
    readme_scene_on_exit,
};

static const SceneManagerHandlers rot13_scene_manager_handlers = {
    .on_enter_handlers = rot13_scene_on_enter_handlers,
    .on_event_handlers = rot13_scene_on_event_handlers,
    .on_exit_handlers = rot13_scene_on_exit_handlers,
    .scene_num = Rot13SceneCount,
};

// This function is called when a custom event happens
// This event can be triggered when some pin is connected, a timer, etc
static bool basic_scene_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

// This is the function for the back button

bool basic_scene_back_event_callback(void* context) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// Alloc for our app
// This is for allocate the memory of our app
static App* app_alloc() {
    App* app = malloc(sizeof(App));
    app->rot13_text_size = 128;
    app->rot13_text = malloc(app->rot13_text_size);
    app->scene_manager = scene_manager_alloc(&rot13_scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, basic_scene_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, basic_scene_back_event_callback);
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Rot13SubmenuView, submenu_get_view(app->submenu));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, Rot13WidgetView, widget_get_view(app->widget));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Rot13TextInputView, text_input_get_view(app->text_input));
    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Rot13TextBoxView, text_box_get_view(app->text_box));
    return app;
}

// For free the memory of the app
static void app_free(App* app) {
    furi_assert(app);
    view_dispatcher_remove_view(app->view_dispatcher, Rot13SubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, Rot13WidgetView);
    view_dispatcher_remove_view(app->view_dispatcher, Rot13TextInputView);
    view_dispatcher_remove_view(app->view_dispatcher, Rot13TextBoxView);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    widget_free(app->widget);
    text_input_free(app->text_input);
    text_box_free(app->text_box);
    free(app);
}

int32_t rot13_main(void* p) {
    UNUSED(p);
    App* app = app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, MainMenuScene);
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    return 0;
}
