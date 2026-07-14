#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>
#include <loader/loader.h>
#include <toolbox/path.h>
#include <ctype.h>
#include <stdlib.h>

typedef enum {
    SmartraMainMenuScene,
    SmartraVinInputScene,
    SmartraPinMessageScene,
    SmartraAboutScene,
    SmartraSceneCount,
} SmartraScene;

typedef enum {
    SmartraSubmenuView,
    SmartraWidgetView,
    SmartraTextInputView,
} SmartraView;

typedef struct SmartraApp {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    char* vin;
    uint8_t vin_size;
} SmartraApp;

typedef enum {
    SmartraMainMenuSceneVinInput,
    SmartraMainMenuSceneAbout,
} SmartraMainMenuSceneIndex;

typedef enum {
    SmartraMainMenuSceneVinInputEvent,
    SmartraMainMenuSceneAboutEvent,
} SmartraMainMenuEvent;

typedef enum {
    SmartraVinInputSceneSaveEvent,
} SmartraVinInputEvent;

typedef enum {
    SmartraPinMessageSceneSaveEvent,
} SmartraPinMessageEvent;

uint32_t calculate_smartra_pin(uint32_t last_six) {
    uint32_t output = last_six;
    for(int i = 0; i < 40; i++) {
        int msb_set = (output >> 31) & 1;
        output <<= 1;
        if(msb_set) {
            uint16_t new_msb = (output >> 16) ^ 0x7798;
            uint16_t new_lsb = (output & 0xFFFF) ^ 0x2990;
            output = ((uint32_t)new_msb << 16) | new_lsb;
        }
    }
    return output % 1000000;
}

void smartra_text_input_callback(void* context);
static bool smartra_custom_callback(void* context, uint32_t custom_event);
bool smartra_back_event_callback(void* context);

void smartra_menu_callback(void* context, uint32_t index) {
    SmartraApp* app = context;
    switch(index) {
    case SmartraMainMenuSceneVinInput:
        scene_manager_handle_custom_event(app->scene_manager, SmartraMainMenuSceneVinInputEvent);
        break;
    case SmartraMainMenuSceneAbout:
        scene_manager_handle_custom_event(app->scene_manager, SmartraMainMenuSceneAboutEvent);
        break;
    }
}

static bool smartra_pin_message_input_callback(InputEvent* event, void* context) {
    SmartraApp* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        scene_manager_handle_custom_event(app->scene_manager, SmartraPinMessageSceneSaveEvent);
        return true;
    }
    return false;
}

void smartra_main_menu_scene_on_enter(void* context) {
    SmartraApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Smartra VIN2PIN");
    submenu_add_item(
        app->submenu, "Enter VIN", SmartraMainMenuSceneVinInput, smartra_menu_callback, app);
    submenu_add_item(app->submenu, "About", SmartraMainMenuSceneAbout, smartra_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SmartraSubmenuView);
}

bool smartra_main_menu_scene_on_event(void* context, SceneManagerEvent event) {
    SmartraApp* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case SmartraMainMenuSceneVinInputEvent:
            scene_manager_next_scene(app->scene_manager, SmartraVinInputScene);
            consumed = true;
            break;
        case SmartraMainMenuSceneAboutEvent:
            scene_manager_next_scene(app->scene_manager, SmartraAboutScene);
            consumed = true;
            break;
        }
        break;
    default:
        break;
    }
    return consumed;
}

void smartra_main_menu_scene_on_exit(void* context) {
    SmartraApp* app = context;
    submenu_reset(app->submenu);
}

void smartra_text_input_callback(void* context) {
    SmartraApp* app = context;

    if(strlen(app->vin) > 17) {
        app->vin[17] = '\0';
    }

    for(int i = 0; app->vin[i] != '\0'; i++) {
        app->vin[i] = toupper(app->vin[i]);
    }

    scene_manager_handle_custom_event(app->scene_manager, SmartraVinInputSceneSaveEvent);
}

void smartra_vin_input_scene_on_enter(void* context) {
    SmartraApp* app = context;
    bool clear_text = true;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter 17-character VIN");

    text_input_set_result_callback(
        app->text_input, smartra_text_input_callback, app, app->vin, app->vin_size, clear_text);

    app->vin[17] = '\0';
    view_dispatcher_switch_to_view(app->view_dispatcher, SmartraTextInputView);
}

bool smartra_vin_input_scene_on_event(void* context, SceneManagerEvent event) {
    SmartraApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SmartraVinInputSceneSaveEvent) {
            scene_manager_next_scene(app->scene_manager, SmartraPinMessageScene);
            consumed = true;
        }
    }
    return consumed;
}

void smartra_vin_input_scene_on_exit(void* context) {
    UNUSED(context);
}

static void smartra_save_result(SmartraApp* app) {
    const char* vin = app->vin;
    int len = strlen(vin);
    if(len != 17) return;

    const char* last_six_ptr = vin + 11;
    for(int i = 0; i < 6; i++) {
        if(!isdigit((unsigned char)last_six_ptr[i])) return;
    }

    uint32_t last_six = atoi(last_six_ptr);
    uint32_t pin = calculate_smartra_pin(last_six);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    const char* file_path = APP_DATA_PATH("results.txt");

    FuriString* dir_path = furi_string_alloc();
    path_extract_dirname(file_path, dir_path);
    storage_common_mkdir(storage, furi_string_get_cstr(dir_path));
    furi_string_free(dir_path);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, file_path, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FuriString* line = furi_string_alloc();
        furi_string_printf(line, "VIN: %s, PIN: %06lu\n", vin, pin);
        storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
        furi_string_free(line);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void smartra_pin_message_scene_on_enter(void* context) {
    SmartraApp* app = context;
    widget_reset(app->widget);
    FuriString* message = furi_string_alloc();

    const char* vin = app->vin;
    int len = strlen(vin);
    if(len != 17) {
        furi_string_printf(message, "Invalid VIN length!\nMust be 17 characters");
        widget_add_string_multiline_element(
            app->widget,
            5,
            30,
            AlignLeft,
            AlignCenter,
            FontSecondary,
            furi_string_get_cstr(message));
    } else {
        const char* last_six_ptr = vin + 11;
        bool all_digits = true;
        for(int i = 0; i < 6; i++) {
            if(!isdigit((unsigned char)last_six_ptr[i])) {
                all_digits = false;
                break;
            }
        }

        if(!all_digits) {
            furi_string_printf(message, "Last 6 must be digits!");
            widget_add_string_multiline_element(
                app->widget,
                5,
                30,
                AlignLeft,
                AlignCenter,
                FontSecondary,
                furi_string_get_cstr(message));
        } else {
            uint32_t last_six = atoi(last_six_ptr);
            uint32_t pin = calculate_smartra_pin(last_six);
            furi_string_printf(message, "VIN: %s\nPIN: %06lu", vin, pin);
            widget_add_string_multiline_element(
                app->widget,
                5,
                30,
                AlignLeft,
                AlignCenter,
                FontSecondary,
                furi_string_get_cstr(message));
        }
    }

    furi_string_free(message);

    View* widget_view = widget_get_view(app->widget);
    view_set_input_callback(widget_view, smartra_pin_message_input_callback);
    view_set_context(widget_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SmartraWidgetView);
}

bool smartra_pin_message_scene_on_event(void* context, SceneManagerEvent event) {
    SmartraApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == SmartraPinMessageSceneSaveEvent) {
        smartra_save_result(app);
        consumed = true;
    }
    return consumed;
}

void smartra_pin_message_scene_on_exit(void* context) {
    SmartraApp* app = context;
    widget_reset(app->widget);
}

void smartra_about_scene_on_enter(void* context) {
    SmartraApp* app = context;
    widget_reset(app->widget);

    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "Smartra VIN2PIN\nVersion 2.0\n\n"
        "Extremely simple calculator\nfor SMARTRA2\nimmobilizer pins.\n"
        "Compatible with all\n"
        "Hyundai and Kia models\n"
        "with SMARTRA2\n\n"
        "Instructions:\n"
        "1. Enter full 17-digit VIN\n"
        "2. Calculates security PIN\n"
        "3. Press OK to save result\n\n"
        "Warning:\n"
        "For educational purposes only\n\n"
        "Author: evillero\n"
        "www.github.com/evillero\n");

    view_dispatcher_switch_to_view(app->view_dispatcher, SmartraWidgetView);
}

bool smartra_about_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void smartra_about_scene_on_exit(void* context) {
    SmartraApp* app = context;
    widget_reset(app->widget);
}

void (*const smartra_scene_on_enter_handlers[])(void*) = {
    smartra_main_menu_scene_on_enter,
    smartra_vin_input_scene_on_enter,
    smartra_pin_message_scene_on_enter,
    smartra_about_scene_on_enter,
};

bool (*const smartra_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    smartra_main_menu_scene_on_event,
    smartra_vin_input_scene_on_event,
    smartra_pin_message_scene_on_event,
    smartra_about_scene_on_event,
};

void (*const smartra_scene_on_exit_handlers[])(void*) = {
    smartra_main_menu_scene_on_exit,
    smartra_vin_input_scene_on_exit,
    smartra_pin_message_scene_on_exit,
    smartra_about_scene_on_exit,
};

static const SceneManagerHandlers smartra_scene_manager_handlers = {
    .on_enter_handlers = smartra_scene_on_enter_handlers,
    .on_event_handlers = smartra_scene_on_event_handlers,
    .on_exit_handlers = smartra_scene_on_exit_handlers,
    .scene_num = SmartraSceneCount,
};

static bool smartra_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    SmartraApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

bool smartra_back_event_callback(void* context) {
    furi_assert(context);
    SmartraApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static SmartraApp* smartra_app_alloc() {
    SmartraApp* app = malloc(sizeof(SmartraApp));
    app->vin_size = 18;
    app->vin = malloc(app->vin_size);
    app->scene_manager = scene_manager_alloc(&smartra_scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, smartra_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, smartra_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SmartraSubmenuView, submenu_get_view(app->submenu));
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SmartraWidgetView, widget_get_view(app->widget));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SmartraTextInputView, text_input_get_view(app->text_input));

    return app;
}

static void smartra_app_free(SmartraApp* app) {
    furi_assert(app);
    view_dispatcher_remove_view(app->view_dispatcher, SmartraSubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, SmartraWidgetView);
    view_dispatcher_remove_view(app->view_dispatcher, SmartraTextInputView);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    widget_free(app->widget);
    text_input_free(app->text_input);
    free(app->vin);
    free(app);
}

int32_t smartra_app(void* p) {
    UNUSED(p);
    SmartraApp* app = smartra_app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, SmartraMainMenuScene);
    view_dispatcher_run(app->view_dispatcher);

    smartra_app_free(app);
    return 0;
}
