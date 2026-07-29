/*
 * Purpose: Route physical input events into active app features.
 * Owns: screen-specific button handling and custom event dispatch.
 * Depends on: morse_flipper_app_i.h and Flipper input events.
 * Tests: firmware build; button flow is hardware-only.
 */

#include "morse_flipper_app_i.h"

#include <string.h>

#define MORSE_FLIPPER_PROGRESS_SCROLL_BASE_MS  125U
#define MORSE_FLIPPER_PROGRESS_SCROLL_FAST_MS  42U
#define MORSE_FLIPPER_PROGRESS_SCROLL_ACCEL_MS 1000U

static bool morse_flipper_content_input(MorseFlipperApp* app, const InputEvent* event) {
    if(app->screen != MorseFlipperScreenOnboarding && app->screen != MorseFlipperScreenHelp &&
       app->screen != MorseFlipperScreenAbout)
        return false;
    if(morse_flipper_content_host_input(app, event, furi_get_tick())) return true;
    if(event->type == InputTypeShort || event->type == InputTypeLong) {
        bool leave = app->screen == MorseFlipperScreenOnboarding ?
                         (event->key == InputKeyOk || event->key == InputKeyBack) :
                     app->screen == MorseFlipperScreenAbout ?
                         (event->key == InputKeyBack || event->key == InputKeyLeft) :
                         event->key == InputKeyBack;
        if(leave) {
            if(app->screen == MorseFlipperScreenOnboarding)
                morse_flipper_onboarding_finish(app);
            else
                morse_flipper_scene_back(app);
            morse_flipper_plugin_runtime_unload_current(app);
        }
    }
    return true;
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
        scene_manager_previous_scene(app->scene_manager);
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
    if(morse_flipper_content_input(app, event)) return true;
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

    if(back_key && event->key == InputKeyBack) return true;

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
        bool edit_key = event->key == InputKeyDown || event->key == InputKeyUp;
        bool delete_event = event->type == InputTypeShort && edit_key;

        if(!edit_key) morse_flipper_session_cancel_answer_flash(app);

        if(event->key == InputKeyLeft && event->type == InputTypeLong) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(delete_event) {
            bool changed;
            if(event->key == InputKeyDown) {
                changed = morse_flipper_session_backspace_answer(app, now_ms);
            } else {
                changed = morse_flipper_session_clear_answer(app, now_ms);
            }
            if(!changed) morse_flipper_session_cancel_answer_flash(app);
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

    if(app->progress_debug_result) {
        uint8_t lesson = app->progress_debug_prev_lesson;
        uint8_t groups = app->progress_debug_prev_groups;

        app->progress_debug_result = false;
        app->progress_debug_returning = true;
        morse_flipper_reset_session_state(app, now_ms);
        morse_trainer_set_lesson(&app->trainer, lesson);
        morse_trainer_set_session_groups(&app->trainer, groups);
        if(scene_manager_previous_scene(app->scene_manager)) return;

        app->progress_debug_returning = false;
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneProgress);
        return;
    }

    morse_flipper_reset_session_state(app, now_ms);
    if(scene_manager_search_and_switch_to_previous_scene(
           app->scene_manager, MorseFlipperSceneMenuTraining))
        return;
    scene_manager_search_and_switch_to_another_scene(
        app->scene_manager, MorseFlipperSceneMenuTraining);
}

static void morse_flipper_session_advance_lesson(MorseFlipperApp* app, uint32_t now_ms) {
    uint8_t lesson;

    lesson = morse_trainer_lesson(&app->trainer);
    app->listening_settings.lesson = (uint8_t)(lesson + 1U);
    morse_trainer_set_lesson(&app->trainer, app->listening_settings.lesson);
    morse_flipper_save_config(app);
    if(!scene_manager_search_and_switch_to_another_scene(
           app->scene_manager, MorseFlipperSceneSession))
        scene_manager_next_scene(app->scene_manager, MorseFlipperSceneSession);
    morse_flipper_start_session(app, now_ms);
}

static bool morse_flipper_session_end_input(
    MorseFlipperApp* app,
    const InputEvent* event,
    uint32_t now_ms) {
    if(app->screen != MorseFlipperScreenSessionEnd) return false;

    if(app->session_offer_next) {
        if(event->type == InputTypeShort || event->type == InputTypeLong) {
            if(event->key == InputKeyOk) {
                morse_flipper_session_advance_lesson(app, now_ms);
                return true;
            }
            if(event->key == InputKeyBack) {
                app->session_offer_next = false;
                goto leave_session_end;
            }
        }
        return true;
    }

    if(app->session_next_eligible &&
       event->key == InputKeyOk && event->type == InputTypeShort) {
        app->session_next_eligible = false;
        app->session_offer_next = true;
        morse_flipper_view_dirty(app);
        return true;
    }

    if((event->key == InputKeyOk || event->key == InputKeyBack) &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
leave_session_end:
        morse_flipper_leave_session_end(app, now_ms);
        return true;
    }

    return false;
}

static void morse_flipper_progress_load_recent(MorseFlipperApp* app) {
    uint16_t today = MORSE_FLIPPER_PROGRESS_DAY_NONE;
    uint16_t day;
    bool today_valid;

    if(app == NULL) return;
    app->progress_scroll_key = 0xFFU;
    app->progress_scroll_started_ms = 0U;
    app->progress_scroll_next_ms = 0U;
    today_valid = morse_flipper_progress_today(&today);
    day = morse_flipper_progress_history_start_day(app->view_progress, today_valid, today);
    morse_flipper_progress_history_view_reset(&app->progress_history, day);
}

static void morse_flipper_progress_reset_scroll_repeat(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->progress_scroll_key = 0xFFU;
    app->progress_scroll_started_ms = 0U;
    app->progress_scroll_next_ms = 0U;
}

static uint16_t
    morse_flipper_progress_scroll_interval_ms(const MorseFlipperApp* app, uint32_t now_ms) {
    if(app != NULL && app->progress_scroll_started_ms != 0U &&
       now_ms - app->progress_scroll_started_ms >= MORSE_FLIPPER_PROGRESS_SCROLL_ACCEL_MS) {
        return MORSE_FLIPPER_PROGRESS_SCROLL_FAST_MS;
    }

    return MORSE_FLIPPER_PROGRESS_SCROLL_BASE_MS;
}

static bool morse_flipper_progress_scroll_history(MorseFlipperApp* app, int8_t dir) {
    if(app == NULL) return false;
    return morse_flipper_progress_history_view_scroll(&app->progress_history, dir) ==
           MorseFlipperProgressHistoryMoved;
}

static const MorseFlipperProgressHistoryRow*
    morse_flipper_progress_focused_history_row(const MorseFlipperApp* app) {
    if(app == NULL) return NULL;
    return morse_flipper_progress_history_view_focused(&app->progress_history);
}

static void morse_flipper_progress_open_history_result(MorseFlipperApp* app) {
    const MorseFlipperProgressHistoryRow* row = morse_flipper_progress_focused_history_row(app);
    uint8_t percent;

    if(app == NULL || row == NULL) return;

    percent = row->percent > 100U ? 100U : row->percent;
    app->progress_debug_result = true;
    app->progress_debug_returning = false;
    app->progress_debug_prev_lesson = morse_trainer_lesson(&app->trainer);
    app->progress_debug_prev_groups = morse_trainer_session_groups(&app->trainer);

    morse_trainer_set_lesson(&app->trainer, row->lesson_idx);
    morse_trainer_reset_session(&app->trainer);
    app->trainer.phase = MorseTrainerPhaseDone;
    app->trainer.session_active = false;
    app->trainer.session_aborted = false;
    app->trainer.session_groups = 1U;
    app->trainer.session_index = 1U;
    app->trainer.session_scored_groups = 1U;
    app->trainer.session_letter_hits = percent;
    app->trainer.session_letter_total = 100U;
    app->trainer.session_score_sum = percent;
    app->trainer.last_score = percent;
    app->trainer.last_failed = percent != 100U;
    app->trainer.last_missed = false;

    app->session_started = true;
    app->session_progress_recorded = true;
    app->session_complete_at = furi_get_tick();
    scene_manager_next_scene(app->scene_manager, MorseFlipperSceneSessionEnd);
}

static void
    morse_flipper_progress_enter_page(MorseFlipperApp* app, MorseFlipperProgressPage page) {
    if(app == NULL) return;

    app->progress_page = page;
    morse_flipper_progress_reset_scroll_repeat(app);
    if(page == MorseFlipperProgressPageHistory) morse_flipper_progress_load_recent(app);
    morse_flipper_view_dirty(app);
}

static bool morse_flipper_progress_history_input(MorseFlipperApp* app, const InputEvent* event) {
    int8_t dir;
    bool changed = false;

    if(event->key == InputKeyOk &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_progress_reset_scroll_repeat(app);
        morse_flipper_progress_open_history_result(app);
        return true;
    }

    if(event->key != InputKeyUp && event->key != InputKeyDown) return false;

    dir = event->key == InputKeyDown ? 1 : -1;
    if(event->type == InputTypePress) {
        app->progress_scroll_key = event->key;
        app->progress_scroll_started_ms = furi_get_tick();
        app->progress_scroll_next_ms = 0U;
        return true;
    } else if(event->type == InputTypeShort) {
        morse_flipper_progress_reset_scroll_repeat(app);
        changed = morse_flipper_progress_scroll_history(app, dir);
    } else if(event->type == InputTypeLong) {
        uint32_t now_ms = furi_get_tick();
        if(app->progress_scroll_key != event->key || app->progress_scroll_started_ms == 0U) {
            app->progress_scroll_started_ms = now_ms;
        }
        app->progress_scroll_key = event->key;
        app->progress_scroll_next_ms =
            now_ms + morse_flipper_progress_scroll_interval_ms(app, now_ms);
        changed = morse_flipper_progress_scroll_history(app, dir);
    } else if(event->type == InputTypeRepeat) {
        uint32_t now_ms = furi_get_tick();
        if(app->progress_scroll_key != event->key || app->progress_scroll_next_ms == 0U) {
            app->progress_scroll_key = event->key;
            if(app->progress_scroll_started_ms == 0U) app->progress_scroll_started_ms = now_ms;
            app->progress_scroll_next_ms =
                now_ms + morse_flipper_progress_scroll_interval_ms(app, now_ms);
        }
        return true;
    } else {
        morse_flipper_progress_reset_scroll_repeat(app);
        return true;
    }

    if(changed) {
        morse_flipper_view_dirty(app);
    }
    return true;
}

void morse_flipper_tick_progress_history_scroll(MorseFlipperApp* app, uint32_t now_ms) {
    MorseFlipperProgressHistoryMove pending;
    int8_t dir;

    if(app == NULL) return;
    if(app->screen != MorseFlipperScreenProgress ||
       app->progress_page != MorseFlipperProgressPageHistory) {
        morse_flipper_progress_reset_scroll_repeat(app);
        return;
    }
    if(app->progress_history.pending_dir != 0) {
        pending = morse_flipper_progress_history_view_continue(&app->progress_history);
        if(pending == MorseFlipperProgressHistoryMoved) morse_flipper_view_dirty(app);
        return;
    }
    if(app->progress_scroll_key != InputKeyUp && app->progress_scroll_key != InputKeyDown) return;
    if(app->progress_scroll_next_ms == 0U) return;
    if((int32_t)(now_ms - app->progress_scroll_next_ms) < 0) return;

    dir = app->progress_scroll_key == InputKeyDown ? 1 : -1;
    if(morse_flipper_progress_scroll_history(app, dir)) {
        app->progress_scroll_next_ms =
            now_ms + morse_flipper_progress_scroll_interval_ms(app, now_ms);
        morse_flipper_view_dirty(app);
    } else {
        app->progress_scroll_next_ms =
            now_ms + morse_flipper_progress_scroll_interval_ms(app, now_ms);
    }
}

static bool morse_flipper_progress_input(MorseFlipperApp* app, const InputEvent* event) {
    if(app->screen != MorseFlipperScreenProgress) return false;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuTraining);
        return true;
    }

    if(event->type == InputTypeRelease) {
        morse_flipper_progress_reset_scroll_repeat(app);
        return true;
    }

    if(event->type != InputTypePress && event->type != InputTypeShort &&
       event->type != InputTypeLong && event->type != InputTypeRepeat)
        return true;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            if(app->progress_page == MorseFlipperProgressPageStats) {
                morse_flipper_progress_enter_page(app, MorseFlipperProgressPageHistory);
            } else if(app->progress_page == MorseFlipperProgressPageTotals) {
                morse_flipper_progress_enter_page(app, MorseFlipperProgressPageStats);
            } else {
                morse_flipper_progress_enter_page(app, MorseFlipperProgressPageTotals);
            }
            return true;
        }
        if(event->key == InputKeyRight) {
            if(app->progress_page == MorseFlipperProgressPageStats) {
                morse_flipper_progress_enter_page(app, MorseFlipperProgressPageTotals);
            } else if(app->progress_page == MorseFlipperProgressPageTotals) {
                morse_flipper_progress_enter_page(app, MorseFlipperProgressPageHistory);
            } else {
                morse_flipper_progress_enter_page(app, MorseFlipperProgressPageStats);
            }
            return true;
        }
    }

    if(app->progress_page == MorseFlipperProgressPageHistory) {
        morse_flipper_progress_history_input(app, event);
        return true;
    }

    return true;
}

static bool morse_flipper_streak_intro_input(MorseFlipperApp* app, const InputEvent* event) {
    if(app->screen != MorseFlipperScreenStreakIntro) return false;

    if((event->key == InputKeyOk || event->key == InputKeyBack) &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MorseFlipperCustomStreakIntroStart);
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
    case MorseFlipperScreenProgress:
        return morse_flipper_progress_input(app, event);
    case MorseFlipperScreenStreakIntro:
        return morse_flipper_streak_intro_input(app, event);
    case MorseFlipperScreenIcr:
        return morse_flipper_icr_host_input(app, event, now_ms);
    case MorseFlipperScreenRxPractice:
        return morse_flipper_rx_practice_host_input(app, event, now_ms);
    case MorseFlipperScreenPassive: {
        MorseFlipperMappedFalResult result = {0};
        bool active = false;
        furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
        if(app->plugin_slot.owner == MorseFlipperPluginOwnerPassive &&
           app->plugin_slot.error == MorseFlipperPluginErrorNone && app->plugin_slot.api != NULL &&
           app->plugin_slot.state != NULL) {
            const MorseFlipperMappedFalApi* api = app->plugin_slot.api;
            active = true;
            result = api->input(app->plugin_slot.state, event, now_ms);
        }
        furi_mutex_release(app->plugin_slot.mutex);
        if(result.request_exit ||
           (!active && event->key == InputKeyBack &&
            (event->type == InputTypeShort || event->type == InputTypeLong)))
            morse_flipper_scene_return_to_training(app);
        return true;
    }
    case MorseFlipperScreenRfFreq:
    case MorseFlipperScreenRfRx:
    case MorseFlipperScreenRf:
        return morse_flipper_radio_host_input(app, event, now_ms);
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

    if(app->screen != MorseFlipperScreenSession && app->screen != MorseFlipperScreenTxGroups &&
       app->screen != MorseFlipperScreenRxPractice)
        return false;
    if(app->screen == MorseFlipperScreenSession && !morse_flipper_session_repeat_active(app))
        return false;
    if(app->screen == MorseFlipperScreenTxGroups && !app->txg_wait_answer) return false;
    if(event->type != InputTypePress && event->type != InputTypeRelease) return false;

    g = morse_flipper_input_gate(app);

    if(app->screen == MorseFlipperScreenRxPractice && !g.live) return false;

    if(event->key == InputKeyOk && g.btn) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(event->key == InputKeyBack && g.back_key) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(app->screen != MorseFlipperScreenRxPractice &&
       event->key == InputKeyLeft && morse_flipper_session_left_exit_active(app)) {
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

    if(morse_flipper_session_repeat_active(app)) morse_flipper_session_cancel_answer_flash(app);

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
