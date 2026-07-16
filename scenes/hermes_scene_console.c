#include "../hermes_i.h"

/* Drained on the GUI thread every tick. Because only this thread ever touches
 * the Term, the terminal model needs no lock at all. */
#define CONSOLE_DRAIN_CHUNK (256u)

/* Pushing a scene runs this one's on_exit, which would otherwise drop the link
 * every time the keyboard opens. The flag tells a temporary detour apart from
 * actually leaving the console. */
typedef enum {
    ConsoleStateActive = 0,
    ConsoleStateSuspended,
} ConsoleState;

static void hermes_console_suspend(HermesApp* app, HermesScene next) {
    scene_manager_set_scene_state(app->scene_manager, HermesSceneConsole, ConsoleStateSuspended);
    scene_manager_next_scene(app->scene_manager, next);
}

static void hermes_scene_console_callback(void* context, ConsoleEventType type) {
    HermesApp* app = context;

    switch(type) {
    case ConsoleEventTypeText:
        view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventConsoleText);
        break;
    case ConsoleEventTypeCtrl:
        view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventConsoleCtrl);
        break;
    case ConsoleEventTypeEnter:
        view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventConsoleEnter);
        break;
    }
}

void hermes_scene_console_on_enter(void* context) {
    HermesApp* app = context;

    scene_manager_set_scene_state(app->scene_manager, HermesSceneConsole, ConsoleStateActive);

    /* Coming back from the keyboard or the Ctrl palette must not wipe the
     * scrollback or re-open the port mid-session. */
    if(!uart_tap_is_open(app->tap)) {
        term_reset(app->term);

        if(!uart_tap_open(
               app->tap, app->port, app->link.baud, app->link.framing, app->tx_enabled)) {
            /* Deferred, not immediate: navigating from on_enter would nest a
             * scene transition inside this one. */
            hermes_notify_found(app, false);
            view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventPortBusy);
            return;
        }
    }

    console_view_set_term(app->console_view, app->term);
    console_view_set_link(
        app->console_view,
        app->link.baud,
        hermes_framing_name(app->link.framing),
        uart_tap_tx_enabled(app->tap));
    console_view_set_callback(app->console_view, hermes_scene_console_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewConsole);
}

bool hermes_scene_console_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        uint8_t chunk[CONSOLE_DRAIN_CHUNK];
        size_t got;
        bool any = false;

        while((got = uart_tap_read(app->tap, chunk, sizeof(chunk))) > 0) {
            term_feed(app->term, chunk, got);
            any = true;
        }
        if(any) console_view_notify_rx(app->console_view);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case HermesCustomEventPortBusy:
            scene_manager_previous_scene(app->scene_manager);
            return true;

        case HermesCustomEventConsoleText:
            hermes_console_suspend(app, HermesSceneKeyboard);
            return true;
        case HermesCustomEventConsoleCtrl:
            hermes_console_suspend(app, HermesSceneCtrl);
            return true;
        case HermesCustomEventConsoleEnter:
            uart_tap_send_enter(app->tap, app->enter_mode);
            if(app->local_echo) term_feed_echo(app->term, '\n');
            return true;
        default:
            break;
        }
    }

    return false;
}

void hermes_scene_console_on_exit(void* context) {
    HermesApp* app = context;

    const uint32_t state = scene_manager_get_scene_state(app->scene_manager, HermesSceneConsole);
    if(state != ConsoleStateSuspended) uart_tap_close(app->tap);
}
