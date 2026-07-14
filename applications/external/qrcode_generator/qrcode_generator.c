#include "qrencode/qrcodegen.h"
#include <furi.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>

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

#define QRCODE_GEN_FOLDER "/ext/apps_data/qrcode_generator/"

// Diffrent app scenes
typedef enum {
    MainMenuScene,
    QrCodeInputScene,
    QrCodeMessageScene,
    ReadmeScene,
    SavedScene,
    QrCodeSceneCount,
} QrCodeScene;

// View references
typedef enum {
    QrCodeSubmenuView,
    QrCodeWidgetView,
    QrCodeTextInputView,
    QrCodeTextBoxView,
    QrCodeCanvasView,
} QrCodeView;

// Model for QrCode View
typedef struct {
    uint8_t* qrcode;
    uint8_t* buffer;
    size_t buffer_size;
} QrCodeModel;

// App object

typedef struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    TextBox* text_box;
    char* qrcode_text;
    uint8_t qrcode_text_size;
    View* qrcode_canvas_view;
} App;

// Refence to item menus. Avoid magic numbers
typedef enum {
    MainMenuSceneQrCode,
    MainMenuSceneReadme,
    MainMenuSceneSaved,
} MainMenuSceneIndex;

// Reference to custom events. Avoid magic numbers
typedef enum {
    MainMenuSceneQrCodeEvent,
    MainMenuSceneReadmeEvent,
    MainMenuSceneSavedEvent,
} QrCodeMainMenuEvent;

typedef enum {
    QrCodeInputSceneSaveEvent,
} QrCodeInputEvent;

// Function to write text to a file
void write_text_to_file(const char* file_path, const char* text) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        printf("Failed to open storage\n");
        return;
    }

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
        storage_file_close(file);
        printf("Text written to file successfully\n");
    } else {
        printf("Failed to open file for writing\n");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Function to read text from a file
void read_text_from_file(const char* file_path, App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        printf("Failed to open storage\n");
        return;
    }

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[128]; // Buffer to store read content
        ssize_t read_bytes;

        while((read_bytes = storage_file_read(file, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[read_bytes] = '\0'; // Null-terminate the string
            printf("Read from file: %s\n", buffer);
        }

        // Apply file context to app context
        app->qrcode_text = strdup(buffer);
        storage_file_close(file);
    } else {
        printf("Failed to open file for reading\n");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// QRCode draw code
void draw_qrcode(Canvas* canvas, void* model) {
    QrCodeModel* qrcodeModel = (QrCodeModel*)model;

    int size = qrcodegen_getSize(qrcodeModel->qrcode);
    const int scale = 2;
    const int offset_x = 64 - (size * scale) / 2;
    const int offset_y = 32 - (size * scale) / 2;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    for(int y = 0; y < size; y++) {
        for(int x = 0; x < size; x++) {
            if(qrcodegen_getModule(qrcodeModel->qrcode, x, y)) {
                canvas_draw_box(canvas, offset_x + x * scale, offset_y + y * scale, scale, scale);
            }
        }
    }
    canvas_commit(canvas);
}

// Generates the qrcode
void generate_qrcode(App* app) {
    QrCodeModel* qrcodeModel = view_get_model(app->qrcode_canvas_view);
    enum qrcodegen_Ecc errCorLvl = qrcodegen_Ecc_LOW;

    // Allocate buffers if not already allocated
    static const size_t buffer_len = qrcodegen_BUFFER_LEN_MAX;
    static const size_t qrcode_len = qrcodegen_BUFFER_LEN_MAX;

    if(!qrcodeModel->buffer) {
        qrcodeModel->buffer = malloc(buffer_len);
        furi_check(qrcodeModel->buffer); // Optional safety check
    }

    if(!qrcodeModel->qrcode) {
        qrcodeModel->qrcode = malloc(qrcode_len);
        furi_check(qrcodeModel->qrcode); // Optional safety check
    }

    qrcodegen_encodeText(
        app->qrcode_text,
        qrcodeModel->buffer,
        qrcodeModel->qrcode,
        errCorLvl,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true);
    view_commit_model(app->qrcode_canvas_view, false);
}

// Function for stub menu
void qrcode_menu_callback(void* context, uint32_t index) {
    App* app = context;

    switch(index) {
    case MainMenuSceneReadme:
        scene_manager_handle_custom_event(app->scene_manager, MainMenuSceneReadmeEvent);
        break;

    case MainMenuSceneQrCode:
        scene_manager_handle_custom_event(app->scene_manager, MainMenuSceneQrCodeEvent);
        break;

    case MainMenuSceneSaved:
        scene_manager_handle_custom_event(app->scene_manager, MainMenuSceneSavedEvent);
        break;
    }
}

// Functions for every scene
// Every scene must have on_enter, on_event and on_exit
void qrcode_main_menu_scene_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "QRCode Generator");

    submenu_add_item(app->submenu, "Generate", MainMenuSceneQrCode, qrcode_menu_callback, app);
    submenu_add_item(app->submenu, "Saved", MainMenuSceneSaved, qrcode_menu_callback, app);
    submenu_add_item(app->submenu, "README", MainMenuSceneReadme, qrcode_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, QrCodeSubmenuView);
}

bool qrcode_main_menu_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case MainMenuSceneReadmeEvent:
            scene_manager_next_scene(app->scene_manager, ReadmeScene);
            consumed = true;
            break;

        case MainMenuSceneQrCodeEvent:
            scene_manager_next_scene(app->scene_manager, QrCodeInputScene);
            consumed = true;
            break;

        case MainMenuSceneSavedEvent:
            scene_manager_next_scene(app->scene_manager, SavedScene);
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
    scene_manager_handle_custom_event(app->scene_manager, QrCodeInputSceneSaveEvent);
}
void qrcode_input_scene_on_enter(void* context) {
    App* app = context;
    bool clear_text = true;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter text");
    text_input_set_result_callback(
        app->text_input,
        text_input_callback,
        app,
        app->qrcode_text,
        app->qrcode_text_size,
        clear_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, QrCodeTextInputView);
}
bool qrcode_greeting_input_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == QrCodeInputSceneSaveEvent) {
            scene_manager_next_scene(app->scene_manager, QrCodeMessageScene);
            consumed = true;
        }
    }
    return consumed;
}
void qrcode_greeting_input_scene_on_exit(void* context) {
    UNUSED(context);
}

void generate_qrcode_on_enter(void* context) {
    App* app = (App*)context;
    generate_qrcode(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, QrCodeCanvasView);
}

bool qrcode_greeting_message_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false; // event not handled.
}
void qrcode_greeting_message_scene_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

void readme_scene_on_enter(void* context) {
    App* app = context;

    text_box_reset(app->text_box);
    text_box_set_text(
        app->text_box,
        "Generates and displays QRCodes on the flipper zero.\n"
        "You can also open saved QRCodes. More details on the repository.\n\n"
        "github.com/qw3rtty/flipperzero-qrcode-generator");

    view_dispatcher_switch_to_view(app->view_dispatcher, QrCodeTextBoxView);
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

void saved_scene_on_enter(void* context) {
    //UNUSED(context);
    App* app = context;

    // Creates and open the flipper file picker
    FuriString* file_path = furi_string_alloc();
    furi_string_set(file_path, QRCODE_GEN_FOLDER);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".txt", 0);
    browser_options.hide_ext = false;
    browser_options.base_path = QRCODE_GEN_FOLDER;

    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    bool res = dialog_file_browser_show(dialogs, file_path, file_path, &browser_options);

    if(!res) {
        //FURI_LOG_E(TAG, "No file selected");
        return;
    }

    read_text_from_file(furi_string_get_cstr(file_path), app);
    generate_qrcode(app);

    furi_record_close(RECORD_DIALOGS);
    view_dispatcher_switch_to_view(app->view_dispatcher, QrCodeCanvasView);
}

bool saved_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false; // event not handled.
}

void saved_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

// Arrays for the handlers

void (*const qrcode_scene_on_enter_handlers[])(void*) = {
    qrcode_main_menu_scene_on_enter,
    qrcode_input_scene_on_enter,
    generate_qrcode_on_enter,
    readme_scene_on_enter,
    saved_scene_on_enter,
};

bool (*const qrcode_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    qrcode_main_menu_scene_on_event,
    qrcode_greeting_input_scene_on_event,
    qrcode_greeting_message_scene_on_event,
    readme_scene_on_event,
    saved_scene_on_event,
};

void (*const qrcode_scene_on_exit_handlers[])(void*) = {
    main_menu_scene_on_exit,
    qrcode_greeting_input_scene_on_exit,
    qrcode_greeting_message_scene_on_exit,
    readme_scene_on_exit,
    saved_scene_on_exit,
};

static const SceneManagerHandlers qrcode_scene_manager_handlers = {
    .on_enter_handlers = qrcode_scene_on_enter_handlers,
    .on_event_handlers = qrcode_scene_on_event_handlers,
    .on_exit_handlers = qrcode_scene_on_exit_handlers,
    .scene_num = QrCodeSceneCount,
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

    app->qrcode_text_size = 128;
    app->qrcode_text = malloc(app->qrcode_text_size);

    app->scene_manager = scene_manager_alloc(&qrcode_scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, basic_scene_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, basic_scene_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, QrCodeSubmenuView, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, QrCodeWidgetView, widget_get_view(app->widget));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, QrCodeTextInputView, text_input_get_view(app->text_input));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, QrCodeTextBoxView, text_box_get_view(app->text_box));

    app->qrcode_canvas_view = view_alloc();
    view_allocate_model(app->qrcode_canvas_view, ViewModelTypeLockFree, sizeof(QrCodeModel));
    view_set_context(app->qrcode_canvas_view, app);
    view_set_draw_callback(app->qrcode_canvas_view, draw_qrcode);
    view_dispatcher_add_view(app->view_dispatcher, QrCodeCanvasView, app->qrcode_canvas_view);

    return app;
}

// For free the memory of the app
static void app_free(App* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, QrCodeSubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, QrCodeWidgetView);
    view_dispatcher_remove_view(app->view_dispatcher, QrCodeTextInputView);
    view_dispatcher_remove_view(app->view_dispatcher, QrCodeTextBoxView);
    view_dispatcher_remove_view(app->view_dispatcher, QrCodeCanvasView);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    submenu_free(app->submenu);
    widget_free(app->widget);
    text_input_free(app->text_input);
    text_box_free(app->text_box);

    QrCodeModel* qrcodeModel = view_get_model(app->qrcode_canvas_view);
    if(qrcodeModel) {
        free(qrcodeModel->buffer);
        free(qrcodeModel->qrcode);
    }
    view_free_model(app->qrcode_canvas_view);
    view_free(app->qrcode_canvas_view);

    free(app);
}

int32_t qrcode_generator_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, MainMenuScene);
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    return 0;
}
