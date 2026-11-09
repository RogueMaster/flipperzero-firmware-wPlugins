#include "app.h"

#include <dolphin/dolphin.h>
#include <furi.h>
#include <notification/notification_messages.h>

#include "../core/actions.h"
#include "../core/deed.h"
#include "../core/feedback.h"
#include "backlight.h"
#include "keymap_input.h"
#include "screen.h"
#include "transport_hid.h"

struct App {
    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* link_queue;
    FuriEventLoopTimer* wait_timer;
    uint32_t wait_ms;
    NotificationApp* notification;
    Backlight* backlight;
    Keymap* keymap;
    Screen* screen;
    Transport* transport;
};

/* Not games: these two draw on the catalog apps' daily cap and weigh the 1 XP and 10 wanted. */
static const DolphinDeed app_dolphin_deeds[DeedMAX] = {
    [DeedDictation] = DolphinDeedPluginGameStart,
    [DeedClear] = DolphinDeedPluginGameWin,
};

static void app_perform(App* app, Action action) {
    /* Before the switch, which holds this loop at least 700 ms: the board changes at once, and a
     * wait of zero makes app_follow_wait start the 60 s afresh. */
    if(action == ActionUseUsb || action == ActionUseBluetooth) {
        screen_set_link(app->screen, action == ActionUseUsb ? LinkUsb : LinkBluetooth);
        app->wait_ms = 0;
    }
    actions_perform(app->transport, action);
    Deed deed = deed_earned(action);
    if(deed != DeedNone) {
        dolphin_deed(app_dolphin_deeds[deed]);
    }
    if(feedback_vibrates(action)) {
        notification_message(app->notification, &sequence_single_vibro);
    }
    uint32_t flash_ms = feedback_flash_ms(action);
    if(flash_ms > 0) {
        screen_flash(app->screen, action, flash_ms);
    }
    if(action == ActionGiveUp || action == ActionStartOver) {
        backlight_set(app->backlight, action == ActionStartOver);
    }
    if(action == ActionExit) {
        furi_event_loop_stop(app->event_loop);
    }
}

/* Not furi_event_loop_timer_is_running: it reads stale while a stop waits in the timer's queue
 * (event_loop_timer.c:89-113), so a report right after one would skip the start. And not a restart
 * on each report: a link-down report repeats, and stopping sends one itself (bt.c:312). */
static void app_follow_wait(App* app) {
    uint32_t wait_ms = keymap_wait_ms(app->keymap);
    if(wait_ms == 0) {
        furi_event_loop_timer_stop(app->wait_timer);
    } else if(app->wait_ms == 0) {
        furi_event_loop_timer_start(app->wait_timer, furi_ms_to_ticks(wait_ms));
    }
    app->wait_ms = wait_ms;
}

static void app_settle(App* app, Action action) {
    if(action != ActionNone) {
        app_perform(app, action);
    }
    screen_set_mode(app->screen, keymap_mode(app->keymap));
    app_follow_wait(app);
}

static void app_input_callback(FuriEventLoopObject* object, void* context) {
    App* app = context;
    InputEvent event;
    furi_check(furi_message_queue_get(object, &event, 0) == FuriStatusOk);

    app_settle(app, keymap_input_handle(app->keymap, &event));
}

static void app_link_callback(FuriEventLoopObject* object, void* context) {
    App* app = context;
    bool connected;
    furi_check(furi_message_queue_get(object, &connected, 0) == FuriStatusOk);

    app_settle(app, keymap_set_connected(app->keymap, connected));
}

static void app_wait_callback(void* context) {
    App* app = context;
    app_settle(app, keymap_time_out(app->keymap));
}

/* After the loop ends an unconsumed signal answers `has to be closed manually`, and
 * ufbt launch repeats its close every 200 ms while the radio takes longer to come down. */
static bool app_closing_signal_callback(uint32_t signal, void* arg, void* context) {
    UNUSED(arg);
    UNUSED(context);
    return signal == FuriSignalExit;
}

App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    app->event_loop = furi_event_loop_alloc();
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    furi_event_loop_subscribe_message_queue(
        app->event_loop, app->input_queue, FuriEventLoopEventIn, app_input_callback, app);
    app->link_queue = furi_message_queue_alloc(8, sizeof(bool));
    furi_event_loop_subscribe_message_queue(
        app->event_loop, app->link_queue, FuriEventLoopEventIn, app_link_callback, app);
    app->wait_timer = furi_event_loop_timer_alloc(
        app->event_loop, app_wait_callback, FuriEventLoopTimerTypeOnce, app);
    app->wait_ms = 0;
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->backlight = backlight_alloc();
    app->keymap = keymap_alloc();
    app->screen = screen_alloc(app->event_loop, app->input_queue, keymap_mode(app->keymap));
    app->transport = transport_hid_alloc(app->link_queue);
    app_follow_wait(app);
    return app;
}

void app_free(App* app) {
    screen_free(app->screen);
    transport_hid_free(app->transport);
    keymap_free(app->keymap);
    backlight_free(app->backlight);
    furi_record_close(RECORD_NOTIFICATION);
    furi_event_loop_timer_free(app->wait_timer);
    furi_event_loop_unsubscribe(app->event_loop, app->link_queue);
    furi_message_queue_free(app->link_queue);
    furi_event_loop_unsubscribe(app->event_loop, app->input_queue);
    furi_message_queue_free(app->input_queue);
    furi_event_loop_free(app->event_loop);
    furi_thread_set_signal_callback(furi_thread_get_current(), NULL, NULL);
    free(app);
}

void app_run(App* app) {
    furi_event_loop_run(app->event_loop);
    furi_thread_set_signal_callback(furi_thread_get_current(), app_closing_signal_callback, NULL);
}
