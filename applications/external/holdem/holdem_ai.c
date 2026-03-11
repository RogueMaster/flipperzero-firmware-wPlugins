#include "holdem_ai.h"

#include "holdem_eval.h"

#include <furi.h>

// AI remains intentionally lightweight: a few heuristics, bounded randomness, and cheap math
// so we stay within Flipper constraints while keeping room for later difficulty work.

// Local clamp keeps AI math independent from UI/main modules.
static int32_t clamp_i32(int32_t value, int32_t min_value, int32_t max_value) {
    if(value < min_value) return min_value;
    if(value > max_value) return max_value;
    return value;
}

// Small helper for bounded random values.
static uint32_t random_uniform(uint32_t upper_bound) {
    if(upper_bound <= 1) return 0;
    uint32_t value = 0;
    furi_hal_random_fill_buf((uint8_t*)&value, sizeof(value));
    return value % upper_bound;
}

// Evaluate best 5-card score from any 5-of-N combination.
static Score best_from_n(const Card* cards, size_t card_count) {
    Score best_score = {.v = {-1, -1, -1, -1, -1, -1}};
    if(card_count < 5) return best_score;

    for(size_t first = 0; first + 4 < card_count; first++) {
        for(size_t second = first + 1; second + 3 < card_count; second++) {
            for(size_t third = second + 1; third + 2 < card_count; third++) {
                for(size_t fourth = third + 1; fourth + 1 < card_count; fourth++) {
                    for(size_t fifth = fourth + 1; fifth < card_count; fifth++) {
                        Card five_cards[5] = {
                            cards[first], cards[second], cards[third], cards[fourth], cards[fifth]};
                        Score candidate_score = eval5(five_cards);
                        if(score_cmp(&candidate_score, &best_score) > 0) best_score = candidate_score;
                    }
                }
            }
        }
    }
    return best_score;
}

// Preflop hand heuristic score in [0,100].
static int preflop_strength(const HoldemGame* game, int player_index, uint8_t ai_level_pct) {
    const Player* player = &game->players[player_index];
    int hole_rank_a = player->hole[0].rank;
    int hole_rank_b = player->hole[1].rank;
    int higher_rank = (hole_rank_a > hole_rank_b) ? hole_rank_a : hole_rank_b;
    int lower_rank = (hole_rank_a > hole_rank_b) ? hole_rank_b : hole_rank_a;

    int strength_score = 0;
    if(hole_rank_a == hole_rank_b) strength_score += 55 + (higher_rank * 3);
    else strength_score += (higher_rank + 2) * 3 + (lower_rank + 2) * 2;

    if(player->hole[0].suit == player->hole[1].suit) strength_score += 6;

    int rank_gap = higher_rank - lower_rank;
    if(rank_gap == 1) strength_score += 5;
    else if(rank_gap == 2) strength_score += 2;
    else if(rank_gap >= 5) strength_score -= 8;

    if(higher_rank >= 9) strength_score += 6;

    if(player_index == game->button) strength_score += 8;
    else if(player_index == game->sb_idx || player_index == game->bb_idx) strength_score -= 3;

    strength_score += ((int)ai_level_pct - 50) / 2;
    return (int)clamp_i32(strength_score, 0, 100);
}

// Board texture heuristic: higher means more coordinated/wet board.
static int board_wetness(const HoldemGame* game) {
    if(game->board_count < 3) return 10;

    uint8_t suit_counts[4] = {0};
    uint8_t rank_counts[13] = {0};
    for(size_t board_index = 0; board_index < game->board_count; board_index++) {
        suit_counts[game->board[board_index].suit]++;
        rank_counts[game->board[board_index].rank]++;
    }

    int wetness_score = 0;
    uint8_t max_suit_count = 0;
    for(size_t suit = 0; suit < 4; suit++) {
        if(suit_counts[suit] > max_suit_count) max_suit_count = suit_counts[suit];
    }
    if(max_suit_count >= 3) wetness_score += 20;
    else if(max_suit_count == 2) wetness_score += 10;

    int pair_group_count = 0;
    for(size_t rank = 0; rank < 13; rank++) if(rank_counts[rank] >= 2) pair_group_count++;
    wetness_score += pair_group_count * 8;

    int min_rank_seen = 12;
    int max_rank_seen = 0;
    for(size_t rank = 0; rank < 13; rank++) {
        if(rank_counts[rank]) {
            if((int)rank < min_rank_seen) min_rank_seen = (int)rank;
            if((int)rank > max_rank_seen) max_rank_seen = (int)rank;
        }
    }

    if((max_rank_seen - min_rank_seen) <= 4) wetness_score += 16;
    else if((max_rank_seen - min_rank_seen) <= 6) wetness_score += 8;

    return (int)clamp_i32(wetness_score, 0, 100);
}

// Postflop hand heuristic in [0,100] from made strength plus board texture.
static int postflop_strength(const HoldemGame* game, int player_index, uint8_t ai_level_pct) {
    Card seven_cards[7];
    seven_cards[0] = game->players[player_index].hole[0];
    seven_cards[1] = game->players[player_index].hole[1];
    for(size_t board_index = 0; board_index < game->board_count && board_index < 5; board_index++) {
        seven_cards[2 + board_index] = game->board[board_index];
    }

    size_t visible_card_count = 2 + game->board_count;
    Score best_score = best_from_n(seven_cards, visible_card_count);

    int made_hand_score = best_score.v[0] * 12 + ((best_score.v[1] >= 0) ? (best_score.v[1] / 2) : 0);
    int board_wetness_score = board_wetness(game);

    int strength_score = 20 + made_hand_score;
    if(best_score.v[0] >= 4) strength_score += 10;
    if(best_score.v[0] >= 2) strength_score += 6;

    strength_score += (100 - board_wetness_score) / 10;
    strength_score += ((int)ai_level_pct - 50) / 3;
    return (int)clamp_i32(strength_score, 0, 100);
}

// Choose raise amount from strength and pot size while respecting stack constraints.
static int32_t choose_raise_by(
    const HoldemGame* game,
    const Player* player,
    int32_t to_call,
    int32_t min_raise,
    int strength) {
    int pot_percent = 50;
    if(strength >= 78) pot_percent = 100;
    else if(strength >= 62) pot_percent = 75;

    int32_t raise_from_pot = (game->pot * pot_percent) / 100;
    int32_t max_raise = player->stack - to_call;
    if(max_raise < min_raise) return 0;

    int32_t raise_by = raise_from_pot;
    if(raise_by < min_raise) raise_by = min_raise;
    if(raise_by > max_raise) raise_by = max_raise;
    return raise_by;
}

void bot_action(
    HoldemApp* app,
    const HoldemGame* game,
    int acting_player_index,
    int32_t to_call,
    int32_t min_raise,
    int32_t current_bet,
    HoldemAction* action,
    int32_t* amount) {
    const Player* player = &game->players[acting_player_index];
    (void)current_bet;

    // Preflop and postflop use different heuristics, but both collapse to the same 0..100 scale
    // so downstream action thresholds stay easy to tune.
    int strength =
        (game->stage == StagePreflop) ?
            preflop_strength(game, acting_player_index, app->ai_level_pct) :
            postflop_strength(game, acting_player_index, app->ai_level_pct);

    int32_t denominator = game->pot + to_call;
    int pot_odds_percent = (denominator > 0) ? (int)((to_call * 100) / denominator) : 0;

    int random_roll = (int)random_uniform(100);
    int bluff_window = 5 + ((int)app->ai_level_pct - 50) / 6;
    if(bluff_window < 3) bluff_window = 3;

    if(to_call <= 0) {
        int32_t raise_by = choose_raise_by(game, player, 0, min_raise, strength);
        bool raise_for_value = (strength >= 78 && random_roll < 55) || (strength >= 62 && random_roll < 22);
        bool light_bluff = (strength < 55 && random_roll < bluff_window && board_wetness(game) < 70);

        if(raise_by > 0 && (raise_for_value || light_bluff)) {
            *action = ActRaise;
            *amount = raise_by;
            return;
        }

        *action = ActCheck;
        *amount = 0;
        return;
    }

    int required_strength = pot_odds_percent + (12 - ((int)app->ai_level_pct / 8));
    if(required_strength < 4) required_strength = 4;

    if(strength + (random_roll / 8) < required_strength) {
        *action = ActFold;
        *amount = 0;
        return;
    }

    int32_t raise_by = choose_raise_by(game, player, to_call, min_raise, strength);
    if(raise_by > 0 && strength >= (required_strength + 22) && random_roll < 30) {
        *action = ActRaise;
        *amount = raise_by;
        return;
    }

    *action = ActCall;
    *amount = 0;
}

