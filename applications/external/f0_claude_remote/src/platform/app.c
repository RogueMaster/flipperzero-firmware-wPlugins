#include "app.h"

#include <furi.h>
#include <notification/notification_messages.h>

#include "../core/actions.h"
#include "../core/feedback.h"
#include "keymap_input.h"
#include "screen.h"
#include "transport_bluetooth.h"

struct App {
    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* link_queue;
    NotificationApp* notification;
    Keymap* keymap;
    Screen* screen;
    Transport* transport;
};

static void app_perform(App* app, Action action) {
    actions_perform(app->transport, action);
    if(feedback_vibrates(action)) {
        notification_message(app->notification, &sequence_single_vibro);
    }
    uint32_t flash_ms = feedback_flash_ms(action);
    if(flash_ms > 0) {
        screen_flash(app->screen, action, flash_ms);
    }
    if(action == ActionExit) {
        furi_event_loop_stop(app->event_loop);
    }
}

static void app_input_callback(FuriEventLoopObject* object, void* context) {
    App* app = context;
    InputEvent event;
    furi_check(furi_message_queue_get(object, &event, 0) == FuriStatusOk);

    Action action = keymap_input_handle(app->keymap, &event);
    if(action != ActionNone) {
        app_perform(app, action);
    }
    screen_set_mode(app->screen, keymap_mode(app->keymap));
}

static void app_link_callback(FuriEventLoopObject* object, void* context) {
    App* app = context;
    bool connected;
    furi_check(furi_message_queue_get(object, &connected, 0) == FuriStatusOk);

    Action action = keymap_set_connected(app->keymap, connected);
    if(action != ActionNone) {
        app_perform(app, action);
    }
    screen_set_mode(app->screen, keymap_mode(app->keymap));
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
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(app->notification, &sequence_display_backlight_enforce_on);
    app->keymap = keymap_alloc();
    app->screen = screen_alloc(app->event_loop, app->input_queue, keymap_mode(app->keymap));
    app->transport = transport_bluetooth_alloc(app->link_queue);
    return app;
}

void app_free(App* app) {
    screen_free(app->screen);
    transport_bluetooth_free(app->transport);
    keymap_free(app->keymap);
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);
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
