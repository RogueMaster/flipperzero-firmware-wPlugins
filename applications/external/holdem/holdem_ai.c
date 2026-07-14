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

static bool is_extreme_ai(uint8_t ai_level_pct) {
    return ai_level_pct >= HOLDEM_AI_EXTREME_LEVEL;
}

static bool is_easy_ai(uint8_t ai_level_pct) {
    return ai_level_pct <= HOLDEM_AI_EASY_LEVEL;
}

static int board_wetness(const HoldemGame* game);

static bool is_heads_up_pot(const HoldemGame* game) {
    size_t contenders = 0;
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        if(game->players[player_index].in_hand) contenders++;
    }
    return contenders == 2;
}

// Heads-up pots need a wider continuing range, especially against oversized single-raise pressure.
// This keeps Easy/Medium from folding almost everything to the same repetitive bully sizing
// without giving them unrealistic read-based knowledge.
static int heads_up_defense_discount(
    const HoldemGame* game,
    const Player* player,
    int32_t to_call,
    int32_t current_bet,
    uint8_t ai_level_pct) {
    if(!is_heads_up_pot(game) || to_call <= 0 || current_bet <= 0 || player->stack <= 0) return 0;
    if(game->stage != StagePreflop && game->stage != StageFlop) return 0;

    int discount = (game->stage == StagePreflop) ? 6 : 4;
    if(current_bet >= game->big_blind * 3) discount += 3;
    if(current_bet >= game->big_blind * 5) discount += 3;
    if(current_bet >= game->big_blind * 8) discount += 2;

    int call_stack_percent = (int)((to_call * 100) / player->stack);
    if(call_stack_percent <= 12)
        discount += 4;
    else if(call_stack_percent <= 20)
        discount += 2;
    else if(call_stack_percent >= 30)
        discount -= 3;

    if(ai_level_pct <= HOLDEM_AI_EASY_LEVEL) {
        discount = (discount * 9) / 10;
    } else if(ai_level_pct <= HOLDEM_AI_MEDIUM_LEVEL) {
        discount += 1;
    } else if(ai_level_pct >= HOLDEM_AI_EXTREME_LEVEL) {
        discount = (discount * 3) / 4;
    }

    return (int)clamp_i32(discount, 0, 18);
}

// Easy bots should occasionally continue cheaply with weak hands so they do not
// feel robotic or trivially bullyable in heads-up pots.
static bool easy_loose_continue(
    const HoldemGame* game,
    const Player* player,
    int32_t to_call,
    int strength,
    int random_roll,
    uint8_t ai_level_pct) {
    if(!is_easy_ai(ai_level_pct) || game->stage != StagePreflop || to_call <= 0) return false;
    if(to_call > game->big_blind * 2) return false;
    if(player->stack <= 0) return false;

    int call_stack_percent = (int)((to_call * 100) / player->stack);
    if(call_stack_percent > 12) return false;

    if(strength >= 34) return true;
    if(strength >= 26 && random_roll < 28) return true;
    return (strength >= 20) && (random_roll < 18);
}

// Give Easy bots an occasional smallest-legal stab when checked to on early streets,
// but only on relatively dry boards so the behavior reads as a light bluff, not clairvoyance.
static bool easy_min_stab_spot(
    const HoldemGame* game,
    int strength,
    int random_roll,
    uint8_t ai_level_pct) {
    if(!is_easy_ai(ai_level_pct)) return false;
    if(game->stage != StageFlop && game->stage != StageTurn) return false;
    if(board_wetness(game) >= 45) return false;
    if(strength >= 48) return false;
    return random_roll < ((game->stage == StageFlop) ? 26 : 18);
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
                        if(score_cmp(&candidate_score, &best_score) > 0)
                            best_score = candidate_score;
                    }
                }
            }
        }
    }
    return best_score;
}

// Small pairs should be playable but not look like premium all-in hands preflop.
static int pair_preflop_bonus(int pair_rank) {
    int bonus = 44 + (pair_rank * 3);
    if(pair_rank <= 3)
        bonus -= 12;
    else if(pair_rank <= 5)
        bonus -= 6;
    else if(pair_rank >= 9)
        bonus += 5;
    return bonus;
}

static int broadway_preflop_bonus(int higher_rank, int lower_rank, bool suited) {
    if(higher_rank < 8 || lower_rank < 8) return 0;

    int bonus = 4;
    if(suited) bonus += 4;
    if(higher_rank == 12 || lower_rank == 12) bonus += 3;
    return bonus;
}

// Preflop hand heuristic score in [0,100].
static int preflop_strength(const HoldemGame* game, int player_index, uint8_t ai_level_pct) {
    const Player* player = &game->players[player_index];
    int hole_rank_a = player->hole[0].rank;
    int hole_rank_b = player->hole[1].rank;
    int higher_rank = (hole_rank_a > hole_rank_b) ? hole_rank_a : hole_rank_b;
    int lower_rank = (hole_rank_a > hole_rank_b) ? hole_rank_b : hole_rank_a;

    int strength_score = 0;
    if(hole_rank_a == hole_rank_b) {
        strength_score += pair_preflop_bonus(higher_rank);
    } else
        strength_score += (higher_rank + 2) * 3 + (lower_rank + 2) * 2;

    if(player->hole[0].suit == player->hole[1].suit) strength_score += 6;

    int rank_gap = higher_rank - lower_rank;
    if(rank_gap == 1)
        strength_score += 5;
    else if(rank_gap == 2)
        strength_score += 2;
    else if(rank_gap >= 5)
        strength_score -= 8;

    if(higher_rank >= 9) strength_score += 6;

    if(player_index == game->button)
        strength_score += 8;
    else if(player_index == game->sb_idx || player_index == game->bb_idx)
        strength_score -= 3;

    if(is_extreme_ai(ai_level_pct)) {
        bool suited = (player->hole[0].suit == player->hole[1].suit);
        strength_score += broadway_preflop_bonus(higher_rank, lower_rank, suited);
        if(suited && rank_gap == 1 && higher_rank >= 7) strength_score += 5;
        if(rank_gap >= 6 && higher_rank < 10) strength_score -= 4;
    }

    strength_score += ((int)ai_level_pct - 50) / 2;
    return (int)clamp_i32(strength_score, 0, 100);
}

// Higher current-bet levels mean the bot is facing stronger pressure than a casual limp pot.
static int bet_pressure_score(
    const HoldemGame* game,
    const Player* player,
    int32_t to_call,
    int32_t current_bet) {
    if(to_call <= 0 || current_bet <= 0) return 0;

    int pressure_score = 0;
    int pressure_vs_street = (int)((to_call * 100) / current_bet);
    int pressure_vs_stack = (int)((to_call * 100) / player->stack);

    pressure_score += pressure_vs_street / 8;
    pressure_score += pressure_vs_stack / 4;

    if(to_call >= game->big_blind * 4) pressure_score += 4;
    if(to_call >= game->big_blind * 8) pressure_score += 4;

    return (int)clamp_i32(pressure_score, 0, 30);
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
    if(max_suit_count >= 3)
        wetness_score += 20;
    else if(max_suit_count == 2)
        wetness_score += 10;

    int pair_group_count = 0;
    for(size_t rank = 0; rank < 13; rank++)
        if(rank_counts[rank] >= 2) pair_group_count++;
    wetness_score += pair_group_count * 8;

    int min_rank_seen = 12;
    int max_rank_seen = 0;
    for(size_t rank = 0; rank < 13; rank++) {
        if(rank_counts[rank]) {
            if((int)rank < min_rank_seen) min_rank_seen = (int)rank;
            if((int)rank > max_rank_seen) max_rank_seen = (int)rank;
        }
    }

    if((max_rank_seen - min_rank_seen) <= 4)
        wetness_score += 16;
    else if((max_rank_seen - min_rank_seen) <= 6)
        wetness_score += 8;

    return (int)clamp_i32(wetness_score, 0, 100);
}

// Extreme difficulty gets basic draw awareness so it can defend or pressure with
// credible flush and straight development instead of only reacting to made hands.
static int draw_potential_score(const HoldemGame* game, int player_index) {
    if(game->board_count < 3) return 0;

    uint8_t suit_counts[4] = {0};
    bool ranks_present[13] = {false};
    Card cards[7];
    size_t card_count = 2 + game->board_count;
    cards[0] = game->players[player_index].hole[0];
    cards[1] = game->players[player_index].hole[1];

    for(size_t board_index = 0; board_index < game->board_count && board_index < 5;
        board_index++) {
        cards[2 + board_index] = game->board[board_index];
    }

    for(size_t card_index = 0; card_index < card_count; card_index++) {
        suit_counts[cards[card_index].suit]++;
        ranks_present[cards[card_index].rank] = true;
    }

    int draw_score = 0;
    for(size_t suit = 0; suit < 4; suit++) {
        if(suit_counts[suit] >= 4) {
            draw_score += 18;
            break;
        }
    }

    for(int window_start = 0; window_start <= 8; window_start++) {
        int ranks_in_window = 0;
        for(int rank = window_start; rank < window_start + 5; rank++) {
            if(ranks_present[rank]) ranks_in_window++;
        }

        if(ranks_in_window >= 4) {
            draw_score += 10;
            break;
        }
    }

    if(ranks_present[12] && ranks_present[0] && ranks_present[1] && ranks_present[2]) {
        draw_score += 6;
    }

    return (int)clamp_i32(draw_score, 0, 28);
}

// Postflop hand heuristic in [0,100] from made strength plus board texture.
static int postflop_strength(const HoldemGame* game, int player_index, uint8_t ai_level_pct) {
    Card seven_cards[7];
    seven_cards[0] = game->players[player_index].hole[0];
    seven_cards[1] = game->players[player_index].hole[1];
    for(size_t board_index = 0; board_index < game->board_count && board_index < 5;
        board_index++) {
        seven_cards[2 + board_index] = game->board[board_index];
    }

    size_t visible_card_count = 2 + game->board_count;
    Score best_score = best_from_n(seven_cards, visible_card_count);

    int made_hand_score =
        best_score.v[0] * 12 + ((best_score.v[1] >= 0) ? (best_score.v[1] / 2) : 0);
    int board_wetness_score = board_wetness(game);
    int draw_score = is_extreme_ai(ai_level_pct) ? draw_potential_score(game, player_index) : 0;

    int strength_score = 20 + made_hand_score;
    if(best_score.v[0] >= 4) strength_score += 10;
    if(best_score.v[0] >= 2) strength_score += 6;

    strength_score += (100 - board_wetness_score) / 10;
    strength_score += draw_score / 2;
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
    int32_t raise_step = (game->small_blind > 0) ? game->small_blind : HOLDEM_BLIND_STEP_SB;
    int pot_percent = 50;
    if(strength >= 78)
        pot_percent = 100;
    else if(strength >= 62)
        pot_percent = 75;

    int32_t raise_from_pot = (game->pot * pot_percent) / 100;
    int32_t max_raise = player->stack - to_call;
    if(max_raise < min_raise) return 0;

    int32_t raise_by = raise_from_pot;
    if(raise_by < min_raise) raise_by = min_raise;
    if(raise_by >= max_raise) return max_raise;

    raise_by = ((raise_by + (raise_step / 2)) / raise_step) * raise_step;
    if(raise_by < min_raise) raise_by = min_raise;
    if(raise_by >= max_raise) {
        int32_t stepped_max = (max_raise / raise_step) * raise_step;
        if(stepped_max >= min_raise)
            raise_by = stepped_max;
        else
            raise_by = max_raise;
    }
    return raise_by;
}

// Prevent medium-strength hands from accidentally turning pot-sized actions into sketchy stack-offs.
static bool can_commit_stack(
    const HoldemGame* game,
    const Player* player,
    int32_t total_commit,
    int strength,
    uint8_t ai_level_pct) {
    if(player->stack <= 0) return false;
    if(total_commit <= 0) return true;

    int32_t stack_after_action = player->stack - total_commit;
    int commit_percent = (int)((total_commit * 100) / player->stack);
    bool effectively_all_in = (stack_after_action <= game->big_blind) || (commit_percent >= 70);
    if(!effectively_all_in) return true;

    int required_strength = (game->stage == StagePreflop) ? 78 : 72;
    required_strength += ((int)ai_level_pct - 50) / 8;
    if(commit_percent >= 85) required_strength += 4;
    if(game->stage == StagePreflop && total_commit >= game->big_blind * 10) required_strength += 4;

    return strength >= required_strength;
}

void bot_action(
    HoldemApp* app,
    const HoldemGame* game,
    int acting_player_index,
    int32_t to_call,
    int32_t min_raise,
    int32_t current_bet,
    bool can_raise,
    HoldemAction* action,
    int32_t* amount) {
    const Player* player = &game->players[acting_player_index];
    (void)current_bet;

    // Preflop and postflop use different heuristics, but both collapse to the same 0..100 scale
    // so downstream action thresholds stay easy to tune.
    int strength = (game->stage == StagePreflop) ?
                       preflop_strength(game, acting_player_index, app->ai_level_pct) :
                       postflop_strength(game, acting_player_index, app->ai_level_pct);
    bool extreme_ai = is_extreme_ai(app->ai_level_pct);

    int32_t denominator = game->pot + to_call;
    int pot_odds_percent = (denominator > 0) ? (int)((to_call * 100) / denominator) : 0;
    int pressure_score = bet_pressure_score(game, player, to_call, current_bet);
    int decision_strength = strength;
    if(extreme_ai && game->stage != StagePreflop) {
        decision_strength += draw_potential_score(game, acting_player_index) / 3;
    }

    int random_roll = (int)random_uniform(100);
    int bluff_window = 5 + ((int)app->ai_level_pct - 50) / 6;
    if(extreme_ai) bluff_window -= 2;
    if(bluff_window < 3) bluff_window = 3;

    if(to_call <= 0) {
        int32_t raise_by = choose_raise_by(game, player, 0, min_raise, strength);
        bool raise_for_value = (decision_strength >= 78 && random_roll < (extreme_ai ? 68 : 55)) ||
                               (decision_strength >= 62 && random_roll < (extreme_ai ? 32 : 22));
        bool light_bluff =
            (decision_strength < 55 && random_roll < bluff_window &&
             board_wetness(game) < (extreme_ai ? 60 : 70));

        if(can_raise && min_raise > 0 &&
           can_commit_stack(game, player, min_raise, decision_strength, app->ai_level_pct) &&
           easy_min_stab_spot(game, decision_strength, random_roll, app->ai_level_pct)) {
            *action = ActRaise;
            *amount = min_raise;
            return;
        }

        if(can_raise && raise_by > 0 &&
           can_commit_stack(game, player, raise_by, decision_strength, app->ai_level_pct) &&
           (raise_for_value || light_bluff)) {
            *action = ActRaise;
            *amount = raise_by;
            return;
        }

        *action = ActCheck;
        *amount = 0;
        return;
    }

    int required_strength = pot_odds_percent + (12 - ((int)app->ai_level_pct / 8));
    required_strength += pressure_score;
    if(extreme_ai) required_strength += pressure_score / 2;
    required_strength -=
        heads_up_defense_discount(game, player, to_call, current_bet, app->ai_level_pct);
    if(required_strength < 4) required_strength = 4;

    if(!can_commit_stack(game, player, to_call, decision_strength, app->ai_level_pct) ||
       decision_strength + (random_roll / 8) < required_strength) {
        if(easy_loose_continue(
               game, player, to_call, decision_strength, random_roll, app->ai_level_pct)) {
            *action = ActCall;
            *amount = 0;
            return;
        }
        *action = ActFold;
        *amount = 0;
        return;
    }

    int32_t raise_by = choose_raise_by(game, player, to_call, min_raise, strength);
    if(can_raise && raise_by > 0 &&
       can_commit_stack(game, player, to_call + raise_by, decision_strength, app->ai_level_pct) &&
       decision_strength >= (required_strength + (extreme_ai ? 16 : 22)) &&
       random_roll < (extreme_ai ? 42 : 30)) {
        *action = ActRaise;
        *amount = raise_by;
        return;
    }

    *action = ActCall;
    *amount = 0;
}
