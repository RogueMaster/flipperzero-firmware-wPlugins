#include "meshcore_cfg.h"

#include "detect/meshcore_detect.h"

static bool meshcore_cfg_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    MeshCoreApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

/* Returning false here pops the last scene and stops the ViewDispatcher,
 * which is how Back on the root scene exits the app. */
static bool meshcore_cfg_back_event_callback(void* context) {
    furi_assert(context);
    MeshCoreApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void meshcore_cfg_tick_event_callback(void* context) {
    furi_assert(context);
    MeshCoreApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* Runs on the session worker thread whenever the node sends something nobody
 * asked for. Must stay trivial: no blocking, no GUI. Stage 2 replaces this
 * with real handling of MSG_WAITING. */
static void meshcore_cfg_push_callback(
    const mc_event_t* event,
    const uint8_t* payload,
    size_t len,
    void* context) {
    MeshCoreApp* app = context;
    UNUSED(payload);
    UNUSED(len);

    /* "Drain me": the node has mail for us. Waking the mailbox only sets an
     * event flag, which is all this callback may do — it runs on the session
     * worker, and draining blocks on that same worker's replies. */
    if(event->code == MC_PUSH_MSG_WAITING) {
        meshcore_mailbox_notify(app->mailbox);
    }
}

static MeshCoreApp* meshcore_cfg_app_alloc(void) {
    MeshCoreApp* app = malloc(sizeof(MeshCoreApp));

    /* Tell any node wired to us that the Flipper is here, before anything
     * else: a node sampling its detect input while we were still booting
     * would come up on radio instead of serial. */
    meshcore_detect_init();

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&meshcore_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, meshcore_cfg_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, meshcore_cfg_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, meshcore_cfg_tick_event_callback, MESHCORE_TICK_PERIOD_MS);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MeshCoreViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, MeshCoreViewWidget, widget_get_view(app->widget));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MeshCoreViewTextBox, text_box_get_view(app->text_box));

    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MeshCoreViewLoading, loading_get_view(app->loading));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MeshCoreViewTextInput, text_input_get_view(app->text_input));

    app->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MeshCoreViewVarList, variable_item_list_get_view(app->var_list));

    app->splash_view = meshcore_scene_splash_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, MeshCoreViewSplash, app->splash_view);
    app->splash_timer = NULL;

    app->log = meshcore_log_alloc();
    /* Configurator and Messenger share one node on the USART. The Logger runs
     * its own session, so it can sit on either port. */
    app->session = meshcore_session_alloc(app->log, FuriHalSerialIdUsart);
    app->logger = meshcore_logger_alloc(app->log);
    memset(&app->node, 0, sizeof(app->node));

    meshcore_contacts_reset(&app->contacts);
    meshcore_messages_reset(&app->messages);
    /* Built-ins only for now; the card is read the first time Profiles opens,
     * so a session that never goes there never waits on storage. */
    meshcore_preset_store_init(&app->presets);
    app->preset_index = 0;
    memset(app->apply_result, 0, sizeof(app->apply_result));
    memset(app->chat_peer, 0, sizeof(app->chat_peer));
    app->chat_peer_name[0] = '\0';
    app->chat_is_channel = false;
    app->chat_channel_idx = 0;
    app->node_time = 0;

    /* The mailbox must exist before the push callback can wake it. Its worker
     * runs for the app's lifetime and no-ops until the session is connected,
     * so mail queued on the node is picked up as soon as we attach. */
    app->mailbox = meshcore_mailbox_alloc(app->session, &app->messages);
    /* Load the durable history into the ring before the worker can touch it, so
     * the chat opens with what was saved, and point the mailbox at the file so
     * new mail is persisted as it is drained. */
    app->msglog = meshcore_msglog_alloc();
    meshcore_msglog_load(app->msglog, &app->messages);
    meshcore_mailbox_set_msglog(app->mailbox, app->msglog);
    meshcore_session_set_event_callback(app->session, meshcore_cfg_push_callback, app);
    meshcore_mailbox_start(app->mailbox);

    app->worker = NULL;
    app->worker_stop = false;
    app->worker_error = NULL;

    app->text_buf[0] = furi_string_alloc();
    app->text_buf[1] = furi_string_alloc();
    app->text_buf_slot = 0;

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void meshcore_cfg_app_free(MeshCoreApp* app) {
    furi_assert(app);

    /* Scenes join their own workers on exit, so nothing should be running by
     * the time we get here — except the two app-lifetime workers, which have a
     * mutual dependency: the mailbox worker calls into the session, and the
     * session worker's push callback calls into the mailbox
     * (meshcore_cfg_push_callback -> meshcore_mailbox_notify). Freeing the
     * mailbox while the session worker is still pumping lets a MSG_WAITING push
     * fire the callback on freed memory.
     *
     * So stop the SESSION worker first (no more push callbacks) while the
     * session object stays alive, so the mailbox worker's in-flight calls just
     * no-op against a stopped session; then free the mailbox; then the rest.
     * Reachable since the messenger auto-connects — before that the session was
     * only running at exit if the user had pressed Connect. */
    meshcore_session_stop(app->session);
    meshcore_mailbox_free(app->mailbox);
    /* After the mailbox worker has stopped, so nothing appends to it any more. */
    meshcore_msglog_free(app->msglog);
    meshcore_logger_free(app->logger);
    meshcore_session_free(app->session);
    /* After everything that logs has stopped, so the file has the whole run in
     * it, and before the log itself goes away. */
    meshcore_log_dump(app->log);
    meshcore_log_free(app->log);

    view_dispatcher_remove_view(app->view_dispatcher, MeshCoreViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, MeshCoreViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, MeshCoreViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, MeshCoreViewLoading);
    view_dispatcher_remove_view(app->view_dispatcher, MeshCoreViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, MeshCoreViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, MeshCoreViewSplash);

    submenu_free(app->submenu);
    widget_free(app->widget);
    text_box_free(app->text_box);
    loading_free(app->loading);
    text_input_free(app->text_input);
    variable_item_list_free(app->var_list);
    view_free(app->splash_view);

    furi_string_free(app->text_buf[0]);
    furi_string_free(app->text_buf[1]);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    /* Release the detect lines last, so a node rebooted after we exit sees
     * LOW and brings its own radio up. */
    meshcore_detect_deinit();
    free(app);
}

/* Launch argument -> the screen to open on. The menu is always underneath, so
 * Back still goes where the user expects.
 *
 * For driving the app from a laptop:
 *
 *     loader open /ext/apps/GPIO/meshcore_cfg.fap logger
 *     input send ok short
 *
 * `input send` does reach a running application, so this is a convenience
 * rather than the only way in — it skips walking the menu, which matters when
 * the thing under test is three screens deep and the run has to be repeatable.
 * `loader open "<App Name>"` does not resolve external FAPs; the path form is
 * the one that takes an argument. */
static const struct {
    const char* name;
    MeshCoreSceneId scene;
} meshcore_cfg_launch_args[] = {
    {"logger", MeshCoreSceneLogger},
    {"connect", MeshCoreSceneConnect},
    {"contacts", MeshCoreSceneContacts},
    {"channels", MeshCoreSceneChannels},
    {"channeladd", MeshCoreSceneChannelAdd},
    {"addcontact", MeshCoreSceneAddContact},
    {"mycard", MeshCoreSceneMyCard},
    {"import", MeshCoreSceneImport},
    {"profiles", MeshCoreSceneProfiles},
    {"radio", MeshCoreSceneRadio},
    {"identity", MeshCoreSceneIdentity},
    {"advert", MeshCoreSceneAdvert},
    {"log", MeshCoreSceneLog},
};

int32_t meshcore_cfg_app(void* p) {
    const char* arg = p;

    MeshCoreApp* app = meshcore_cfg_app_alloc();
    app->launch_scene = MeshCoreSceneNum;

    if(arg != NULL && arg[0] != '\0') {
        for(size_t i = 0; i < COUNT_OF(meshcore_cfg_launch_args); i++) {
            if(strcmp(arg, meshcore_cfg_launch_args[i].name) == 0) {
                app->launch_scene = meshcore_cfg_launch_args[i].scene;
                break;
            }
        }
        /* An unknown argument leaves the menu open rather than failing to
         * start: a typo should not look like a crash. */
    }

    /* The menu opens the requested scene on its first tick, once the dispatcher
     * is serving its queue. Doing it here instead crashes: a scene whose
     * on_enter starts a worker would have that worker post an event into a
     * queue nobody is reading yet. */
    scene_manager_next_scene(app->scene_manager, MeshCoreSceneMenu);

    /* On a normal launch, play the splash on top of the menu; it pops itself
     * when done. A launch argument means a script is driving, so skip straight
     * past it. */
    if(app->launch_scene == MeshCoreSceneNum) {
        scene_manager_next_scene(app->scene_manager, MeshCoreSceneSplash);
    }

    view_dispatcher_run(app->view_dispatcher);
    meshcore_cfg_app_free(app);

    return 0;
}
