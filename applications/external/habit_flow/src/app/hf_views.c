#include "include/app/hf_app_presenter.h"
#include "include/app/hf_app_state.h"
#include "include/domain/habit.h"
#include "include/persistence/habit_store.h"
#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <input/input.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void hf_main_clamp_scroll(HabitFlowApp *app) {
    if (app->store.habit_count == 0) {
        app->list_sel = 0;
        app->list_scroll = 0;
        return;
    }
    if (app->list_sel >= app->store.habit_count) {
        app->list_sel = app->store.habit_count - 1u;
    }
    const uint32_t vis = 2u;
    if (app->list_sel < app->list_scroll) {
        app->list_scroll = app->list_sel;
    }
    if (app->list_sel >= app->list_scroll + vis) {
        app->list_scroll = app->list_sel - (vis - 1u);
    }
}

static void hf_manage_clamp(HabitFlowApp *app) {
    const uint32_t total = app->store.habit_count + 1u;
    if (app->manage_sel >= total) {
        app->manage_sel = total - 1u;
    }
    const uint32_t vis = 4u;
    if (app->manage_sel < app->manage_scroll) {
        app->manage_scroll = app->manage_sel;
    }
    if (app->manage_sel >= app->manage_scroll + vis) {
        app->manage_scroll = app->manage_sel - (vis - 1u);
    }
}

static void hf_draw_history_bar_ex(Canvas *canvas, int x, int y, const Habit *h, int box, int gap) {
    for (int i = 0; i < HF_HISTORY_DAYS; i++) {
        const int px = x + i * (box + gap);
        if (h->history[i]) {
            canvas_draw_box(canvas, px, y, box, box);
        } else {
            canvas_draw_frame(canvas, px, y, box, box);
        }
    }
}

static void hf_draw_screen_title(Canvas *canvas, const char *title) {
    const int bar_h = 14;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, bar_h);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, title);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
}

static void hf_draw_goal_star_badge(Canvas *canvas, int cx, int cy) {
    const int R = 3;
    const float pi = 3.14159265f;
    int tx[5];
    int ty[5];
    for (int k = 0; k < 5; k++) {
        const float th = (-pi / 2.f) + (float)k * ((2.f * pi) / 5.f);
        tx[k] = cx + (int)lroundf((float)R * cosf(th));
        ty[k] = cy + (int)lroundf((float)R * sinf(th));
    }
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_disc(canvas, cx, cy, 1);
    for (int k = 0; k < 5; k++) {
        canvas_draw_line(canvas, cx, cy, tx[k], ty[k]);
    }
    for (int k = 0; k < 5; k++) {
        const int n = (k + 1) % 5;
        canvas_draw_line(canvas, tx[k], ty[k], tx[n], ty[n]);
    }
}

static void hf_draw_habit_title_strip(Canvas *canvas, int top_y, const char *title,
                                      bool show_medal) {
    const int bar_h = 14;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, top_y, 128, bar_h);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, top_y + 11, title);
    if (show_medal) {
        hf_draw_goal_star_badge(canvas, 118, top_y + 7);
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
}

void hf_views_main_draw(Canvas *canvas, void *model) {
    const HfCtx *ctx = model;
    furi_assert(ctx && ctx->app);
    HabitFlowApp *app = ctx->app;
    canvas_clear(canvas);
    hf_draw_screen_title(canvas, "HabitFlow");
    if (app->store.habit_count == 0) {
        canvas_draw_str(canvas, 2, 20, "No habits yet");
        elements_button_left(canvas, "Menu");
        elements_button_right(canvas, "Info");
        return;
    }
    hf_main_clamp_scroll(app);
    const uint32_t vis = 2u;
    uint32_t row_y = 24;
    for (uint32_t row = 0; row < vis && app->list_scroll + row < app->store.habit_count; row++) {
        const uint32_t i = app->list_scroll + row;
        const Habit *h = &app->store.habits[i];
        char line[20];
        snprintf(line, sizeof(line), "%s%.14s", (i == app->list_sel) ? ">" : " ", h->name);
        canvas_draw_str(canvas, 2, row_y, line);

        hf_draw_history_bar_ex(canvas, 2, (int)row_y + 5, h, 3, 1);
        row_y += 18;
    }

    elements_button_left(canvas, "Menu");
    elements_button_center(canvas, "OK");
    elements_button_right(canvas, "Info");
}

bool hf_views_main_input(InputEvent *event, void *context) {
    HabitFlowApp *app = context;
    if (event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }
    if (app->store.habit_count == 0) {
        if (event->key == InputKeyLeft) {
            app->manage_sel = 0;
            app->manage_scroll = 0;
            hf_manage_clamp(app);
            hf_switch(app, HfViewManage);
            return true;
        }
        if (event->key == InputKeyRight) {
            hf_build_credits(app);
            hf_switch(app, HfViewCredits);
            return true;
        }
        return false;
    }
    if (event->key == InputKeyUp) {
        if (app->list_sel > 0) {
            app->list_sel--;
        }
        hf_main_clamp_scroll(app);
        return true;
    }
    if (event->key == InputKeyDown) {
        if (app->list_sel + 1 < app->store.habit_count) {
            app->list_sel++;
        }
        hf_main_clamp_scroll(app);
        return true;
    }
    if (event->key == InputKeyOk) {
        app->detail_index = app->list_sel;
        hf_switch(app, HfViewDetail);
        return true;
    }
    if (event->key == InputKeyLeft) {
        app->manage_sel = 0;
        app->manage_scroll = 0;
        hf_manage_clamp(app);
        hf_switch(app, HfViewManage);
        return true;
    }
    if (event->key == InputKeyRight) {
        hf_build_credits(app);
        hf_switch(app, HfViewCredits);
        return true;
    }
    return false;
}

void hf_views_detail_draw(Canvas *canvas, void *model) {
    const HfCtx *ctx = model;
    furi_assert(ctx && ctx->app);
    HabitFlowApp *app = ctx->app;
    const Habit *h = &app->store.habits[app->detail_index];
    canvas_clear(canvas);
    char title[16];
    snprintf(title, sizeof(title), "%.10s", h->name);
    hf_draw_habit_title_strip(canvas, 14, title, hf_habit_shows_medal(h));
    const uint8_t pct_u = hf_habit_progress_pct(h);
    const float prog = (float)pct_u / 100.0f;
    char pct_buf[8];
    snprintf(pct_buf, sizeof(pct_buf), "%u%%", (unsigned)pct_u);

    const int progress_y = 30;
    const int stats_y = 51;
    elements_progress_bar_with_text(canvas, 4, progress_y, 120, prog, pct_buf);
    char part1[20];
    char part2[20];
    snprintf(part1, sizeof(part1), "Streak: %u", (unsigned)h->streak);
    snprintf(part2, sizeof(part2), "Record: %u", (unsigned)h->max_streak);
    const int x1 = 2;
    const int gap = 12;
    const int w1 = (int)canvas_string_width(canvas, part1);
    const int w2 = (int)canvas_string_width(canvas, part2);
    int x2 = x1 + w1 + gap;
    if (x2 + w2 > 126) {
        x2 = 126 - w2;
    }
    if (x2 < x1 + w1 + 4) {
        x2 = x1 + w1 + 4;
    }
    canvas_draw_str(canvas, x1, stats_y, part1);
    canvas_draw_str(canvas, x2, stats_y, part2);
    elements_button_up(canvas, "Reset");
    elements_button_center(canvas, h->completed_today ? "Undo" : "Mark");
}

bool hf_views_detail_input(InputEvent *event, void *context) {
    HabitFlowApp *app = context;
    if (event->key == InputKeyBack &&
        (event->type == InputTypeShort || event->type == InputTypeLong)) {
        hf_switch(app, HfViewMain);
        return true;
    }
    if (event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }
    if (event->key == InputKeyOk) {
        Habit *h = &app->store.habits[app->detail_index];
        bool just_mastered = false;
        hf_habit_toggle_today(h, &just_mastered);
        hf_app_save(app);
        if (just_mastered) {
            popup_reset(app->popup_mastered);
            popup_set_header(app->popup_mastered, "Habit Mastered!", 64, 6, AlignCenter, AlignTop);
            popup_set_text(app->popup_mastered, "Goal reached!", 64, 22, AlignCenter, AlignTop);
            popup_set_timeout(app->popup_mastered, 2000);
            popup_enable_timeout(app->popup_mastered);
            hf_switch(app, HfViewPopupMastered);
        }
        return true;
    }
    if (event->key == InputKeyUp) {
        app->dlg_mode = HfDlgResetStreak;
        dialog_ex_reset(app->dialog);
        hf_dialog_rebind(app);
        dialog_ex_set_header(app->dialog, "Reset streak?", 64, 2, AlignCenter, AlignTop);
        dialog_ex_set_text(app->dialog, "Clear current streak?", 64, 16, AlignCenter, AlignTop);
        dialog_ex_set_left_button_text(app->dialog, "No");
        dialog_ex_set_right_button_text(app->dialog, "Yes");
        hf_switch(app, HfViewDialog);
        return true;
    }
    return false;
}

void hf_views_manage_draw(Canvas *canvas, void *model) {
    const HfCtx *ctx = model;
    furi_assert(ctx && ctx->app);
    HabitFlowApp *app = ctx->app;
    canvas_clear(canvas);
    hf_draw_screen_title(canvas, "Manage");
    hf_manage_clamp(app);
    const uint32_t total = app->store.habit_count + 1u;
    const uint32_t vis = 4u;
    uint32_t y = 22;
    for (uint32_t row = 0; row < vis && app->manage_scroll + row < total; row++) {
        const uint32_t i = app->manage_scroll + row;
        char line[32];
        if (i < app->store.habit_count) {
            snprintf(line, sizeof(line), "%s%s", (i == app->manage_sel) ? "> " : "  ",
                     app->store.habits[i].name);
        } else {
            snprintf(line, sizeof(line), "%sAdd habit", (i == app->manage_sel) ? "> " : "  ");
        }
        canvas_draw_str(canvas, 2, y, line);
        y += 8;
    }

    elements_button_left(canvas, "");
    elements_button_center(canvas, "Edit");
}

bool hf_views_manage_input(InputEvent *event, void *context) {
    HabitFlowApp *app = context;
    if (event->key == InputKeyBack &&
        (event->type == InputTypeShort || event->type == InputTypeLong)) {
        hf_switch(app, HfViewMain);
        return true;
    }
    if (event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }
    if (event->key == InputKeyLeft) {
        hf_switch(app, HfViewMain);
        return true;
    }
    const uint32_t total = app->store.habit_count + 1u;
    if (event->key == InputKeyUp && app->manage_sel > 0) {
        app->manage_sel--;
        hf_manage_clamp(app);
        return true;
    }
    if (event->key == InputKeyDown && app->manage_sel + 1 < total) {
        app->manage_sel++;
        hf_manage_clamp(app);
        return true;
    }
    if (event->key == InputKeyOk) {
        if (app->manage_sel < app->store.habit_count) {
            app->edit_buf = app->store.habits[app->manage_sel];
            app->edit_index = app->manage_sel;
            app->edit_is_new = false;
        } else {
            hf_habit_set_defaults(&app->edit_buf, "New");
            app->edit_is_new = true;
            app->edit_index = 0;
        }
        app->edit_row = 0;
        hf_switch(app, HfViewEdit);
        return true;
    }
    return false;
}

static uint8_t edit_row_max(const HabitFlowApp *app) {
    return app->edit_is_new ? (uint8_t)2u : (uint8_t)3u;
}

void hf_views_edit_draw(Canvas *canvas, void *model) {
    const HfCtx *ctx = model;
    furi_assert(ctx && ctx->app);
    HabitFlowApp *app = ctx->app;
    canvas_clear(canvas);
    hf_draw_screen_title(canvas, app->edit_is_new ? "New habit" : "Edit habit");
    char line[40];
    snprintf(line, sizeof(line), "%s%.12s", (app->edit_row == 0) ? ">" : " ",
             app->edit_buf.name[0] ? app->edit_buf.name : "(name)");
    canvas_draw_str(canvas, 2, 23, line);
    snprintf(line, sizeof(line), "%sGoal %u d", (app->edit_row == 1) ? ">" : " ",
             (unsigned)app->edit_buf.goal_days);
    canvas_draw_str(canvas, 2, 31, line);
    snprintf(line, sizeof(line), "%sSave", (app->edit_row == 2) ? ">" : " ");
    canvas_draw_str(canvas, 2, 39, line);
    if (!app->edit_is_new) {
        snprintf(line, sizeof(line), "%sDel", (app->edit_row == 3) ? ">" : " ");
        canvas_draw_str(canvas, 2, 47, line);
    }
    if (app->edit_row == 1) {
        elements_button_left(canvas, "-");
        elements_button_right(canvas, "+");
    }
    elements_button_center(canvas, "OK");
}

bool hf_views_edit_input(InputEvent *event, void *context) {
    HabitFlowApp *app = context;
    if (event->key == InputKeyBack &&
        (event->type == InputTypeShort || event->type == InputTypeLong)) {
        hf_switch(app, HfViewManage);
        return true;
    }
    if (event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }
    const uint8_t rmax = edit_row_max(app);
    if (event->key == InputKeyUp) {
        if (app->edit_row > 0) {
            app->edit_row--;
        }
        return true;
    }
    if (event->key == InputKeyDown) {
        if (app->edit_row < rmax) {
            app->edit_row++;
        }
        return true;
    }
    if (event->key == InputKeyLeft || event->key == InputKeyRight) {
        if (app->edit_row == 1) {
            const int s = (event->key == InputKeyRight) ? 1 : -1;
            int g = (int)app->edit_buf.goal_days + s;
            if (g < (int)HF_GOAL_MIN) {
                g = HF_GOAL_MIN;
            }
            if (g > (int)HF_GOAL_MAX) {
                g = HF_GOAL_MAX;
            }
            app->edit_buf.goal_days = (uint16_t)g;
        }
        return true;
    }
    if (event->key == InputKeyOk) {
        if (app->edit_row == 0) {
            snprintf(app->text_buf, sizeof(app->text_buf), "%s", app->edit_buf.name);
            text_input_set_header_text(app->text_input, "Habit name");
            text_input_set_result_callback(app->text_input, text_input_ok_cb, app, app->text_buf,
                                           sizeof(app->text_buf), false);
            hf_switch(app, HfViewTextInput);
            return true;
        }
        if (app->edit_row == 2) {
            if (strlen(app->edit_buf.name) == 0) {
                snprintf(app->edit_buf.name, sizeof(app->edit_buf.name), "Habit");
            }
            if (app->edit_is_new) {
                habit_store_add(&app->store, &app->edit_buf);
            } else {
                habit_store_replace_at(&app->store, app->edit_index, &app->edit_buf);
            }
            hf_switch(app, HfViewManage);
            return true;
        }
        if (app->edit_row == 3 && !app->edit_is_new) {
            app->dlg_mode = HfDlgDeleteHabit;
            dialog_ex_reset(app->dialog);
            hf_dialog_rebind(app);
            dialog_ex_set_header(app->dialog, "Delete?", 64, 2, AlignCenter, AlignTop);
            dialog_ex_set_text(app->dialog, "Remove habit?", 64, 16, AlignCenter, AlignTop);
            dialog_ex_set_left_button_text(app->dialog, "No");
            dialog_ex_set_right_button_text(app->dialog, "Yes");
            hf_switch(app, HfViewDialog);
            return true;
        }
    }
    return false;
}
