/*
 * Purpose: Route physical input events into active app features.
 * Owns: screen-specific button handling and custom event dispatch.
 * Depends on: morse_flipper_app_i.h and Flipper input events.
 * Tests: firmware build; button flow is hardware-only.
 */

#include "morse_flipper_app_i.h"

#define MORSE_FLIPPER_ABOUT_OK_FAST_MS 500U
#define MORSE_FLIPPER_MD_LINE_H        9U
#define MORSE_FLIPPER_MD_VIEW_H        48U
#define MORSE_FLIPPER_MD_VISIBLE_LINES (MORSE_FLIPPER_MD_VIEW_H / MORSE_FLIPPER_MD_LINE_H)
#define MORSE_FLIPPER_MD_SCROLL_STEP_PX \
    ((MORSE_FLIPPER_MD_VISIBLE_LINES - 1U) * MORSE_FLIPPER_MD_LINE_H)

static bool morse_flipper_help_input(MorseFlipperApp* app, const InputEvent* event) {
    if(app->screen != MorseFlipperScreenHelp) return false;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event->key == InputKeyLeft &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MorseFlipperCustomHelpPrev);
        return true;
    }

    if((event->key == InputKeyUp || event->key == InputKeyDown || event->key == InputKeyRight) &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        int16_t old_target = app->help_md.target_scroll_px;
        int16_t max_scroll = app->help_md.max_scroll_px;
        bool forward = event->key != InputKeyUp;

        if(forward && morse_flipper_help_is_chapter_card(app)) {
            view_dispatcher_send_custom_event(app->view_dispatcher, MorseFlipperCustomHelpNext);
            return true;
        }

        cwmd_scroll_step(
            &app->help_md, forward ? 1 : -1, max_scroll, MORSE_FLIPPER_MD_SCROLL_STEP_PX);
        if(app->help_md.target_scroll_px == old_target) {
            if(forward && app->help_md.scroll_px >= max_scroll && old_target >= max_scroll) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MorseFlipperCustomHelpNext);
            }
            return true;
        }
        morse_flipper_view_dirty(app);
        return true;
    }

    return true;
}

static bool morse_flipper_about_input(MorseFlipperApp* app, const InputEvent* event) {
    uint32_t now_ms;

    if(app->screen != MorseFlipperScreenAbout) return false;

    if(app->about_mode == MorseFlipperAboutModeLanding && event->type == InputTypeShort) {
        if(app->about_show_next) {
            app->about_mode = MorseFlipperAboutModeText;
            app->about_md = (CwmdState){0};
            app->about_ok_count = 0U;
            app->about_last_ok_ms = 0U;
            morse_flipper_view_dirty(app);
        }
        return true;
    }

    if(app->about_mode == MorseFlipperAboutModeText && event->key == InputKeyOk &&
       event->type == InputTypeShort) {
        now_ms = furi_get_tick();
        if(app->about_last_ok_ms != 0U &&
           now_ms - app->about_last_ok_ms <= MORSE_FLIPPER_ABOUT_OK_FAST_MS) {
            app->about_ok_count++;
        } else {
            app->about_ok_count = 1U;
        }
        app->about_last_ok_ms = now_ms;

        if(app->about_ok_count >= 3U) {
            app->about_ok_count = 0U;
            app->about_last_ok_ms = 0U;
            morse_flipper_scene_open(app, MorseFlipperSceneTrace);
            return true;
        }

        int16_t old_target = app->about_md.target_scroll_px;
        cwmd_scroll_step(
            &app->about_md, 1, app->about_md.max_scroll_px, MORSE_FLIPPER_MD_SCROLL_STEP_PX);
        if(app->about_md.target_scroll_px == old_target) return true;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(app->about_mode == MorseFlipperAboutModeText && event->key == InputKeyDown &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app->about_ok_count = 0U;
        app->about_last_ok_ms = 0U;
        int16_t old_target = app->about_md.target_scroll_px;
        cwmd_scroll_step(
            &app->about_md, 1, app->about_md.max_scroll_px, MORSE_FLIPPER_MD_SCROLL_STEP_PX);
        if(app->about_md.target_scroll_px == old_target) return true;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(app->about_mode == MorseFlipperAboutModeText && event->key == InputKeyUp &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app->about_ok_count = 0U;
        app->about_last_ok_ms = 0U;
        int16_t old_target = app->about_md.target_scroll_px;
        cwmd_scroll_step(
            &app->about_md, -1, app->about_md.max_scroll_px, MORSE_FLIPPER_MD_SCROLL_STEP_PX);
        if(app->about_md.target_scroll_px == old_target) return true;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        app->about_mode = MorseFlipperAboutModeLanding;
        app->about_md = (CwmdState){0};
        app->about_ok_count = 0U;
        app->about_last_ok_ms = 0U;
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static bool morse_flipper_startup_probe_input(MorseFlipperApp* app, const InputEvent* event) {
    if(app->screen != MorseFlipperScreenStartupProbe) return false;

    if(!morse_flipper_gpio_probe_forces_straight(app->startup_gpio_probe_state)) {
        return false;
    }

    if(event->key == InputKeyOk &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        app->input_source = MorseFlipperInputSourceStraight;
        app->startup_gpio_probe_state = MorseFlipperGpioProbeOk;
        morse_flipper_save_config(app);
        morse_flipper_refresh_keyer(app, furi_get_tick());
        morse_flipper_poll(app);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuMain);
        return true;
    }

    return false;
}

static uint8_t morse_flipper_ham_dir_from_key(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return MorseFlipperHamKeyerDirUp;
    case InputKeyDown:
        return MorseFlipperHamKeyerDirDown;
    case InputKeyLeft:
        return MorseFlipperHamKeyerDirLeft;
    case InputKeyRight:
        return MorseFlipperHamKeyerDirRight;
    case InputKeyOk:
        return MorseFlipperHamKeyerDirOk;
    default:
        return MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS;
    }
}

static void morse_flipper_ham_bump_run_wpm(MorseFlipperApp* app, InputKey key) {
    if(key == InputKeyUp) {
        morse_flipper_set_run_wpm(app, (uint8_t)(morse_flipper_current_wpm(app) + 1U));
    } else if(key == InputKeyDown) {
        uint8_t wpm = morse_flipper_current_wpm(app);
        morse_flipper_set_run_wpm(app, wpm > 0U ? (uint8_t)(wpm - 1U) : 0U);
    }
}

/*
 * Ham run mode borrows the same five buttons for macros, WPM, break-in, and exit.
 * Consume those modal meanings here before the general live-keying path sees them.
 */
static bool morse_flipper_ham_shell_input(MorseFlipperApp* app, const InputEvent* event) {
    uint8_t dir;

    if(app->screen != MorseFlipperScreenHamStartRefusal &&
       app->screen != MorseFlipperScreenHamAssign &&
       app->screen != MorseFlipperScreenHamAssignments &&
       app->screen != MorseFlipperScreenHamCopyNotice &&
       app->screen != MorseFlipperScreenHamDeleteConfirm &&
       app->screen != MorseFlipperScreenHamRun)
        return false;

    if(app->screen == MorseFlipperScreenHamDeleteConfirm) {
        if(event->key == InputKeyOk &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            uint8_t next_selection = app->ham.selected_message;

            morse_flipper_ham_keyer_delete_message(&app->ham_keyer, app->ham.selected_message);
            if(app->ham_keyer.message_count == 0U) {
                scene_manager_set_scene_state(
                    app->scene_manager,
                    MorseFlipperSceneHamConfigure,
                    MorseFlipperHamConfigureAdd);
            } else {
                if(next_selection >= app->ham_keyer.message_count)
                    next_selection = (uint8_t)(app->ham_keyer.message_count - 1U);
                app->ham.selected_message = next_selection;
                scene_manager_set_scene_state(
                    app->scene_manager,
                    MorseFlipperSceneHamConfigure,
                    MorseFlipperHamConfigureMessageBase + next_selection);
            }
            morse_flipper_save_config(app);
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, MorseFlipperSceneHamConfigure);
            return true;
        }

        if((event->key == InputKeyBack || event->key == InputKeyLeft) &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
            return true;
        }

        return true;
    }

    if(app->screen == MorseFlipperScreenHamCopyNotice) {
        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, MorseFlipperSceneHamConfigure);
            return true;
        }

        return true;
    }

    if(app->screen == MorseFlipperScreenHamAssignments && event->key == InputKeyLeft &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(app->screen == MorseFlipperScreenHamRun) {
        if(event->key == InputKeyLeft && event->type == InputTypeLong) {
            morse_flipper_leave_live_screen(app, furi_get_tick());
            return true;
        }

        if(event->key == InputKeyBack && event->type == InputTypeShort) {
            if(app->ham.macro_active) {
                morse_flipper_ham_stop_macro(app);
                morse_flipper_update_sidetone(app);
                morse_flipper_view_dirty(app);
                return true;
            }

            app->ham_keyer.break_in_enabled = !app->ham_keyer.break_in_enabled;
            if(!app->ham_keyer.break_in_enabled) {
                app->ptt_tail_until = 0U;
            }
            if(app->ham_keyer.break_in_enabled) {
                morse_flipper_run_history_append(&app->run_history, "\n[BKON]\n");
                morse_flipper_ham_log_append_marker(app, "[BKON]", furi_get_tick());
            } else {
                morse_flipper_run_history_append(&app->run_history, "\n[BKOFF]\n");
                morse_flipper_ham_log_append_marker(app, "[BKOFF]", furi_get_tick());
            }
            morse_flipper_sync_audio_output(app);
            morse_flipper_view_dirty(app);
            return true;
        }

        if((event->key == InputKeyUp || event->key == InputKeyDown) &&
           event->type == InputTypeLong) {
            uint32_t now_ms = furi_get_tick();

            morse_flipper_ham_bump_run_wpm(app, event->key);
            app->ham.wpm_hold_key = (uint8_t)event->key;
            app->ham.wpm_hold_next_at = now_ms + MORSE_FLIPPER_HAM_WPM_HOLD_REPEAT_MS;
            return true;
        }

        if((event->key == InputKeyUp || event->key == InputKeyDown) &&
           event->type == InputTypeRelease && app->ham.wpm_hold_key == (uint8_t)event->key) {
            app->ham.wpm_hold_key = MORSE_FLIPPER_HAM_WPM_HOLD_NONE;
            app->ham.wpm_hold_next_at = 0U;
            return true;
        }

        if(event->type == InputTypeShort) {
            dir = morse_flipper_ham_dir_from_key(event->key);
            if(dir < MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS) {
                const char* text = morse_flipper_ham_keyer_assignment_text(&app->ham_keyer, dir);

                if(text[0] != '\0') {
                    morse_flipper_ham_start_macro(app, text, furi_get_tick());
                    app->ham.macro_dir = dir;
                } else {
                    snprintf(
                        app->ham.notice,
                        sizeof(app->ham.notice),
                        "No %s",
                        morse_flipper_ham_keyer_dir_label(dir));
                    app->ham.notice_until = furi_get_tick() + 700U;
                    morse_flipper_view_dirty(app);
                }
                return true;
            }
        }

        return true;
    }

    if(app->screen == MorseFlipperScreenHamAssign && event->type == InputTypeShort) {
        dir = morse_flipper_ham_dir_from_key(event->key);
        if(dir < MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS) {
            morse_flipper_ham_keyer_assign(&app->ham_keyer, dir, app->ham.selected_message);
            morse_flipper_save_config(app);
            morse_flipper_scene_back(app);
            return true;
        }
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    return true;
}

bool morse_flipper_input_chunk_a(MorseFlipperApp* app, InputEvent* event) {
    if(morse_flipper_help_input(app, event)) return true;
    if(morse_flipper_about_input(app, event)) return true;
    if(morse_flipper_startup_probe_input(app, event)) return true;
    if(morse_flipper_ham_shell_input(app, event)) return true;
    return false;
}

static bool morse_flipper_trainer_input(MorseFlipperApp* app, const InputEvent* event) {
    if(app->screen != MorseFlipperScreenTrainer) return false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        morse_flipper_scene_open(app, MorseFlipperSceneSession);
        return true;
    }

    if(event->key == InputKeyUp &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app->trainer_row = app->trainer_row == 0U ? 3U : (app->trainer_row - 1U);
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyDown &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app->trainer_row = (app->trainer_row + 1U) % 4U;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyLeft &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        morse_flipper_cycle_trainer_value(app, -1);
        return true;
    }

    if(event->key == InputKeyRight &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        morse_flipper_cycle_trainer_value(app, 1);
        return true;
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        app->trainer_playback_active = false;
        app->trainer_playback_mark = false;
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static bool
    morse_flipper_straight_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    if(app->screen != MorseFlipperScreenStraight) return false;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(morse_flipper_gpio_probe_blocks_start(app)) {
        return true;
    }

    if((app->straight_done || morse_flipper_straight_countdown_active(app)) &&
       (event->type == InputTypePress || event->type == InputTypeShort ||
        event->type == InputTypeLong)) {
        if(app->straight_next_at > now_ms + 1000U) {
            app->straight_next_at = now_ms + 1000U;
            app->straight_next_draw_s = 0xFFU;
            morse_flipper_view_dirty(app);
        }
        return true;
    }

    if(app->straight_wait_answer && app->input_source == MorseFlipperInputSourceButtons &&
       event->key == InputKeyOk) {
        if(app->straight_cutoff_wait_release) {
            if(event->type == InputTypeRelease) {
                app->ok_down = false;
                morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
                morse_flipper_finish_straight_round(app, now_ms);
            }
            return true;
        }

        if(event->type == InputTypePress) {
            app->ok_down = true;
            app->straight_key_down = true;
            if(app->straight_answer_started_at == 0U) app->straight_answer_started_at = now_ms;
            app->straight_mark_started_at = now_ms;
            morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, true);
            morse_flipper_view_dirty(app);
        } else if(event->type == InputTypeRelease) {
            uint32_t dt = 0U;

            app->ok_down = false;
            morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
            if(app->straight_key_down && app->straight_mark_started_at != 0U)
                dt = now_ms - app->straight_mark_started_at;
            app->straight_key_down = false;
            if(dt != 0U) {
                if(dt > UINT16_MAX) dt = UINT16_MAX;
                morse_flipper_feed_straight_mark(app, (uint16_t)dt, now_ms);
                morse_flipper_view_dirty(app);
            }
            app->straight_mark_started_at = 0U;
        }
        return true;
    }

    if(!app->straight_playback_active && !app->straight_wait_answer && !app->straight_done &&
       event->key == InputKeyOk &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_start_straight_countdown(app, now_ms);
        return true;
    }

    return false;
}

static bool
    morse_flipper_tx_groups_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    bool back_key;

    if(app->screen != MorseFlipperScreenTxGroups) return false;

    back_key = app->input_source == MorseFlipperInputSourceButtons && !app->txg_sk;

    if(morse_flipper_gpio_probe_blocks_start(app)) {
        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
        }
        return true;
    }

    if(!app->txg_started) {
        if(event->key == InputKeyOk &&
           (event->type == InputTypePress || event->type == InputTypeShort ||
            event->type == InputTypeLong)) {
            morse_flipper_start_tx_groups_round(app, now_ms);
            return true;
        }

        if(back_key && event->key == InputKeyBack && event->type == InputTypePress) {
            morse_flipper_start_tx_groups_round(app, now_ms);
            return true;
        }

        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
            return true;
        }

        return false;
    }

    if(event->key == InputKeyLeft && event->type == InputTypeLong) {
        morse_flipper_leave_tx_groups(app, now_ms);
        return true;
    }

    if(!back_key && event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_leave_tx_groups(app, now_ms);
        return true;
    }

    return false;
}

static bool morse_flipper_tx_groups_result_input(
    MorseFlipperApp* app,
    const InputEvent* event,
    uint32_t now_ms) {
    if(app->screen != MorseFlipperScreenTxGroupsResult) return false;

    if((event->key == InputKeyBack &&
        (event->type == InputTypeShort || event->type == InputTypeLong)) ||
       (event->key == InputKeyLeft && event->type == InputTypeLong)) {
        morse_flipper_leave_tx_groups(app, now_ms);
        return true;
    }

    if(event->type == InputTypePress || event->type == InputTypeShort ||
       event->type == InputTypeLong) {
        if(app->txg_result_until > now_ms + 1000U) {
            app->txg_result_until = now_ms + 1000U;
            app->txg_result_draw_s = 0xFFU;
            morse_flipper_view_dirty(app);
        }
        return true;
    }

    return true;
}

static bool morse_flipper_tx_groups_final_input(
    MorseFlipperApp* app,
    const InputEvent* event,
    uint32_t now_ms) {
    if(app->screen != MorseFlipperScreenTxGroupsFinal) return false;
    UNUSED(now_ms);

    if((event->key == InputKeyBack || event->key == InputKeyOk) &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuTraining);
        return true;
    }

    if(event->key == InputKeyLeft && event->type == InputTypeLong) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuTraining);
        return true;
    }

    return true;
}

static bool
    morse_flipper_session_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    MorseFlipperInputGate g;

    if(app->screen != MorseFlipperScreenSession) return false;
    g = morse_flipper_input_gate(app);

    if((morse_flipper_gpio_probe_notice_active(app) ||
        morse_flipper_gpio_probe_blocks_start(app)) &&
       event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_leave_session(app, now_ms);
        return true;
    }

    if(morse_flipper_gpio_probe_notice_active(app) || morse_flipper_gpio_probe_blocks_start(app)) {
        return true;
    }

    if(morse_flipper_session_repeat_active(app)) {
        if(event->key == InputKeyLeft && event->type == InputTypeLong) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(g.back_exit && event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(event->key == InputKeyBack && g.back_key) return true;
        if(event->key == InputKeyOk && g.btn) return true;
        if(event->key == InputKeyLeft && morse_flipper_session_left_exit_active(app)) return true;

        return false;
    }

    if(morse_flipper_session_running_view(app)) {
        if(event->key == InputKeyLeft && event->type == InputTypeLong) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(g.back_exit && event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if((event->type == InputTypePress || event->type == InputTypeShort ||
            event->type == InputTypeLong) &&
           ((event->key == InputKeyOk && g.btn) || (event->key == InputKeyBack && g.back_key))) {
            if(morse_flipper_session_hurry(app, now_ms)) return true;
        }

        if(event->key == InputKeyBack && g.back_key) return true;
        if(event->key == InputKeyOk && g.btn) return true;
        if(event->key == InputKeyLeft && morse_flipper_session_left_exit_active(app)) return true;
    }

    if(event->key == InputKeyOk && event->type == InputTypeShort &&
       !morse_trainer_session_active(&app->trainer) && !app->trainer_playback_active) {
        morse_flipper_start_session(app, now_ms);
        return true;
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_leave_session(app, now_ms);
        return true;
    }

    return false;
}

static void morse_flipper_leave_session_end(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;
    morse_flipper_reset_session_state(app, now_ms);
    if(scene_manager_search_and_switch_to_previous_scene(
           app->scene_manager, MorseFlipperSceneMenuTraining))
        return;
    scene_manager_search_and_switch_to_another_scene(
        app->scene_manager, MorseFlipperSceneMenuTraining);
}

static bool morse_flipper_session_end_input(
    MorseFlipperApp* app,
    const InputEvent* event,
    uint32_t now_ms) {
    if(app->screen != MorseFlipperScreenSessionEnd) return false;

    if((event->key == InputKeyOk || event->key == InputKeyBack) &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_leave_session_end(app, now_ms);
        return true;
    }

    return false;
}

static bool morse_flipper_rf_freq_input(MorseFlipperApp* app, const InputEvent* event) {
    if(app->screen != MorseFlipperScreenRfFreq) return false;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_rf_commit_edit(app);
        morse_flipper_scene_back(app);
        return true;
    }

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return true;

    if(event->key == InputKeyLeft) {
        morse_flipper_rf_bump_focus(app, -1);
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyRight) {
        morse_flipper_rf_bump_focus(app, 1);
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyUp) {
        morse_flipper_rf_bump_digit(app, 1);
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyDown) {
        morse_flipper_rf_bump_digit(app, -1);
        morse_flipper_view_dirty(app);
        return true;
    }

    return true;
}

static bool morse_flipper_rf_input(MorseFlipperApp* app, const InputEvent* event) {
    if(app->screen != MorseFlipperScreenRf) return false;
    if(!morse_flipper_rf_tx_allowed_khz(morse_flipper_rf_frequency_khz(&app->rf))) {
        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
        }
        return true;
    }

    morse_flipper_handle_active_keying_event(app, event);
    return true;
}

static bool morse_flipper_rf_rx_input(MorseFlipperApp* app, const InputEvent* event) {
    if(app->screen != MorseFlipperScreenRfRx) return false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        app->rf_rx_audio_enabled = !app->rf_rx_audio_enabled;
        app->rf_monitor_tone = app->rf_rx_audio_enabled && app->rf_rx_level;
        morse_flipper_update_sidetone(app);
        morse_flipper_view_dirty(app);
        return true;
    }

    if((event->key == InputKeyLeft || event->key == InputKeyRight) &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        int dir = event->key == InputKeyLeft ? -1 : 1;

        app->rf_monitor_threshold_dbm =
            morse_flipper_rf_clamp_dbm((int8_t)(app->rf_monitor_threshold_dbm + dir));
        app->rf_monitor_tone = app->rf_rx_audio_enabled && app->rf_rx_level;
        morse_flipper_update_sidetone(app);
        morse_flipper_view_dirty(app);
        return true;
    }

    if((event->key == InputKeyUp || event->key == InputKeyDown) &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        morse_flipper_rf_rx_bump_wpm(app, event->key == InputKeyUp ? 1 : -1);
        return true;
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    return true;
}

static bool morse_flipper_run_trace_home_input(MorseFlipperApp* app, InputEvent* event) {
    if(app->screen == MorseFlipperScreenRun) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(app->screen == MorseFlipperScreenTrace) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(app->screen != MorseFlipperScreenHome) return false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyLeft) {
            morse_flipper_tone_nudge(app, -1);
            return true;
        } else if(event->key == InputKeyRight) {
            morse_flipper_tone_nudge(app, 1);
            return true;
        }
    }

    if(event->key == InputKeyUp && event->type == InputTypeShort) {
        morse_flipper_toggle_source(app);
        return true;
    }

    if(event->key == InputKeyDown && event->type == InputTypeShort) {
        morse_flipper_cycle_mode(app);
        return true;
    }

    if(event->key == InputKeyDown && event->type == InputTypeLong) {
        morse_flipper_toggle_handedness(app);
        return true;
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

/* Active screens get first refusal for navigation; live keying is the fallback. */
bool morse_flipper_active_mode_input(MorseFlipperApp* app, InputEvent* event, uint32_t now_ms) {
    if(app == NULL || event == NULL) return false;

    switch(app->screen) {
    case MorseFlipperScreenTrainer:
        return morse_flipper_trainer_input(app, event);
    case MorseFlipperScreenStraight:
        return morse_flipper_straight_input(app, event, now_ms);
    case MorseFlipperScreenTxGroups:
        return morse_flipper_tx_groups_input(app, event, now_ms);
    case MorseFlipperScreenTxGroupsResult:
        return morse_flipper_tx_groups_result_input(app, event, now_ms);
    case MorseFlipperScreenTxGroupsFinal:
        return morse_flipper_tx_groups_final_input(app, event, now_ms);
    case MorseFlipperScreenSession:
        return morse_flipper_session_input(app, event, now_ms);
    case MorseFlipperScreenSessionEnd:
        return morse_flipper_session_end_input(app, event, now_ms);
    case MorseFlipperScreenRfFreq:
        return morse_flipper_rf_freq_input(app, event);
    case MorseFlipperScreenRfRx:
        return morse_flipper_rf_rx_input(app, event);
    case MorseFlipperScreenRf:
        return morse_flipper_rf_input(app, event);
    case MorseFlipperScreenRun:
    case MorseFlipperScreenTrace:
    case MorseFlipperScreenHome:
        return morse_flipper_run_trace_home_input(app, event);
    default:
        break;
    }

    return false;
}

/*
 * Button input can be straight key, paddle, or plain navigation depending on mode.
 * The gate tells us which meaning is live, so Back does not become a dah by accident.
 */
bool morse_flipper_input_chunk_b(MorseFlipperApp* app, InputEvent* event, uint32_t now_ms) {
    return morse_flipper_active_mode_input(app, event, now_ms);
}

static bool
    morse_flipper_session_live_keying_input(MorseFlipperApp* app, const InputEvent* event) {
    MorseFlipperInputGate g;

    if(app->screen != MorseFlipperScreenSession && app->screen != MorseFlipperScreenTxGroups)
        return false;
    if(app->screen == MorseFlipperScreenSession && !morse_flipper_session_repeat_active(app))
        return false;
    if(app->screen == MorseFlipperScreenTxGroups && !app->txg_wait_answer) return false;
    if(event->type != InputTypePress && event->type != InputTypeRelease) return false;

    g = morse_flipper_input_gate(app);

    if(event->key == InputKeyOk && g.btn) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(event->key == InputKeyBack && g.back_key) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(event->key == InputKeyLeft && morse_flipper_session_left_exit_active(app)) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    return false;
}

void morse_flipper_handle_active_keying_event(MorseFlipperApp* app, const InputEvent* event) {
    uint32_t now_ms = furi_get_tick();
    MorseFlipperInputGate g = morse_flipper_input_gate(app);
    bool btn_src = g.btn;
    bool btn_str = g.btn_str;
    bool btn_pad = g.btn_pad;

    /* Left is a key while held, but a short tap clears run text. Awkward, documented. */
    if((app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenRf) &&
       event->key == InputKeyLeft && event->type == InputTypeShort) {
        morse_flipper_reset_run_state(app);
        morse_flipper_view_dirty(app);
        return;
    }

    if(btn_src && event->key == InputKeyLeft) {
        if(event->type == InputTypePress) {
            app->left_down = true;
        } else if(event->type == InputTypeRelease) {
            app->left_down = false;
        } else if(event->type == InputTypeLong) {
            if(app->screen == MorseFlipperScreenTxGroups)
                morse_flipper_leave_tx_groups(app, now_ms);
            else
                morse_flipper_leave_live_screen(app, now_ms);
        }
        return;
    }

    if(btn_src && event->key == InputKeyOk) {
        if(event->type == InputTypePress) {
            app->ok_down = true;
            if(btn_str) {
                morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, true);
            } else if(btn_pad) {
                morse_flipper_resync_button_paddles(app, now_ms);
            }
        } else if(event->type == InputTypeRelease) {
            app->ok_down = false;
            if(btn_str) {
                morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
            } else if(btn_pad) {
                morse_flipper_resync_button_paddles(app, now_ms);
            }
        }
        return;
    }

    if(btn_pad && event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            app->back_down = true;
            morse_flipper_resync_button_paddles(app, now_ms);
        } else if(event->type == InputTypeRelease) {
            app->back_down = false;
            morse_flipper_resync_button_paddles(app, now_ms);
        }
        return;
    }

    if(g.back_exit && event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        if(app->screen == MorseFlipperScreenTxGroups)
            morse_flipper_leave_tx_groups(app, now_ms);
        else
            morse_flipper_leave_live_screen(app, now_ms);
        return;
    }

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenRf) {
        if(app->screen == MorseFlipperScreenRun && event->key == InputKeyUp &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            morse_flipper_set_run_wpm(app, (uint8_t)(morse_flipper_current_wpm(app) + 1U));
            return;
        }

        if(app->screen == MorseFlipperScreenRun && event->key == InputKeyDown &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            uint8_t wpm = morse_flipper_current_wpm(app);
            morse_flipper_set_run_wpm(app, wpm > 0U ? (uint8_t)(wpm - 1U) : 0U);
            return;
        }

        return;
    }

    if(event->key == InputKeyUp && event->type == InputTypeShort) {
        morse_flipper_toggle_source(app);
        return;
    }

    if(event->key == InputKeyDown && event->type == InputTypeShort) {
        morse_flipper_cycle_mode(app);
        return;
    }

    if(event->key == InputKeyDown && event->type == InputTypeLong) {
        morse_flipper_toggle_handedness(app);
        return;
    }

    if(event->key == InputKeyRight &&
       (event->type == InputTypeShort || event->type == InputTypeLong))
        morse_flipper_scene_back(app);
}

void morse_flipper_tone_nudge(MorseFlipperApp* app, int dir) {
    if(app->audio_path == MorseFlipperAudioPathVibration) return;

    int idx = app->tone_idx < COUNT_OF(morse_flipper_tones) ? (int)app->tone_idx :
                                                              (int)MORSE_FLIPPER_DEFAULT_TONE_IDX;
    int current_idx = idx;

    idx += dir;

    if(idx < 0) idx = 0;
    if(idx >= (int)COUNT_OF(morse_flipper_tones)) idx = (int)COUNT_OF(morse_flipper_tones) - 1;
    if(idx == current_idx) return;

    app->tone_idx = (uint8_t)idx;
    app->preview_ticks = MORSE_FLIPPER_PREVIEW_TICKS;

    morse_flipper_save_config(app);
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

bool morse_flipper_live_input(InputEvent* event, void* ctx) {
    MorseFlipperApp* app = ctx;
    uint32_t now_ms = furi_get_tick();

    if(morse_flipper_input_chunk_a(app, event)) return true;
    if(morse_flipper_session_live_keying_input(app, event)) return true;
    if(morse_flipper_active_mode_input(app, event, now_ms)) return true;
    return false;
}

bool morse_flipper_custom_event_callback(void* context, uint32_t event) {
    MorseFlipperApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

bool morse_flipper_back_event_callback(void* context) {
    MorseFlipperApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}
