#include <furi.h>
#include <furi_hal_power.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdio.h>

#include "app.h"
#include "status.h"

typedef struct {
    bool has_fix;
    float lat;
    float lon;
    float altitude;
    float hdop;
    uint8_t sats;
    uint32_t fix_age_s;
    bool fix_ever;
    uint32_t nmea_bytes_rx;
    uint32_t nmea_lines_rx;
    char last_saved_preset[24];
    char last_saved_time[8];
} StatusModel;

static void status_draw_callback(Canvas* canvas, void* ctx) {
    StatusModel* m = (StatusModel*)ctx;

    canvas_clear(canvas);

    char bat[8];
    snprintf(bat, sizeof(bat), "%u%%", (unsigned)furi_hal_power_get_pct());
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 127, 2, AlignRight, AlignTop, bat);

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(
        canvas, 64, 2, AlignCenter, AlignTop, m->has_fix ? "GPS: FIX OK" : "GPS: NO FIX");

    canvas_set_font(canvas, FontSecondary);

    char line1[48];
    char line2[48];
    char line3[48];

    if(m->has_fix) {
        snprintf(line1, sizeof(line1), "%.6f, %.6f", (double)m->lat, (double)m->lon);
        float prec_m = m->hdop * 3.0f;
        snprintf(
            line2,
            sizeof(line2),
            "alt=%.1fm HDOP=%.1f ~%.0fm",
            (double)m->altitude,
            (double)m->hdop,
            (double)prec_m);
    } else {
        snprintf(line1, sizeof(line1), "--, --");
        snprintf(line2, sizeof(line2), "alt=-- HDOP=--");
    }

    if(m->fix_ever) {
        snprintf(
            line3,
            sizeof(line3),
            "sats=%u  fix=%lus",
            m->sats,
            (unsigned long)m->fix_age_s);
    } else {
        snprintf(line3, sizeof(line3), "sats=%u  fix=never", m->sats);
    }

    char line4[48];
    snprintf(
        line4,
        sizeof(line4),
        "NMEA: %lu B / %lu lines",
        (unsigned long)m->nmea_bytes_rx,
        (unsigned long)m->nmea_lines_rx);

    elements_multiline_text_aligned(canvas, 64, 18, AlignCenter, AlignTop, line1);
    elements_multiline_text_aligned(canvas, 64, 28, AlignCenter, AlignTop, line2);
    elements_multiline_text_aligned(canvas, 64, 38, AlignCenter, AlignTop, line3);
    elements_multiline_text_aligned(canvas, 64, 48, AlignCenter, AlignTop, line4);

    // Footer : last save si présent, sinon juste "Back"
    if(m->last_saved_preset[0]) {
        char footer[48];
        snprintf(footer, sizeof(footer), "last: %s @%s",
                 m->last_saved_preset, m->last_saved_time);
        elements_multiline_text_aligned(canvas, 64, 62, AlignCenter, AlignBottom, footer);
    } else {
        elements_multiline_text_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Back");
    }
}

static bool status_input_callback(InputEvent* event, void* ctx) {
    (void)event;
    (void)ctx;
    return false; // Back -> previous_callback -> menu
}

static uint32_t status_previous_callback(void* ctx) {
    (void)ctx;
    return AppViewMenu;
}

static void status_enter(void* ctx) {
    status_refresh((App*)ctx);
}
static void status_exit(void* ctx) {
    (void)ctx;
}

void status_refresh(App* app) {
    if(!app->status_view) return;

    uint32_t age_s = 0;
    bool fix_ever = (app->last_fix_tick != 0);
    if(fix_ever) {
        uint32_t freq = furi_kernel_get_tick_frequency();
        if(freq == 0) freq = 1;
        age_s = (furi_get_tick() - app->last_fix_tick) / freq;
    }
    // Hystérésis 5s (voir quick_log_refresh pour l'explication)
    bool display_has_fix = app->has_fix || (fix_ever && age_s < 5);

    with_view_model(
        app->status_view,
        StatusModel * m,
        {
            m->has_fix = display_has_fix;
            m->lat = app->lat;
            m->lon = app->lon;
            m->altitude = app->altitude;
            m->hdop = app->hdop;
            m->sats = app->sats;
            m->fix_age_s = age_s;
            m->fix_ever = fix_ever;
            m->nmea_bytes_rx = app->nmea_bytes_rx;
            m->nmea_lines_rx = app->nmea_lines_rx;
            strncpy(m->last_saved_preset, app->last_saved_preset,
                    sizeof(m->last_saved_preset) - 1);
            m->last_saved_preset[sizeof(m->last_saved_preset) - 1] = '\0';
            strncpy(m->last_saved_time, app->last_saved_time,
                    sizeof(m->last_saved_time) - 1);
            m->last_saved_time[sizeof(m->last_saved_time) - 1] = '\0';
        },
        true);
}

void app_start_status(App* app) {
    if(!app->status_view) {
        View* view = view_alloc();
        view_allocate_model(view, ViewModelTypeLockFree, sizeof(StatusModel));
        view_set_context(view, app);
        view_set_draw_callback(view, status_draw_callback);
        view_set_input_callback(view, status_input_callback);
        view_set_enter_callback(view, status_enter);
        view_set_exit_callback(view, status_exit);
        view_set_previous_callback(view, status_previous_callback);

        app->status_view = view;
        view_dispatcher_add_view(app->dispatcher, AppViewStatus, view);
    }
    view_dispatcher_switch_to_view(app->dispatcher, AppViewStatus);
}

void app_stop_status(App* app) {
    if(app->status_view) {
        view_dispatcher_remove_view(app->dispatcher, AppViewStatus);
        view_free(app->status_view);
        app->status_view = NULL;
    }
}
