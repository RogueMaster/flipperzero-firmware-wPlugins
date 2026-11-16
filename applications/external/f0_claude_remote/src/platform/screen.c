#include "screen.h"

#include <gui/gui.h>

#include "../ui/view.h"

struct Screen {
    Gui* gui;
    ViewPort* view_port;
    FuriEventLoopTimer* flash_timer;
    FuriEventLoopTimer* sent_timer;
    /* The draw callback copies the state on the GUI thread. */
    FuriMutex* mutex;
    ViewState state;
};

static void screen_draw_callback(Canvas* canvas, void* context) {
    Screen* screen = context;
    ViewState state = screen_state(screen);
    view_draw(canvas, &state);
}

/* Runs on the GUI thread, and event_loop.h binds its functions to the loop's own thread. */
static void screen_input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* input_queue = context;
    furi_message_queue_put(input_queue, event, 0);
}

static void screen_flash_callback(void* context) {
    Screen* screen = context;
    ViewState state = screen_state(screen);
    state.flash = ActionNone;
    screen_set_state(screen, &state);
}

static void screen_sent_callback(void* context) {
    Screen* screen = context;
    ViewState state = screen_state(screen);
    state.sent = ActionNone;
    screen_set_state(screen, &state);
}

/* The state comes before the view port joins the GUI, which asks for a frame at once (gui.c:245-270). */
Screen*
    screen_alloc(FuriEventLoop* event_loop, FuriMessageQueue* input_queue, const ViewState* state) {
    Screen* screen = malloc(sizeof(Screen));
    screen->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    screen->state = *state;
    screen->flash_timer = furi_event_loop_timer_alloc(
        event_loop, screen_flash_callback, FuriEventLoopTimerTypeOnce, screen);
    screen->sent_timer = furi_event_loop_timer_alloc(
        event_loop, screen_sent_callback, FuriEventLoopTimerTypeOnce, screen);

    screen->view_port = view_port_alloc();
    view_port_draw_callback_set(screen->view_port, screen_draw_callback, screen);
    view_port_input_callback_set(screen->view_port, screen_input_callback, input_queue);
    screen->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(screen->gui, screen->view_port, GuiLayerFullscreen);

    return screen;
}

/* The mutex goes after the view port leaves the GUI, which waits for a frame being drawn
 * (gui.c:351-406). */
void screen_free(Screen* screen) {
    gui_remove_view_port(screen->gui, screen->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(screen->view_port);
    furi_event_loop_timer_free(screen->sent_timer);
    furi_event_loop_timer_free(screen->flash_timer);
    furi_mutex_free(screen->mutex);
    free(screen);
}

ViewState screen_state(Screen* screen) {
    furi_check(furi_mutex_acquire(screen->mutex, FuriWaitForever) == FuriStatusOk);
    ViewState state = screen->state;
    furi_check(furi_mutex_release(screen->mutex) == FuriStatusOk);
    return state;
}

/* Only the event loop's callbacks set the state, so none loses a change between its read and
 * its set. The mutex is released first: the update waits for the lock the draw callback runs
 * under (view_port.c:202-220). */
void screen_set_state(Screen* screen, const ViewState* state) {
    furi_check(furi_mutex_acquire(screen->mutex, FuriWaitForever) == FuriStatusOk);
    screen->state = *state;
    furi_check(furi_mutex_release(screen->mutex) == FuriStatusOk);
    view_port_update(screen->view_port);
}

/* A timer of no ticks is not documented (event_loop_timer.h:69-74), so none stops it. */
static void screen_set_timer(FuriEventLoopTimer* timer, uint32_t duration_ms) {
    if(duration_ms > 0) {
        furi_event_loop_timer_start(timer, furi_ms_to_ticks(duration_ms));
    } else {
        furi_event_loop_timer_stop(timer);
    }
}

/* One set asks for one frame (screen_set_state): two would flash under `Connected` first. */
void screen_confirm(Screen* screen, Action action, uint32_t flash_ms, uint32_t sent_ms) {
    ViewState state = screen_state(screen);
    state.flash = flash_ms > 0 ? action : ActionNone;
    state.sent = sent_ms > 0 ? action : ActionNone;
    screen_set_state(screen, &state);
    screen_set_timer(screen->flash_timer, flash_ms);
    screen_set_timer(screen->sent_timer, sent_ms);
}
