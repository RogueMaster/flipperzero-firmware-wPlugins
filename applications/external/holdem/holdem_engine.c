#include "holdem_engine.h"

#include "holdem_eval.h"

#include <furi.h>

#include <string.h>
#include <stdio.h>

// Small helper for bounded random values.
static uint32_t random_uniform(uint32_t upper_bound) {
    if(upper_bound <= 1) return 0;
    uint32_t value = 0;
    furi_hal_random_fill_buf((uint8_t*)&value, sizeof(value));
    return value % upper_bound;
}

int next_alive(HoldemGame* game, int start_index) {
    int player_count = (int)game->player_count;
    int player_index = ((start_index % player_count) + player_count) % player_count;
    for(int probe = 0; probe < player_count; probe++) {
        if(game->players[player_index].stack > 0) return player_index;
        player_index = (player_index + 1) % player_count;
    }
    return -1;
}

size_t alive_count(HoldemGame* game) {
    size_t alive_players = 0;
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        if(game->players[player_index].stack > 0) alive_players++;
    }
    return alive_players;
}

size_t active_in_hand_count(HoldemGame* game) {
    size_t in_hand_players = 0;
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        if(game->players[player_index].in_hand) in_hand_players++;
    }
    return in_hand_players;
}

size_t active_can_bet_count(HoldemGame* game) {
    size_t can_bet_players = 0;
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        if(game->players[player_index].in_hand && !game->players[player_index].all_in) can_bet_players++;
    }
    return can_bet_players;
}

bool should_auto_runout(HoldemGame* game) {
    return (active_in_hand_count(game) > 1) && (active_can_bet_count(game) <= 1);
}

void delay_for_visual_step(const HoldemApp* app) {
    if(app->bot_delay_enabled && app->bot_delay_ms > 0) {
        furi_delay_ms(app->bot_delay_ms);
    }
}

void put_chips(HoldemGame* game, int player_index, int32_t amount) {
    Player* player = &game->players[player_index];
    if(amount > player->stack) amount = player->stack;
    if(amount < 0) amount = 0;

    player->stack -= amount;
    player->street_bet += amount;
    player->hand_contrib += amount;
    game->pot += amount;

    if(player->stack == 0) player->all_in = true;
}

// Build a canonical deck and shuffle it with Fisher-Yates.
void init_deck(HoldemGame* game) {
    size_t deck_index = 0;
    for(uint8_t rank = 0; rank < 13; rank++) {
        for(uint8_t suit = 0; suit < 4; suit++) {
            game->deck[deck_index].rank = rank;
            game->deck[deck_index].suit = suit;
            deck_index++;
        }
    }

    for(size_t shuffle_index = HOLDEM_DECK_SIZE - 1; shuffle_index > 0; shuffle_index--) {
        size_t swap_index = (size_t)random_uniform((uint32_t)(shuffle_index + 1));
        Card temp_card = game->deck[shuffle_index];
        game->deck[shuffle_index] = game->deck[swap_index];
        game->deck[swap_index] = temp_card;
    }

    game->deck_pos = 0;
}

Card pop_card(HoldemGame* game) {
    Card next_card = game->deck[game->deck_pos];
    game->deck_pos++;
    return next_card;
}

void reset_hand(HoldemGame* game) {
    // Hand reset preserves long-lived table state like stacks, button position, and blind size,
    // but wipes every per-hand field so the next deal starts from a clean slate.
    init_deck(game);
    game->board_count = 0;
    game->pot = 0;
    game->stage = StagePreflop;

    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        Player* player = &game->players[player_index];
        player->in_hand = (player->stack > 0);
        player->all_in = false;
        player->street_bet = 0;
        player->hand_contrib = 0;
    }
}

bool post_blinds(HoldemGame* game) {
    if(alive_count(game) < 2) return false;

    // Dealer and blind positions only advance across players who still have chips.
    game->button = next_alive(game, game->button + 1);
    game->sb_idx = next_alive(game, game->button + 1);
    game->bb_idx = next_alive(game, game->sb_idx + 1);

    put_chips(game, game->sb_idx, game->small_blind);
    put_chips(game, game->bb_idx, game->big_blind);
    return true;
}

void deal_hole(HoldemGame* game) {
    for(int round = 0; round < 2; round++) {
        for(size_t player_index = 0; player_index < game->player_count; player_index++) {
            if(game->players[player_index].stack > 0) {
                game->players[player_index].hole[round] = pop_card(game);
            }
        }
    }
}

void deal_community(HoldemGame* game, size_t card_count) {
    // Burn one card before each community reveal to match real Hold'em dealing flow.
    (void)pop_card(game);
    for(size_t board_index = 0; board_index < card_count; board_index++) {
        game->board[game->board_count++] = pop_card(game);
    }
}

bool resolve_fold_win(HoldemGame* game, PayoutResult* payout_result) {
    int winner_index = -1;
    size_t active_players = 0;

    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        if(game->players[player_index].in_hand) {
            active_players++;
            winner_index = (int)player_index;
        }
    }

    if(active_players == 1 && winner_index >= 0) {
        int32_t payout = game->pot;
        memset(payout_result, 0, sizeof(*payout_result));
        payout_result->idx[0] = winner_index;
        payout_result->count = 1;
        payout_result->payout[winner_index] = payout;
        return true;
    }

    return false;
}

void resolve_showdown(HoldemGame* game, PayoutResult* payout_result) {
    memset(payout_result, 0, sizeof(*payout_result));

    // Build sorted unique contribution levels used to distribute side pots.
    int32_t contribution_levels[HOLDEM_MAX_PLAYERS];
    size_t contribution_level_count = 0;

    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        int32_t contribution = game->players[player_index].hand_contrib;
        if(contribution <= 0) continue;

        bool already_seen = false;
        for(size_t level_index = 0; level_index < contribution_level_count; level_index++) {
            if(contribution_levels[level_index] == contribution) {
                already_seen = true;
                break;
            }
        }

        if(!already_seen) contribution_levels[contribution_level_count++] = contribution;
    }

    for(size_t left = 0; left < contribution_level_count; left++) {
        for(size_t right = left + 1; right < contribution_level_count; right++) {
            if(contribution_levels[right] < contribution_levels[left]) {
                int32_t temp_level = contribution_levels[left];
                contribution_levels[left] = contribution_levels[right];
                contribution_levels[right] = temp_level;
            }
        }
    }

    Score cached_scores[HOLDEM_MAX_PLAYERS];
    bool score_ready[HOLDEM_MAX_PLAYERS] = {0};

    int32_t previous_level = 0;
    for(size_t level_index = 0; level_index < contribution_level_count; level_index++) {
        int32_t current_level = contribution_levels[level_index];
        int contender_indexes[HOLDEM_MAX_PLAYERS];
        size_t contender_count = 0;
        size_t players_in_layer_count = 0;

        for(size_t player_index = 0; player_index < game->player_count; player_index++) {
            Player* player = &game->players[player_index];
            if(player->hand_contrib >= current_level) {
                players_in_layer_count++;
                if(player->in_hand) contender_indexes[contender_count++] = (int)player_index;
            }
        }

        // Each unique contribution level defines one side-pot layer.
        int32_t layer_size = (current_level - previous_level) * (int32_t)players_in_layer_count;
        previous_level = current_level;
        if(layer_size <= 0 || contender_count == 0) continue;

        Score best_score = {.v = {-1, -1, -1, -1, -1, -1}};
        int winner_indexes[HOLDEM_MAX_PLAYERS];
        size_t winner_count = 0;

        for(size_t contender_index = 0; contender_index < contender_count; contender_index++) {
            int player_index = contender_indexes[contender_index];
            if(!score_ready[player_index]) {
                Card seven_cards[7];
                seven_cards[0] = game->players[player_index].hole[0];
                seven_cards[1] = game->players[player_index].hole[1];
                for(size_t board_index = 0; board_index < 5; board_index++) {
                    seven_cards[2 + board_index] = game->board[board_index];
                }

                cached_scores[player_index] = best_five_from_seven(seven_cards);
                score_ready[player_index] = true;
            }

            int comparison = score_cmp(&cached_scores[player_index], &best_score);
            if(comparison > 0) {
                best_score = cached_scores[player_index];
                winner_count = 0;
                winner_indexes[winner_count++] = player_index;
            } else if(comparison == 0) {
                winner_indexes[winner_count++] = player_index;
            }
        }

        if(winner_count == 0) continue;

        int32_t base_share = layer_size / (int32_t)winner_count;
        int32_t remainder = layer_size % (int32_t)winner_count;

        for(size_t winner_offset = 0; winner_offset < winner_count; winner_offset++) {
            int player_index = winner_indexes[winner_offset];
            int32_t payout = base_share + (winner_offset < (size_t)remainder ? 1 : 0);
            payout_result->payout[player_index] += payout;
        }
    }

    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        if(payout_result->payout[player_index] > 0) {
            payout_result->idx[payout_result->count++] = (int)player_index;
        }
    }
}

void set_showdown_winner_mask(HoldemApp* app, const PayoutResult* payout_result) {
    app->showdown_winner_mask = 0;
    for(size_t payout_index = 0; payout_index < payout_result->count; payout_index++) {
        int player_index = payout_result->idx[payout_index];
        if(player_index >= 0 && player_index < (int)HOLDEM_MAX_PLAYERS) {
            app->showdown_winner_mask |= (uint8_t)(1u << player_index);
        }
    }
}

void apply_payout(HoldemGame* game, const PayoutResult* payout_result) {
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        if(payout_result->payout[player_index] > 0) {
            game->players[player_index].stack += payout_result->payout[player_index];
        }
    }
    game->pot = 0;
}

void reset_street_bets(HoldemGame* game) {
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        game->players[player_index].street_bet = 0;
    }
}

int champion_idx(HoldemGame* game) {
    int last_alive_index = -1;
    int alive_players = 0;
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        if(game->players[player_index].stack > 0) {
            alive_players++;
            last_alive_index = (int)player_index;
        }
    }
    return (alive_players == 1) ? last_alive_index : -1;
}

void init_game(HoldemGame* game) {
    memset(game, 0, sizeof(*game));
    game->player_count = HOLDEM_MAX_PLAYERS;
    game->small_blind = 10;
    game->big_blind = 20;
    game->button = -1;
    game->hand_in_progress = false;

    snprintf(game->players[0].name, sizeof(game->players[0].name), "You");
    game->players[0].stack = 1000;
    game->players[0].is_bot = false;

    snprintf(game->players[1].name, sizeof(game->players[1].name), "Bot1");
    game->players[1].stack = 1000;
    game->players[1].is_bot = true;

    snprintf(game->players[2].name, sizeof(game->players[2].name), "Bot2");
    game->players[2].stack = 1000;
    game->players[2].is_bot = true;

    snprintf(game->players[3].name, sizeof(game->players[3].name), "Bot3");
    game->players[3].stack = 1000;
    game->players[3].is_bot = true;
}

void init_game_with_player_count(HoldemGame* game, size_t player_count) {
    init_game(game);
    if(player_count < HOLDEM_MIN_PLAYERS) player_count = HOLDEM_MIN_PLAYERS;
    if(player_count > HOLDEM_MAX_PLAYERS) player_count = HOLDEM_MAX_PLAYERS;
    game->player_count = player_count;
}

