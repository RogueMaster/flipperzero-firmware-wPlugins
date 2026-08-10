/*
 * Purpose: Draw Listening progress summary, totals, and bounded history pages.
 * Owns: progress screen layout only; persistence and history loading live in progress.c.
 * Depends on: morse_flipper_app_i.h and progress helpers.
 * Tests: firmware build; rendering is hardware-only.
 */

#include "morse_flipper_app_i.h"

static void morse_flipper_draw_progress_bar(Canvas* canvas, uint8_t done, uint8_t total) {
    uint8_t x = 3U;
    uint8_t y = 44U;
    uint8_t w = 122U;
    uint8_t h = 7U;
    uint8_t fill_w = 120U;
    uint8_t fill;

    if(total == 0U) total = 1U;
    if(done > total) done = total;
    fill = (uint8_t)(((uint16_t)done * fill_w) / total);

    canvas_draw_rframe(canvas, x, y, w, h, 2);
    if(fill != 0U) canvas_draw_box(canvas, x + 1U, y + 1U, fill, (uint8_t)(h - 2U));
}

static void morse_flipper_draw_progress_stats(Canvas* canvas, MorseFlipperApp* app) {
    MorseFlipperProgress* progress = app->view_progress;
    char line[40];
    char lesson_label[16];
    char attempt_label[8];
    char weak[12];
    uint8_t mastered;
    uint8_t total_lessons;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 10, "Listening stats");
    canvas_set_font(canvas, FontSecondary);
    snprintf(line, sizeof(line), "Streak: %u days", (unsigned)progress->streak_days);
    canvas_draw_str(canvas, 4, 21, line);

    mastered = morse_flipper_progress_mastered_lesson(progress);
    total_lessons = (uint8_t)morse_trainer_lesson_count();
    if(total_lessons >= MORSE_FLIPPER_PROGRESS_LESSON_CAP)
        total_lessons = MORSE_FLIPPER_PROGRESS_LESSON_CAP - 1U;
    if(mastered == 0U) {
        canvas_draw_str(canvas, 4, 31, "Progress: none");
    } else {
        char lesson_line[32];
        morse_trainer_lesson_label(mastered, lesson_label, sizeof(lesson_label));
        snprintf(lesson_line, sizeof(lesson_line), "Progress: %s", lesson_label);
        canvas_draw_str(canvas, 4, 31, lesson_line);
    }
    snprintf(
        attempt_label, sizeof(attempt_label), "%u/%u", (unsigned)mastered, (unsigned)total_lessons);
    canvas_draw_str_aligned(canvas, 124, 31, AlignRight, AlignBottom, attempt_label);

    morse_flipper_progress_top_weak(
        progress, morse_trainer_lesson_charset(&app->trainer), weak, sizeof(weak));
    snprintf(line, sizeof(line), "Errors: %s", weak);
    canvas_draw_str(canvas, 4, 41, line);

    morse_flipper_draw_progress_bar(canvas, mastered, total_lessons);
    elements_button_left(canvas, "Lessons");
    elements_button_right(canvas, "More");
}

static void morse_flipper_draw_progress_totals(Canvas* canvas, MorseFlipperApp* app) {
    MorseFlipperProgress* progress = app->view_progress;
    char line[32];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 10, "Listening totals");
    canvas_set_font(canvas, FontSecondary);
    snprintf(line, sizeof(line), "Total attempts: %u", (unsigned)progress->total_attempts);
    canvas_draw_str(canvas, 4, 21, line);
    snprintf(line, sizeof(line), "Daily record: %u", (unsigned)progress->daily_record);
    canvas_draw_str(canvas, 4, 31, line);
    snprintf(
        line,
        sizeof(line),
        "Best score: %u%%",
        (unsigned)morse_flipper_progress_best_score(progress));
    canvas_draw_str(canvas, 4, 41, line);
    elements_button_left(canvas, "Stats");
    elements_button_right(canvas, "Lessons");
}

static void morse_flipper_progress_history_reference_date(
    const MorseFlipperApp* app,
    uint16_t* out_year,
    uint8_t* out_month) {
    DateTime dt = {0};
    uint16_t today = MORSE_FLIPPER_PROGRESS_DAY_NONE;
    uint8_t ignored_day;

    if(out_year == NULL || out_month == NULL) return;
    *out_year = 0U;
    *out_month = 0U;

    furi_hal_rtc_get_datetime(&dt);
    if(morse_flipper_progress_date_to_day(dt.year, dt.month, dt.day, &today)) {
        *out_year = dt.year;
        *out_month = dt.month;
        return;
    }

    if(app == NULL || app->view_progress == NULL) return;
    morse_flipper_progress_day_to_date(
        app->view_progress->last_stats_day, out_year, out_month, &ignored_day);
}

static void morse_flipper_draw_progress_history_row(
    Canvas* canvas,
    uint8_t y,
    const MorseFlipperProgressHistoryRow* row,
    bool selected,
    uint16_t reference_year,
    uint8_t reference_month) {
    enum {
        CursorX = 0U,
        CursorW = 2U,
        CursorH = 8U,
        LessonX = 4U,
        DateX = 13U,
        TimeX = 47U,
        ScoreX = 75U,
        StarCx = 103U,
        StarGap = 10U,
    };
    char date[8];
    char time[8];
    char score[8];
    char lesson[2] = {row->lesson, '\0'};
    uint8_t stars;
    uint8_t i;

    morse_flipper_progress_history_date_label(
        row, reference_year, reference_month, date, sizeof(date));
    if(date[0] == '\0') return;
    snprintf(time, sizeof(time), "%02u:%02u", (unsigned)row->hour, (unsigned)row->minute);
    snprintf(score, sizeof(score), "%u%%", (unsigned)row->percent);
    stars = morse_flipper_progress_stars(row->percent);

    if(selected) canvas_draw_box(canvas, CursorX, (uint8_t)(y - 7U), CursorW, CursorH);
    canvas_draw_str(canvas, LessonX, y, lesson);
    canvas_draw_str(canvas, DateX, y, date);
    canvas_draw_str(canvas, TimeX, y, time);
    canvas_draw_str(canvas, ScoreX, y, score);
    for(i = 0U; i < 3U; i++) {
        morse_flipper_draw_star_glyph(
            canvas, (uint8_t)(StarCx + (i * StarGap)), (uint8_t)(y - 4U), i < stars);
    }
}

static void morse_flipper_draw_progress_history(Canvas* canvas, MorseFlipperApp* app) {
    uint8_t i;
    uint8_t visible = 0U;
    uint16_t reference_year = 0U;
    uint8_t reference_month = 0U;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 10, "Recent lessons");
    canvas_set_font(canvas, FontSecondary);

    if(app->progress_row_offset < app->progress_row_count)
        visible = (uint8_t)(app->progress_row_count - app->progress_row_offset);
    if(visible > MORSE_FLIPPER_PROGRESS_HISTORY_ROWS)
        visible = MORSE_FLIPPER_PROGRESS_HISTORY_ROWS;

    if(visible == 0U) {
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "No lesson history yet");
    } else {
        morse_flipper_progress_history_reference_date(app, &reference_year, &reference_month);
        for(i = 0U; i < visible; i++) {
            morse_flipper_draw_progress_history_row(
                canvas,
                (uint8_t)(21U + (i * 9U)),
                &app->progress_rows[app->progress_row_offset + i],
                i == app->progress_row_cursor,
                reference_year,
                reference_month);
        }
    }

    elements_button_left(canvas, "More");
    if(visible != 0U) elements_button_center(canvas, "View");
    elements_button_right(canvas, "Stats");
}

void morse_flipper_draw_progress(Canvas* canvas, MorseFlipperApp* app) {
    if(canvas == NULL || app == NULL) return;

    if(app->view_progress == NULL) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Progress unavailable");
        return;
    }

    if(app->progress_page == MorseFlipperProgressPageTotals) {
        morse_flipper_draw_progress_totals(canvas, app);
    } else if(app->progress_page == MorseFlipperProgressPageHistory) {
        morse_flipper_draw_progress_history(canvas, app);
    } else {
        morse_flipper_draw_progress_stats(canvas, app);
    }
}
