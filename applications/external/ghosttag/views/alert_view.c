#include "alert_view.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>

/* The banner inverts every Nth 100 ms tick. The old code flipped on EVERY
 * tick: a 10 Hz full-width strobe, which is both unpleasant and squarely in
 * the flicker band that photosensitive people are warned about. Four ticks is
 * 2.5 Hz - still unmissable, no longer an assault. */
#define BLINK_TICKS 4

struct AlertView {
    View* view;
    AlertViewCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    TrackerRecord rec;
    bool valid;
    bool demo;
    uint8_t phase;
} AlertModel;

static void draw_big_warning(Canvas* canvas) {
    /* Triangle, doubled lines for weight. Kept inside rows 16..47 so the
     * text column and the footer rule both stay clear of it. */
    const int tx = 24, ty = 16, lx = 9, ly = 47, rx = 39, ry = 47;
    canvas_draw_line(canvas, tx, ty, lx, ly);
    canvas_draw_line(canvas, tx + 1, ty, lx + 1, ly);
    canvas_draw_line(canvas, tx, ty, rx, ry);
    canvas_draw_line(canvas, tx - 1, ty, rx - 1, ry);
    canvas_draw_line(canvas, lx, ly, rx, ry);
    canvas_draw_line(canvas, lx, ly - 1, rx, ry - 1);
    /* exclamation */
    canvas_draw_box(canvas, 23, 25, 3, 13);
    canvas_draw_box(canvas, 23, 41, 3, 3);
}

static void alert_view_draw(Canvas* canvas, void* model) {
    AlertModel* m = model;
    char buf[32];
    bool lit = m->phase < BLINK_TICKS;

    /* ---- strobing banner (rows 0..13) ---- */
    if(lit) {
        canvas_draw_box(canvas, 0, 0, 128, 14);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_frame(canvas, 0, 0, 128, 14);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 11, AlignCenter, AlignBottom, m->demo ? "DEMO ALERT" : "TRACKER ALERT");
    canvas_set_color(canvas, ColorBlack);

    draw_big_warning(canvas);

    /* ---- detail column (x 48..127) ---- */
    if(m->valid) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 48, 26, tracker_type_short(m->rec.type));

        canvas_set_font(canvas, FontSecondary);
        uint32_t dur_s = (m->rec.last_seen - m->rec.first_seen) / 1000UL;
        uint32_t mins = dur_s / 60UL;
        if(mins > 99UL) {
            snprintf(buf, sizeof(buf), "with you 99m+");
        } else {
            snprintf(
                buf, sizeof(buf), "with you %u:%02u", (unsigned)mins, (unsigned)(dur_s % 60UL));
        }
        canvas_draw_str(canvas, 48, 38, buf);
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 48, 26, "Unknown tag");
    }

    canvas_set_font(canvas, FontSecondary);
    /* Baseline 48 so the descenders in "travelling" clear the rule on 52.
     *
     * "travelling with you", not "following you": in range across time is the
     * only thing GhostTag measured. It has no GPS and cannot match your route,
     * so it reports what it saw. */
    /* "SIMULATED" alone, because the longer wording ran eleven pixels off the
     * right-hand edge on a real device. The banner above already says DEMO
     * ALERT, so this is the second of two markers, not the only one. */
    canvas_draw_str(canvas, 48, 48, m->demo ? "SIMULATED" : "travelling with you");

    /* ---- footer (rule on 52, text on baseline 61) ---- */
    canvas_draw_line(canvas, 0, 52, 127, 52);
    canvas_draw_str(canvas, 2, 61, "OK: details");
    canvas_draw_str_aligned(canvas, 126, 61, AlignRight, AlignBottom, "Back: dismiss");
}

static bool alert_view_input(InputEvent* event, void* context) {
    AlertView* av = context;
    /* Short OR Long: the firmware sends Long, never Short, once a key is held
     * past the long-press threshold. This screen fires when somebody is being
     * followed - it is the worst possible place for a press to go nowhere. */
    if((event->type == InputTypeShort || event->type == InputTypeLong) &&
       event->key == InputKeyOk) {
        if(av->ok_cb) av->ok_cb(av->ok_ctx);
        return true;
    }
    return false;
}

AlertView* alert_view_alloc(void) {
    AlertView* av = malloc(sizeof(AlertView));
    memset(av, 0, sizeof(AlertView));
    av->view = view_alloc();
    view_set_context(av->view, av);
    view_set_draw_callback(av->view, alert_view_draw);
    view_set_input_callback(av->view, alert_view_input);
    view_allocate_model(av->view, ViewModelTypeLocking, sizeof(AlertModel));
    return av;
}

void alert_view_free(AlertView* av) {
    furi_assert(av);
    view_free(av->view);
    free(av);
}

View* alert_view_get_view(AlertView* av) {
    furi_assert(av);
    return av->view;
}

void alert_view_set_record(AlertView* av, const TrackerRecord* rec, bool demo) {
    furi_assert(av);
    with_view_model(
        av->view,
        AlertModel * m,
        {
            m->rec = *rec;
            m->valid = true;
            m->demo = demo;
            m->phase = 0; /* always open lit, so a second alert re-announces */
        },
        true);
}

void alert_view_tick(AlertView* av) {
    furi_assert(av);
    with_view_model(
        av->view,
        AlertModel * m,
        { m->phase = (uint8_t)((m->phase + 1) % (BLINK_TICKS * 2)); },
        true);
}

void alert_view_set_ok_callback(AlertView* av, AlertViewCallback cb, void* context) {
    furi_assert(av);
    av->ok_cb = cb;
    av->ok_ctx = context;
}
