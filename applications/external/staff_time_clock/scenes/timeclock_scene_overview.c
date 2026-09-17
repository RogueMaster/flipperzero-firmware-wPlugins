// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>

// Overview - a one-glance-per-person dashboard: name in the middle, today /
// week / month worked time and today's break underneath. Left/Right switches
// to the previous/next collaborator, so checking on everyone is just a few
// taps with no menus in between. The current index is kept in scene state so
// leaving and coming back resumes on the same person.

#define OVERVIEW_NAV 260u

static int overview_hhmm_to_minutes(const char* s) {
    if(strlen(s) < 5) return -1;
    int h = (s[0] - '0') * 10 + (s[1] - '0');
    int m = (s[3] - '0') * 10 + (s[4] - '0');
    if(h < 0 || h > 23 || m < 0 || m > 59) return -1;
    return h * 60 + m;
}

static uint32_t overview_week_minutes(const char* uid) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    uint32_t now_ts = datetime_datetime_to_timestamp(&now);
    uint32_t days = now_ts / 86400u;
    int dow = (int)((days + 4u) % 7u); // 0 = Sunday
    int since_monday = (dow + 6) % 7;
    uint32_t monday_ts = now_ts - (uint32_t)since_monday * 86400u;

    uint32_t total = 0;
    for(int i = 0; i < 7; i++) {
        DateTime d;
        datetime_timestamp_to_datetime(monday_ts + (uint32_t)i * 86400u, &d);
        char date[TC_DT_MAX];
        snprintf(date, sizeof(date), "%04u-%02u-%02u", d.year, d.month, d.day);
        total += tc_history_minutes_for_date(date, uid, NULL, 0, NULL, 0);
    }
    return total;
}

static void overview_refresh(TimeClock* app) {
    size_t idx = scene_manager_get_scene_state(app->scene_manager, TimeClockSceneOverview);

    if(app->badge_count == 0) {
        overview_view_set_empty(app->overview_view, tc_str(StrRegisterFirst));
        return;
    }
    if(idx >= app->badge_count) idx = 0;
    scene_manager_set_scene_state(app->scene_manager, TimeClockSceneOverview, idx);

    Badge* b = &app->badges[idx];

    char first_in[8] = {0};
    char last_out[8] = {0};
    uint32_t today_min =
        tc_history_today_minutes(b->uid, first_in, sizeof(first_in), last_out, sizeof(last_out));
    uint32_t brk = 0;
    if(first_in[0] && last_out[0]) {
        int fi = overview_hhmm_to_minutes(first_in);
        int lo = overview_hhmm_to_minutes(last_out);
        if(fi >= 0 && lo >= fi) {
            int d = (lo - fi) - (int)today_min;
            if(d > 0) brk = (uint32_t)d;
        }
    }

    uint32_t week_min = overview_week_minutes(b->uid);

    char date[TC_DT_MAX];
    tc_now_date(date, sizeof(date));
    char month[8];
    strncpy(month, date, 7);
    month[7] = '\0';
    uint32_t month_min = tc_history_month_minutes(month, b->uid);

    char line1[24], line2[24], line3[24], line4[24];
    snprintf(
        line1, sizeof(line1), "%s: %02lu:%02lu", tc_str(StrToday), today_min / 60, today_min % 60);
    snprintf(
        line2, sizeof(line2), "%s: %02lu:%02lu", tc_str(StrThisWeek), week_min / 60, week_min % 60);
    snprintf(
        line3,
        sizeof(line3),
        "%s: %02lu:%02lu",
        tc_str(StrThisMonth),
        month_min / 60,
        month_min % 60);
    snprintf(line4, sizeof(line4), "%s: %02lu:%02lu", tc_str(StrBreak), brk / 60, brk % 60);

    overview_view_set_person(
        app->overview_view, b->name, line1, line2, line3, line4, idx + 1, app->badge_count);
}

static void overview_nav_callback(int direction, void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, OVERVIEW_NAV + (direction > 0 ? 1u : 0u));
}

void timeclock_scene_overview_on_enter(void* context) {
    TimeClock* app = context;
    overview_view_set_nav_callback(app->overview_view, overview_nav_callback, app);
    overview_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewOverview);
}

bool timeclock_scene_overview_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && app->badge_count > 0) {
        int dir = (event.event == OVERVIEW_NAV + 1u) ? 1 : (event.event == OVERVIEW_NAV) ? -1 : 0;
        if(dir != 0) {
            size_t idx = scene_manager_get_scene_state(app->scene_manager, TimeClockSceneOverview);
            size_t count = app->badge_count;
            idx = (size_t)(((long)idx + dir + (long)count) % (long)count);
            scene_manager_set_scene_state(app->scene_manager, TimeClockSceneOverview, idx);
            overview_refresh(app);
            consumed = true;
        }
    }

    return consumed;
}

void timeclock_scene_overview_on_exit(void* context) {
    UNUSED(context);
}
