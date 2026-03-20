#include "holdem_app_internal.h"

static UiMode holdem_resolve_exit_cancel_mode(const HoldemApp* app) {
    UiMode resume_mode = app->exit_return_mode;

    if(resume_mode == UiModeTable && app->return_mode == UiModeActionPrompt &&
       app->game.hand_in_progress && !app->showdown_waiting && !app->bot_turn_active &&
       app->acting_idx >= 0 && app->acting_idx < (int)app->game.player_count &&
       !app->game.players[app->acting_idx].is_bot && app->game.players[app->acting_idx].in_hand) {
        resume_mode = UiModeActionPrompt;
    }

    return resume_mode;
}

static uint8_t holdem_clamp_progressive_period(uint8_t period_hands) {
    return (uint8_t)clamp_i32(
        period_hands, HOLDEM_PROGRESSIVE_MIN_PERIOD_HANDS, HOLDEM_PROGRESSIVE_MAX_PERIOD_HANDS);
}

static int32_t holdem_clamp_progressive_step(int32_t step_sb) {
    return clamp_i32(step_sb, HOLDEM_PROGRESSIVE_MIN_STEP_SB, HOLDEM_PROGRESSIVE_MAX_STEP_SB);
}

static bool can_apply_blinds_immediately(const HoldemGame* game) {
    if(!game->hand_in_progress) return true;
    if(game->stage != StagePreflop) return false;

    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        int32_t contributed = game->players[player_index].hand_contrib;

        if((int)player_index == game->sb_idx) {
            if(contributed > game->small_blind) return false;
            continue;
        }
        if((int)player_index == game->bb_idx) {
            if(contributed > game->big_blind) return false;
            continue;
        }
        if(contributed > 0) return false;
    }

    return true;
}

static void handle_back_short(HoldemApp* app) {
    if(app->mode == UiModeStartChoice) {
        app->mode = UiModeStartReady;
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeBlindEdit || app->mode == UiModeBotCountEdit) {
        app->mode = UiModeHelp;
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeNewGameConfirm) {
        app->mode = app->new_game_confirm_from_bot_edit ? UiModeBotCountEdit : UiModeHelp;
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeHelp) {
        app->mode = app->return_mode;
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeExitPrompt) {
        app->mode = holdem_resolve_exit_cancel_mode(app);
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeTable || app->mode == UiModeActionPrompt || app->mode == UiModePause) {
        show_help(app);
    }
}

static void handle_back_long(HoldemApp* app) {
    if(app->mode == UiModeExitPrompt) {
        app->save_on_exit = false;
        app->exit_requested = true;
        app->running = false;
    } else {
        show_exit_prompt(app);
    }
}

void holdem_input_callback(InputEvent* event, void* ctx) {
    HoldemApp* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

void show_help(HoldemApp* app) {
    app->return_mode = app->mode;
    app->help_page = 0;
    app->mode = UiModeHelp;
    view_port_update(app->view_port);
}

void show_exit_prompt(HoldemApp* app) {
    app->exit_return_mode = app->mode;
    app->mode = UiModeExitPrompt;
    view_port_update(app->view_port);
}

void reset_to_new_game(HoldemApp* app) {
    init_game_with_player_count(&app->game, app->configured_player_count);
    set_ai_level(app, app->configured_ai_level_pct);
    holdem_apply_blind_values_now(app, app->configured_small_blind);
    app->pending_blinds_dirty = false;
    if(app->progressive_blinds_enabled) {
        holdem_reset_progressive_schedule(app);
    } else {
        app->progressive_next_raise_hand_no = 0;
    }
    app->mode = UiModeStartReady;
    app->return_mode = UiModeTable;
    app->reveal_all = false;
    app->showdown_waiting = false;
    app->reset_requested = false;
    app->showdown_winner_mask = 0;
    app->bot_turn_active = false;
    app->bot_turn_idx = -1;
    app->bot_turn_text[0] = '\0';
    app->acting_idx = -1;
    app->action_ready = false;
    app->prompt_to_call = 0;
    app->prompt_min_raise = 0;
    app->prompt_raise_by = 0;
    app->prompt_bet_total = 0;
    app->chosen_raise_by = 0;
    app->skip_autoplay_requested = false;
    app->start_requested = false;
    app->pending_small_blind = app->configured_small_blind;
    app->blind_edit_sb = app->configured_small_blind;
    app->blind_edit_initial_sb = app->configured_small_blind;
    app->blind_edit_progressive_enabled = app->progressive_blinds_enabled;
    app->blind_edit_progressive_period_hands = app->progressive_period_hands;
    app->blind_edit_progressive_step_sb = app->progressive_step_sb;
    app->new_game_confirm_from_bot_edit = false;
}

bool prompt_showdown_inspect(HoldemApp* app) {
    holdem_set_foreground_mode(app, UiModeTable);
    app->reveal_all = true;
    app->showdown_waiting = true;
    app->skip_autoplay_requested = false;
    view_port_update(app->view_port);
    flush_input_queue(app);

    InputEvent ev;
    while(app->running) {
        if(furi_message_queue_get(app->input_queue, &ev, FuriWaitForever) != FuriStatusOk)
            continue;
        if(process_global_event(app, &ev)) {
            if(app->exit_requested) {
                app->showdown_waiting = false;
                app->showdown_winner_mask = 0;
                return false;
            }
            continue;
        }
        if(ev.type == InputTypeShort && ev.key == InputKeyOk) {
            app->showdown_waiting = false;
            app->showdown_winner_mask = 0;
            return true;
        }
    }

    app->showdown_waiting = false;
    app->showdown_winner_mask = 0;
    return false;
}

int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

void set_ai_level(HoldemApp* app, uint8_t level_pct) {
    app->ai_level_pct =
        (uint8_t)clamp_i32(level_pct, HOLDEM_AI_EASY_LEVEL, HOLDEM_AI_EXTREME_LEVEL);
}

void set_bot_action_delay(HoldemApp* app, bool enabled, uint16_t delay_ms) {
    app->bot_delay_enabled = enabled;
    app->bot_delay_ms = (uint16_t)clamp_i32((int32_t)delay_ms, 0, 5000);
}

int32_t holdem_default_small_blind(void) {
    return 10;
}

int32_t holdem_active_small_blind(const HoldemApp* app) {
    return app->pending_blinds_dirty ? app->pending_small_blind : app->game.small_blind;
}

bool holdem_blind_edit_is_dirty(const HoldemApp* app) {
    if(app->blind_edit_progressive_enabled != app->progressive_blinds_enabled) return true;

    if(app->blind_edit_progressive_enabled) {
        return app->blind_edit_progressive_period_hands != app->progressive_period_hands ||
               app->blind_edit_progressive_step_sb != app->progressive_step_sb;
    }

    return app->blind_edit_sb != holdem_active_small_blind(app);
}

bool holdem_bot_edit_is_dirty(const HoldemApp* app) {
    return app->bot_count_edit_value != app->configured_player_count ||
           app->bot_difficulty_edit_value != app->configured_ai_level_pct;
}

void holdem_reset_progressive_schedule(HoldemApp* app) {
    app->progressive_next_raise_hand_no = app->game.hand_no + app->progressive_period_hands;
}

void holdem_apply_blind_values_now(HoldemApp* app, int32_t new_small_blind) {
    app->game.small_blind = new_small_blind;
    app->game.big_blind = new_small_blind * 2;
}

bool holdem_apply_progressive_increase_if_due(HoldemApp* app) {
    bool increased = false;
    if(!app->progressive_blinds_enabled) return false;

    while(app->game.hand_no >= app->progressive_next_raise_hand_no) {
        int32_t next_small_blind =
            clamp_i32(app->game.small_blind + app->progressive_step_sb, HOLDEM_BLIND_STEP_SB, 500);
        holdem_apply_blind_values_now(app, next_small_blind);
        app->progressive_next_raise_hand_no += app->progressive_period_hands;
        increased = true;
    }

    return increased;
}

void apply_or_defer_blind_edit(HoldemApp* app, int32_t new_small_blind) {
    new_small_blind = clamp_i32(new_small_blind, HOLDEM_BLIND_STEP_SB, 500);

    if(can_apply_blinds_immediately(&app->game)) {
        holdem_apply_blind_values_now(app, new_small_blind);
        app->pending_blinds_dirty = false;
    } else {
        app->pending_small_blind = new_small_blind;
        app->pending_blinds_dirty = true;
    }
}

void holdem_commit_blind_edit(HoldemApp* app) {
    if(app->blind_edit_progressive_enabled) {
        bool was_disabled = !app->progressive_blinds_enabled;
        app->configured_small_blind = app->blind_edit_sb;
        app->progressive_blinds_enabled = true;
        app->progressive_period_hands =
            holdem_clamp_progressive_period(app->blind_edit_progressive_period_hands);
        app->progressive_step_sb =
            holdem_clamp_progressive_step(app->blind_edit_progressive_step_sb);
        if(was_disabled) {
            holdem_reset_progressive_schedule(app);
        }
    } else {
        bool was_enabled = app->progressive_blinds_enabled;
        app->configured_small_blind = app->blind_edit_sb;
        app->progressive_blinds_enabled = false;
        if(was_enabled) {
            app->progressive_next_raise_hand_no = 0;
        }
        apply_or_defer_blind_edit(app, app->blind_edit_sb);
    }
}

bool process_global_event(HoldemApp* app, const InputEvent* ev) {
    if(!app || !ev) return false;

    if(ev->key == InputKeyBack) {
        if(ev->type == InputTypePress) {
            app->back_down = true;
            app->back_long_handled = false;
            app->back_press_tick = furi_get_tick();
            return true;
        }
        if(ev->type == InputTypeRepeat || ev->type == InputTypeLong) {
            if(app->back_down && !app->back_long_handled) {
                uint32_t held_ms = furi_get_tick() - app->back_press_tick;
                if(held_ms >= HOLDEM_BACK_HOLD_MS) {
                    app->back_long_handled = true;
                    app->back_down = false;
                    handle_back_long(app);
                }
            }
            return true;
        }
        if(ev->type == InputTypeRelease) {
            uint32_t held_ms = app->back_down ? (furi_get_tick() - app->back_press_tick) : 0;
            bool long_handled = app->back_long_handled;
            app->back_down = false;
            app->back_long_handled = false;
            if(!long_handled) {
                if(held_ms >= HOLDEM_BACK_HOLD_MS) {
                    handle_back_long(app);
                } else {
                    handle_back_short(app);
                }
            }
            return true;
        }
        if(ev->type == InputTypeShort) {
            if(!app->back_down && !app->back_long_handled) {
                handle_back_short(app);
            }
            return true;
        }
    }

    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;

    if(app->mode == UiModeStartChoice) {
        if(ev->key == InputKeyOk) {
            uint8_t loaded_ai_level = HOLDEM_AI_DEFAULT_LEVEL;
            int32_t loaded_configured_small_blind = holdem_default_small_blind();
            app->progressive_blinds_enabled = false;
            app->progressive_period_hands = HOLDEM_PROGRESSIVE_DEFAULT_PERIOD_HANDS;
            app->progressive_step_sb = HOLDEM_PROGRESSIVE_DEFAULT_STEP_SB;
            app->progressive_next_raise_hand_no = 0;
            (void)load_progress(
                &app->game,
                &loaded_ai_level,
                &loaded_configured_small_blind,
                &app->progressive_blinds_enabled,
                &app->progressive_period_hands,
                &app->progressive_step_sb,
                &app->progressive_next_raise_hand_no);
            set_ai_level(app, loaded_ai_level);
            app->configured_small_blind = loaded_configured_small_blind;
            app->configured_player_count = app->game.player_count;
            app->bot_count_edit_value = app->configured_player_count;
            app->configured_ai_level_pct = app->ai_level_pct;
            app->bot_difficulty_edit_value = app->configured_ai_level_pct;
            app->pending_blinds_dirty = !app->progressive_blinds_enabled &&
                                        (app->configured_small_blind != app->game.small_blind);
            app->pending_small_blind = app->pending_blinds_dirty ? app->configured_small_blind :
                                                                   app->game.small_blind;
            app->blind_edit_sb = app->configured_small_blind;
            app->blind_edit_initial_sb = app->configured_small_blind;
            app->blind_edit_progressive_enabled = app->progressive_blinds_enabled;
            app->blind_edit_progressive_period_hands = app->progressive_period_hands;
            app->blind_edit_progressive_step_sb = app->progressive_step_sb;
            app->mode = UiModeTable;
            app->new_game_confirm_from_bot_edit = false;
            view_port_update(app->view_port);
            return true;
        }
        return false;
    }

    if(app->mode == UiModeStartReady) {
        if(ev->key == InputKeyOk) {
            app->start_requested = true;
            return true;
        }
        return true;
    }

    if(app->mode == UiModeExitPrompt) {
        if(ev->key == InputKeyOk) {
            app->save_on_exit = true;
            app->exit_requested = true;
            app->running = false;
            return true;
        }
        return false;
    }

    if(app->mode == UiModeHelp) {
        if(ev->key == InputKeyUp) {
            app->help_page = (uint8_t)((app->help_page + 2) % 3);
            view_port_update(app->view_port);
            return true;
        }
        if(ev->key == InputKeyDown) {
            app->help_page = (uint8_t)((app->help_page + 1) % 3);
            view_port_update(app->view_port);
            return true;
        }
        if(app->help_page == 1 && ev->key == InputKeyOk) {
            app->blind_edit_sb = app->configured_small_blind;
            app->blind_edit_initial_sb = app->blind_edit_sb;
            app->blind_edit_progressive_enabled = app->progressive_blinds_enabled;
            app->blind_edit_progressive_period_hands = app->progressive_period_hands;
            app->blind_edit_progressive_step_sb = app->progressive_step_sb;
            app->mode = UiModeBlindEdit;
            view_port_update(app->view_port);
            return true;
        }
        if(ev->key == InputKeyOk) {
            app->mode = app->return_mode;
            view_port_update(app->view_port);
            return true;
        }
        if(app->help_page == 1 && ev->key == InputKeyRight) {
            app->bot_count_edit_value = app->configured_player_count;
            app->bot_difficulty_edit_value = app->configured_ai_level_pct;
            app->mode = UiModeBotCountEdit;
            view_port_update(app->view_port);
            return true;
        }
        if(app->help_page == 1 && ev->key == InputKeyLeft) {
            app->new_game_confirm_from_bot_edit = false;
            app->mode = UiModeNewGameConfirm;
            view_port_update(app->view_port);
            return true;
        }
        return false;
    }

    if(app->mode == UiModeBlindEdit) {
        if(ev->key == InputKeyUp) {
            if(app->blind_edit_progressive_enabled) {
                app->blind_edit_progressive_step_sb = holdem_clamp_progressive_step(
                    app->blind_edit_progressive_step_sb + HOLDEM_BLIND_STEP_SB);
            } else {
                app->blind_edit_sb = clamp_i32(
                    app->blind_edit_sb + HOLDEM_BLIND_STEP_SB, HOLDEM_BLIND_STEP_SB, 500);
            }
            view_port_update(app->view_port);
            return true;
        }
        if(ev->key == InputKeyDown) {
            if(app->blind_edit_progressive_enabled) {
                app->blind_edit_progressive_step_sb = holdem_clamp_progressive_step(
                    app->blind_edit_progressive_step_sb - HOLDEM_BLIND_STEP_SB);
            } else {
                app->blind_edit_sb = clamp_i32(
                    app->blind_edit_sb - HOLDEM_BLIND_STEP_SB, HOLDEM_BLIND_STEP_SB, 500);
            }
            view_port_update(app->view_port);
            return true;
        }
        if(ev->key == InputKeyRight) {
            if(app->blind_edit_progressive_enabled) {
                app->blind_edit_progressive_period_hands = holdem_clamp_progressive_period(
                    app->blind_edit_progressive_period_hands +
                    HOLDEM_PROGRESSIVE_PERIOD_STEP_HANDS);
            } else {
                app->blind_edit_progressive_enabled = true;
                app->blind_edit_progressive_period_hands = HOLDEM_PROGRESSIVE_DEFAULT_PERIOD_HANDS;
                app->blind_edit_progressive_step_sb = HOLDEM_PROGRESSIVE_DEFAULT_STEP_SB;
            }
            view_port_update(app->view_port);
            return true;
        }
        if(ev->key == InputKeyLeft) {
            if(app->blind_edit_progressive_enabled) {
                if(app->blind_edit_progressive_period_hands >
                   HOLDEM_PROGRESSIVE_MIN_PERIOD_HANDS) {
                    app->blind_edit_progressive_period_hands = holdem_clamp_progressive_period(
                        app->blind_edit_progressive_period_hands -
                        HOLDEM_PROGRESSIVE_PERIOD_STEP_HANDS);
                } else {
                    app->blind_edit_progressive_enabled = false;
                }
            } else {
                app->blind_edit_sb = app->blind_edit_initial_sb;
            }
            view_port_update(app->view_port);
            return true;
        }
        if(ev->key == InputKeyOk) {
            if(holdem_blind_edit_is_dirty(app)) {
                holdem_commit_blind_edit(app);
                app->mode = UiModeHelp;
                view_port_update(app->view_port);
            }
            return true;
        }
        return false;
    }

    if(app->mode == UiModeBotCountEdit) {
        if(ev->key == InputKeyUp) {
            if(app->bot_count_edit_value < HOLDEM_MAX_PLAYERS) app->bot_count_edit_value++;
            view_port_update(app->view_port);
            return true;
        }
        if(ev->key == InputKeyDown) {
            if(app->bot_count_edit_value > HOLDEM_MIN_PLAYERS) app->bot_count_edit_value--;
            view_port_update(app->view_port);
            return true;
        }
        if(ev->key == InputKeyLeft) {
            app->bot_difficulty_edit_value = holdem_prev_ai_level(app->bot_difficulty_edit_value);
            view_port_update(app->view_port);
            return true;
        }
        if(ev->key == InputKeyRight) {
            app->bot_difficulty_edit_value = holdem_next_ai_level(app->bot_difficulty_edit_value);
            view_port_update(app->view_port);
            return true;
        }
        if(ev->key == InputKeyOk) {
            if(!holdem_bot_edit_is_dirty(app)) {
                view_port_update(app->view_port);
                return true;
            }
            bool bot_count_changed = (app->bot_count_edit_value != app->configured_player_count);
            if(!bot_count_changed) {
                app->configured_ai_level_pct = app->bot_difficulty_edit_value;
                set_ai_level(app, app->configured_ai_level_pct);
                app->new_game_confirm_from_bot_edit = false;
                app->mode = UiModeHelp;
            } else {
                app->new_game_confirm_from_bot_edit = true;
                app->mode = UiModeNewGameConfirm;
            }
            view_port_update(app->view_port);
            return true;
        }
        return false;
    }

    if(app->mode == UiModeNewGameConfirm) {
        if(ev->key == InputKeyOk) {
            if(app->new_game_confirm_from_bot_edit) {
                app->configured_player_count = app->bot_count_edit_value;
                app->configured_ai_level_pct = app->bot_difficulty_edit_value;
                set_ai_level(app, app->configured_ai_level_pct);
                app->new_game_confirm_from_bot_edit = false;
            }
            reset_to_new_game(app);
            app->reset_requested = true;
            view_port_update(app->view_port);
            return true;
        }
        return false;
    }

    return false;
}

void flush_input_queue(HoldemApp* app) {
    InputEvent queued_event;
    while(furi_message_queue_get(app->input_queue, &queued_event, 0) == FuriStatusOk) {
    }
}

bool holdem_can_skip_folded_autoplay(const HoldemApp* app) {
    const Player* you = &app->game.players[0];
    return app->game.hand_in_progress && !app->showdown_waiting && (!you->in_hand || you->all_in);
}

void startup_splash_and_jingle(HoldemApp* app) {
    static const float notes[] = {
        523.25f, 659.25f, 783.99f, 659.25f, 523.25f, 659.25f, 880.00f, 783.99f, 659.25f, 587.33f};
    static const uint16_t duration_ms[] = {220, 220, 280, 180, 220, 220, 320, 220, 220, 320};
    static const uint16_t gap_ms[] = {40, 40, 60, 40, 40, 40, 60, 40, 40, 120};

    app->mode = UiModeSplash;
    view_port_update(app->view_port);

    if(furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_set_volume(0.8f);
        for(size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); i++) {
            furi_hal_speaker_start(notes[i], 0.8f);
            furi_delay_ms(duration_ms[i]);
            furi_hal_speaker_stop();
            furi_delay_ms(gap_ms[i]);
        }
        furi_hal_speaker_release();
    } else {
        furi_delay_ms(1200);
    }

    app->mode = UiModeTable;
    view_port_update(app->view_port);
    flush_input_queue(app);
}

void startup_choose_load_or_new(HoldemApp* app) {
    if(!save_exists_on_storage()) {
        app->mode = UiModeStartReady;
        view_port_update(app->view_port);
        flush_input_queue(app);
        return;
    }

    app->mode = UiModeStartChoice;
    view_port_update(app->view_port);
    flush_input_queue(app);

    InputEvent ev;
    while(app->running && app->mode == UiModeStartChoice) {
        if(furi_message_queue_get(app->input_queue, &ev, FuriWaitForever) != FuriStatusOk)
            continue;
        (void)process_global_event(app, &ev);
    }
}

bool wait_for_start_confirmation(HoldemApp* app) {
    if(app->mode != UiModeStartReady) return true;

    view_port_update(app->view_port);
    flush_input_queue(app);

    InputEvent ev;
    while(app->running && app->mode == UiModeStartReady && !app->start_requested) {
        if(furi_message_queue_get(app->input_queue, &ev, FuriWaitForever) != FuriStatusOk)
            continue;
        (void)process_global_event(app, &ev);
        if(app->exit_requested) return false;
    }

    if(app->start_requested) {
        app->start_requested = false;
        return true;
    }

    return app->running && !app->exit_requested;
}
