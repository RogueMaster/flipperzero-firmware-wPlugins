#ifndef HOLDEM_ENGINE_H
#define HOLDEM_ENGINE_H

#include "holdem_types.h"

int next_alive(HoldemGame* game, int start_index);
size_t alive_count(HoldemGame* game);
size_t active_in_hand_count(HoldemGame* game);
size_t active_can_bet_count(HoldemGame* game);
bool should_auto_runout(HoldemGame* game);
void delay_for_visual_step(const HoldemApp* app);
void put_chips(HoldemGame* game, int player_index, int32_t amount);
void init_deck(HoldemGame* game);
Card pop_card(HoldemGame* game);
void reset_hand(HoldemGame* game);
bool post_blinds(HoldemGame* game);
void deal_hole(HoldemGame* game);
void deal_community(HoldemGame* game, size_t card_count);
bool resolve_fold_win(HoldemGame* game, PayoutResult* payout_result);
void resolve_showdown(HoldemGame* game, PayoutResult* payout_result);
void set_showdown_winner_mask(HoldemApp* app, const PayoutResult* payout_result);
void apply_payout(HoldemGame* game, const PayoutResult* payout_result);
void reset_street_bets(HoldemGame* game);
int champion_idx(HoldemGame* game);
void init_game(HoldemGame* game);
void init_game_with_player_count(HoldemGame* game, size_t player_count);

#endif


