#include "../hotspot_arcade_i.h"
#include "../helpers/ha_session.h"

#define LB_ROWS 6

static void ha_leaderboard_render(HotspotArcadeApp* app) {
    widget_reset(app->widget);
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
            if(app->players[idx[j]].score > app->players[idx[best]].score) best = j;
        int t = idx[r];
        idx[r] = idx[best];
        idx[best] = t;

        HaPlayer* p = &app->players[idx[r]];
        FuriString* row = furi_string_alloc();
        furi_string_printf(row, "%d. %s", r + 1, p->nick);
        widget_add_string_element(
            app->widget,
            0,
            15 + r * 8,
            AlignLeft,
            AlignTop,
            FontSecondary,
            furi_string_get_cstr(row));
        furi_string_printf(row, "%ld", (long)p->score);
        widget_add_string_element(
            app->widget,
            127,
            15 + r * 8,
            AlignRight,
            AlignTop,
            FontSecondary,
            furi_string_get_cstr(row));
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
