#include <furi.h>
#include <furi_hal_power.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <math.h>
#include <stdio.h>

#include "app.h"
#include "track.h"
#include "storage_helpers.h"

// Distance équirectangulaire en mètres (<1% d'erreur sur quelques km).
static float approx_distance_m(float lat1, float lon1, float lat2, float lon2) {
    const float DEG_TO_RAD = 0.01745329f;
    const float M_PER_DEG_LAT = 110574.0f;
    float avg_lat_rad = (lat1 + lat2) * 0.5f * DEG_TO_RAD;
    float m_per_deg_lon = 111320.0f * cosf(avg_lat_rad);
    float dlat = (lat2 - lat1) * M_PER_DEG_LAT;
    float dlon = (lon2 - lon1) * m_per_deg_lon;
    return sqrtf(dlat * dlat + dlon * dlon);
}

// Intervalle entre deux trkpts : lu depuis app->settings.track_interval_s
// au moment de l'entrée dans la vue. Default 5 s si settings pas chargés.

typedef struct {
    bool has_fix;
    float lat;
    float lon;
    float hdop;
    float altitude;
    float heading_deg;
    float speed_kmh;
    float total_dist_m;
    float max_speed_kmh;
    uint8_t sats;
    uint32_t points;
    uint32_t duration_s;
    uint8_t interval_s;
    bool active;
} TrackModel;

void track_refresh(App* app) {
    if(!app->track_view) return;

    uint32_t duration = 0;
    if(app->track_start_tick != 0) {
        uint32_t freq = furi_kernel_get_tick_frequency();
        if(freq == 0) freq = 1;
        duration = (furi_get_tick() - app->track_start_tick) / freq;
    }
    // Hystérésis 5s pour l'affichage (la logique de write trkpt reste sur
    // app->has_fix réel dans track_timer_callback).
    bool display_has_fix = app->has_fix;
    if(!display_has_fix && app->last_fix_tick != 0) {
        uint32_t freq = furi_kernel_get_tick_frequency();
        if(freq == 0) freq = 1;
        uint32_t age = (furi_get_tick() - app->last_fix_tick) / freq;
        if(age < 5) display_has_fix = true;
    }

    with_view_model(
        app->track_view,
        TrackModel * m,
        {
            m->has_fix = display_has_fix;
            m->lat = app->lat;
            m->lon = app->lon;
            m->hdop = app->hdop;
            m->altitude = app->altitude;
            m->heading_deg = app->heading_deg;
            m->speed_kmh = app->speed_knots * 1.852f;
            m->total_dist_m = app->track_total_dist_m;
            m->max_speed_kmh = app->track_max_speed_kmh;
            m->sats = app->sats;
            m->points = app->track_points;
            m->duration_s = duration;
            m->interval_s = app->settings.track_interval_s;
            m->active = (app->track_timer != NULL);
        },
        true);
}

// Callback du timer : écrit un trkpt si on a un fix (et si assez éloigné du précédent)
static void track_timer_callback(void* ctx) {
    App* app = (App*)ctx;
    if(!app->has_fix) return;

    // Si HDOP strict activé, refuser les fixes dégradés (seuil = settings.hdop_max_x10)
    if(app->settings.track_hdop_strict && app->settings.hdop_max_x10 > 0) {
        float hdop_max = (float)app->settings.hdop_max_x10 / 10.0f;
        if(app->hdop > hdop_max) return;
    }

    // Filtre de distance : skip si on n'a pas assez bougé depuis le dernier trkpt.
    uint8_t min_dist = app->settings.track_min_dist_m;
    if(min_dist > 0 && app->last_trk_valid && !app->track_new_segment) {
        float d = approx_distance_m(
            app->last_trk_lat, app->last_trk_lon, app->lat, app->lon);
        if(d < (float)min_dist) return;
    }

    // Cumul de distance depuis le trkpt précédent (si on en a un et qu'on
    // n'ouvre pas un nouveau segment)
    if(app->last_trk_valid && !app->track_new_segment) {
        app->track_total_dist_m += approx_distance_m(
            app->last_trk_lat, app->last_trk_lon, app->lat, app->lon);
    }

    // Vitesse max : kmh = knots * 1.852
    float kmh = app->speed_knots * 1.852f;
    if(kmh > app->track_max_speed_kmh) app->track_max_speed_kmh = kmh;

    storage_append_trkpt(app->lat, app->lon, app->altitude, app->track_new_segment);
    app->track_new_segment = false;
    app->track_points++;
    app->last_trk_lat = app->lat;
    app->last_trk_lon = app->lon;
    app->last_trk_valid = true;
}

static void track_draw_callback(Canvas* canvas, void* ctx) {
    TrackModel* m = (TrackModel*)ctx;

    canvas_clear(canvas);

    char bat[8];
    snprintf(bat, sizeof(bat), "%u%%", (unsigned)furi_hal_power_get_pct());
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 127, 2, AlignRight, AlignTop, bat);

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(
        canvas, 64, 2, AlignCenter, AlignTop, m->active ? "REC" : "Idle");

    canvas_set_font(canvas, FontSecondary);

    char line1[48];
    char line2[48];
    if(m->has_fix) {
        snprintf(line1, sizeof(line1), "%.5f, %.5f", (double)m->lat, (double)m->lon);
        snprintf(
            line2,
            sizeof(line2),
            "HDOP=%.1f sats=%u hdg=%.0f\xb0",
            (double)m->hdop, m->sats, (double)m->heading_deg);
    } else {
        snprintf(line1, sizeof(line1), "Waiting for fix...");
        snprintf(line2, sizeof(line2), "sats=%u", m->sats);
    }
    elements_multiline_text_aligned(canvas, 64, 16, AlignCenter, AlignTop, line1);
    elements_multiline_text_aligned(canvas, 64, 26, AlignCenter, AlignTop, line2);

    // Ligne stats : pts + durée
    char line3[48];
    uint32_t h = m->duration_s / 3600;
    uint32_t min = (m->duration_s / 60) % 60;
    uint32_t s = m->duration_s % 60;
    snprintf(
        line3, sizeof(line3), "pts=%lu  %02lu:%02lu:%02lu",
        (unsigned long)m->points,
        (unsigned long)h, (unsigned long)min, (unsigned long)s);
    elements_multiline_text_aligned(canvas, 64, 36, AlignCenter, AlignTop, line3);

    // Ligne stats : distance totale + vitesse inst + max
    char line4[48];
    if(m->total_dist_m >= 1000.0f) {
        snprintf(
            line4, sizeof(line4),
            "%.2fkm v=%.0f max=%.0fkm/h",
            (double)(m->total_dist_m / 1000.0f),
            (double)m->speed_kmh,
            (double)m->max_speed_kmh);
    } else {
        snprintf(
            line4, sizeof(line4),
            "%.0fm v=%.0f max=%.0fkm/h",
            (double)m->total_dist_m,
            (double)m->speed_kmh,
            (double)m->max_speed_kmh);
    }
    elements_multiline_text_aligned(canvas, 64, 46, AlignCenter, AlignTop, line4);

    char footer[48];
    snprintf(
        footer, sizeof(footer), "Auto-log %us  Back to stop", (unsigned)m->interval_s);
    elements_multiline_text_aligned(canvas, 64, 62, AlignCenter, AlignBottom, footer);
}

static bool track_input_callback(InputEvent* event, void* ctx) {
    (void)event;
    (void)ctx;
    return false; // Back -> previous_callback -> menu
}

static uint32_t track_previous_callback(void* ctx) {
    (void)ctx;
    return AppViewMenu;
}

static void track_enter(void* ctx) {
    App* app = (App*)ctx;

    // Démarre la session : flag nouveau segment, reset compteurs + filtre distance
    app->track_new_segment = true;
    app->track_points = 0;
    app->track_start_tick = furi_get_tick();
    app->last_trk_valid = false;
    app->track_total_dist_m = 0.0f;
    app->track_max_speed_kmh = 0.0f;

    if(!app->track_timer) {
        app->track_timer =
            furi_timer_alloc(track_timer_callback, FuriTimerTypePeriodic, app);
    }
    if(app->track_timer) {
        uint32_t interval_ms = app->settings.track_interval_s > 0
                                   ? (uint32_t)app->settings.track_interval_s * 1000u
                                   : 5000u;
        furi_timer_start(app->track_timer, furi_ms_to_ticks(interval_ms));
    }

    track_refresh(app);
}

static void track_exit(void* ctx) {
    App* app = (App*)ctx;
    if(app->track_timer) {
        furi_timer_stop(app->track_timer);
    }
}

void app_start_track(App* app) {
    if(!app->track_view) {
        View* view = view_alloc();
        view_allocate_model(view, ViewModelTypeLockFree, sizeof(TrackModel));
        view_set_context(view, app);
        view_set_draw_callback(view, track_draw_callback);
        view_set_input_callback(view, track_input_callback);
        view_set_enter_callback(view, track_enter);
        view_set_exit_callback(view, track_exit);
        view_set_previous_callback(view, track_previous_callback);

        app->track_view = view;
        view_dispatcher_add_view(app->dispatcher, AppViewTrack, view);
    }
    view_dispatcher_switch_to_view(app->dispatcher, AppViewTrack);
}

void app_stop_track(App* app) {
    if(app->track_timer) {
        furi_timer_stop(app->track_timer);
        furi_timer_free(app->track_timer);
        app->track_timer = NULL;
    }
    if(app->track_view) {
        view_dispatcher_remove_view(app->dispatcher, AppViewTrack);
        view_free(app->track_view);
        app->track_view = NULL;
    }
}
