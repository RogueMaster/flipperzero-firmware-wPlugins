#include "screen.h"

#include <gui/gui.h>

#include "../ui/view.h"

struct Screen {
    Gui* gui;
    ViewPort* view_port;
    FuriEventLoopTimer* flash_timer;
    /* The draw callback reads these on the GUI thread. */
    _Atomic Mode mode;
    _Atomic Link link;
    _Atomic Action flash;
};

static void screen_draw_callback(Canvas* canvas, void* context) {
    Screen* screen = context;
    view_draw(canvas, screen->mode, screen->link, screen->flash);
}

/* Runs on the GUI thread, and event_loop.h binds its functions to the loop's own thread. */
static void screen_input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* input_queue = context;
    furi_message_queue_put(input_queue, event, 0);
}

static void screen_flash_callback(void* context) {
    Screen* screen = context;
    screen->flash = ActionNone;
    view_port_update(screen->view_port);
}

Screen* screen_alloc(FuriEventLoop* event_loop, FuriMessageQueue* input_queue, Mode mode) {
    Screen* screen = malloc(sizeof(Screen));
    screen->mode = mode;
    screen->link = LinkBluetooth;
    screen->flash = ActionNone;
    screen->flash_timer = furi_event_loop_timer_alloc(
        event_loop, screen_flash_callback, FuriEventLoopTimerTypeOnce, screen);

    screen->view_port = view_port_alloc();
    view_port_draw_callback_set(screen->view_port, screen_draw_callback, screen);
    view_port_input_callback_set(screen->view_port, screen_input_callback, input_queue);
    screen->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(screen->gui, screen->view_port, GuiLayerFullscreen);

    return screen;
}

void screen_free(Screen* screen) {
    gui_remove_view_port(screen->gui, screen->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(screen->view_port);
    furi_event_loop_timer_free(screen->flash_timer);
    free(screen);
}

void screen_set_mode(Screen* screen, Mode mode) {
    screen->mode = mode;
    view_port_update(screen->view_port);
}

void screen_set_link(Screen* screen, Link link) {
    screen->link = link;
    view_port_update(screen->view_port);
}

void screen_flash(Screen* screen, Action action, uint32_t duration_ms) {
    screen->flash = action;
    furi_event_loop_timer_start(screen->flash_timer, furi_ms_to_ticks(duration_ms));
    view_port_update(screen->view_port);
}
