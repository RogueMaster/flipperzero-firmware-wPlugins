#include "clipboard_screen.h"
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

typedef struct {
    FuriMessageQueue* input;
    FuriMutex* mutex;
    bool qr;
} ClipboardInfo;

static void info_draw(Canvas* canvas, void* context) {
    ClipboardInfo* info = context;
    ClipboardScreen screen;
    furi_mutex_acquire(info->mutex, FuriWaitForever);
    clipboard_screen_info(&screen, info->qr);
    furi_mutex_release(info->mutex);
    clipboard_screen_draw(canvas, &screen);
}

static void info_input(InputEvent* event, void* context) {
    ClipboardInfo* info = context;
    furi_message_queue_put(info->input, event, 0);
}

int32_t clipboard_info_app(void) {
    ClipboardInfo info = {
        .input = furi_message_queue_alloc(8, sizeof(InputEvent)),
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
    };
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view = view_port_alloc();
    view_port_draw_callback_set(view, info_draw, &info);
    view_port_input_callback_set(view, info_input, &info);
    gui_add_view_port(gui, view, GuiLayerFullscreen);
    bool running = true;
    while(running) {
        InputEvent event;
        if(furi_message_queue_get(info.input, &event, FuriWaitForever) != FuriStatusOk ||
           event.type != InputTypePress)
            continue;
        furi_mutex_acquire(info.mutex, FuriWaitForever);
        if(event.key == InputKeyBack || event.key == InputKeyLeft) {
            if(info.qr)
                info.qr = false;
            else
                running = false;
        } else if(event.key == InputKeyOk)
            info.qr = true;
        furi_mutex_release(info.mutex);
        view_port_update(view);
    }
    gui_remove_view_port(gui, view);
    view_port_free(view);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(info.mutex);
    furi_message_queue_free(info.input);
    return 0;
}
