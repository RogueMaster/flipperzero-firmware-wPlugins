/*
 * Purpose: Dispatch live drawing for every Morse Flipper screen.
 * Owns: top-level canvas clearing, status lines, and screen draw selection.
 * Depends on: morse_flipper_app_i.h and split live-view drawing modules.
 * Tests: firmware build; rendering is hardware-only.
 */

#include "morse_flipper_app_i.h"
#include "morse_flipper_cw_token.h"

void morse_flipper_draw(Canvas* canvas, void* ctx) {
    MorseFlipperApp* app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->audio_wait_active) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Please wait");
        return;
    }

    if(app->screen == MorseFlipperScreenAbout) {
        morse_flipper_draw_about(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenHelp) {
        morse_flipper_draw_help(canvas, app);
        return;
    }

    char tone_line[32];
    char input_line[32];
    char trace_line1[32];
    char trace_line2[32];
    char trace_line3[32];
    char trace_line4[32];
    char browse_line[32];

    if(app->screen == MorseFlipperScreenStartupProbe) {
        morse_flipper_draw_startup_gpio_probe(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenHamStartRefusal) {
        morse_flipper_draw_ham_start_refusal(canvas);
        return;
    }

    if(app->screen == MorseFlipperScreenHamAssign) {
        morse_flipper_draw_ham_assign(canvas);
        return;
    }

    if(app->screen == MorseFlipperScreenHamAssignments) {
        morse_flipper_draw_ham_assignments(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenHamCopyNotice) {
        morse_flipper_draw_ham_copy_notice(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenHamDeleteConfirm) {
        morse_flipper_draw_ham_delete_confirm(canvas);
        return;
    }

    if(app->screen == MorseFlipperScreenHamRun) {
        morse_flipper_draw_ham_run(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenRun) {
        morse_flipper_draw_tx_history_screen(canvas, app, morse_flipper_run_usb_name(app));
        return;
    }

    if(app->screen == MorseFlipperScreenRfFreq) {
        morse_flipper_draw_rf_freq_picker(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenTrainer) {
        morse_flipper_draw_trainer_setup(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenStraight) {
        morse_flipper_draw_straight_screen(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenSession) {
        if(morse_flipper_gpio_probe_notice_active(app) ||
           morse_flipper_gpio_probe_blocks_start(app)) {
            morse_flipper_draw_gpio_probe_overlay(canvas, app);
            return;
        }

        morse_flipper_draw_session_rows(canvas, app);
        morse_flipper_draw_session_bottom(canvas, app);
        if(morse_flipper_live_left_hint(app)) {
            morse_flipper_draw_left_exit_hint(canvas);
        }
        return;
    }

    if(app->screen == MorseFlipperScreenSessionEnd) {
        morse_flipper_draw_session_end(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenTxGroups ||
       app->screen == MorseFlipperScreenTxGroupsResult ||
       app->screen == MorseFlipperScreenTxGroupsFinal) {
        morse_flipper_draw_tx_groups_screen(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenRf) {
        if(!morse_flipper_rf_tx_allowed_khz(morse_flipper_rf_frequency_khz(&app->rf))) {
            morse_flipper_draw_rf_tx_blocked(canvas, app);
            return;
        }

        morse_flipper_draw_tx_history_screen(
            canvas, app, morse_flipper_rf_khz_line(app, browse_line, sizeof(browse_line)));
        return;
    }

    if(app->screen == MorseFlipperScreenRfRx) {
        morse_flipper_draw_rf_rx_screen(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenTrace) {
        char tx_trace[48];
        char gpio_trace[48];

        morse_flipper_cw_token_expand_text(
            tx_trace, sizeof(tx_trace), app->rf_tx_text[0] ? app->rf_tx_text : "-");
        morse_flipper_cw_token_expand_text(
            gpio_trace, sizeof(gpio_trace), app->gpio_text[0] ? app->gpio_text : "-");
        snprintf(
            trace_line1,
            sizeof(trace_line1),
            "tr %s %s %s",
            morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)),
            morse_flipper_source_short_name(app, browse_line, sizeof(browse_line)),
            morse_flipper_hand_name(app));
        snprintf(
            trace_line2,
            sizeof(trace_line2),
            "pd %u%u out %u%u rep %ld",
            app->keyer.paddle_down[MorseKeyerPaddleDit] ? 1U : 0U,
            app->keyer.paddle_down[MorseKeyerPaddleDah] ? 1U : 0U,
            app->keyer.output_active[MorseKeyerPaddleDit] ? 1U : 0U,
            app->keyer.output_active[MorseKeyerPaddleDah] ? 1U : 0U,
            (long)app->keyer.next_repeat_paddle);
        snprintf(
            trace_line3,
            sizeof(trace_line3),
            "set %u fifo %u pulse %u",
            app->keyer.set_queue_len,
            app->keyer.fifo_queue_len,
            app->keyer.pulse_active ? 1U : 0U);
        snprintf(trace_line4, sizeof(trace_line4), "tx %.12s gp %.11s", tx_trace, gpio_trace);

        canvas_draw_str(canvas, 2, 10, trace_line1);
        canvas_draw_str(
            canvas, 2, 20, morse_flipper_input_line(app, input_line, sizeof(input_line)));
        canvas_draw_str(canvas, 2, 30, trace_line2);
        canvas_draw_str(canvas, 2, 40, trace_line3);
        canvas_draw_str(canvas, 2, 50, trace_line4);
        canvas_draw_str(
            canvas, 2, 60, morse_flipper_trace_hint(app, browse_line, sizeof(browse_line)));
        if(morse_flipper_live_left_hint(app)) {
            morse_flipper_draw_left_exit_hint(canvas);
        }
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 14, "Keying");

    snprintf(
        tone_line,
        sizeof(tone_line),
        "< %s > %s",
        morse_flipper_current_tone_name(app),
        morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 30, morse_flipper_pc_state_name(app));
    canvas_draw_str(canvas, 8, 42, tone_line);
    canvas_draw_str(
        canvas, 8, 52, morse_flipper_status_line(app, browse_line, sizeof(browse_line)));
    canvas_draw_str(canvas, 8, 62, "L/R tone U src D/hD");
}
