#include "holdem_app_internal.h"

static void wait_human_action(
    HoldemApp* app,
    int acting_idx,
    int32_t to_call,
    int32_t min_raise,
    bool can_raise);
static void wait_for_bot_message(HoldemApp* app);
static void show_timed_table_message(HoldemApp* app, const char* message);
static void run_betting_round(HoldemApp* app, int start_player_index, int32_t min_raise);
static void run_betting_round_or_auto(HoldemApp* app, int start_idx, int32_t min_raise);

int32_t holdem_total_bankroll(const HoldemGame* game) {
    int32_t total = game->pot;
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        total += game->players[player_index].stack;
    }
    return total;
}

bool qualifies_for_big_win(const HoldemApp* app, const PayoutResult* payout) {
    int32_t total_bankroll = holdem_total_bankroll(&app->game);
    if(total_bankroll <= 0 || payout->count == 0) return false;

    int you_index = -1;
    for(size_t player_index = 0; player_index < app->game.player_count; player_index++) {
        if(!app->game.players[player_index].is_bot) {
            you_index = (int)player_index;
            break;
        }
    }
    if(you_index < 0) return false;

    return payout->payout[you_index] * 5 >= total_bankroll;
}

bool show_interstitial_screen(
    HoldemApp* app,
    const char* text,
    bool fireworks,
    uint32_t duration_ms,
    bool allow_back_skip,
    bool show_continue_hint) {
    holdem_set_foreground_mode(app, UiModeBigWin);
    snprintf(app->interstitial_text, sizeof(app->interstitial_text), "%s", text);
    app->interstitial_fireworks = fireworks;
    app->interstitial_show_continue_hint = show_continue_hint;
    view_port_update(app->view_port);
    flush_input_queue(app);

    uint32_t start_tick = furi_get_tick();
    InputEvent ev;
    while(app->running) {
        if(duration_ms > 0 && (furi_get_tick() - start_tick) >= duration_ms) break;
        view_port_update(app->view_port);
        if(furi_message_queue_get(app->input_queue, &ev, 50) != FuriStatusOk) continue;
        if(app->mode == UiModeBigWin) {
            if(ev.key == InputKeyBack && duration_ms > 0 && !allow_back_skip) continue;
            if(ev.type == InputTypeShort && ev.key == InputKeyOk) break;
            if(allow_back_skip && ev.type == InputTypeShort && ev.key == InputKeyBack) break;
        }
        if(process_global_event(app, &ev)) {
            if(app->exit_requested) return false;
        }
    }

    holdem_set_foreground_mode(app, UiModeTable);
    return !app->exit_requested;
}

bool show_big_win_screen(HoldemApp* app) {
    return show_interstitial_screen(app, "Big Win!", true, 2000u, true, false);
}

bool show_progressive_blinds_screen(HoldemApp* app) {
    char message[32];
    snprintf(
        message,
        sizeof(message),
        "SB/BB increased to\n$%d/$%d",
        (int)app->game.small_blind,
        (int)app->game.big_blind);
    return show_interstitial_screen(app, message, false, 2000u, false, false);
}

void show_hand_result_screen(HoldemApp* app, const PayoutResult* payout) {
    holdem_set_foreground_mode(app, UiModePause);
    app->reveal_all = true;
    app->skip_autoplay_requested = false;

    int wi = -1;
    int32_t amt = 0;
    bool split_pot = (payout->count > 1);

    if(payout->payout[0] > 0) {
        wi = 0;
    } else if(payout->count > 0) {
        wi = payout->idx[0];
        for(size_t payout_index = 1; payout_index < payout->count; payout_index++) {
            int candidate_index = payout->idx[payout_index];
            if(payout->payout[candidate_index] > payout->payout[wi]) {
                wi = candidate_index;
            }
        }
    }
    amt = (wi >= 0) ? payout->payout[wi] : 0;

    snprintf(app->pause_title, sizeof(app->pause_title), "Hand %d Results", app->game.hand_no);
    snprintf(app->pause_footer, sizeof(app->pause_footer), "Next ");
    app->pause_cards_label[0] = '\0';
    app->pause_card_count = 0;
    app->pause_body3[0] = '\0';
    app->pause_body4[0] = '\0';

    if(wi >= 0) {
        snprintf(
            app->pause_body,
            sizeof(app->pause_body),
            "%s +$%d",
            app->game.players[wi].name,
            (int)amt);
        if(split_pot) {
            snprintf(app->pause_body4, sizeof(app->pause_body4), "[Split Pot]");
        }

        if(app->game.stage == StageShowdown) {
            Card seven[7];
            Card best_five[5];
            seven[0] = app->game.players[wi].hole[0];
            seven[1] = app->game.players[wi].hole[1];
            for(size_t i = 0; i < 5; i++)
                seven[2 + i] = app->game.board[i];
            Score s = best_five_from_seven_with_cards(seven, best_five);
            sort_five_for_display(best_five);
            snprintf(app->pause_body2, sizeof(app->pause_body2), "%s", hand_rank_label(s.v[0]));
            for(size_t card_index = 0; card_index < 5; card_index++) {
                app->pause_cards[card_index] = best_five[card_index];
            }
            app->pause_card_count = 5;
        } else if(!app->game.players[wi].is_bot) {
            snprintf(app->pause_body2, sizeof(app->pause_body2), "Fold Win");
            app->pause_cards[0] = app->game.players[wi].hole[0];
            app->pause_cards[1] = app->game.players[wi].hole[1];
            app->pause_card_count = 2;
        } else {
            snprintf(app->pause_body2, sizeof(app->pause_body2), "Fold Win");
            snprintf(app->pause_body3, sizeof(app->pause_body3), "?? ??");
        }
    } else {
        snprintf(app->pause_body, sizeof(app->pause_body), "No payout");
        snprintf(app->pause_body2, sizeof(app->pause_body2), "XX");
        snprintf(app->pause_body3, sizeof(app->pause_body3), "XX");
        app->pause_body4[0] = '\0';
    }

    view_port_update(app->view_port);
    flush_input_queue(app);

    InputEvent ev;
    while(app->running) {
        if(furi_message_queue_get(app->input_queue, &ev, FuriWaitForever) != FuriStatusOk)
            continue;
        if(process_global_event(app, &ev)) {
            if(app->exit_requested) break;
            continue;
        }
        if(app->mode == UiModePause && ev.type == InputTypeShort && ev.key == InputKeyOk) break;
    }

    holdem_set_foreground_mode(app, UiModeTable);
    app->reveal_all = false;
}

static void wait_human_action(
    HoldemApp* app,
    int acting_idx,
    int32_t to_call,
    int32_t min_raise,
    bool can_raise) {
    holdem_set_foreground_mode(app, UiModeActionPrompt);
    app->acting_idx = acting_idx;
    app->prompt_to_call = to_call;
    app->prompt_min_raise = min_raise;
    app->prompt_raise_by = 0;
    app->prompt_bet_total = to_call;
    app->action_ready = false;
    app->bot_turn_active = false;
    app->bot_turn_idx = -1;
    app->bot_turn_text[0] = '\0';

    int32_t stack = app->game.players[acting_idx].stack;
    int32_t max_total = stack;
    bool right_down = false;
    bool right_long_handled = false;
    uint32_t right_press_tick = 0u;
    if(max_total < to_call) max_total = to_call;

    view_port_update(app->view_port);
    flush_input_queue(app);

    InputEvent ev;
    while(app->running && !app->action_ready) {
        if(furi_message_queue_get(app->input_queue, &ev, FuriWaitForever) != FuriStatusOk)
            continue;
        if(process_global_event(app, &ev)) {
            if(app->exit_requested || app->reset_requested) {
                app->chosen_action = ActFold;
                app->chosen_raise_by = 0;
                app->action_ready = true;
            }
            continue;
        }
        if(app->mode != UiModeActionPrompt) continue;
        if(ev.type != InputTypeShort && ev.type != InputTypeRepeat && ev.type != InputTypeLong &&
           ev.type != InputTypePress && ev.type != InputTypeRelease)
            continue;

        if(ev.key == InputKeyLeft) {
            if(ev.type != InputTypeShort && ev.type != InputTypeRepeat) continue;
            app->chosen_action = ActFold;
            app->chosen_raise_by = 0;
            app->action_ready = true;
        } else if(ev.key == InputKeyOk) {
            if(ev.type != InputTypeShort && ev.type != InputTypeRepeat) continue;
            if(app->prompt_bet_total <= to_call) {
                app->chosen_action = (to_call == 0) ? ActCheck : ActCall;
                app->chosen_raise_by = 0;
            } else if(can_raise) {
                int32_t raise_by = app->prompt_bet_total - to_call;
                if(raise_by < min_raise) raise_by = min_raise;
                if(raise_by > (stack - to_call)) raise_by = stack - to_call;
                if(raise_by > 0) {
                    app->chosen_action = ActRaise;
                    app->chosen_raise_by = raise_by;
                } else {
                    app->chosen_action = (to_call == 0) ? ActCheck : ActCall;
                    app->chosen_raise_by = 0;
                }
            } else {
                app->chosen_action = (to_call == 0) ? ActCheck : ActCall;
                app->chosen_raise_by = 0;
            }
            app->action_ready = true;
        } else if(ev.key == InputKeyUp && can_raise) {
            if(ev.type != InputTypeShort && ev.type != InputTypeRepeat) continue;
            if(app->prompt_bet_total < max_total) {
                int32_t next = app->prompt_bet_total + app->game.big_blind;
                if(next > max_total) next = max_total;
                if(next > to_call && next < (to_call + min_raise)) next = to_call + min_raise;
                if(next > max_total) next = max_total;
                app->prompt_bet_total = next;
                app->prompt_raise_by =
                    (app->prompt_bet_total > to_call) ? (app->prompt_bet_total - to_call) : 0;
            }
        } else if(ev.key == InputKeyDown && can_raise) {
            if(ev.type != InputTypeShort && ev.type != InputTypeRepeat) continue;
            int32_t next = app->prompt_bet_total - app->game.big_blind;
            if(next < to_call) next = to_call;
            app->prompt_bet_total = next;
            app->prompt_raise_by =
                (app->prompt_bet_total > to_call) ? (app->prompt_bet_total - to_call) : 0;
        } else if(ev.key == InputKeyRight && can_raise) {
            if(ev.type == InputTypePress) {
                right_down = true;
                right_long_handled = false;
                right_press_tick = furi_get_tick();
                continue;
            }

            if(ev.type == InputTypeRepeat || ev.type == InputTypeLong) {
                if(right_down && !right_long_handled) {
                    uint32_t held_ms = furi_get_tick() - right_press_tick;
                    if(held_ms >= HOLDEM_RIGHT_HOLD_MS) {
                        app->prompt_bet_total = max_total;
                        app->prompt_raise_by = (app->prompt_bet_total > to_call) ?
                                                   (app->prompt_bet_total - to_call) :
                                                   0;
                        right_long_handled = true;
                        view_port_update(app->view_port);
                    }
                }
                continue;
            }

            if(ev.type == InputTypeRelease) {
                uint32_t held_ms = right_down ? (furi_get_tick() - right_press_tick) : 0u;
                bool long_handled = right_long_handled;
                right_down = false;
                right_long_handled = false;

                if(!long_handled) {
                    if(held_ms >= HOLDEM_RIGHT_HOLD_MS) {
                        app->prompt_bet_total = max_total;
                        app->prompt_raise_by = (app->prompt_bet_total > to_call) ?
                                                   (app->prompt_bet_total - to_call) :
                                                   0;
                    } else {
                        app->prompt_bet_total = to_call;
                        app->prompt_raise_by = 0;
                    }
                }
                view_port_update(app->view_port);
                continue;
            }

            if(ev.type == InputTypeShort && !right_down && !right_long_handled) {
                app->prompt_bet_total = to_call;
                app->prompt_raise_by = 0;
            }
        }

        view_port_update(app->view_port);
    }

    holdem_set_foreground_mode(app, UiModeTable);
}

static void wait_for_bot_message(HoldemApp* app) {
    if(!app->bot_delay_enabled || app->bot_delay_ms == 0) return;

    uint32_t wait_start_tick = furi_get_tick();
    while((furi_get_tick() - wait_start_tick) < app->bot_delay_ms) {
        view_port_update(app->view_port);
        InputEvent ev;
        if(furi_message_queue_get(app->input_queue, &ev, 50) == FuriStatusOk) {
            if(process_global_event(app, &ev)) {
                if(app->exit_requested) return;
                continue;
            }

            if(holdem_can_skip_folded_autoplay(app) && ev.type == InputTypeShort &&
               ev.key == InputKeyOk) {
                app->skip_autoplay_requested = true;
                return;
            }
        }

        if(app->skip_autoplay_requested) return;
    }
}

static void show_timed_table_message(HoldemApp* app, const char* message) {
    app->bot_turn_active = true;
    app->bot_turn_idx = -1;
    snprintf(app->bot_turn_text, sizeof(app->bot_turn_text), "%s", message);
    view_port_update(app->view_port);
    wait_for_bot_message(app);
    app->bot_turn_active = false;
    app->bot_turn_idx = -1;
    app->bot_turn_text[0] = '\0';
}

static void run_betting_round(HoldemApp* app, int start_player_index, int32_t min_raise) {
    HoldemGame* game = &app->game;
    int32_t current_bet = 0;
    int32_t current_min_raise = min_raise;

    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        if(game->players[player_index].in_hand &&
           game->players[player_index].street_bet > current_bet) {
            current_bet = game->players[player_index].street_bet;
        }
    }

    bool needs_action[HOLDEM_MAX_PLAYERS] = {0};
    bool acted_since_full_raise[HOLDEM_MAX_PLAYERS] = {0};
    size_t needs_action_count = 0;
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        Player* player = &game->players[player_index];
        if(player->in_hand && !player->all_in) {
            needs_action[player_index] = true;
            needs_action_count++;
        }
    }

    app->bot_turn_active = false;
    app->bot_turn_idx = -1;
    app->bot_turn_text[0] = '\0';

    if(needs_action_count == 0) return;

    int active_player_index = start_player_index;
    while(needs_action_count > 0 && active_in_hand_count(game) > 1 && app->running &&
          !app->reset_requested) {
        active_player_index =
            ((active_player_index % (int)game->player_count) + (int)game->player_count) %
            (int)game->player_count;
        Player* active_player = &game->players[active_player_index];

        if(!needs_action[active_player_index] || !active_player->in_hand ||
           active_player->all_in) {
            if(needs_action[active_player_index]) {
                needs_action[active_player_index] = false;
                if(needs_action_count > 0) needs_action_count--;
            }
            active_player_index++;
            continue;
        }

        int32_t to_call = current_bet - active_player->street_bet;
        HoldemAction selected_action = ActCall;
        int32_t raise_amount = 0;
        bool can_raise = !acted_since_full_raise[active_player_index];

        if(active_player->is_bot) {
            bot_action(
                app,
                game,
                active_player_index,
                to_call,
                current_min_raise,
                current_bet,
                can_raise,
                &selected_action,
                &raise_amount);
        } else {
            app->bot_turn_active = false;
            app->bot_turn_idx = -1;
            app->bot_turn_text[0] = '\0';
            wait_human_action(app, active_player_index, to_call, current_min_raise, can_raise);
            selected_action = app->chosen_action;
            raise_amount = app->chosen_raise_by;
        }

        if(app->exit_requested) return;

        if(selected_action == ActFold) {
            active_player->in_hand = false;
            needs_action[active_player_index] = false;
            needs_action_count--;
            acted_since_full_raise[active_player_index] = true;
        } else if(selected_action == ActCheck || selected_action == ActCall) {
            put_chips(game, active_player_index, to_call);
            needs_action[active_player_index] = false;
            needs_action_count--;
            acted_since_full_raise[active_player_index] = true;
        } else if(selected_action == ActRaise) {
            int32_t normalized_raise =
                can_raise && (raise_amount < current_min_raise) ? current_min_raise : raise_amount;
            int32_t total_required = to_call + normalized_raise;
            if(total_required < 0) total_required = 0;
            int32_t prior_current_bet = current_bet;
            put_chips(game, active_player_index, total_required);
            int32_t actual_raise_size = active_player->street_bet - prior_current_bet;
            bool full_raise = actual_raise_size >= current_min_raise;

            if(active_player->street_bet > current_bet) {
                current_bet = active_player->street_bet;
            }

            if(active_player->street_bet > prior_current_bet && full_raise) {
                current_bet = active_player->street_bet;
                current_min_raise = actual_raise_size;
                memset(needs_action, 0, sizeof(needs_action));
                memset(acted_since_full_raise, 0, sizeof(acted_since_full_raise));
                needs_action_count = 0;
                for(size_t player_index = 0; player_index < game->player_count; player_index++) {
                    if((int)player_index == active_player_index) continue;
                    Player* player = &game->players[player_index];
                    if(player->in_hand && !player->all_in) {
                        needs_action[player_index] = true;
                        needs_action_count++;
                    }
                }
                acted_since_full_raise[active_player_index] = true;
            } else if(active_player->street_bet > prior_current_bet) {
                needs_action[active_player_index] = false;
                if(needs_action_count > 0) needs_action_count--;
                acted_since_full_raise[active_player_index] = true;
                for(size_t player_index = 0; player_index < game->player_count; player_index++) {
                    if((int)player_index == active_player_index) continue;
                    Player* player = &game->players[player_index];
                    if(player->in_hand && !player->all_in && player->street_bet < current_bet &&
                       !needs_action[player_index]) {
                        needs_action[player_index] = true;
                        needs_action_count++;
                    }
                }
            } else {
                needs_action[active_player_index] = false;
                needs_action_count--;
                acted_since_full_raise[active_player_index] = true;
            }
        }

        if(active_player->is_bot) {
            bool short_all_in_call = (selected_action == ActCall) && active_player->all_in &&
                                     (active_player->street_bet < current_bet);
            app->bot_turn_active = true;
            app->bot_turn_idx = active_player_index;
            if(selected_action == ActFold) {
                snprintf(
                    app->bot_turn_text,
                    sizeof(app->bot_turn_text),
                    "%s Folded",
                    active_player->name);
            } else if(selected_action == ActCheck) {
                snprintf(
                    app->bot_turn_text,
                    sizeof(app->bot_turn_text),
                    "%s Checked",
                    active_player->name);
            } else if(short_all_in_call) {
                snprintf(
                    app->bot_turn_text,
                    sizeof(app->bot_turn_text),
                    "%s is All In",
                    active_player->name);
            } else if(selected_action == ActCall) {
                if(to_call > 0) {
                    snprintf(
                        app->bot_turn_text,
                        sizeof(app->bot_turn_text),
                        "%s Called $%d",
                        active_player->name,
                        (int)to_call);
                } else {
                    snprintf(
                        app->bot_turn_text,
                        sizeof(app->bot_turn_text),
                        "%s Called",
                        active_player->name);
                }
            } else if(selected_action == ActRaise) {
                snprintf(
                    app->bot_turn_text,
                    sizeof(app->bot_turn_text),
                    "%s Raised $%d",
                    active_player->name,
                    (int)active_player->street_bet);
            }
            view_port_update(app->view_port);
            wait_for_bot_message(app);
            app->bot_turn_active = false;
            app->bot_turn_idx = -1;
            app->bot_turn_text[0] = '\0';
        } else {
            view_port_update(app->view_port);
        }

        if(needs_action_count == 0) break;
        active_player_index++;
    }
}

static void run_betting_round_or_auto(HoldemApp* app, int start_idx, int32_t min_raise) {
    HoldemGame* game = &app->game;
    if(should_auto_runout(game)) {
        if(app->skip_autoplay_requested) return;
        delay_for_visual_step(app);
        return;
    }
    run_betting_round(app, start_idx, min_raise);
}

bool play_one_hand(HoldemApp* app, PayoutResult* payout) {
    HoldemGame* g = &app->game;
    bool blinds_increased = false;
    if(alive_count(g) < 2) return false;

    if(app->pending_blinds_dirty) {
        holdem_apply_blind_values_now(app, app->pending_small_blind);
        app->pending_blinds_dirty = false;
    }
    blinds_increased = holdem_apply_progressive_increase_if_due(app);
    if(blinds_increased) {
        if(!show_progressive_blinds_screen(app)) return false;
        if(app->exit_requested || app->reset_requested) return !app->exit_requested;
    }

    reset_hand(g);
    if(!post_blinds(g)) return false;
    deal_hole(g);
    g->hand_in_progress = true;
    holdem_set_foreground_mode(app, UiModeTable);
    show_timed_table_message(app, "Hand Start");
    if(app->exit_requested || app->reset_requested) return !app->exit_requested;

    app->reveal_all = false;

    g->stage = StagePreflop;
    view_port_update(app->view_port);
    run_betting_round_or_auto(app, next_alive(g, g->bb_idx + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
    if(resolve_fold_win(g, payout)) {
        g->hand_in_progress = false;
        g->hand_no++;
        furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
        return true;
    }
    normalize_contested_pot(g);

    reset_street_bets(g);
    g->stage = StageFlop;
    deal_community(g, 3);
    view_port_update(app->view_port);
    run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
    if(resolve_fold_win(g, payout)) {
        g->hand_in_progress = false;
        g->hand_no++;
        furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
        return true;
    }
    normalize_contested_pot(g);

    reset_street_bets(g);
    g->stage = StageTurn;
    deal_community(g, 1);
    view_port_update(app->view_port);
    run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
    if(resolve_fold_win(g, payout)) {
        g->hand_in_progress = false;
        g->hand_no++;
        furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
        return true;
    }
    normalize_contested_pot(g);

    reset_street_bets(g);
    g->stage = StageRiver;
    deal_community(g, 1);
    view_port_update(app->view_port);
    run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
    if(resolve_fold_win(g, payout)) {
        g->hand_in_progress = false;
        g->hand_no++;
        furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
        return true;
    }
    normalize_contested_pot(g);

    g->stage = StageShowdown;
    app->reveal_all = true;
    resolve_showdown(g, payout);
    set_showdown_winner_mask(app, payout);
    if(!prompt_showdown_inspect(app)) return false;
    g->hand_in_progress = false;
    g->hand_no++;
    view_port_update(app->view_port);
    return true;
}

bool resume_loaded_hand(HoldemApp* app, PayoutResult* payout) {
    HoldemGame* g = &app->game;
    if(!g->hand_in_progress) return play_one_hand(app, payout);

    app->reveal_all = false;

    if(g->stage == StagePreflop) {
        view_port_update(app->view_port);
        run_betting_round_or_auto(app, next_alive(g, g->bb_idx + 1), g->big_blind);
        if(app->exit_requested) return false;
        if(app->reset_requested) return true;
        if(resolve_fold_win(g, payout)) {
            g->hand_in_progress = false;
            g->hand_no++;
            furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
            return true;
        }
        normalize_contested_pot(g);
        reset_street_bets(g);
        g->stage = StageFlop;
        deal_community(g, 3);
    }

    if(g->stage == StageFlop) {
        view_port_update(app->view_port);
        run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
        if(app->exit_requested) return false;
        if(app->reset_requested) return true;
        if(resolve_fold_win(g, payout)) {
            g->hand_in_progress = false;
            g->hand_no++;
            furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
            return true;
        }
        normalize_contested_pot(g);
        reset_street_bets(g);
        g->stage = StageTurn;
        deal_community(g, 1);
    }

    if(g->stage == StageTurn) {
        view_port_update(app->view_port);
        run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
        if(app->exit_requested) return false;
        if(app->reset_requested) return true;
        if(resolve_fold_win(g, payout)) {
            g->hand_in_progress = false;
            g->hand_no++;
            furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
            return true;
        }
        normalize_contested_pot(g);
        reset_street_bets(g);
        g->stage = StageRiver;
        deal_community(g, 1);
    }

    if(g->stage == StageRiver) {
        view_port_update(app->view_port);
        run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
        if(app->exit_requested) return false;
        if(app->reset_requested) return true;
        if(resolve_fold_win(g, payout)) {
            g->hand_in_progress = false;
            g->hand_no++;
            furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
            return true;
        }
        normalize_contested_pot(g);
        g->stage = StageShowdown;
    }

    app->reveal_all = true;
    resolve_showdown(g, payout);
    set_showdown_winner_mask(app, payout);
    if(!prompt_showdown_inspect(app)) return false;
    g->hand_in_progress = false;
    g->hand_no++;
    view_port_update(app->view_port);
    return true;
}
