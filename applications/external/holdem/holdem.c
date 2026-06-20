#include "holdem_app_internal.h"

int32_t holdem_main(void* p) {
    UNUSED(p);

    HoldemApp* app = malloc(sizeof(HoldemApp));
    furi_check(app);
    memset(app, 0, sizeof(HoldemApp));

    init_game(&app->game);
    app->configured_small_blind = app->game.small_blind;
    app->configured_player_count = app->game.player_count;
    app->bot_count_edit_value = app->configured_player_count;
    app->pending_blinds_dirty = false;
    app->pending_small_blind = app->configured_small_blind;
    app->blind_edit_sb = app->configured_small_blind;
    app->blind_edit_initial_sb = app->configured_small_blind;
    app->progressive_blinds_enabled = false;
    app->progressive_period_hands = HOLDEM_PROGRESSIVE_DEFAULT_PERIOD_HANDS;
    app->progressive_step_sb = HOLDEM_PROGRESSIVE_DEFAULT_STEP_SB;
    app->progressive_next_raise_hand_no = 0;
    app->blind_edit_progressive_enabled = false;
    app->blind_edit_progressive_period_hands = HOLDEM_PROGRESSIVE_DEFAULT_PERIOD_HANDS;
    app->blind_edit_progressive_step_sb = HOLDEM_PROGRESSIVE_DEFAULT_STEP_SB;

    app->running = true;
    set_ai_level(app, HOLDEM_AI_DEFAULT_LEVEL);
    app->configured_ai_level_pct = app->ai_level_pct;
    app->bot_difficulty_edit_value = app->configured_ai_level_pct;
    set_bot_action_delay(app, true, HOLDEM_BOT_DELAY_MS);
    app->mode = UiModeStartReady;
    app->return_mode = UiModeTable;

    app->input_queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    app->view_port = view_port_alloc();

    view_port_draw_callback_set(app->view_port, holdem_draw_callback, app);
    view_port_input_callback_set(app->view_port, holdem_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    startup_splash_and_jingle(app);
    startup_choose_load_or_new(app);
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

    while(app->running) {
        if(!wait_for_start_confirmation(app)) break;

        PayoutResult payout;
        if(!resume_loaded_hand(app, &payout)) break;
        if(app->exit_requested) break;
        if(app->reset_requested) {
            reset_to_new_game(app);
            continue;
        }

        if(qualifies_for_big_win(app, &payout)) {
            if(!show_big_win_screen(app)) break;
        }

        show_hand_result_screen(app, &payout);

        if(app->exit_requested) break;
        apply_payout(&app->game, &payout);

        int champ = champion_idx(&app->game);
        bool you_busted = (app->game.players[0].stack <= 0);
        if(you_busted) {
            if(!show_interstitial_screen(app, "Game Over", false, 0u, true, true)) break;
            if(app->exit_requested) break;
            reset_to_new_game(app);
            continue;
        }

        if(champ >= 0) {
            if(champ == 0) {
                if(!show_interstitial_screen(app, "You Won!", true, 0u, true, false)) break;
                if(app->exit_requested) break;
                reset_to_new_game(app);
                continue;
            }

            if(!show_interstitial_screen(app, "Game Over", false, 0u, true, true)) break;
            if(app->exit_requested) break;
            reset_to_new_game(app);
            continue;
        }
    }

    if(app->exit_requested && app->save_on_exit) {
        (void)save_progress(
            &app->game,
            app->configured_ai_level_pct,
            app->configured_small_blind,
            app->progressive_blinds_enabled,
            app->progressive_period_hands,
            app->progressive_step_sb,
            app->progressive_next_raise_hand_no);
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->input_queue);

    free(app);
    return 0;
}
