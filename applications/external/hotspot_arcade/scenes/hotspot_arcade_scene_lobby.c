#include "../hotspot_arcade_i.h"
#include "../helpers/ha_session.h"

static const char* game_name(uint8_t g) {
    switch(g) {
    case HA_GAME_TRIVIA:
        return "Trivia";
    case HA_GAME_CONNECT4:
        return "Connect 4";
    case HA_GAME_TICTACTOE:
        return "Tic-Tac-Toe";
    case HA_GAME_DOTS:
        return "Dots & Boxes";
    case HA_GAME_REVERSI:
        return "Reversi";
    case HA_GAME_DRAW:
        return "Drawing";
    case HA_GAME_PONG:
        return "Pong";
    case HA_GAME_REACT:
        return "Reaction Duel";
    case HA_GAME_WYR:
        return "Would You Rather";
    case HA_GAME_SCRAMBLE:
        return "Word Scramble";
    default:
        return "None";
    }
}

static void ha_lobby_button_cb(GuiButtonType result, InputType type, void* context) {
    HotspotArcadeApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, HaEventPickGame);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, HaEventShowLeaderboard);
    }
}

// "Install firmware" on the no-board prompt (added for the on-device flasher).
static void ha_noboard_button_cb(GuiButtonType result, InputType type, void* context) {
    HotspotArcadeApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter)
        view_dispatcher_send_custom_event(app->view_dispatcher, HaEventInstallFirmware);
}

static void ha_status_screen(HotspotArcadeApp* app, const char* l1, const char* l2) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Hotspot Arcade");
    widget_add_line_element(app->widget, 0, 20, 127, 20);
    widget_add_string_element(app->widget, 64, 27, AlignCenter, AlignTop, FontSecondary, l1);
    if(l2 && l2[0])
        widget_add_string_element(app->widget, 64, 42, AlignCenter, AlignTop, FontSecondary, l2);
}

// Map the handshake stage to a label + a 0..100 progress estimate.
static const char* ha_stage_label(HotspotArcadeApp* app) {
    switch(app->hs) {
    case HaHsClear:
        return "Preparing board...";
    case HaHsFiles:
        return "Uploading game...";
    case HaHsSetAp:
        return "Naming hotspot...";
    case HaHsStart:
        return "Starting hotspot...";
    default:
        return "Starting session...";
    }
}

static int ha_stage_progress(HotspotArcadeApp* app) {
    switch(app->hs) {
    case HaHsClear:
        return 8;
    case HaHsFiles: {
        int n = app->asset_count ? app->asset_count : 1;
        return 12 + (73 * app->file_idx) / n;
    }
    case HaHsSetAp:
        return 88;
    case HaHsStart:
        return 96;
    case HaHsUp:
        return 100;
    default:
        return 0;
    }
}

// Title + a labelled progress bar for the start/reconnect sequence.
static void ha_progress_screen(HotspotArcadeApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Hotspot Arcade");
    widget_add_line_element(app->widget, 0, 18, 127, 18);
    widget_add_string_element(
        app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, ha_stage_label(app));

    // Bar: a 1px frame with a filled interior proportional to progress.
    widget_add_frame_element(app->widget, 14, 44, 100, 10, 1);
    int pct = ha_stage_progress(app);
    if(pct < 0) pct = 0;
    if(pct > 100) pct = 100;
    int w = (96 * pct) / 100;
    if(w > 0) widget_add_rect_element(app->widget, 16, 46, w, 6, 0, true);
}

static void ha_dashboard(HotspotArcadeApp* app) {
    widget_reset(app->widget);
    FuriString* tmp = furi_string_alloc();

    // Header = the live status (the thing the host most needs to see), with a
    // filled dot when broadcasting.
    bool live = app->portal_running && !app->link_lost;
    const char* state = app->link_lost ? "Reconnecting" : live ? "Broadcasting" : "Starting...";
    widget_add_circle_element(app->widget, 5, 6, 3, live);
    widget_add_string_element(app->widget, 13, 0, AlignLeft, AlignTop, FontPrimary, state);
    widget_add_line_element(app->widget, 0, 14, 127, 14);

    // How players join: the network name, then the address if the captive page
    // does not pop on its own.
    furi_string_printf(tmp, "Join: %s", furi_string_get_cstr(app->ssid));
    widget_add_string_element(
        app->widget, 0, 18, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));
    widget_add_string_element(
        app->widget, 0, 29, AlignLeft, AlignTop, FontSecondary, "then 192.168.4.1");

    // Player count (left) and the selected game (right) share one line so nothing
    // collides with the button row.
    furi_string_printf(tmp, "Players: %d", ha_player_count(app));
    widget_add_string_element(
        app->widget, 0, 40, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));
    widget_add_string_element(
        app->widget, 127, 40, AlignRight, AlignTop, FontSecondary, game_name(app->active_game));

    // Left picks the game; Right shows scores. Games are player-driven, so there is no
    // host-side game screen — the main menu's Console shows the live event feed.
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Games", ha_lobby_button_cb, app);
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Scores", ha_lobby_button_cb, app);

    furi_string_free(tmp);
}

static void ha_lobby_render(HotspotArcadeApp* app) {
    const char* s = furi_string_get_cstr(app->status);
    if(strcmp(s, "detecting") == 0) {
        ha_status_screen(app, "Detecting board...", "");
        return;
    }
    if(strcmp(s, "noboard") == 0) {
        // No magic beacon: the board may be attached but running other/no firmware,
        // or not attached at all. Don't claim "no board" - point at the firmware and
        // offer to install it (while auto-detection keeps running in the background).
        ha_status_screen(app, "Firmware needed", "Attach board, install:");
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Install fw", ha_noboard_button_cb, app);
        return;
    }
    if(strcmp(s, "outdated") == 0) {
        // Our board, but older firmware than this app needs: offer to update it.
        FuriString* l2 = furi_string_alloc();
        furi_string_printf(l2, "Board v%u, need v%u", app->board_fw_version, HA_FW_VERSION);
        ha_status_screen(app, "Update firmware?", furi_string_get_cstr(l2));
        furi_string_free(l2);
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Update fw", ha_noboard_button_cb, app);
        return;
    }
    if(app->portal_running || app->link_lost) {
        ha_dashboard(app);
        return;
    }
    if(strstr(s, "err") != NULL) {
        ha_status_screen(app, "Startup error", s);
        return;
    }
    ha_progress_screen(app);
}

void hotspot_arcade_scene_lobby_on_enter(void* context) {
    HotspotArcadeApp* app = context;
    if(!app->session_active) {
        app->awaiting_board = false;
        app->link_lost = false;
        furi_string_set(app->status, "detecting");
        ha_lobby_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, HaViewWidget);
        view_dispatcher_send_custom_event(app->view_dispatcher, HaEventDetectBoard);
    } else {
        ha_lobby_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, HaViewWidget);
    }
}

bool hotspot_arcade_scene_lobby_on_event(void* context, SceneManagerEvent event) {
    HotspotArcadeApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case HaEventDetectBoard:
        if(ha_board_present(app, 2500)) {
            if(app->board_fw_version < HA_FW_VERSION) {
                // Our board, but running an older firmware version: offer to update.
                furi_string_set(app->status, "outdated");
                ha_lobby_render(app);
            } else {
                furi_string_set(app->status, "starting");
                ha_lobby_render(app);
                view_dispatcher_send_custom_event(app->view_dispatcher, HaEventBeginStart);
            }
        } else {
            app->awaiting_board = true;
            furi_string_set(app->status, "noboard");
            ha_lobby_render(app);
        }
        return true;
    case HaEventBeginStart:
        ha_session_start(app); // blocks while the bundle streams
        ha_lobby_render(app);
        return true;
    case HaEventInstallFirmware:
        // Go flash the bundled firmware; on return this scene re-enters, re-detects
        // the now-flashed board, and continues the start. Stop watching while away.
        app->awaiting_board = false;
        furi_string_set(app->flash_manifest, HA_DEFAULT_FW);
        scene_manager_next_scene(app->scene_manager, HaSceneFlasher);
        return true;
    case HaEventRefreshView:
        ha_lobby_render(app);
        return true;
    case HaEventPickGame:
        scene_manager_next_scene(app->scene_manager, HaSceneGameSelect);
        return true;
    case HaEventShowLeaderboard:
        scene_manager_next_scene(app->scene_manager, HaSceneLeaderboard);
        return true;
    default:
        return false;
    }
}

void hotspot_arcade_scene_lobby_on_exit(void* context) {
    HotspotArcadeApp* app = context;
    app->awaiting_board = false;
}
