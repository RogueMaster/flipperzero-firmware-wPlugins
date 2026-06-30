#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <flipper_format/flipper_format.h>

#define TAG          "ID"
#define ID_SAVE_PATH EXT_PATH("id.txt")
#define FIELD_SIZE   64

typedef enum {
    IDScenesMainMenuScene,
    IDScenesIDCardScene,
    IDScenesInputNameScene,
    IDScenesInputEmailScene,
    IDScenesInputTelScene,
    IDScenesInputAddrScene,
    IDScenesInputNotesScene,
    IDScenesGreetingMessageScene,
    IDScenesAboutScene,
    IDScenesSceneCount,
} IDScenesScene;

typedef enum {
    IDScenesSubmenuView,
    IDScenesWidgetView,
    IDScenesTextInputView,
} IDScenesView;

typedef struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    FuriString* name;
    FuriString* email;
    FuriString* tel;
    FuriString* addr;
    FuriString* notes;
    char temp_buffer[FIELD_SIZE];
} App;

typedef enum {
    IDScenesMainMenuSceneIDCard,
    IDScenesMainMenuSceneSetup,
    IDScenesMainMenuSceneAbout,
} IDScenesMainMenuSceneIndex;

typedef enum {
    IDScenesMainMenuSceneIDCardEvent,
    IDScenesMainMenuSceneSetupEvent,
} IDScenesMainMenuEvent;

typedef enum {
    IDScenesInputNameEvent,
    IDScenesInputEmailEvent,
    IDScenesInputTelEvent,
    IDScenesInputAddrEvent,
    IDScenesInputNotesEvent,
    IDScenesGreetingInputSceneSaveEvent,
} IDScenesInputEvent;

void save_data(App* app) {
    FlipperFormat* file = flipper_format_file_alloc(furi_record_open(RECORD_STORAGE));
    if(flipper_format_file_open_always(file, ID_SAVE_PATH)) {
        flipper_format_write_header_cstr(file, "ID Data", 1);
        flipper_format_write_string(file, "Name", app->name);
        flipper_format_write_string(file, "Email", app->email);
        flipper_format_write_string(file, "Tel", app->tel);
        flipper_format_write_string(file, "Addr", app->addr);
        flipper_format_write_string(file, "Notes", app->notes);
    }
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

void load_data(App* app) {
    FlipperFormat* file = flipper_format_file_alloc(furi_record_open(RECORD_STORAGE));
    FuriString* value = furi_string_alloc();

    if(flipper_format_file_open_existing(file, ID_SAVE_PATH)) {
        if(flipper_format_read_string(file, "Name", value)) {
            furi_string_set(app->name, value);
        }
        if(flipper_format_read_string(file, "Email", value)) {
            furi_string_set(app->email, value);
        }
        if(flipper_format_read_string(file, "Tel", value)) {
            furi_string_set(app->tel, value);
        }
        if(flipper_format_read_string(file, "Addr", value)) {
            furi_string_set(app->addr, value);
        }
        if(flipper_format_read_string(file, "Notes", value)) {
            furi_string_set(app->notes, value);
        }
    }

    furi_string_free(value);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

void id_menu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
    case IDScenesMainMenuSceneIDCard:
        scene_manager_handle_custom_event(app->scene_manager, IDScenesMainMenuSceneIDCardEvent);
        break;
    case IDScenesMainMenuSceneSetup:
        scene_manager_handle_custom_event(app->scene_manager, IDScenesMainMenuSceneSetupEvent);
        break;
    case IDScenesMainMenuSceneAbout:
        scene_manager_next_scene(app->scene_manager, IDScenesAboutScene);
        break;
    }
}

bool id_main_menu_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case IDScenesMainMenuSceneIDCardEvent:
            scene_manager_next_scene(app->scene_manager, IDScenesIDCardScene);
            consumed = true;
            break;
        case IDScenesMainMenuSceneSetupEvent:
            scene_manager_next_scene(app->scene_manager, IDScenesInputNameScene);
            consumed = true;
            break;
        }
        break;
    default:
        break;
    }
    return consumed;
}

void id_main_menu_scene_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ID Card v2");
    submenu_add_item(app->submenu, "ID Card", IDScenesMainMenuSceneIDCard, id_menu_callback, app);
    submenu_add_item(app->submenu, "Setup", IDScenesMainMenuSceneSetup, id_menu_callback, app);
    submenu_add_item(app->submenu, "About", IDScenesMainMenuSceneAbout, id_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, IDScenesSubmenuView);
}

void id_main_menu_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

void id_card_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 90, 5, AlignLeft, AlignCenter, FontPrimary, "ID Card");
    load_data(app);

    int y_position = 20;

    if(!furi_string_empty(app->name)) {
        widget_add_string_element(
            app->widget, 0, y_position, AlignLeft, AlignCenter, FontSecondary, "Name:");
        widget_add_string_element(
            app->widget,
            26,
            y_position,
            AlignLeft,
            AlignCenter,
            FontSecondary,
            furi_string_get_cstr(app->name));
        y_position += 10;
    }

    if(!furi_string_empty(app->email)) {
        widget_add_string_element(
            app->widget, 0, y_position, AlignLeft, AlignCenter, FontSecondary, "Email:");
        widget_add_string_element(
            app->widget,
            26,
            y_position,
            AlignLeft,
            AlignCenter,
            FontSecondary,
            furi_string_get_cstr(app->email));
        y_position += 10;
    }

    if(!furi_string_empty(app->tel)) {
        widget_add_string_element(
            app->widget, 0, y_position, AlignLeft, AlignCenter, FontSecondary, "Tel:");
        widget_add_string_element(
            app->widget,
            26,
            y_position,
            AlignLeft,
            AlignCenter,
            FontSecondary,
            furi_string_get_cstr(app->tel));
        y_position += 10;
    }

    if(!furi_string_empty(app->addr)) {
        widget_add_string_element(
            app->widget, 0, y_position, AlignLeft, AlignCenter, FontSecondary, "Addr:");
        widget_add_string_element(
            app->widget,
            26,
            y_position,
            AlignLeft,
            AlignCenter,
            FontSecondary,
            furi_string_get_cstr(app->addr));
        y_position += 10;
    }

    if(!furi_string_empty(app->notes)) {
        widget_add_string_element(
            app->widget, 0, y_position, AlignLeft, AlignCenter, FontSecondary, "Notes:");
        widget_add_text_scroll_element(
            app->widget, 26, y_position - 5, 100, 5, furi_string_get_cstr(app->notes));
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, IDScenesWidgetView);
}

bool id_card_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void id_card_scene_on_exit(void* context) {
    UNUSED(context);
}

void id_text_input_callback(void* context) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, IDScenesGreetingInputSceneSaveEvent);
}

void id_input_name_scene_on_enter(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter Name:");
    text_input_set_minimum_length(app->text_input, 0);
    text_input_set_result_callback(
        app->text_input, id_text_input_callback, app, app->temp_buffer, FIELD_SIZE, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, IDScenesTextInputView);
}

bool id_input_name_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == IDScenesGreetingInputSceneSaveEvent) {
        furi_string_set(app->name, app->temp_buffer);
        scene_manager_next_scene(app->scene_manager, IDScenesInputEmailScene);
        return true;
    }
    return false;
}

void id_input_name_scene_on_exit(void* context) {
    UNUSED(context);
}

void id_input_email_scene_on_enter(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter Email:");
    text_input_set_minimum_length(app->text_input, 0);
    text_input_set_result_callback(
        app->text_input, id_text_input_callback, app, app->temp_buffer, FIELD_SIZE, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, IDScenesTextInputView);
}

bool id_input_email_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == IDScenesGreetingInputSceneSaveEvent) {
        furi_string_set(app->email, app->temp_buffer);
        scene_manager_next_scene(app->scene_manager, IDScenesInputTelScene);
        return true;
    }
    return false;
}

void id_input_email_scene_on_exit(void* context) {
    UNUSED(context);
}

void id_input_tel_scene_on_enter(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter Tel:");
    text_input_set_minimum_length(app->text_input, 0);
    text_input_set_result_callback(
        app->text_input, id_text_input_callback, app, app->temp_buffer, FIELD_SIZE, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, IDScenesTextInputView);
}

bool id_input_tel_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == IDScenesGreetingInputSceneSaveEvent) {
        furi_string_set(app->tel, app->temp_buffer);
        scene_manager_next_scene(app->scene_manager, IDScenesInputAddrScene);
        return true;
    }
    return false;
}

void id_input_tel_scene_on_exit(void* context) {
    UNUSED(context);
}

void id_input_addr_scene_on_enter(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter Address:");
    text_input_set_minimum_length(app->text_input, 0);
    text_input_set_result_callback(
        app->text_input, id_text_input_callback, app, app->temp_buffer, FIELD_SIZE, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, IDScenesTextInputView);
}

bool id_input_addr_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == IDScenesGreetingInputSceneSaveEvent) {
        furi_string_set(app->addr, app->temp_buffer);
        scene_manager_next_scene(app->scene_manager, IDScenesInputNotesScene);
        return true;
    }
    return false;
}

void id_input_addr_scene_on_exit(void* context) {
    UNUSED(context);
}

void id_input_notes_scene_on_enter(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter Notes:");
    text_input_set_minimum_length(app->text_input, 0);
    text_input_set_result_callback(
        app->text_input, id_text_input_callback, app, app->temp_buffer, FIELD_SIZE, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, IDScenesTextInputView);
}

bool id_input_notes_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == IDScenesGreetingInputSceneSaveEvent) {
        furi_string_set(app->notes, app->temp_buffer);
        save_data(app);
        scene_manager_next_scene(app->scene_manager, IDScenesGreetingMessageScene);
        return true;
    }
    return false;
}

void id_input_notes_scene_on_exit(void* context) {
    UNUSED(context);
}

void id_greeting_message_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Saved!");
    view_dispatcher_switch_to_view(app->view_dispatcher, IDScenesWidgetView);
}

bool id_greeting_message_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void id_greeting_message_scene_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

void id_about_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 5, 5, AlignLeft, AlignCenter, FontSecondary, "Simple ID app");
    widget_add_string_element(
        app->widget, 5, 15, AlignLeft, AlignCenter, FontSecondary, "Version 3.0");
    widget_add_string_element(
        app->widget, 5, 55, AlignLeft, AlignCenter, FontPrimary, "author:@evillero");
    view_dispatcher_switch_to_view(app->view_dispatcher, IDScenesWidgetView);
}

bool id_about_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void id_about_scene_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

void (*const id_scene_on_enter_handlers[])(void*) = {
    id_main_menu_scene_on_enter,
    id_card_scene_on_enter,
    id_input_name_scene_on_enter,
    id_input_email_scene_on_enter,
    id_input_tel_scene_on_enter,
    id_input_addr_scene_on_enter,
    id_input_notes_scene_on_enter,
    id_greeting_message_scene_on_enter,
    id_about_scene_on_enter,
};

bool (*const id_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    id_main_menu_scene_on_event,
    id_card_scene_on_event,
    id_input_name_scene_on_event,
    id_input_email_scene_on_event,
    id_input_tel_scene_on_event,
    id_input_addr_scene_on_event,
    id_input_notes_scene_on_event,
    id_greeting_message_scene_on_event,
    id_about_scene_on_event,
};

void (*const id_scene_on_exit_handlers[])(void*) = {
    id_main_menu_scene_on_exit,
    id_card_scene_on_exit,
    id_input_name_scene_on_exit,
    id_input_email_scene_on_exit,
    id_input_tel_scene_on_exit,
    id_input_addr_scene_on_exit,
    id_input_notes_scene_on_exit,
    id_greeting_message_scene_on_exit,
    id_about_scene_on_exit,
};

static const SceneManagerHandlers id_scene_manager_handlers = {
    .on_enter_handlers = id_scene_on_enter_handlers,
    .on_event_handlers = id_scene_on_event_handlers,
    .on_exit_handlers = id_scene_on_exit_handlers,
    .scene_num = IDScenesSceneCount,
};

static bool id_scene_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

static bool id_scene_back_event_callback(void* context) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static App* app_alloc() {
    App* app = malloc(sizeof(App));

    app->name = furi_string_alloc();
    app->email = furi_string_alloc();
    app->tel = furi_string_alloc();
    app->addr = furi_string_alloc();
    app->notes = furi_string_alloc();

    app->scene_manager = scene_manager_alloc(&id_scene_manager_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, id_scene_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, id_scene_back_event_callback);

    app->submenu = submenu_alloc();
    app->widget = widget_alloc();
    app->text_input = text_input_alloc();

    view_dispatcher_add_view(
        app->view_dispatcher, IDScenesSubmenuView, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, IDScenesWidgetView, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->view_dispatcher, IDScenesTextInputView, text_input_get_view(app->text_input));

    load_data(app);
    return app;
}

static void app_free(App* app) {
    furi_assert(app);

    furi_string_free(app->name);
    furi_string_free(app->email);
    furi_string_free(app->tel);
    furi_string_free(app->addr);
    furi_string_free(app->notes);

    view_dispatcher_remove_view(app->view_dispatcher, IDScenesSubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, IDScenesWidgetView);
    view_dispatcher_remove_view(app->view_dispatcher, IDScenesTextInputView);
    view_dispatcher_free(app->view_dispatcher);

    scene_manager_free(app->scene_manager);

    submenu_free(app->submenu);
    widget_free(app->widget);
    text_input_free(app->text_input);

    free(app);
}

int32_t id_app(void* p) {
    UNUSED(p);

    App* app = app_alloc();
    Gui* gui = furi_record_open(RECORD_GUI);

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, IDScenesMainMenuScene);
    view_dispatcher_run(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    app_free(app);

    return 0;
}
