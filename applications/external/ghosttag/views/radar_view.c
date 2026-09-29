#include "radar_view.h"
#include <furi.h>
#include <math.h>
#include <stdio.h>

#define RADAR_MAX_BLIPS 24
#define RADAR_2PI       6.28318530718f

/* Dial: left half, clear of the panel divider at x=54. */
#define CX 27
#define CY 32
#define R  18

/* Panel: right of the divider, above the footer strip. */
#define PANEL_X     57
#define PANEL_RIGHT 126
#define FOOT_TOP    53

struct RadarView {
    View* view;
    RadarViewCallback ok_cb;
    void* ok_ctx;
    RadarViewCallback help_cb;
    void* help_ctx;
};

typedef struct {
    RadarBlip blips[RADAR_MAX_BLIPS];
    size_t blip_count;
    size_t seen;
    size_t tags;
    size_t following;
    RadarLinkState link;
    uint8_t sweep; /* 0..255 bearing of the sweep line */
    uint8_t anim; /* free-running frame counter */
} RadarModel;

static void radar_point(uint8_t angle, float radius, int* x, int* y) {
    float rad = (float)angle * (RADAR_2PI / 256.0f);
    *x = CX + (int)(cosf(rad) * radius);
    *y = CY + (int)(sinf(rad) * radius);
}

/* Counters are drawn from a clamped copy: -Werror=format-truncation will not
 * accept a width it cannot prove, and the database is bounded anyway. */
static unsigned clamp_count(size_t n) {
    return (unsigned)(n > 999 ? 999 : n);
}

static void draw_panel_row(Canvas* canvas, const char* label, size_t value, int baseline) {
    char buf[8];
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, PANEL_X, baseline, label);
    snprintf(buf, sizeof(buf), "%u", clamp_count(value));
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, PANEL_RIGHT, baseline, AlignRight, AlignBottom, buf);
}

static void radar_view_draw(Canvas* canvas, void* model) {
    RadarModel* m = model;
    char buf[26];

    /* ---- header (rows 0..11, divider on 12) ---- */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, m->link == RadarLinkDemo ? "DEMO" : "HUNT");

    const char* chip;
    switch(m->link) {
    case RadarLinkDemo:
        chip = "SIMULATED";
        break;
    case RadarLinkUp:
        chip = "LINK";
        break;
    case RadarLinkLost:
        chip = "LINK LOST";
        break;
    default:
        chip = "NO BOARD";
        break;
    }
    canvas_draw_str_aligned(canvas, 114, 9, AlignRight, AlignBottom, chip);
    if(m->link == RadarLinkUp) {
        canvas_draw_disc(canvas, 122, 5, 2);
    } else if(m->link == RadarLinkDemo) {
        /* A hollow square, so DEMO never looks like a live link at a glance. */
        canvas_draw_frame(canvas, 119, 2, 6, 6);
    } else {
        canvas_draw_circle(canvas, 122, 5, 2);
    }
    canvas_draw_line(canvas, 0, 12, 127, 12);

    /* ---- range rings ----
     * Rings mean range and nothing else. There is deliberately no crosshair:
     * a cross through the middle reads as N/E/S/W, and GhostTag has no
     * antenna array and no compass, so it cannot know a bearing. */
    canvas_draw_circle(canvas, CX, CY, R);
    canvas_draw_circle(canvas, CX, CY, 12);
    canvas_draw_circle(canvas, CX, CY, 6);
    canvas_draw_dot(canvas, CX, CY);

    /* ---- sweep ---- */
    int ex, ey;
    radar_point(m->sweep, R, &ex, &ey);
    canvas_draw_line(canvas, CX, CY, ex, ey);
    canvas_draw_disc(canvas, ex, ey, 1);
    int tx, ty;
    radar_point((uint8_t)(m->sweep - 12), R - 4, &tx, &ty);
    canvas_draw_line(canvas, CX, CY, tx, ty);

    /* ---- blips: radius is measured signal, angle is an arbitrary slot ---- */
    for(size_t i = 0; i < m->blip_count; i++) {
        int rssi = m->blips[i].rssi;
        if(rssi < -100) rssi = -100;
        if(rssi > -40) rssi = -40;
        float near = (float)(rssi + 100) / 60.0f; /* 0 far .. 1 close */
        float br = (1.0f - near) * (float)(R - 3) + 2.0f;
        int bx, by;
        radar_point(m->blips[i].slot, br, &bx, &by);
        if(m->blips[i].following) {
            canvas_draw_disc(canvas, bx, by, 2);
            canvas_draw_circle(canvas, bx, by, 3 + (m->anim % 4));
        } else if(m->blips[i].threat) {
            canvas_draw_disc(canvas, bx, by, 1);
        } else {
            /* Ambient kit that is not a stalking risk: present, but quiet. */
            canvas_draw_dot(canvas, bx, by);
        }
    }

    /* ---- panel (x 55..127, rows 13..51) ---- */
    canvas_draw_line(canvas, 54, 13, 54, FOOT_TOP - 2);
    draw_panel_row(canvas, "SEEN", m->seen, 22);
    draw_panel_row(canvas, "TAGS", m->tags, 33);
    canvas_draw_line(canvas, 56, 37, PANEL_RIGHT, 37);

    if(m->following > 0) {
        canvas_draw_box(canvas, 55, 40, 73, 12);
        canvas_set_color(canvas, ColorWhite);
        draw_panel_row(canvas, "FOLLOW", m->following, 49);
        canvas_set_color(canvas, ColorBlack);
    } else {
        draw_panel_row(canvas, "FOLLOW", 0, 49);
    }

    /* ---- footer (rows 53..63) ----
     * Baseline 61, not 62: descenders drop about two rows below the baseline
     * and row 64 does not exist. */
    canvas_set_font(canvas, FontSecondary);
    if(m->following > 0) {
        canvas_draw_box(canvas, 0, FOOT_TOP, 128, 11);
        canvas_set_color(canvas, ColorWhite);
        /* Not "! %u FOLLOWING YOU". FontSecondary's space is under three
         * pixels, so a digit between two spaces closes up against its
         * neighbours and the alarm line rendered as "!1FOLLOWING YOU" on a
         * real device - on the one screen that most needs to be readable at a
         * glance. The count moves to a suffix where nothing can collapse into
         * it, and is omitted entirely when it is one. */
        if(m->following > 1) {
            snprintf(buf, sizeof(buf), "! FOLLOWING YOU x%u", clamp_count(m->following));
        } else {
            snprintf(buf, sizeof(buf), "! FOLLOWING YOU");
        }
        canvas_draw_str(canvas, 3, 61, buf);
        canvas_draw_str_aligned(canvas, 125, 61, AlignRight, AlignBottom, "OK");
        canvas_set_color(canvas, ColorBlack);
        return;
    }

    switch(m->link) {
    case RadarLinkWaiting:
        /* The single most common first-run state, so it gets the only text on
         * screen that says what to actually do about it. */
        canvas_draw_str(canvas, 2, 61, "No ESP32 board");
        canvas_draw_str_aligned(canvas, 125, 61, AlignRight, AlignBottom, "Left:help");
        break;
    case RadarLinkLost:
        canvas_draw_str(canvas, 2, 61, "Board went quiet");
        canvas_draw_str_aligned(canvas, 125, 61, AlignRight, AlignBottom, "Left:help");
        break;
    default: {
        static const char* const dots[] = {"", ".", "..", "..."};
        snprintf(buf, sizeof(buf), "Scanning%s", dots[m->anim % 4]);
        canvas_draw_str(canvas, 2, 61, buf);
        canvas_draw_str_aligned(canvas, 125, 61, AlignRight, AlignBottom, "OK:list");
        break;
    }
    }
}

static bool radar_view_input(InputEvent* event, void* context) {
    RadarView* radar = context;

    /* Accept Short OR Long. The firmware sends Long instead of Short once a key
     * is held past the long-press threshold, so a view that handles only Short
     * silently drops a slightly firm press - which is indistinguishable from a
     * dead button. Press/Repeat/Release are ignored so one press fires once. */
    if(event->type != InputTypeShort && event->type != InputTypeLong) return false;

    if(event->key == InputKeyOk) {
        if(radar->ok_cb) radar->ok_cb(radar->ok_ctx);
        return true;
    }
    if(event->key == InputKeyLeft) {
        if(radar->help_cb) radar->help_cb(radar->help_ctx);
        return true;
    }
    return false;
}

RadarView* radar_view_alloc(void) {
    RadarView* radar = malloc(sizeof(RadarView));
    memset(radar, 0, sizeof(RadarView));
    radar->view = view_alloc();
    view_set_context(radar->view, radar);
    view_set_draw_callback(radar->view, radar_view_draw);
    view_set_input_callback(radar->view, radar_view_input);
    view_allocate_model(radar->view, ViewModelTypeLocking, sizeof(RadarModel));
    return radar;
}

void radar_view_free(RadarView* radar) {
    furi_assert(radar);
    view_free(radar->view);
    free(radar);
}

View* radar_view_get_view(RadarView* radar) {
    furi_assert(radar);
    return radar->view;
}

void radar_view_set_data(
    RadarView* radar,
    const RadarBlip* blips,
    size_t blip_count,
    size_t seen,
    size_t tags,
    size_t following,
    RadarLinkState link) {
    furi_assert(radar);
    with_view_model(
        radar->view,
        RadarModel * m,
        {
            size_t n = blip_count < RADAR_MAX_BLIPS ? blip_count : RADAR_MAX_BLIPS;
            for(size_t i = 0; i < n; i++)
                m->blips[i] = blips[i];
            m->blip_count = n;
            m->seen = seen;
            m->tags = tags;
            m->following = following;
            m->link = link;
        },
        true);
}

void radar_view_tick(RadarView* radar) {
    furi_assert(radar);
    with_view_model(
        radar->view,
        RadarModel * m,
        {
            m->sweep += 7;
            m->anim++;
        },
        true);
}

void radar_view_set_ok_callback(RadarView* radar, RadarViewCallback cb, void* context) {
    furi_assert(radar);
    radar->ok_cb = cb;
    radar->ok_ctx = context;
}

void radar_view_set_help_callback(RadarView* radar, RadarViewCallback cb, void* context) {
    furi_assert(radar);
    radar->help_cb = cb;
    radar->help_ctx = context;
}
