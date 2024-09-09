#include <malloc.h>

#include <furi.h>
#include <gui/gui.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include <mp_flipper_runtime.h>
#include <mp_flipper_compiler.h>

#include "upython_icons.h"

#define TAG "uPython"

typedef enum {
    ActionNone,
    ActionOpen,
    ActionExit
} Action;

static Action action = ActionNone;

static void execute_file(FuriString* file) {
    size_t stack;

    const char* path = furi_string_get_cstr(file);
    FuriString* file_path = furi_string_alloc_printf("%s", path);

    do {
        FURI_LOG_I(TAG, "executing script %s", path);

        const size_t heap_size = memmgr_get_free_heap() * 0.1;
        const size_t stack_size = 2 * 1024;
        uint8_t* heap = malloc(heap_size * sizeof(uint8_t));

        FURI_LOG_D(TAG, "initial heap size is %zu bytes", heap_size);
        FURI_LOG_D(TAG, "stack size is %zu bytes", stack_size);

        size_t index = furi_string_search_rchar(file_path, '/');

        furi_check(index != FURI_STRING_FAILURE);

        bool is_py_file = furi_string_end_with_str(file_path, ".py");

        furi_string_left(file_path, index);

        mp_flipper_set_root_module_path(furi_string_get_cstr(file_path));

        mp_flipper_init(heap, heap_size, stack_size, &stack);

        if(is_py_file) {
            mp_flipper_exec_py_file(path);
        } else {
            mp_flipper_exec_mpy_file(path);
        }

        mp_flipper_deinit();

        free(heap);
    } while(false);

    furi_string_free(file_path);
}

static bool select_python_file(FuriString* file_path) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    DialogsFileBrowserOptions browser_options;

    dialog_file_browser_set_basic_options(&browser_options, "py", NULL);

    browser_options.hide_ext = false;
    browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;

    bool result = dialog_file_browser_show(dialogs, file_path, file_path, &browser_options);

    furi_record_close(RECORD_DIALOGS);

    return result;
}

static void on_input(const void* event, void* ctx) {
    UNUSED(ctx);

    InputKey key = ((InputEvent*)event)->key;
    InputType type = ((InputEvent*)event)->type;

    if(type != InputTypeRelease) {
        return;
    }

    switch(key) {
    case InputKeyOk:
        action = ActionOpen;
        break;
    case InputKeyBack:
        action = ActionExit;
        break;
    default:
        action = ActionNone;
        break;
    }
}

static void show_splash_screen() {
    Gui* gui = furi_record_open(RECORD_GUI);
    FuriPubSub* input_event_queue = furi_record_open(RECORD_INPUT_EVENTS);
    FuriPubSubSubscription* input_event = furi_pubsub_subscribe(input_event_queue, on_input, NULL);

    ViewPort* view_port = view_port_alloc();

    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    Canvas* canvas = gui_direct_draw_acquire(gui);

    canvas_draw_icon(canvas, 0, 0, &I_splash);
    canvas_draw_icon(canvas, 82, 17, &I_qrcode);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 66, 3, AlignLeft, AlignTop, "Micro");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 90, 2, AlignLeft, AlignTop, "Python");

    canvas_set_font(canvas, FontSecondary);

    canvas_draw_icon(canvas, 65, 53, &I_Pin_back_arrow_10x8);
    canvas_draw_str_aligned(canvas, 78, 54, AlignLeft, AlignTop, "Exit");

    canvas_draw_icon(canvas, 98, 54, &I_ButtonCenter_7x7);
    canvas_draw_str_aligned(canvas, 107, 54, AlignLeft, AlignTop, "Open");

    canvas_commit(canvas);

    while(action == ActionNone) {
        furi_delay_ms(1);
    }

    furi_pubsub_unsubscribe(input_event_queue, input_event);

    gui_direct_draw_release(gui);
    gui_remove_view_port(gui, view_port);

    view_port_free(view_port);

    furi_record_close(RECORD_INPUT_EVENTS);
    furi_record_close(RECORD_GUI);
}

int32_t upython(void* p) {
    UNUSED(p);

    do {
        show_splash_screen();

        if(action == ActionExit) {
            break;
        }

        FuriString* file_path = furi_string_alloc_set_str(APP_ASSETS_PATH("upython"));

        if(select_python_file(file_path)) {
            execute_file(file_path);
        }

        furi_string_free(file_path);

        action = ActionNone;
    } while(true);

    return 0;
}
