#include "survey_view.h"
#include <furi.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

/* Running: a progress bar, the live numbers, and the waveform that produced
 * them. Finished: one verdict, one next step, and the evidence underneath. */
#define BAR_X 4
#define BAR_Y 15
#define BAR_W 120
#define BAR_H 11

#define RUN_STAT1_BASE 35
#define RUN_STAT2_BASE 45
#define RUN_WAVE_TOP   50
#define RUN_WAVE_BASE  63

#define BANNER_X 2
#define BANNER_Y 14
#define BANNER_W 124
#define BANNER_H 14

#define DONE_VERDICT_BASE 25
#define DONE_ADVICE_BASE  37
#define DONE_STAT1_BASE   50
#define DONE_STAT2_BASE   60
#define COL_RIGHT         66

struct SurveyView {
    View* view;
    SurveyViewCallback restart_cb;
    void* restart_ctx;
};

typedef struct {
    bool finished;
    bool error;
    /* running */
    bool present;
    uint8_t strength;
    uint8_t peak;
    uint32_t contacts;
    uint32_t elapsed_ms;
    uint32_t total_ms;
    uint8_t history[SPECTER_HISTORY_LEN];
    uint8_t history_head;
    /* finished */
    SurveySummary summary;
    SurveyVerdict verdict;
    uint8_t anim;
} SurveyModel;

static void draw_header(Canvas* canvas, const SurveyModel* m) {
    char buf[16];
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "SITE SURVEY");

    if(m->finished) {
        canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, "OK=again");
    } else {
        uint32_t left_ms = m->total_ms > m->elapsed_ms ? m->total_ms - m->elapsed_ms : 0;
        uint32_t left_s = (left_ms + 999u) / 1000u; // round up: never show 0:00 early
        snprintf(
            buf,
            sizeof(buf),
            "%lu:%02lu",
            (unsigned long)(left_s / 60u),
            (unsigned long)(left_s % 60u));
        canvas_draw_str_aligned(canvas, 116, 9, AlignRight, AlignBottom, buf);
        if(m->present) {
            canvas_draw_disc(canvas, 123, 5, 2);
        } else {
            canvas_draw_circle(canvas, 123, 5, 2);
        }
    }
    canvas_draw_line(canvas, 0, 11, 127, 11);
}

static void draw_running(Canvas* canvas, const SurveyModel* m) {
    char buf[24];

    /* progress */
    canvas_draw_frame(canvas, BAR_X, BAR_Y, BAR_W, BAR_H);
    uint32_t fill = 0;
    if(m->total_ms) {
        uint32_t pct = (m->elapsed_ms * 100u) / m->total_ms;
        if(pct > 100u) pct = 100u;
        fill = (pct * (BAR_W - 2u)) / 100u;
    }
    if(fill) canvas_draw_box(canvas, BAR_X + 1, BAR_Y + 1, (int)fill, BAR_H - 2);

    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "FIELD %u%%", (unsigned)m->strength);
    canvas_draw_str(canvas, 2, RUN_STAT1_BASE, buf);
    snprintf(buf, sizeof(buf), "PEAK %u%%", (unsigned)m->peak);
    canvas_draw_str(canvas, COL_RIGHT, RUN_STAT1_BASE, buf);

    snprintf(buf, sizeof(buf), "HITS %lu", (unsigned long)m->contacts);
    canvas_draw_str(canvas, 2, RUN_STAT2_BASE, buf);
    canvas_draw_str(canvas, COL_RIGHT, RUN_STAT2_BASE, "sweep slowly");

    /* live waveform of the strength history */
    canvas_draw_line(canvas, 0, RUN_WAVE_TOP - 2, 127, RUN_WAVE_TOP - 2);
    int span = RUN_WAVE_BASE - RUN_WAVE_TOP;
    for(int k = 0; k < 62; k++) {
        int idx = (m->history_head - k + 2 * (int)SPECTER_HISTORY_LEN) % (int)SPECTER_HISTORY_LEN;
        int v = m->history[idx];
        int x = 126 - k * 2;
        int y = RUN_WAVE_BASE - (v * span) / 100;
        if(y < RUN_WAVE_BASE)
            canvas_draw_line(canvas, x, RUN_WAVE_BASE, x, y);
        else
            canvas_draw_dot(canvas, x, RUN_WAVE_BASE);
    }
}

static void draw_verdict(Canvas* canvas, const SurveyModel* m) {
    char buf[28];

    /* The verdict banner is inverted for ACTIVE - the one outcome that should
     * stop you in your tracks - and outlined for the calmer two. */
    bool alarm = m->verdict == SurveyVerdictActive;
    if(alarm) {
        canvas_draw_box(canvas, BANNER_X, BANNER_Y, BANNER_W, BANNER_H);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_frame(canvas, BANNER_X, BANNER_Y, BANNER_W, BANNER_H);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, DONE_VERDICT_BASE, AlignCenter, AlignBottom, survey_verdict_name(m->verdict));
    if(alarm) canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, DONE_ADVICE_BASE, AlignCenter, AlignBottom, survey_verdict_advice(m->verdict));

    canvas_draw_line(canvas, 0, DONE_ADVICE_BASE + 3, 127, DONE_ADVICE_BASE + 3);

    const SurveySummary* s = &m->summary;
    snprintf(buf, sizeof(buf), "MAX %u%%", (unsigned)s->peak);
    canvas_draw_str(canvas, 2, DONE_STAT1_BASE, buf);
    snprintf(buf, sizeof(buf), "AVG %u%%", (unsigned)s->average);
    canvas_draw_str(canvas, COL_RIGHT, DONE_STAT1_BASE, buf);

    snprintf(buf, sizeof(buf), "HITS %lu", (unsigned long)s->contacts);
    canvas_draw_str(canvas, 2, DONE_STAT2_BASE, buf);
    snprintf(buf, sizeof(buf), "FIELD %u%%", (unsigned)survey_in_field_pct(s));
    canvas_draw_str(canvas, COL_RIGHT, DONE_STAT2_BASE, buf);

    if(alarm) {
        canvas_draw_frame(canvas, 0, 0, 128, 64);
    }
}

static void survey_view_draw(Canvas* canvas, void* model) {
    SurveyModel* m = model;

    draw_header(canvas, m);

    if(m->error) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "NFC unavailable");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 44, AlignCenter, AlignCenter, "Close any other NFC app.");
        return;
    }

    if(m->finished) {
        draw_verdict(canvas, m);
    } else {
        draw_running(canvas, m);
    }
}

static bool survey_view_input(InputEvent* event, void* context) {
    SurveyView* v = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(v->restart_cb) v->restart_cb(v->restart_ctx);
        return true;
    }
    return false; // everything else (incl. BACK) bubbles to the scene manager
}

SurveyView* survey_view_alloc(void) {
    SurveyView* v = malloc(sizeof(SurveyView));
    memset(v, 0, sizeof(SurveyView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, survey_view_draw);
    view_set_input_callback(v->view, survey_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(SurveyModel));
    return v;
}

void survey_view_free(SurveyView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* survey_view_get_view(SurveyView* v) {
    furi_assert(v);
    return v->view;
}

void survey_view_set_restart_callback(SurveyView* v, SurveyViewCallback cb, void* ctx) {
    furi_assert(v);
    v->restart_cb = cb;
    v->restart_ctx = ctx;
}

void survey_view_update_running(
    SurveyView* v,
    const FieldStats* stats,
    uint32_t elapsed_ms,
    uint32_t total_ms) {
    furi_assert(v);
    furi_assert(stats);
    with_view_model(
        v->view,
        SurveyModel * m,
        {
            m->finished = false;
            m->error = stats->error;
            m->present = stats->present;
            m->strength = stats->strength;
            m->peak = stats->peak;
            m->contacts = stats->contacts;
            m->elapsed_ms = elapsed_ms;
            m->total_ms = total_ms;
            memcpy(m->history, stats->history, sizeof(m->history));
            m->history_head = stats->history_head;
        },
        true);
}

void survey_view_show_verdict(SurveyView* v, const SurveySummary* summary) {
    furi_assert(v);
    furi_assert(summary);
    with_view_model(
        v->view,
        SurveyModel * m,
        {
            m->finished = true;
            m->summary = *summary;
            m->verdict = survey_verdict(summary);
        },
        true);
}

void survey_view_tick(SurveyView* v) {
    furi_assert(v);
    with_view_model(v->view, SurveyModel * m, { m->anim++; }, true);
}
