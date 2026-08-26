#include "../hotspot_arcade_i.h"
#include "../helpers/ha_session.h"

// The screen is 64px tall and rows are 8px, so this is what actually fits under the header.
// It used to be 6 against a roster of HA_MAX_PLAYERS, which silently dropped players 7-12
// off the bottom of a full room with no hint that they existed.
#define LB_ROWS 7

static void ha_leaderboard_render(HotspotArcadeApp* app) {
    widget_reset(app->widget);
    // Ranked on the EVENING (total), not the current game. Each game scores on its own
    // scale -- a trivia session runs to five figures, a werewolf win pays 1 -- so ranking
    // this screen on the per-game score made it a trivia leaderboard with other games'
    // players mixed in. The game's own number still shows, dimmer, in the middle column.
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Leaderboard");
    widget_add_line_element(app->widget, 0, 12, 127, 12);

    // Collect used players, then selection-sort the top LB_ROWS by score desc.
    int idx[HA_MAX_PLAYERS];
    int n = 0;
    for(int i = 0; i < HA_MAX_PLAYERS; i++)
        if(app->players[i].used) idx[n++] = i;

    if(n == 0) {
        widget_add_string_element(
            app->widget, 64, 30, AlignCenter, AlignTop, FontSecondary, "No players yet");
        return;
    }

    int shown = n < LB_ROWS ? n : LB_ROWS;
    for(int r = 0; r < shown; r++) {
        int best = r;
        for(int j = r + 1; j < n; j++)
            if(app->players[idx[j]].total > app->players[idx[best]].total) best = j;
        int t = idx[r];
        idx[r] = idx[best];
        idx[best] = t;

        HaPlayer* p = &app->players[idx[r]];
        FuriString* row = furi_string_alloc();
        int y = 15 + r * 8;
        furi_string_printf(row, "%d. %s", r + 1, p->nick);
        widget_add_string_element(
            app->widget, 0, y, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(row));
        // This game's own score, parked left of the total so the two never run together.
        // Blank while it is zero, which is every player between games.
        if(p->score) {
            furi_string_printf(row, "%ld", (long)p->score);
            widget_add_string_element(
                app->widget,
                100,
                y,
                AlignRight,
                AlignTop,
                FontSecondary,
                furi_string_get_cstr(row));
        }
        furi_string_printf(row, "%ld", (long)p->total);
        widget_add_string_element(
            app->widget, 127, y, AlignRight, AlignTop, FontSecondary, furi_string_get_cstr(row));
        furi_string_free(row);
    }
}

void hotspot_arcade_scene_leaderboard_on_enter(void* context) {
    HotspotArcadeApp* app = context;
    ha_leaderboard_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HaViewWidget);
}

bool hotspot_arcade_scene_leaderboard_on_event(void* context, SceneManagerEvent event) {
    HotspotArcadeApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == HaEventRefreshView) {
        ha_leaderboard_render(app);
        return true;
    }
    return false;
}

void hotspot_arcade_scene_leaderboard_on_exit(void* context) {
    HotspotArcadeApp* app = context;
    widget_reset(app->widget);
}
