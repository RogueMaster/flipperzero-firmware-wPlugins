#include "detect_view.h"

#include <gui/elements.h>
#include <string.h>

/* One bit is drawn this wide, so the trace reads as real data: a start bit is a
 * narrow notch, a run of zeros is a wide trough. */
#define WAVE_PX_PER_BIT (3)
#define WAVE_MAX_SEG_PX (30) // stop idle stretches from eating the window

#define WAVE_X0 (4)
#define WAVE_X1 (123)
#define WAVE_Y_HIGH (17)
#define WAVE_Y_LOW (33)

typedef struct {
    DetectPhase phase;
    char port[12];
    char pins[16];

    AutobaudSegment trace[AUTOBAUD_TRACE_EDGES];
    size_t trace_count;
    uint32_t bit_time;

    uint32_t edges;
    uint32_t target;
    uint32_t live_baud;

    uint8_t verify_pct;
    uint32_t verify_baud;

    bool idle_high;
} DetectViewModel;

struct DetectView {
    View* view;
};

/* ---------------------------------------------------------------- wave ---- */

/** Pick the scale for the trace: the fitted bit time when we have one, the
 *  shortest pulse seen when we do not. */
static uint32_t detect_wave_bit_time(const DetectViewModel* m) {
    if(m->bit_time > 0) return m->bit_time;

    uint32_t shortest = UINT32_MAX;
    for(size_t i = 0; i < m->trace_count; i++) {
        if(m->trace[i].delta < shortest) shortest = m->trace[i].delta;
    }
    return (shortest == UINT32_MAX || shortest == 0) ? 1u : shortest;
}

static int detect_seg_width(uint32_t delta, uint32_t bit_time) {
    int w = (int)(((uint64_t)delta * WAVE_PX_PER_BIT) / bit_time);
    if(w < 1) w = 1;
    if(w > WAVE_MAX_SEG_PX) w = WAVE_MAX_SEG_PX;
    return w;
}

static void detect_draw_wave(Canvas* canvas, const DetectViewModel* m) {
    const int width = WAVE_X1 - WAVE_X0;

    if(m->trace_count == 0) {
        /* Nothing has moved. Draw the line where it actually sits, so a probe
         * stuck low looks different from one that is simply quiet. */
        const int y = m->idle_high ? WAVE_Y_HIGH : WAVE_Y_LOW;
        canvas_set_color(canvas, ColorBlack);
        for(int x = WAVE_X0; x <= WAVE_X1; x += 2) {
            canvas_draw_dot(canvas, x, y);
        }

        canvas_set_font(canvas, FontSecondary);
        const char* msg = m->idle_high ? "line idle (high)" : "line stuck LOW";
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, msg);
        return;
    }

    const uint32_t bit_time = detect_wave_bit_time(m);

    int widths[AUTOBAUD_TRACE_EDGES];
    for(size_t i = 0; i < m->trace_count; i++) {
        widths[i] = detect_seg_width(m->trace[i].delta, bit_time);
    }

    /* Fill from the newest backwards, then draw forward - the trace scrolls
     * left like a scope and the freshest data is always on screen. */
    int used = 0;
    size_t first = m->trace_count;
    while(first > 0) {
        const int w = widths[first - 1];
        if(used + w > width) break;
        used += w;
        first--;
    }

    canvas_set_color(canvas, ColorBlack);
    int x = WAVE_X0 + (width - used);

    for(size_t i = first; i < m->trace_count; i++) {
        const int w = widths[i];
        const int y = m->trace[i].high ? WAVE_Y_HIGH : WAVE_Y_LOW;

        canvas_draw_line(canvas, x, y, x + w - 1, y);

        if(i + 1 < m->trace_count) {
            const int y_next = m->trace[i + 1].high ? WAVE_Y_HIGH : WAVE_Y_LOW;
            if(y_next != y) {
                canvas_draw_line(canvas, x + w - 1, WAVE_Y_HIGH, x + w - 1, WAVE_Y_LOW);
            }
        }
        x += w;
    }
}

/* ---------------------------------------------------------------- draw ---- */

static void detect_view_draw(Canvas* canvas, void* model) {
    const DetectViewModel* m = model;
    /* Sized for two full-width %lu, which is what the compiler assumes even
     * though the edge count never passes 512. */
    char buf[40];

    canvas_clear(canvas);

    /* header */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 8, (m->phase == DetectPhaseListening) ? "Listening" : "Verifying");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, m->pins);
    canvas_draw_line(canvas, 0, 10, 127, 10);

    /* scope */
    detect_draw_wave(canvas, m);

    /* stats + progress */
    canvas_set_font(canvas, FontSecondary);

    if(m->phase == DetectPhaseListening) {
        if(m->edges == 0) {
            canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, "no edges yet");
            /* The two things that are wrong 90% of the time. */
            canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignTop, "check GND + TX->RX");
        } else {
            if(m->live_baud > 0) {
                snprintf(buf, sizeof(buf), "%lu edges  ~%lu bd", m->edges, m->live_baud);
            } else {
                snprintf(buf, sizeof(buf), "%lu edges", m->edges);
            }
            canvas_draw_str(canvas, 2, 48, buf);

            const float progress = (m->target > 0) ? ((float)m->edges / (float)m->target) : 0.0f;
            elements_progress_bar(canvas, 2, 53, 124, (progress > 1.0f) ? 1.0f : progress);
        }
    } else {
        snprintf(buf, sizeof(buf), "trying %lu baud", m->verify_baud);
        canvas_draw_str(canvas, 2, 48, buf);
        elements_progress_bar(canvas, 2, 53, 124, (float)m->verify_pct / 100.0f);
    }
}

static bool detect_view_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false; // Back is the scene's business
}

/* ----------------------------------------------------------------- api ---- */

DetectView* detect_view_alloc(void) {
    DetectView* dv = malloc(sizeof(DetectView));
    dv->view = view_alloc();
    view_allocate_model(dv->view, ViewModelTypeLocking, sizeof(DetectViewModel));
    view_set_context(dv->view, dv);
    view_set_draw_callback(dv->view, detect_view_draw);
    view_set_input_callback(dv->view, detect_view_input);
    detect_view_reset(dv);
    return dv;
}

void detect_view_free(DetectView* dv) {
    furi_assert(dv);
    view_free(dv->view);
    free(dv);
}

View* detect_view_get_view(DetectView* dv) {
    furi_assert(dv);
    return dv->view;
}

void detect_view_reset(DetectView* dv) {
    furi_assert(dv);
    with_view_model(
        dv->view,
        DetectViewModel * m,
        {
            memset(m, 0, sizeof(DetectViewModel));
            m->phase = DetectPhaseListening;
            m->idle_high = true;
            m->target = AUTOBAUD_MAX_EDGES;
        },
        true);
}

void detect_view_set_phase(DetectView* dv, DetectPhase phase) {
    furi_assert(dv);
    with_view_model(dv->view, DetectViewModel * m, { m->phase = phase; }, true);
}

void detect_view_set_port(DetectView* dv, const char* port_name, const char* pins) {
    furi_assert(dv);
    with_view_model(
        dv->view,
        DetectViewModel * m,
        {
            strncpy(m->port, port_name, sizeof(m->port) - 1);
            m->port[sizeof(m->port) - 1] = '\0';
            strncpy(m->pins, pins, sizeof(m->pins) - 1);
            m->pins[sizeof(m->pins) - 1] = '\0';
        },
        true);
}

void detect_view_set_trace(
    DetectView* dv,
    const AutobaudSegment* segs,
    size_t count,
    uint32_t bit_time) {
    furi_assert(dv);
    if(count > AUTOBAUD_TRACE_EDGES) count = AUTOBAUD_TRACE_EDGES;

    with_view_model(
        dv->view,
        DetectViewModel * m,
        {
            if(count > 0) memcpy(m->trace, segs, count * sizeof(AutobaudSegment));
            m->trace_count = count;
            m->bit_time = bit_time;
        },
        true);
}

void detect_view_set_listen(DetectView* dv, uint32_t edges, uint32_t target, uint32_t live_baud) {
    furi_assert(dv);
    with_view_model(
        dv->view,
        DetectViewModel * m,
        {
            m->edges = edges;
            m->target = target;
            m->live_baud = live_baud;
        },
        true);
}

void detect_view_set_verify(DetectView* dv, uint8_t percent, uint32_t baud) {
    furi_assert(dv);
    with_view_model(
        dv->view,
        DetectViewModel * m,
        {
            m->verify_pct = percent;
            m->verify_baud = baud;
        },
        true);
}

void detect_view_set_idle(DetectView* dv, bool line_idle_high) {
    furi_assert(dv);
    with_view_model(dv->view, DetectViewModel * m, { m->idle_high = line_idle_high; }, true);
}
