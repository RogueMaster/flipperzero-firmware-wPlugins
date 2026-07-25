#include "mf_tx_groups_draw.h"

#include <gui/canvas.h>
#include <furi.h>
#include <stdio.h>
#include <string.h>

static void mf_tx_groups_score_line(const MfTxGroupsDrawSnapshot* snapshot, char* out, size_t out_sz) {
    unsigned pct = snapshot->session_total != 0U ?
                       ((unsigned)snapshot->session_good * 100U) / snapshot->session_total :
                       0U;

    snprintf(
        out,
        out_sz,
        "%u/%u  %u%%",
        (unsigned)snapshot->session_good,
        (unsigned)snapshot->session_total,
        pct);
}

static void mf_tx_groups_draw_big_slots(
    Canvas* canvas,
    const MfTxGroupsDrawServices* services,
    int32_t cy,
    const char* text) {
    const int32_t gap = 3;
    const int32_t cell = 12;
    const int32_t total = (cell * 5) + (gap * 4);
    int32_t cx = ((128 - total) / 2) + (cell / 2);

    for(uint8_t i = 0U; i < MORSE_FLIPPER_TX_GROUP_LEN && text[i] != '\0'; i++)
        services->draw_prompt(canvas, services->context, cx + ((cell + gap) * i), cy, text[i]);
}

static void mf_tx_groups_label(Canvas* canvas, int32_t x, int32_t y, const char* text, bool bad) {
    uint16_t width;

    if(!bad) {
        canvas_draw_str(canvas, x, y, text);
        return;
    }
    width = canvas_string_width(canvas, text);
    canvas_draw_box(canvas, x - 1, y - 7, width + 2, 8);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, x, y, text);
    canvas_set_color(canvas, ColorBlack);
}

static void mf_tx_groups_metric(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    const char* label,
    const char* value,
    bool bad) {
    mf_tx_groups_label(canvas, x, y, label, bad);
    canvas_draw_str(canvas, x + 28, y, value);
}

static uint16_t mf_tx_groups_average(uint32_t sum, uint16_t count) {
    return count == 0U ? 0U : (uint16_t)((sum + (count / 2U)) / count);
}

static void mf_tx_groups_draw_practice(
    Canvas* canvas,
    const MfTxGroupsDrawServices* services,
    const MfTxGroupsDrawSnapshot* snapshot) {
    char score[12];
    unsigned pct;

    mf_tx_groups_draw_big_slots(canvas, services, 18, snapshot->target);
    services->draw_history_divider(canvas, snapshot->left_hint);
    mf_tx_groups_draw_big_slots(canvas, services, 49, snapshot->answer);
    canvas_set_font(canvas, FontSecondary);
    pct = snapshot->session_total != 0U ?
              ((unsigned)snapshot->session_good * 100U) / snapshot->session_total :
              0U;
    if(pct > 100U) pct = 100U;
    snprintf(score, sizeof(score), "%u%%", pct);
    canvas_draw_str(canvas, 126 - canvas_string_width(canvas, score), 64, score);
}

static void mf_tx_groups_draw_result(
    Canvas* canvas,
    const MfTxGroupsDrawServices* services,
    const MfTxGroupsDrawSnapshot* snapshot) {
    char value[12];
    char countdown[4];
    const MorseFlipperTxGroupResult* result = &snapshot->result;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, result->passed ? "OK" : "Fail");
    canvas_set_font(canvas, FontKeyboard);
    snprintf(value, sizeof(value), "%u/5", (unsigned)result->correct);
    mf_tx_groups_metric(canvas, 1, 20, "Corr", value, !result->correct_pass);
    snprintf(value, sizeof(value), "%u%%", (unsigned)result->speed_pct);
    mf_tx_groups_metric(canvas, 1, 29, "Time", value, !result->speed_pass);
    snprintf(value, sizeof(value), "%u%%", (unsigned)result->letter_gap_pct);
    mf_tx_groups_metric(canvas, 1, 38, "LGap", value, !result->letter_gap_pass);
    if(snapshot->sk) {
        snprintf(value, sizeof(value), "%u.%02u", (unsigned)(result->ratio_x100 / 100U), (unsigned)(result->ratio_x100 % 100U));
        mf_tx_groups_metric(canvas, 64, 20, "Rtio", value, !result->ratio_pass);
        snprintf(value, sizeof(value), "%u%%", (unsigned)result->accuracy_pct);
        mf_tx_groups_metric(canvas, 64, 29, "Acc", value, !result->accuracy_pass);
        snprintf(value, sizeof(value), "%u%%", (unsigned)result->dit_gap_pct);
        mf_tx_groups_metric(canvas, 64, 38, "DGap", value, !result->dit_gap_pass);
        snprintf(value, sizeof(value), "%u%%", (unsigned)result->variance_pct);
        mf_tx_groups_metric(canvas, 64, 47, "Var", value, !result->variance_pass);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 56, snapshot->fault);
    snprintf(countdown, sizeof(countdown), "%u", (unsigned)snapshot->countdown_s);
    canvas_draw_str(canvas, 2, 64, countdown);
    mf_tx_groups_score_line(snapshot, value, sizeof(value));
    canvas_draw_str(canvas, 126 - canvas_string_width(canvas, value), 64, value);
    if(snapshot->show_left_exit_hint) services->draw_left_exit_hint(canvas);
}

static void mf_tx_groups_draw_final(
    Canvas* canvas,
    const MfTxGroupsDrawServices* services,
    const MfTxGroupsDrawSnapshot* snapshot) {
    char value[24];
    uint16_t average;
    uint8_t y = 20U;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "Final score");
    canvas_set_font(canvas, FontKeyboard);
    snprintf(value, sizeof(value), "%u/%u", (unsigned)snapshot->session_good, (unsigned)snapshot->session_total);
    mf_tx_groups_metric(canvas, 1, y, "Pass", value, false);
    y += 9U;
    snprintf(value, sizeof(value), "%u%%", (unsigned)mf_tx_groups_average(snapshot->sum_speed, snapshot->session_total));
    mf_tx_groups_metric(canvas, 1, y, "Time", value, false);
    y += 9U;
    snprintf(value, sizeof(value), "%u%%", (unsigned)mf_tx_groups_average(snapshot->sum_lgap, snapshot->session_total));
    mf_tx_groups_metric(canvas, 1, y, "LGap", value, false);
    if(snapshot->session_sk != 0U) {
        average = mf_tx_groups_average(snapshot->sum_ratio, snapshot->session_sk);
        snprintf(value, sizeof(value), "%u.%02u", (unsigned)(average / 100U), (unsigned)(average % 100U));
        mf_tx_groups_metric(canvas, 64, 20, "Rtio", value, false);
        snprintf(value, sizeof(value), "%u%%", (unsigned)mf_tx_groups_average(snapshot->sum_accuracy, snapshot->session_sk));
        mf_tx_groups_metric(canvas, 64, 29, "Acc", value, false);
        snprintf(value, sizeof(value), "%u%%", (unsigned)mf_tx_groups_average(snapshot->sum_dgap, snapshot->session_sk));
        mf_tx_groups_metric(canvas, 64, 38, "DGap", value, false);
        snprintf(value, sizeof(value), "%u%%", (unsigned)mf_tx_groups_average(snapshot->sum_variance, snapshot->session_sk));
        mf_tx_groups_metric(canvas, 64, 47, "Var", value, false);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 64, "Back exit");
    mf_tx_groups_score_line(snapshot, value, sizeof(value));
    canvas_draw_str(canvas, 126 - canvas_string_width(canvas, value), 64, value);
    if(snapshot->show_left_exit_hint) services->draw_left_exit_hint(canvas);
}

static void mf_tx_groups_draw_start(Canvas* canvas, uint8_t mode) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "TX Groups of 5");
    canvas_set_font(canvas, FontSecondary);
    if(mode == 3U)
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Press OK to start");
    else {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to start");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "Press your key to start");
    }
}

void mf_tx_groups_draw_tx_groups(void* state, Canvas* canvas) {
    const MfTxGroupsState* tx_groups_state = state;
    const MfTxGroupsDrawServices* services;
    MfTxGroupsDrawSnapshot snapshot = {0};
    const MorseFlipperTxGroup* group;
    uint8_t answer_len = 0U;

    if(tx_groups_state == NULL || tx_groups_state->draw_services.context == NULL || canvas == NULL)
        return;
    services = &tx_groups_state->draw_services;
    group = services->group;
    if(group == NULL || services->sum_speed == NULL || services->sum_lgap == NULL ||
       services->sum_ratio == NULL || services->sum_accuracy == NULL || services->sum_dgap == NULL ||
       services->sum_variance == NULL || services->session_total == NULL ||
       services->session_good == NULL || services->session_sk == NULL ||
       services->result_until == NULL || services->screen == NULL ||
       services->input_source == NULL || services->started == NULL || services->txg_sk == NULL ||
       services->answer_preview == NULL)
        return;
    memcpy(snapshot.target, group->target, sizeof(snapshot.target));
    while(answer_len < MORSE_FLIPPER_TX_GROUP_LEN && group->answer[answer_len] != '\0') {
        snapshot.answer[answer_len] = group->answer[answer_len];
        answer_len++;
    }
    if(answer_len < MORSE_FLIPPER_TX_GROUP_LEN) {
        char preview = services->answer_preview(services->context);
        if(preview != '\0') snapshot.answer[answer_len++] = preview;
    }
    snapshot.answer[answer_len] = '\0';
    snapshot.result = group->result;
    if(group->result.fault != NULL)
        snprintf(snapshot.fault, sizeof(snapshot.fault), "%s", group->result.fault);
    snapshot.result.fault = NULL;
    snapshot.sum_speed = *services->sum_speed;
    snapshot.sum_lgap = *services->sum_lgap;
    snapshot.sum_ratio = *services->sum_ratio;
    snapshot.sum_accuracy = *services->sum_accuracy;
    snapshot.sum_dgap = *services->sum_dgap;
    snapshot.sum_variance = *services->sum_variance;
    snapshot.session_total = *services->session_total;
    snapshot.session_good = *services->session_good;
    snapshot.session_sk = *services->session_sk;
    snapshot.mode = *services->screen == 20U ?
                        (*services->started ? 0U : (*services->input_source == 2U ? 3U : 4U)) :
                    *services->screen == 21U ? 1U : 2U;
    if(*services->result_until > furi_get_tick())
        snapshot.countdown_s = (uint8_t)((*services->result_until - furi_get_tick() + 999U) / 1000U);
    snapshot.sk = group->sk;
    snapshot.left_hint = *services->input_source == 2U && !*services->txg_sk;
    snapshot.show_left_exit_hint = snapshot.left_hint;
    if(snapshot.mode == 0U)
        mf_tx_groups_draw_practice(canvas, services, &snapshot);
    else if(snapshot.mode == 1U)
        mf_tx_groups_draw_result(canvas, services, &snapshot);
    else if(snapshot.mode == 2U)
        mf_tx_groups_draw_final(canvas, services, &snapshot);
    else
        mf_tx_groups_draw_start(canvas, snapshot.mode);
}
