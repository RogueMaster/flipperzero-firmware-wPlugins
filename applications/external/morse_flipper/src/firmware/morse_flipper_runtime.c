/*
 * Purpose: Tick live runtime features and GPIO input sampling.
 * Owns: periodic app updates, trainer/keyer/radio ticks, and result notes.
 * Depends on: morse_flipper_app_i.h plus keyer, trainer, RF, and GPIO modules.
 * Tests: feature host tests cover models; scheduler flow is hardware-only.
 */

#include "morse_flipper_app_i.h"

static void morse_flipper_note_tx_group_result(MorseFlipperApp* app);

void morse_flipper_gpio_init(MorseFlipperApp* app) {
    morse_flipper_gpio_apply(app);
}

void morse_flipper_gpio_deinit(void) {
    morse_flipper_gpio_reset_candidates();
}

bool morse_flipper_straight_down(void) {
    return !furi_hal_gpio_read(morse_flipper_straight_pin);
}

static bool morse_flipper_dit_down(void) {
    return !furi_hal_gpio_read(morse_flipper_dit_pin);
}

static bool morse_flipper_dah_down(void) {
    return !furi_hal_gpio_read(morse_flipper_dah_pin);
}

bool morse_flipper_logical_dit_down(const MorseFlipperApp* app) {
    if(app->handedness == MorseFlipperHandednessSwapped) {
        return morse_flipper_dah_down();
    }

    return morse_flipper_dit_down();
}

bool morse_flipper_logical_dah_down(const MorseFlipperApp* app) {
    if(app->handedness == MorseFlipperHandednessSwapped) {
        return morse_flipper_dit_down();
    }

    return morse_flipper_dah_down();
}

static void morse_flipper_append_text(char* dst, size_t dst_sz, const char* add) {
    size_t len;
    size_t add_len;

    if(dst == NULL || dst_sz < 2U || add == NULL || add[0] == '\0') return;

    len = strlen(dst);
    add_len = strlen(add);
    if(add_len >= dst_sz) {
        memcpy(dst, add + add_len - (dst_sz - 1U), dst_sz - 1U);
        dst[dst_sz - 1U] = '\0';
        return;
    }

    if(len + add_len >= dst_sz) {
        size_t drop = len + add_len - (dst_sz - 1U);
        memmove(dst, dst + drop, len - drop + 1U);
        len = strlen(dst);
    }

    memcpy(dst + len, add, add_len + 1U);
}

void morse_flipper_decoder_drain_into(MorseFlipperCwDecoder* decoder, char* out, size_t out_sz) {
    if(decoder == NULL || out == NULL || out_sz < 2U) return;

    if(morse_flipper_cw_decoder_output(decoder)[0] != '\0') {
        morse_flipper_append_text(out, out_sz, morse_flipper_cw_decoder_output(decoder));
        morse_flipper_cw_decoder_clear_output(decoder);
    }
}

static void morse_flipper_finish_tx_group_answer(MorseFlipperApp* app, uint32_t now_ms) {
    uint16_t dit_ms;

    if(app == NULL || app->screen != MorseFlipperScreenTxGroups || !app->txg_wait_answer) return;

    dit_ms = morse_flipper_current_dit_ms(app);
    if(!morse_flipper_tx_group_complete(&app->tx_group) &&
       morse_flipper_tx_group_finalize_answer_from_raw(&app->tx_group, dit_ms))
        morse_flipper_view_dirty(app);

    app->txg_wait_answer = false;
    app->txg_done = true;
    morse_flipper_tx_group_score(&app->tx_group, dit_ms, false);
    morse_flipper_note_tx_group_result(app);
    app->txg_repeated_timeouts = 0U;
    if(app->tx_group.result.passed)
        morse_flipper_feedback_pass(app);
    else
        morse_flipper_feedback_fail(app);
    app->txg_result_open_at = now_ms + MORSE_FLIPPER_TXG_RESULT_DELAY_MS;
    app->txg_result_until = 0U;
    app->txg_result_draw_s = 0xFFU;
    morse_flipper_drop_live_keying_for_playback(app, now_ms);
}

static void morse_flipper_drain_tx_decoder(MorseFlipperApp* app) {
    const char* out;

    if(app == NULL) return;
    out = morse_flipper_cw_decoder_output(&app->tx_decoder);
    if(out[0] == '\0') return;

    if(app->screen == MorseFlipperScreenTxGroups && app->txg_wait_answer) {
        morse_flipper_tx_group_feed_text(&app->tx_group, out);
        app->txg_last_input_at = furi_get_tick();
        morse_flipper_view_dirty(app);
        if(morse_flipper_tx_group_complete(&app->tx_group)) {
            morse_flipper_finish_tx_group_answer(app, furi_get_tick());
        }
    }

    morse_flipper_append_text(app->rf_tx_text, sizeof(app->rf_tx_text), out);
    if(app->screen == MorseFlipperScreenHamRun) {
        morse_flipper_ham_log_append_text(app, out, furi_get_tick());
    }
    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenRf ||
       app->screen == MorseFlipperScreenHamRun) {
        morse_flipper_run_history_append(&app->run_history, out);
        morse_flipper_view_dirty(app);
    }
    morse_flipper_cw_decoder_clear_output(&app->tx_decoder);
}

static void morse_flipper_finish_tx_group_timeout(MorseFlipperApp* app, uint32_t now_ms) {
    uint16_t dit_ms;

    if(app == NULL) return;
    dit_ms = morse_flipper_current_dit_ms(app);
    if(!morse_flipper_tx_group_complete(&app->tx_group) &&
       morse_flipper_tx_group_finalize_answer_from_raw(&app->tx_group, dit_ms))
        morse_flipper_view_dirty(app);
    app->txg_wait_answer = false;
    app->txg_done = true;
    morse_flipper_tx_group_score(&app->tx_group, dit_ms, true);
    morse_flipper_note_tx_group_result(app);
    app->txg_repeated_timeouts++;

    if(app->txg_repeated_timeouts >= 3U) {
        morse_flipper_drop_live_keying_for_playback(app, now_ms);
        morse_flipper_clear_button_keying(app, now_ms);
        morse_flipper_release_all_notes(app);
        app->session_result_tone = false;
        app->session_result_until = 0U;
        morse_flipper_update_sidetone(app);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuTraining);
        return;
    }

    morse_flipper_feedback_timeout(app);
    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    app->txg_result_open_at = 0U;
    app->txg_result_until = now_ms + ((uint32_t)app->straight_next_delay_s * 1000U);
    app->txg_result_draw_s = 0xFFU;
    scene_manager_next_scene(app->scene_manager, MorseFlipperSceneTxGroupsResult);
}

static void morse_flipper_maybe_finish_tx_group_raw(MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t idle_ms;
    uint32_t settle_ms;

    if(app == NULL || app->screen != MorseFlipperScreenTxGroups || !app->txg_wait_answer) return;
    if(app->rf_tx_level || app->rf_tx_edge_at == 0U) return;
    if(!morse_flipper_tx_group_marks_complete(&app->tx_group)) return;

    idle_ms = now_ms - app->rf_tx_edge_at;
    settle_ms = ((uint32_t)morse_flipper_current_dit_ms(app) * 5U) / 2U;
    if(idle_ms < settle_ms) return;

    morse_flipper_finish_tx_group_answer(app, now_ms);
}

static void morse_flipper_note_tx_group_result(MorseFlipperApp* app) {
    const MorseFlipperTxGroupResult* r;

    if(app == NULL) return;
    r = &app->tx_group.result;
    app->txg_session_total++;
    if(r->passed) app->txg_session_good++;
    app->txg_sum_speed += r->speed_pct;
    app->txg_sum_lgap += r->letter_gap_pct;
    if(app->tx_group.sk) {
        app->txg_session_sk++;
        app->txg_sum_ratio += r->ratio_x100;
        app->txg_sum_accuracy += r->accuracy_pct;
        app->txg_sum_dgap += r->dit_gap_pct;
        app->txg_sum_variance += r->variance_pct;
    }
}

static void morse_flipper_tick_tx_groups(MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t timeout_from;
    uint32_t left_ms;
    uint8_t left_s;

    if(app == NULL) return;

    if(app->session_result_tone && now_ms >= app->session_result_until &&
       (app->screen == MorseFlipperScreenTxGroups ||
        app->screen == MorseFlipperScreenTxGroupsResult)) {
        app->session_result_tone = false;
        morse_flipper_update_sidetone(app);
    }

    if(app->screen == MorseFlipperScreenTxGroups && app->txg_done && !app->txg_wait_answer &&
       app->txg_result_open_at != 0U) {
        if(now_ms >= app->txg_result_open_at) {
            app->txg_result_open_at = 0U;
            app->txg_result_until = now_ms + ((uint32_t)app->straight_next_delay_s * 1000U);
            app->txg_result_draw_s = 0xFFU;
            scene_manager_next_scene(app->scene_manager, MorseFlipperSceneTxGroupsResult);
        }
        return;
    }

    timeout_from = app->txg_last_input_at ? app->txg_last_input_at : app->txg_wait_started_at;
    if(app->screen == MorseFlipperScreenTxGroups && app->txg_wait_answer && timeout_from != 0U &&
       now_ms - timeout_from >= ((uint32_t)app->straight_answer_timeout_s * 1000U)) {
        morse_flipper_finish_tx_group_timeout(app, now_ms);
        return;
    }

    if(app->screen == MorseFlipperScreenTxGroupsResult && app->txg_result_until != 0U) {
        if(app->txg_result_until > now_ms + 1000U && morse_flipper_straight_answer_down(app)) {
            app->txg_result_until = now_ms + 1000U;
            app->txg_result_draw_s = 0xFFU;
            morse_flipper_view_dirty(app);
        }
        if(now_ms >= app->txg_result_until) {
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, MorseFlipperSceneTxGroups);
            morse_flipper_start_tx_groups_round(app, now_ms);
        } else {
            left_ms = app->txg_result_until - now_ms;
            left_s = (uint8_t)((left_ms + 999U) / 1000U);
            if(left_s != app->txg_result_draw_s) {
                app->txg_result_draw_s = left_s;
                morse_flipper_view_dirty(app);
            }
        }
    }
}

static char morse_flipper_tx_symbol(const MorseFlipperApp* app) {
    if(app->note_sources[1] != 0U) return '.';
    if(app->note_sources[2] != 0U) return '-';
    if(app->note_sources[0] != 0U) return 'K';
    return '?';
}

static bool morse_flipper_tx_decoder_allowed(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->trainer_playback_active || app->straight_playback_active) return false;
    if(app->screen == MorseFlipperScreenSession && !morse_flipper_session_repeat_active(app))
        return false;
    if(app->screen == MorseFlipperScreenTxGroups && !app->txg_wait_answer) return false;
    if(app->screen == MorseFlipperScreenTxGroupsResult ||
       app->screen == MorseFlipperScreenTxGroupsFinal)
        return false;
    return true;
}

static void morse_flipper_feed_tx_edge(MorseFlipperApp* app, bool level, uint32_t now_ms) {
    uint32_t dt;

    if(app == NULL) return;
    if(level == app->rf_tx_level) return;
    if(app->screen == MorseFlipperScreenTxGroups && app->txg_wait_answer)
        app->txg_last_input_at = now_ms;

    /* TX edges feed three consumers: decoder text, TX-group raw timings, and RF trace. */
    if(app->rf_tx_edge_at != 0U) {
        dt = now_ms - app->rf_tx_edge_at;
        if(dt != 0U && morse_flipper_tx_decoder_allowed(app)) {
            if(app->rf_tx_level) {
                morse_flipper_cw_decoder_feed_mark(&app->tx_decoder, (uint16_t)dt);
                if(app->screen == MorseFlipperScreenTxGroups && app->txg_wait_answer)
                    morse_flipper_tx_group_feed_mark(&app->tx_group, (uint16_t)dt);
            } else {
                morse_flipper_cw_decoder_feed_space(&app->tx_decoder, (uint16_t)dt);
                if(app->screen == MorseFlipperScreenTxGroups && app->txg_wait_answer)
                    morse_flipper_tx_group_feed_space(&app->tx_group, (uint16_t)dt);
            }
            morse_flipper_drain_tx_decoder(app);
            if(app->screen == MorseFlipperScreenTxGroups && app->txg_wait_answer)
                morse_flipper_view_dirty(app);
        }
    }

    app->rf_tx_level = level;
    app->rf_tx_edge_at = now_ms;
    app->rf_tx_gap_flushed = level;
    morse_flipper_rf_handle_tx(&app->rf, level, morse_flipper_tx_symbol(app));
}

static void morse_flipper_feed_gpio_edge(MorseFlipperApp* app, bool level, uint32_t now_ms) {
    uint32_t dt;

    if(app == NULL) return;
    if(level == app->gpio_level) return;

    if(app->gpio_edge_at != 0U) {
        dt = now_ms - app->gpio_edge_at;
        if(dt != 0U) {
            if(app->gpio_level) {
                morse_flipper_cw_decoder_feed_mark(&app->gpio_decoder, (uint16_t)dt);
            } else {
                morse_flipper_cw_decoder_feed_space(&app->gpio_decoder, (uint16_t)dt);
            }
            morse_flipper_decoder_drain_into(
                &app->gpio_decoder, app->gpio_text, sizeof(app->gpio_text));
        }
    }

    app->gpio_level = level;
    app->gpio_edge_at = now_ms;
    app->gpio_gap_flushed = level;
}

void morse_flipper_feed_straight_mark(MorseFlipperApp* app, uint16_t mark_ms, uint32_t now_ms) {
    char elem;
    uint16_t gap_ms = 0U;

    if(app == NULL || mark_ms == 0U) return;

    if(app->straight_answer_started_at == 0U)
        app->straight_answer_started_at =
            app->straight_mark_started_at != 0U ? app->straight_mark_started_at : now_ms;

    if(app->straight_mark_started_at != 0U && app->straight_last_input_at != 0U &&
       app->straight_mark_started_at > app->straight_last_input_at) {
        uint32_t gap = app->straight_mark_started_at - app->straight_last_input_at;

        gap_ms = gap > UINT16_MAX ? UINT16_MAX : (uint16_t)gap;
    }

    elem = mark_ms >= (morse_flipper_current_straight_dit_ms(app) * 2U) ? '-' : '.';
    morse_flipper_straight_trainer_feed(&app->straight_trainer, elem, mark_ms, gap_ms);
    app->straight_last_input_at = now_ms;
}

uint32_t morse_flipper_straight_answer_settle_ms(const MorseFlipperApp* app) {
    uint32_t dit_ms;
    uint8_t target_symbols;
    uint8_t answer_symbols;

    if(app == NULL) return 0U;

    dit_ms = morse_flipper_current_straight_dit_ms(app);
    target_symbols = morse_flipper_straight_trainer_target_symbol_count(&app->straight_trainer);
    answer_symbols = morse_flipper_straight_trainer_answer_symbol_count(&app->straight_trainer);

    if(answer_symbols == 0U) return 0U;
    if(app->input_source == MorseFlipperInputSourceButtons) {
        if(answer_symbols >= target_symbols) return dit_ms * 6U;
        return dit_ms * 8U;
    }
    if(answer_symbols >= target_symbols) return dit_ms * 5U;
    return dit_ms * 6U;
}

static bool
    morse_flipper_live_straight_active(MorseFlipperApp* app, bool raw_down, uint32_t now_ms) {
    if(app == NULL) return raw_down;
    return morse_flipper_straight_filter_update(
        &app->straight_filter, raw_down, now_ms, MORSE_FLIPPER_STRAIGHT_RELEASE_DEBOUNCE_MS);
}

static void morse_flipper_sync_gpio_inputs(MorseFlipperApp* app, uint32_t now_ms) {
    bool straight_active = false;
    bool dit_active = false;
    bool dah_active = false;

    /* Probe and training modes can temporarily veto physical GPIO, even if a pin is down. */
    if(app->input_source == MorseFlipperInputSourceStraight) {
        straight_active =
            morse_flipper_live_straight_active(app, morse_flipper_straight_down(), now_ms);
    } else if(app->input_source == MorseFlipperInputSourcePaddle) {
        if(morse_flipper_gpio_probe_use_straight(app)) {
            straight_active = morse_flipper_live_straight_active(
                app, !furi_hal_gpio_read(morse_flipper_dit_pin), now_ms);
        } else if(!morse_flipper_gpio_probe_blocks_start(app)) {
            morse_flipper_straight_filter_reset(&app->straight_filter);
            dit_active = morse_flipper_logical_dit_down(app);
            dah_active = morse_flipper_logical_dah_down(app);
        }
    } else {
        morse_flipper_straight_filter_reset(&app->straight_filter);
    }

    if(app->screen == MorseFlipperScreenTxGroups && app->txg_start_holdoff) {
        if(morse_flipper_straight_answer_down(app)) {
            morse_flipper_straight_filter_reset(&app->straight_filter);
            straight_active = false;
            dit_active = false;
            dah_active = false;
        } else {
            app->txg_start_holdoff = false;
        }
    }

    if(app->screen == MorseFlipperScreenSession && app->session_start_holdoff) {
        if(morse_flipper_session_wait_key_down(app)) {
            morse_flipper_straight_filter_reset(&app->straight_filter);
            straight_active = false;
            dit_active = false;
            dah_active = false;
        } else {
            app->session_start_holdoff = false;
        }
    }

    if(app->screen == MorseFlipperScreenStraight && app->straight_cutoff_wait_release) {
        morse_flipper_straight_filter_reset(&app->straight_filter);
        straight_active = false;
        dit_active = false;
        dah_active = false;
    }

    if(morse_flipper_training_playback_active(app) ||
       app->screen == MorseFlipperScreenSessionEnd || app->screen == MorseFlipperScreenRfRx ||
       app->screen == MorseFlipperScreenTxGroupsResult ||
       app->screen == MorseFlipperScreenTxGroupsFinal ||
       (app->screen == MorseFlipperScreenStraight && !app->straight_wait_answer) ||
       (app->screen == MorseFlipperScreenTxGroups && !app->txg_wait_answer) ||
       (app->screen == MorseFlipperScreenSession && !morse_flipper_session_repeat_active(app))) {
        morse_flipper_straight_filter_reset(&app->straight_filter);
        straight_active = false;
        dit_active = false;
        dah_active = false;
    }

    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, straight_active);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_GPIO_DIT, dit_active, now_ms);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_GPIO_DAH, dah_active, now_ms);
}

static uint8_t morse_flipper_read_input_mask(const MorseFlipperApp* app) {
    uint8_t mask = 0U;

    if(app->left_down) {
        mask |= 1U << 0;
    }
    if(app->ok_down) {
        mask |= 1U << 1;
    }
    if(app->back_down) {
        mask |= 1U << 2;
    }
    if(morse_flipper_straight_down()) {
        mask |= 1U << 3;
    }
    if(morse_flipper_dah_down()) {
        mask |= 1U << 4;
    }
    if(morse_flipper_dit_down()) {
        mask |= 1U << 5;
    }

    return mask;
}

const char* morse_flipper_input_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    uint8_t straight_idx = morse_flipper_gpio_straight_idx(app);
    size_t n = snprintf(buf, buf_sz, "raw ");

    if(app->input_mask & (1U << 0)) {
        n += snprintf(buf + n, buf_sz - n, "lt ");
    }
    if(app->input_mask & (1U << 1)) {
        n += snprintf(buf + n, buf_sz - n, "ok ");
    }
    if(app->input_mask & (1U << 2)) {
        n += snprintf(buf + n, buf_sz - n, "bk ");
    }
    if(app->input_mask & (1U << 3)) {
        n += snprintf(buf + n, buf_sz - n, "%s ", morse_flipper_gpio_name(straight_idx));
    }
    if((app->input_mask & (1U << 4)) && app->gpio_dah_idx != straight_idx) {
        n += snprintf(buf + n, buf_sz - n, "%s ", morse_flipper_gpio_name(app->gpio_dah_idx));
    }
    if((app->input_mask & (1U << 5)) && app->gpio_dit_idx != straight_idx &&
       app->gpio_dit_idx != app->gpio_dah_idx) {
        n += snprintf(buf + n, buf_sz - n, "%s ", morse_flipper_gpio_name(app->gpio_dit_idx));
    }

    if(n == 4U) {
        snprintf(buf, buf_sz, "raw -");
    }

    return buf;
}

static void morse_flipper_session_mode_tick(MorseFlipperApp* app, uint32_t now_ms) {
    morse_flipper_tick_session(app, now_ms);
}

static void morse_flipper_rf_mode_tick(MorseFlipperApp* app, uint32_t now_ms) {
    morse_flipper_tick_live_rf(app, now_ms);
#if MORSE_FLIPPER_RF_LIVE_DECODERS
    if(app->screen == MorseFlipperScreenRfRx) {
        morse_flipper_radio_drain_rx(&app->radio);
    }
#endif
}

static void morse_flipper_tick_ham_wpm_hold(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(app->screen != MorseFlipperScreenHamRun) {
        app->ham.wpm_hold_key = MORSE_FLIPPER_HAM_WPM_HOLD_NONE;
        app->ham.wpm_hold_next_at = 0U;
        return;
    }

    if(app->ham.wpm_hold_key != InputKeyUp && app->ham.wpm_hold_key != InputKeyDown) return;
    if(app->ham.wpm_hold_next_at == 0U || now_ms < app->ham.wpm_hold_next_at) return;

    /* Catch up if the scheduler was late; one missed tick should not slow the ramp. */
    do {
        if(app->ham.wpm_hold_key == InputKeyUp) {
            morse_flipper_set_run_wpm(app, (uint8_t)(morse_flipper_current_wpm(app) + 1U));
        } else {
            uint8_t wpm = morse_flipper_current_wpm(app);
            morse_flipper_set_run_wpm(app, wpm > 0U ? (uint8_t)(wpm - 1U) : 0U);
        }
        app->ham.wpm_hold_next_at += MORSE_FLIPPER_HAM_WPM_HOLD_REPEAT_MS;
    } while(now_ms >= app->ham.wpm_hold_next_at);
}

static void morse_flipper_tick_ham_copy_notice(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL || app->screen != MorseFlipperScreenHamCopyNotice) return;
    if(app->ham.notice_until == 0U || now_ms < app->ham.notice_until) return;

    app->ham.notice_until = 0U;
    app->ham.notice[0] = '\0';
    scene_manager_search_and_switch_to_another_scene(
        app->scene_manager, MorseFlipperSceneHamConfigure);
}

void morse_flipper_active_mode_tick(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    switch(app->screen) {
    case MorseFlipperScreenSession:
        morse_flipper_session_mode_tick(app, now_ms);
        break;
    case MorseFlipperScreenStraight:
        morse_flipper_tick_straight(app, now_ms);
        break;
    case MorseFlipperScreenTxGroups:
    case MorseFlipperScreenTxGroupsResult:
        morse_flipper_tick_tx_groups(app, now_ms);
        break;
    case MorseFlipperScreenHamCopyNotice:
        morse_flipper_tick_ham_copy_notice(app, now_ms);
        break;
    case MorseFlipperScreenHamRun:
        morse_flipper_tick_ham_wpm_hold(app, now_ms);
        morse_flipper_tick_ham_macro(app, now_ms);
        break;
    case MorseFlipperScreenRun:
    case MorseFlipperScreenTrace:
    case MorseFlipperScreenRf:
    case MorseFlipperScreenRfRx:
        morse_flipper_rf_mode_tick(app, now_ms);
        break;
    default:
        break;
    }
}

static void morse_flipper_tick_markdown_scroll(MorseFlipperApp* app) {
    bool changed = false;

    if(app == NULL) return;
    if(app->screen == MorseFlipperScreenOnboarding) {
        changed = cwmd_scroll_tick(&app->onboarding_md);
    } else if(app->screen == MorseFlipperScreenHelp) {
        changed = cwmd_scroll_tick(&app->help_md);
    } else if(app->screen == MorseFlipperScreenAbout && app->about_mode == MorseFlipperAboutModeText) {
        changed = cwmd_scroll_tick(&app->about_md);
    }

    if(changed) morse_flipper_view_dirty(app);
}

static bool morse_flipper_star_animation_active(const MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t elapsed;

    if(app == NULL || app->star_anim_started_at == 0U) return false;
    elapsed = now_ms - app->star_anim_started_at;

    if(app->screen == MorseFlipperScreenSessionEnd) {
        uint8_t score = morse_trainer_session_letter_percent(&app->trainer);
        uint8_t stars = morse_flipper_progress_stars(score);
        uint16_t score_wheel_ms =
            score >= 100U ?
                0U :
                (MORSE_FLIPPER_SCORE_WHEEL_RANDOM_MS + MORSE_FLIPPER_SCORE_WHEEL_RANDOM_JITTER_MS +
                 MORSE_FLIPPER_SCORE_WHEEL_SETTLE_MS + MORSE_FLIPPER_SCORE_WHEEL_SETTLE_JITTER_MS);
        uint16_t duration =
            (uint16_t)(score_wheel_ms + MORSE_FLIPPER_SCORE_SETTLED_PAUSE_MS +
                       MORSE_FLIPPER_SCORE_SLIDE_MS + morse_flipper_star_anim_duration(stars));

        if(elapsed < duration + MORSE_FLIPPER_STAR_REDRAW_MS) return true;
        return score >= 99U;
    }

    return false;
}

static void morse_flipper_tick_star_animation(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(!morse_flipper_star_animation_active(app, now_ms)) {
        app->star_anim_next_redraw_ms = 0U;
        return;
    }

    if(app->star_anim_next_redraw_ms == 0U || now_ms >= app->star_anim_next_redraw_ms) {
        app->star_anim_next_redraw_ms = now_ms + MORSE_FLIPPER_STAR_REDRAW_MS;
        morse_flipper_view_dirty(app);
    }
}

void morse_flipper_poll(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();
    bool old_tone = app->tone_on;
    bool old_busy = app->speaker_busy;
    uint8_t old_mask = app->input_mask;
    bool old_transport = app->transport_connected;
    uint32_t pwm_tone_hz;
    bool raw_straight;
    bool tx_now;

    /*
     * Main tick order is deliberate:
     * mode timers first, then input sampling/keyer events, then edge decoders,
     * then physical outputs. Moving it tends to produce charmingly wrong Morse.
     */
    if(app->pc_mode == MorseFlipperPcModeMidi && app->midi_rx_pending) {
        app->midi_rx_pending = false;
        morse_flipper_handle_midi_rx(app);
    }

    if(morse_flipper_use_pwm_buzzer(app)) {
        pwm_tone_hz = (uint32_t)(morse_flipper_active_tone_hz(app) + 0.5f);
        if(app->audio_pwm.tone_hz != pwm_tone_hz)
            morse_flipper_audio_pwm_set_tone_hz(&app->audio_pwm, pwm_tone_hz);
    }

    morse_flipper_active_mode_tick(app, now_ms);
    morse_flipper_gpio_probe_tick(app, now_ms);

    app->transport_connected = morse_flipper_transport_connected(app);
    if(!old_transport && app->transport_connected) {
        morse_flipper_resync_transport_notes(app);
    }

    app->input_mask = morse_flipper_read_input_mask(app);
    morse_flipper_sync_gpio_inputs(app, now_ms);
    morse_keyer_tick(&app->keyer, now_ms);
    morse_flipper_drain_keyer_events(app);

    raw_straight = morse_flipper_straight_down();
    morse_flipper_feed_gpio_edge(app, raw_straight, now_ms);

    if(!app->gpio_level && !app->gpio_gap_flushed && app->gpio_edge_at != 0U) {
        uint32_t gap = now_ms - app->gpio_edge_at;
        if(gap >= (morse_flipper_current_dit_ms(app) * 5U) / 2U) {
            morse_flipper_cw_decoder_feed_space(&app->gpio_decoder, (uint16_t)gap);
            morse_flipper_decoder_drain_into(
                &app->gpio_decoder, app->gpio_text, sizeof(app->gpio_text));
            app->gpio_gap_flushed = true;
        }
    }

    raw_straight = morse_flipper_straight_answer_down(app);
    if(app->screen == MorseFlipperScreenSession && !app->session_started &&
       !app->trainer_playback_active && app->input_source != MorseFlipperInputSourceButtons &&
       morse_flipper_session_wait_key_down(app)) {
        morse_flipper_start_session(app, now_ms);
        raw_straight = false;
    }
    if(app->screen == MorseFlipperScreenStraight && !app->straight_started &&
       !app->straight_playback_active && !app->straight_wait_answer && !app->straight_done &&
       raw_straight) {
        morse_flipper_start_straight_countdown(app, now_ms);
        raw_straight = false;
    }
    if(app->screen == MorseFlipperScreenTxGroups && !app->txg_started &&
       app->input_source != MorseFlipperInputSourceButtons && raw_straight) {
        morse_flipper_start_tx_groups_round(app, now_ms);
        app->txg_start_holdoff = true;
        raw_straight = false;
    }
    if(app->screen == MorseFlipperScreenStraight &&
       (app->straight_done || morse_flipper_straight_countdown_active(app)) &&
       app->straight_next_at > now_ms + 1000U && raw_straight) {
        app->straight_next_at = now_ms + 1000U;
        app->straight_next_draw_s = 0xFFU;
        morse_flipper_view_dirty(app);
        raw_straight = false;
    }
    if(app->screen == MorseFlipperScreenStraight && app->straight_cutoff_wait_release) {
        if(app->input_source != MorseFlipperInputSourceButtons && !raw_straight) {
            morse_flipper_finish_straight_round(app, now_ms);
        }
        raw_straight = false;
    }
    if(app->input_source != MorseFlipperInputSourceButtons &&
       app->screen == MorseFlipperScreenStraight && !raw_straight &&
       app->straight_mark_started_at != 0U && app->straight_key_down) {
        uint32_t mark_ms = now_ms - app->straight_mark_started_at;

        app->straight_key_down = false;
        if(mark_ms > UINT16_MAX) mark_ms = UINT16_MAX;
        morse_flipper_feed_straight_mark(app, (uint16_t)mark_ms, now_ms);
        app->straight_mark_started_at = 0U;
        morse_flipper_view_dirty(app);
    } else if(
        app->input_source != MorseFlipperInputSourceButtons &&
        app->screen == MorseFlipperScreenStraight && raw_straight && !app->straight_key_down &&
        app->straight_wait_answer) {
        app->straight_key_down = true;
        if(app->straight_answer_started_at == 0U) app->straight_answer_started_at = now_ms;
        app->straight_mark_started_at = now_ms;
        morse_flipper_view_dirty(app);
    }

    tx_now = morse_flipper_any_active_notes(app);
    morse_flipper_feed_tx_edge(app, tx_now, now_ms);
    if(!app->rf_tx_level && !app->rf_tx_gap_flushed && app->rf_tx_edge_at != 0U) {
        uint32_t gap = now_ms - app->rf_tx_edge_at;
        if(gap >= (morse_flipper_current_dit_ms(app) * 5U) / 2U) {
            if(morse_flipper_tx_decoder_allowed(app)) {
                morse_flipper_cw_decoder_feed_space(&app->tx_decoder, (uint16_t)gap);
                morse_flipper_drain_tx_decoder(app);
                if(app->screen == MorseFlipperScreenTxGroups && app->txg_wait_answer)
                    morse_flipper_view_dirty(app);
            }
            app->rf_tx_gap_flushed = true;
        }
    }
    morse_flipper_maybe_finish_tx_group_raw(app, now_ms);
    morse_flipper_ham_log_flush_if_idle(app, now_ms);
    morse_flipper_tick_star_animation(app, now_ms);

    morse_flipper_update_sidetone(app);
    morse_flipper_sync_ptt(app, now_ms);
    morse_flipper_sync_backlight(app, now_ms);

    if(old_tone != app->tone_on || old_busy != app->speaker_busy || old_mask != app->input_mask ||
       old_transport != app->transport_connected) {
        morse_flipper_view_dirty(app);
    }
}

void morse_flipper_tick_callback(void* context) {
    MorseFlipperApp* app = context;
    uint32_t now_ms = furi_get_tick();

    morse_flipper_poll(app);
    morse_flipper_tick_trainer_playback(app, now_ms);
    morse_flipper_tick_about(app, now_ms);
    morse_flipper_tick_markdown_scroll(app);
    morse_flipper_tick_progress_history_scroll(app, now_ms);

    if(app->preview_ticks > 0U) {
        app->preview_ticks--;
        morse_flipper_update_sidetone(app);
    }

    scene_manager_handle_tick_event(app->scene_manager);
}
