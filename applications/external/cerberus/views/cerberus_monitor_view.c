#include "cerberus_monitor_view.h"
#include "../helpers/cerberus_detector.h"

#include <gui/canvas.h>
#include <input/input.h>
#include <stdio.h>

#define RSSI_FLOOR_VIEW (-110)
#define RSSI_CEIL_VIEW  (-25)

#define SCOPE_W 118 // history columns (== graph width)

typedef enum {
    MonitorModeMeters = 0,
    MonitorModeScope,
} MonitorMode;

struct CerberusMonitorView {
    View* view;
    CerberusMonitorCallback callback;
    void* context;
};

typedef struct {
    CerberusSnapshot snap;
    bool has_data;
    uint8_t frame; // animation counter for blinking

    MonitorMode mode;
    uint8_t scope_band; // band shown in scope mode

    int8_t hist[CERBERUS_BAND_COUNT][SCOPE_W]; // rolling RSSI history (dBm)
    uint8_t hist_head[CERBERUS_BAND_COUNT];

    bool armed; // title-bar status
    uint32_t alert_total;
} CerberusMonitorModel;

// Map a dBm reading onto a [0..width] bar length.
static uint8_t cerberus_rssi_to_w(int16_t rssi, uint8_t width) {
    int v = rssi;
    if(v < RSSI_FLOOR_VIEW) v = RSSI_FLOOR_VIEW;
    if(v > RSSI_CEIL_VIEW) v = RSSI_CEIL_VIEW;
    int span = RSSI_CEIL_VIEW - RSSI_FLOOR_VIEW;
    int fill = (v - RSSI_FLOOR_VIEW) * width / span;
    if(fill < 0) fill = 0;
    if(fill > width) fill = width;
    return (uint8_t)fill;
}

static int8_t cerberus_clamp8(int16_t v) {
    if(v < -120) v = -120;
    if(v > -10) v = -10;
    return (int8_t)v;
}

// ---------------------------------------------------------------------------
// Title bar (shared by both modes)
// ---------------------------------------------------------------------------
static void cerberus_draw_title(Canvas* canvas, CerberusMonitorModel* m, const char* mode_txt) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, "CERBERUS");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, mode_txt);

    // arm / silent indicator, sat just left of the mode text
    uint8_t mw = canvas_string_width(canvas, mode_txt);
    uint8_t ax = (uint8_t)(122 - mw - 4);
    if(m->armed) {
        canvas_draw_disc(canvas, ax, 5, 2); // filled = armed
    } else {
        canvas_draw_circle(canvas, ax, 5, 2); // hollow + slash = silent
        canvas_draw_line(canvas, ax - 2, 7, ax + 2, 3);
    }
    canvas_set_color(canvas, ColorBlack);
}

// ---------------------------------------------------------------------------
// Meters mode
// ---------------------------------------------------------------------------
static void cerberus_draw_meter(
    Canvas* canvas,
    uint8_t row,
    const CerberusBand* band,
    const CerberusBandStatus* status,
    bool active,
    bool blink) {
    const uint8_t row_top = 13 + row * 12;
    const uint8_t cy = row_top + 4;
    const bool alarm = status->threat != CerberusThreatNone;
    const bool invert = alarm && blink;

    if(invert) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, row_top - 1, 128, 12);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }

    if(active) {
        canvas_draw_line(canvas, 0, cy - 3, 0, cy + 3);
        canvas_draw_line(canvas, 1, cy - 2, 1, cy + 2);
        canvas_draw_line(canvas, 2, cy - 1, 2, cy + 1);
        canvas_draw_dot(canvas, 3, cy);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 6, row_top + 8, band->label);

    const uint8_t bx = 24, bw = 70, by = row_top + 1, bh = 7;
    canvas_draw_frame(canvas, bx - 1, by - 1, bw + 2, bh + 2);
    uint8_t fill = cerberus_rssi_to_w(status->rssi, bw);
    if(fill) canvas_draw_box(canvas, bx, by, fill, bh);

    uint8_t px = bx + cerberus_rssi_to_w(status->peak, bw);
    if(px > bx) {
        canvas_draw_line(canvas, px, by + bh, px, by + bh);
        canvas_draw_dot(canvas, px, by + bh + 1);
    }

    char buf[12];
    if(alarm) {
        snprintf(buf, sizeof(buf), "!%s", cerberus_threat_short(status->threat));
    } else {
        snprintf(buf, sizeof(buf), "%d", status->rssi);
    }
    canvas_draw_str_aligned(canvas, 127, row_top + 8, AlignRight, AlignBottom, buf);
}

static void cerberus_draw_meters(Canvas* canvas, CerberusMonitorModel* m) {
    const CerberusSnapshot* s = &m->snap;
    const bool blink = ((m->frame / 3) & 1) != 0;

    for(uint8_t i = 0; i < CERBERUS_BAND_COUNT; i++) {
        bool active = (i == s->active_band);
        cerberus_draw_meter(canvas, i, &cerberus_bands[i], &s->band[i], active, blink);
    }
    canvas_set_color(canvas, ColorBlack);

    CerberusThreat worst = CerberusThreatNone;
    uint8_t worst_band = 0;
    for(uint8_t i = 0; i < CERBERUS_BAND_COUNT; i++) {
        if(s->band[i].threat > worst) {
            worst = s->band[i].threat;
            worst_band = i;
        }
    }

    if(worst != CerberusThreatNone) {
        if(blink) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, 51, 128, 13);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_frame(canvas, 0, 51, 128, 13);
        }
        canvas_set_font(canvas, FontPrimary);
        char banner[26];
        snprintf(
            banner,
            sizeof(banner),
            "! %s %u !",
            cerberus_threat_name(worst),
            (unsigned)cerberus_bands[worst_band].mhz);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, banner);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_line(canvas, 0, 50, 127, 50);
        canvas_set_font(canvas, FontSecondary);

        char left[16];
        uint32_t up = s->uptime_s;
        snprintf(
            left, sizeof(left), "UP %02lu:%02lu", (unsigned long)(up / 60), (unsigned long)(up % 60));
        canvas_draw_str(canvas, 2, 61, left);

        if(m->alert_total > 0) {
            char mid[16];
            snprintf(mid, sizeof(mid), "!%lu", (unsigned long)m->alert_total);
            canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, mid);
        }

        char right[18];
        snprintf(
            right,
            sizeof(right),
            "%u %ub/s",
            (unsigned)cerberus_bands[s->active_band].mhz,
            (unsigned)s->band[s->active_band].bursts);
        canvas_draw_str_aligned(canvas, 127, 61, AlignRight, AlignBottom, right);
    }
}

// ---------------------------------------------------------------------------
// Scope mode
// ---------------------------------------------------------------------------
static uint8_t cerberus_scope_y(int16_t rssi, uint8_t gy0, uint8_t gh) {
    int v = rssi;
    if(v < RSSI_FLOOR_VIEW) v = RSSI_FLOOR_VIEW;
    if(v > RSSI_CEIL_VIEW) v = RSSI_CEIL_VIEW;
    int span = RSSI_CEIL_VIEW - RSSI_FLOOR_VIEW;
    int up = (v - RSSI_FLOOR_VIEW) * gh / span; // 0 at floor .. gh at ceil
    return (uint8_t)(gy0 + gh - up);
}

static void cerberus_draw_dotted_h(Canvas* canvas, uint8_t x0, uint8_t x1, uint8_t y) {
    for(uint8_t x = x0; x <= x1; x += 3) canvas_draw_dot(canvas, x, y);
}

static void cerberus_draw_scope(Canvas* canvas, CerberusMonitorModel* m) {
    const uint8_t b = m->scope_band;
    const CerberusBandStatus* st = &m->snap.band[b];
    const uint8_t gx0 = 5, gy0 = 14, gw = SCOPE_W, gh = 40;
    const uint8_t gx1 = gx0 + gw - 1;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_frame(canvas, gx0 - 1, gy0 - 1, gw + 2, gh + 2);

    // reference lines: noise floor + detection threshold
    cerberus_draw_dotted_h(canvas, gx0, gx1, cerberus_scope_y(st->floor, gy0, gh));
    uint8_t yt = cerberus_scope_y(st->threshold, gy0, gh);
    cerberus_draw_dotted_h(canvas, gx0, gx1, yt);
    canvas_draw_str(canvas, gx1 - 16, (uint8_t)(yt - 1), "thr");

    // the trace (oldest left -> newest right)
    uint8_t head = m->hist_head[b];
    uint8_t prev_y = cerberus_scope_y(m->hist[b][head % SCOPE_W], gy0, gh);
    for(uint8_t c = 1; c < SCOPE_W; c++) {
        uint8_t idx = (uint8_t)((head + c) % SCOPE_W);
        uint8_t y = cerberus_scope_y(m->hist[b][idx], gy0, gh);
        canvas_draw_line(canvas, (uint8_t)(gx0 + c - 1), prev_y, (uint8_t)(gx0 + c), y);
        prev_y = y;
    }

    // status line under the graph
    canvas_set_font(canvas, FontSecondary);
    char left[18];
    snprintf(left, sizeof(left), "%u  %ddBm", (unsigned)cerberus_bands[b].mhz, st->rssi);
    canvas_draw_str(canvas, 2, 63, left);

    char right[20];
    snprintf(right, sizeof(right), "F%d T%d", st->floor, st->threshold);
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, right);
}

// ---------------------------------------------------------------------------
static void cerberus_monitor_draw(Canvas* canvas, void* model) {
    CerberusMonitorModel* m = model;
    canvas_clear(canvas);

    char mode_txt[14];
    if(m->mode == MonitorModeScope) {
        snprintf(mode_txt, sizeof(mode_txt), "SCOPE");
    } else if(m->snap.hop) {
        snprintf(mode_txt, sizeof(mode_txt), "HOP");
    } else {
        snprintf(mode_txt, sizeof(mode_txt), "PIN %u", (unsigned)cerberus_bands[m->snap.active_band].mhz);
    }
    cerberus_draw_title(canvas, m, mode_txt);

    if(!m->has_data || !m->snap.running) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "waking the watchdog...");
        return;
    }

    if(m->mode == MonitorModeScope) {
        cerberus_draw_scope(canvas, m);
    } else {
        cerberus_draw_meters(canvas, m);
    }
}

static bool cerberus_monitor_input(InputEvent* event, void* context) {
    CerberusMonitorView* view = context;

    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyOk:
            if(view->callback) view->callback(CerberusMonitorEventResetFloor, view->context);
            return true;
        case InputKeyLeft:
        case InputKeyRight:
            with_view_model(
                view->view,
                CerberusMonitorModel * m,
                { m->mode = (m->mode == MonitorModeScope) ? MonitorModeMeters : MonitorModeScope; },
                true);
            return true;
        case InputKeyUp:
            with_view_model(
                view->view,
                CerberusMonitorModel * m,
                {
                    if(m->mode == MonitorModeScope)
                        m->scope_band =
                            (uint8_t)((m->scope_band + CERBERUS_BAND_COUNT - 1) % CERBERUS_BAND_COUNT);
                },
                true);
            return true;
        case InputKeyDown:
            with_view_model(
                view->view,
                CerberusMonitorModel * m,
                {
                    if(m->mode == MonitorModeScope)
                        m->scope_band = (uint8_t)((m->scope_band + 1) % CERBERUS_BAND_COUNT);
                },
                true);
            return true;
        default:
            break;
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(view->callback) view->callback(CerberusMonitorEventToggleArm, view->context);
        return true;
    }

    return false; // let Back propagate to the navigation handler
}

CerberusMonitorView* cerberus_monitor_view_alloc(void) {
    CerberusMonitorView* view = malloc(sizeof(CerberusMonitorView));
    view->callback = NULL;
    view->context = NULL;
    view->view = view_alloc();
    view_set_context(view->view, view);
    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(CerberusMonitorModel));
    view_set_draw_callback(view->view, cerberus_monitor_draw);
    view_set_input_callback(view->view, cerberus_monitor_input);

    with_view_model(
        view->view,
        CerberusMonitorModel * model,
        {
            memset(model, 0, sizeof(CerberusMonitorModel));
            model->has_data = false;
            model->mode = MonitorModeMeters;
            model->scope_band = 0;
            model->armed = true;
            for(uint8_t b = 0; b < CERBERUS_BAND_COUNT; b++) {
                for(uint16_t c = 0; c < SCOPE_W; c++) model->hist[b][c] = RSSI_FLOOR_VIEW;
            }
        },
        true);
    return view;
}

void cerberus_monitor_view_free(CerberusMonitorView* view) {
    furi_assert(view);
    view_free(view->view);
    free(view);
}

View* cerberus_monitor_view_get_view(CerberusMonitorView* view) {
    furi_assert(view);
    return view->view;
}

void cerberus_monitor_view_set_callback(
    CerberusMonitorView* view,
    CerberusMonitorCallback callback,
    void* context) {
    furi_assert(view);
    view->callback = callback;
    view->context = context;
}

void cerberus_monitor_view_update(CerberusMonitorView* view, const CerberusSnapshot* snap) {
    furi_assert(view);
    with_view_model(
        view->view,
        CerberusMonitorModel * model,
        {
            model->snap = *snap;
            model->has_data = true;
            model->frame++;
            // append the freshly sampled band into its history ring
            uint8_t a = snap->active_band;
            if(a < CERBERUS_BAND_COUNT) {
                uint8_t h = model->hist_head[a];
                model->hist[a][h] = cerberus_clamp8(snap->band[a].rssi);
                model->hist_head[a] = (uint8_t)((h + 1) % SCOPE_W);
            }
        },
        true);
}

void cerberus_monitor_view_set_status(
    CerberusMonitorView* view,
    bool armed,
    uint32_t alert_total) {
    furi_assert(view);
    with_view_model(
        view->view,
        CerberusMonitorModel * model,
        {
            model->armed = armed;
            model->alert_total = alert_total;
        },
        false);
}
