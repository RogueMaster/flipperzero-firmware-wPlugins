#include "hotspot_arcade_i.h"
#include "helpers/ha_storage.h"
#include "helpers/ha_session.h"

static bool ha_custom_event_callback(void* context, uint32_t event) {
    HotspotArcadeApp* app = context;
    if(event == HaEventRxData) {
        if(app->session_active) {
            ha_session_rx(app);
            return scene_manager_handle_custom_event(app->scene_manager, HaEventRefreshView);
        }
        // Idle: still parse RX so the board's PING beacon is tracked (last_ping_tick)
        // for firmware detection before a session starts. Non-PING/STATUS frames are
        // ignored while idle (guarded in dispatch_frame).
        ha_session_rx(app);
        return true;
    }
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool ha_back_event_callback(void* context) {
    HotspotArcadeApp* app = context;
    if(app->flashing) return true; // swallow Back: a flash can't be safely aborted mid-write
    return scene_manager_handle_back_event(app->scene_manager);
}

static void ha_tick_callback(void* context) {
    HotspotArcadeApp* app = context;
    if(app->awaiting_board) {
        if(furi_get_tick() - app->last_ping_tick < 2500) {
            app->awaiting_board = false;
            scene_manager_handle_custom_event(app->scene_manager, HaEventDetectBoard);
        }
        return;
    }
    if(!app->session_active) return;

    // Handshake watchdog: we got past board-detection but the ESP isn't acking our
    // protocol (wrong or absent firmware). Don't hang on "Preparing board..." forever;
    // drop to the "no board -> Install firmware" prompt so the user can flash the
    // right firmware on-device. (A board running our firmware keeps the watchdog fed
    // via on_status, so this only fires for the wrong-firmware case.)
    if(app->hs > HaHsIdle && app->hs < HaHsUp &&
       (furi_get_tick() - app->last_handshake_tick) > HA_HANDSHAKE_TIMEOUT_MS) {
        app->session_active = false;
        app->portal_running = false;
        app->hs = HaHsIdle;
        app->link_lost = false;
        furi_string_set(app->status, "noboard");
        scene_manager_handle_custom_event(app->scene_manager, HaEventRefreshView);
        return;
    }

    bool stale = (furi_get_tick() - app->last_rx_tick) > HA_LINK_TIMEOUT_MS;
    if(stale && !app->link_lost) {
        // Board went quiet. Keep the session owned and flag the link so the
        // dashboard shows "Reconnecting..."; if the board comes back (PING resumes,
        // or it reboots and re-handshakes) the session recovers on its own.
        app->link_lost = true;
        scene_manager_handle_custom_event(app->scene_manager, HaEventRefreshView);
    }
}

// Runs on the UART worker thread: just wake the GUI to drain/parse RX.
static void ha_uart_notify(void* ctx) {
    HotspotArcadeApp* app = ctx;
    if(app->closing) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, HaEventRxData);
}

static HotspotArcadeApp* ha_app_alloc(void) {
    HotspotArcadeApp* app = malloc(sizeof(HotspotArcadeApp));
    memset(app, 0, sizeof(HotspotArcadeApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&ha_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, ha_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, ha_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, ha_tick_callback, 1000);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, HaViewSubmenu, submenu_get_view(app->submenu));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HaViewTextInput, text_input_get_view(app->text_input));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, HaViewWidget, widget_get_view(app->widget));
    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HaViewVarItemList, variable_item_list_get_view(app->var_item_list));

    app->ssid = furi_string_alloc();
    app->status = furi_string_alloc_set_str("idle");
    app->console = furi_string_alloc();
    app->last_event = furi_string_alloc();
    app->flash_manifest = furi_string_alloc();

    app->sound_on = true;
    app->vibro_on = true;
    app->active_game = HA_GAME_NONE;
    ha_storage_ensure_dirs();
    ha_storage_load_config(app);

    app->uart = ha_uart_init(HA_UART_BAUD, ha_uart_notify, app);

    scene_manager_next_scene(app->scene_manager, HaSceneMainMenu);
    return app;
}

static void ha_app_free(HotspotArcadeApp* app) {
    app->closing = true;
    ha_uart_stop_rx(app->uart);

    // AP down: reset the ESP (its reply is ignored since RX is off).
    ha_proto_send(app->uart, HA_MSG_RESET, NULL, 0);
    furi_delay_ms(20);

    view_dispatcher_remove_view(app->view_dispatcher, HaViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, HaViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, HaViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, HaViewVarItemList);

    submenu_free(app->submenu);
    text_input_free(app->text_input);
    widget_free(app->widget);
    variable_item_list_free(app->var_item_list);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    ha_uart_deinit(app->uart);

    furi_string_free(app->ssid);
    furi_string_free(app->status);
    furi_string_free(app->console);
    furi_string_free(app->last_event);
    furi_string_free(app->flash_manifest);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t hotspot_arcade_app(void* p) {
    UNUSED(p);
    HotspotArcadeApp* app = ha_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    ha_app_free(app);
    return 0;
}
