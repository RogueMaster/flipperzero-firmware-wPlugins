/*
 * scene_logger — field logging: attach to the node, record what it hears.
 *
 * Back stops the session and closes the files. That is deliberate: holding the
 * UART while the user wanders off into other screens would be a surprising way
 * to lose a walk's worth of data.
 */
#include "../logger/meshcore_rxlog.h"
#include "../meshcore_cfg.h"

#define MESHCORE_LOGGER_EVENT_STARTED 0x300u
#define MESHCORE_LOGGER_EVENT_MARK 0x301u
#define MESHCORE_LOGGER_WORKER_STACK 2048u

/* The OK button drops a point mark into events.csv. It runs on the GUI thread,
 * so it only posts an event: everything the mark does happens back in the
 * scene's own handler, off the button callback. */
static void meshcore_scene_logger_button(GuiButtonType type, InputType input, void* context) {
    MeshCoreApp* app = context;
    if(type != GuiButtonTypeCenter || input != InputTypeShort) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_LOGGER_EVENT_MARK);
}

static int32_t meshcore_logger_scene_worker(void* context) {
    MeshCoreApp* app = context;

    /* The logger runs its own session, but on the same UART the messenger's
     * session uses. If the messenger connected itself (opening Contacts now
     * does), that session is holding the port — hand it over, or the logger
     * would fail to take the UART. The mailbox no-ops while it is down, and the
     * messenger reconnects itself the next time it is opened. */
    if(meshcore_session_is_running(app->session)) {
        meshcore_session_stop(app->session);
    }

    meshcore_logger_start(app->logger);
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_LOGGER_EVENT_STARTED);
    return 0;
}

/* The static screens can scroll: nothing redraws them, so the scroll position
 * survives. */
static void meshcore_scene_logger_show_text(MeshCoreApp* app, const char* text) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
}

/* The live screen must NOT be a scrolling element. It is rebuilt whenever a
 * packet lands, and rebuilding a text scroll element resets its scroll
 * position — which reads as "the screen refuses to scroll". So this lays out
 * fixed lines that fit 128x64 with nothing to scroll to. */
static void meshcore_scene_logger_show_running(MeshCoreApp* app) {
    const MeshCoreLogNode* node = meshcore_logger_node(app->logger);

    char line[48];
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 0, 10, AlignLeft, AlignBottom, FontPrimary, "Logging");

    snprintf(line, sizeof(line), "%.20s", node->name);
    widget_add_string_element(app->widget, 64, 10, AlignLeft, AlignBottom, FontSecondary, line);

    /* SNR gets the primary font and the top line, because it is the number the
     * operator is actually walking by — everything else is confirmation that
     * the thing is alive. The grade word turns the raw number into the
     * acceptance verdict (GOOD/OK/WEAK against +5 dB) so nobody does quarter-dB
     * arithmetic while walking a riverbank. */
    int8_t snr_q4 = 0;
    int8_t rssi = 0;
    if(meshcore_logger_last_rx(app->logger, &snr_q4, &rssi)) {
        char snr[MESHCORE_SNR_LEN];
        meshcore_rxlog_format_snr(snr_q4, snr, sizeof(snr));
        snprintf(line, sizeof(line), "SNR %s  RSSI %d", snr, (int)rssi);
        widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignBottom, FontPrimary, line);

        /* Right-aligned so it sits at the edge and reads as a verdict, not part
         * of the number. */
        const char* grade = meshcore_rxlog_grade_label(meshcore_rxlog_grade_snr(snr_q4));
        widget_add_string_element(app->widget, 128, 24, AlignRight, AlignBottom, FontPrimary, grade);
    } else {
        snprintf(line, sizeof(line), "waiting for traffic");
        widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignBottom, FontPrimary, line);
    }

    /* Packets prove the radio is heard at all; the ping ratio is the number the
     * acceptance criteria are written in, so both belong on one line. */
    uint32_t sent = 0;
    uint32_t ok = 0;
    meshcore_logger_ping_stats(app->logger, &sent, &ok);
    if(sent > 0) {
        snprintf(
            line,
            sizeof(line),
            "pkt %lu   ping %lu/%lu",
            (unsigned long)meshcore_logger_rx_count(app->logger),
            (unsigned long)ok,
            (unsigned long)sent);
    } else {
        snprintf(
            line, sizeof(line), "pkt %lu", (unsigned long)meshcore_logger_rx_count(app->logger));
    }
    widget_add_string_element(app->widget, 0, 36, AlignLeft, AlignBottom, FontSecondary, line);

    uint32_t dropped = meshcore_logger_dropped(app->logger);
    if(dropped > 0) {
        /* Only shown when it happens — a dropped row means the card could not
         * keep up, which the user needs to know about while still recording,
         * and it displaces the session name because it matters more. */
        snprintf(line, sizeof(line), "dropped: %lu", (unsigned long)dropped);
    } else {
        /* Just the session name; the full path would not fit and the user
         * already knows the directory it lives in. */
        const char* path = meshcore_logger_session_path(app->logger);
        const char* leaf = strrchr(path, '/');
        snprintf(
            line,
            sizeof(line),
            "%s  marks %lu",
            leaf ? leaf + 1 : path,
            (unsigned long)meshcore_logger_marks(app->logger));
    }
    widget_add_string_element(app->widget, 0, 47, AlignLeft, AlignBottom, FontSecondary, line);

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Mark", meshcore_scene_logger_button, app);
}

/* Everything on screen, folded into one number. Redrawing only when this
 * changes is what keeps the display from flickering and from fighting the
 * user; folding *all* the counters in is what makes a mark appear the instant
 * it is pressed rather than at the next packet. */
static uint32_t meshcore_scene_logger_signature(MeshCoreApp* app) {
    uint32_t sent = 0;
    uint32_t ok = 0;
    meshcore_logger_ping_stats(app->logger, &sent, &ok);

    int8_t snr_q4 = 0;
    int8_t rssi = 0;
    meshcore_logger_last_rx(app->logger, &snr_q4, &rssi);

    /* The +1 keeps zero meaning "never drawn". */
    return 1u + meshcore_logger_rx_count(app->logger) * 7u +
           meshcore_logger_marks(app->logger) * 1000003u + sent * 101u + ok * 10007u +
           (uint32_t)(uint8_t)snr_q4 * 31u + (uint32_t)(uint8_t)rssi * 37u +
           meshcore_logger_dropped(app->logger) * 13u;
}

static void meshcore_scene_logger_show(MeshCoreApp* app, bool started) {
    if(!started) {
        meshcore_scene_logger_show_text(
            app,
            "\e#Logger\n"
            "Starting...\n\n"
            "13 = TX  ->  node RX\n"
            "14 = RX  <-  node TX\n"
            "18 = GND");
        return;
    }

    const char* error = meshcore_logger_error(app->logger);
    if(error) {
        char text[256];
        snprintf(text, sizeof(text), "\e#Logger failed\n%s", error);
        meshcore_scene_logger_show_text(app, text);
        return;
    }

    meshcore_scene_logger_show_running(app);
}

void meshcore_scene_logger_on_enter(void* context) {
    MeshCoreApp* app = context;

    meshcore_scene_logger_show(app, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);

    app->worker = furi_thread_alloc_ex(
        "MeshCoreLoggerUi", MESHCORE_LOGGER_WORKER_STACK, meshcore_logger_scene_worker, app);
    furi_thread_start(app->worker);
}

bool meshcore_scene_logger_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == MESHCORE_LOGGER_EVENT_STARTED) {
        /* The worker posted this as its last act, so joining is immediate.
         * Clearing it here is what lets the tick handler take over refreshing. */
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }
        meshcore_scene_logger_show(app, true);
        scene_manager_set_scene_state(
            app->scene_manager, MeshCoreSceneLogger, meshcore_scene_logger_signature(app));
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event == MESHCORE_LOGGER_EVENT_MARK) {
        /* Cheap enough for the GUI thread: it formats a row and posts it to the
         * writer, and never touches the card or the link. */
        meshcore_logger_mark(app->logger);
        meshcore_scene_logger_show(app, true);
        scene_manager_set_scene_state(
            app->scene_manager, MeshCoreSceneLogger, meshcore_scene_logger_signature(app));
        return true;
    }

    if(event.type == SceneManagerEventTypeTick) {
        /* Only once the worker is done, or the display would race the start. */
        if(!app->worker && meshcore_logger_is_running(app->logger)) {
            /* Redraw only when something on screen actually changed. Rebuilding
             * on every tick regardless would make the screen flicker and would
             * fight anything the user is doing with it. */
            uint32_t shown = scene_manager_get_scene_state(app->scene_manager, MeshCoreSceneLogger);
            uint32_t now = meshcore_scene_logger_signature(app);
            if(shown != now) {
                scene_manager_set_scene_state(app->scene_manager, MeshCoreSceneLogger, now);
                meshcore_scene_logger_show(app, true);
            }
        }
        return true;
    }

    return false;
}

void meshcore_scene_logger_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }

    meshcore_logger_stop(app->logger);
    widget_reset(app->widget);
}
