#include "holdem_eval.h"

#include <string.h>

// Rank/suit lookup tables back the compact "AH" style formatter used by save diagnostics and UI helpers.
static const char k_rank_char[] = "23456789TJQKA";
static const char k_suit_char[] = {'C', 'D', 'H', 'S'};

// 21 combinations of choosing 5 cards from 7 cards.
static const uint8_t k_combo_7c5[21][5] = {
    {0, 1, 2, 3, 4}, {0, 1, 2, 3, 5}, {0, 1, 2, 3, 6}, {0, 1, 2, 4, 5}, {0, 1, 2, 4, 6},
    {0, 1, 2, 5, 6}, {0, 1, 3, 4, 5}, {0, 1, 3, 4, 6}, {0, 1, 3, 5, 6}, {0, 1, 4, 5, 6},
    {0, 2, 3, 4, 5}, {0, 2, 3, 4, 6}, {0, 2, 3, 5, 6}, {0, 2, 4, 5, 6}, {0, 3, 4, 5, 6},
    {1, 2, 3, 4, 5}, {1, 2, 3, 4, 6}, {1, 2, 3, 5, 6}, {1, 2, 4, 5, 6}, {1, 3, 4, 5, 6},
    {2, 3, 4, 5, 6},
};

static int straight_high(const uint8_t rank_counts[13]) {
    for(int high_rank = 12; high_rank >= 4; high_rank--) {
        if(
            rank_counts[high_rank] && rank_counts[high_rank - 1] && rank_counts[high_rank - 2] &&
            rank_counts[high_rank - 3] && rank_counts[high_rank - 4]) {
            return high_rank;
        }
    }

    // Wheel straight: A-2-3-4-5.
    if(rank_counts[12] && rank_counts[0] && rank_counts[1] && rank_counts[2] && rank_counts[3]) return 3;
    return -1;
}

void card_to_str(Card card, char out[3]) {
    out[0] = k_rank_char[card.rank];
    out[1] = k_suit_char[card.suit];
    out[2] = '\0';
}

int score_cmp(const Score* left, const Score* right) {
    for(size_t vector_index = 0; vector_index < 6; vector_index++) {
        if(left->v[vector_index] > right->v[vector_index]) return 1;
        if(left->v[vector_index] < right->v[vector_index]) return -1;
    }
    return 0;
}

Score eval5(const Card cards[5]) {
    // Score vectors are ordered by hand class first, then by kickers, so lexicographic comparison works.
    Score score = {.v = {0, 0, 0, 0, 0, 0}};
    uint8_t rank_counts[13] = {0};
    uint8_t suit_counts[4] = {0};

    int ranks[5];
    for(size_t card_index = 0; card_index < 5; card_index++) {
        ranks[card_index] = cards[card_index].rank;
        rank_counts[cards[card_index].rank]++;
        suit_counts[cards[card_index].suit]++;
    }

    bool is_flush =
        (suit_counts[0] == 5 || suit_counts[1] == 5 || suit_counts[2] == 5 || suit_counts[3] == 5);
    int straight_top = straight_high(rank_counts);

    int group_ranks[5] = {0};
    int group_counts[5] = {0};
    int group_count = 0;

    for(int rank = 12; rank >= 0; rank--) {
        if(rank_counts[rank]) {
            group_ranks[group_count] = rank;
            group_counts[group_count] = rank_counts[rank];
            group_count++;
        }
    }

    // Sort groups by size first, then by rank, so classification is straightforward.
    for(int left = 0; left < group_count; left++) {
        for(int right = left + 1; right < group_count; right++) {
            bool should_swap = false;
            if(group_counts[right] > group_counts[left]) should_swap = true;
            else if(group_counts[right] == group_counts[left] && group_ranks[right] > group_ranks[left])
                should_swap = true;

            if(should_swap) {
                int temp_rank = group_ranks[left];
                int temp_count = group_counts[left];
                group_ranks[left] = group_ranks[right];
                group_counts[left] = group_counts[right];
                group_ranks[right] = temp_rank;
                group_counts[right] = temp_count;
            }
        }
    }

    if(is_flush && straight_top >= 0) {
        score.v[0] = 8;
        score.v[1] = straight_top;
        return score;
    }

    if(group_counts[0] == 4) {
        score.v[0] = 7;
        score.v[1] = group_ranks[0];
        score.v[2] = group_ranks[1];
        return score;
    }

    if(group_counts[0] == 3 && group_counts[1] == 2) {
        score.v[0] = 6;
        score.v[1] = group_ranks[0];
        score.v[2] = group_ranks[1];
        return score;
    }

    if(is_flush) {
        int ordered_ranks[5];
        memcpy(ordered_ranks, ranks, sizeof(ordered_ranks));

        for(int left = 0; left < 5; left++) {
            for(int right = left + 1; right < 5; right++) {
                if(ordered_ranks[right] > ordered_ranks[left]) {
                    int temp_rank = ordered_ranks[left];
                    ordered_ranks[left] = ordered_ranks[right];
                    ordered_ranks[right] = temp_rank;
                }
            }
        }

        score.v[0] = 5;
        for(int index = 0; index < 5; index++) score.v[index + 1] = ordered_ranks[index];
        return score;
    }

    if(straight_top >= 0) {
        score.v[0] = 4;
        score.v[1] = straight_top;
        return score;
    }

    if(group_counts[0] == 3) {
        score.v[0] = 3;
        score.v[1] = group_ranks[0];

        int kicker_slot = 2;
        for(int rank = 12; rank >= 0 && kicker_slot < 4; rank--) {
            if(rank_counts[rank] == 1) score.v[kicker_slot++] = rank;
        }
        return score;
    }

    if(group_counts[0] == 2 && group_counts[1] == 2) {
        score.v[0] = 2;
        score.v[1] = group_ranks[0];
        score.v[2] = group_ranks[1];
        for(int rank = 12; rank >= 0; rank--) {
            if(rank_counts[rank] == 1) {
                score.v[3] = rank;
                break;
            }
        }
        return score;
    }

    if(group_counts[0] == 2) {
        score.v[0] = 1;
        score.v[1] = group_ranks[0];

        int kicker_slot = 2;
        for(int rank = 12; rank >= 0 && kicker_slot < 5; rank--) {
            if(rank_counts[rank] == 1) score.v[kicker_slot++] = rank;
        }
        return score;
    }

    score.v[0] = 0;
    int ordered_ranks[5];
    memcpy(ordered_ranks, ranks, sizeof(ordered_ranks));

    for(int left = 0; left < 5; left++) {
        for(int right = left + 1; right < 5; right++) {
            if(ordered_ranks[right] > ordered_ranks[left]) {
                int temp_rank = ordered_ranks[left];
                ordered_ranks[left] = ordered_ranks[right];
                ordered_ranks[right] = temp_rank;
            }
        }
    }

    for(int index = 0; index < 5; index++) score.v[index + 1] = ordered_ranks[index];
    return score;
}

Score best_five_from_seven(const Card cards[7]) {
    Score best_score = {.v = {-1, -1, -1, -1, -1, -1}};

    for(size_t combo_index = 0; combo_index < 21; combo_index++) {
        Card candidate_five[5];
        for(size_t card_index = 0; card_index < 5; card_index++) {
            candidate_five[card_index] = cards[k_combo_7c5[combo_index][card_index]];
        }

        Score candidate_score = eval5(candidate_five);
        if(score_cmp(&candidate_score, &best_score) > 0) best_score = candidate_score;
    }

    return best_score;
}

Score best_five_from_seven_with_cards(const Card cards[7], Card best_five[5]) {
    // This variant mirrors `best_five_from_seven` but also returns the exact winning five cards
    // so showdown UI can present something meaningful instead of just a category label.
    Score best_score = {.v = {-1, -1, -1, -1, -1, -1}};

    for(size_t combo_index = 0; combo_index < 21; combo_index++) {
        Card candidate_five[5];
        for(size_t card_index = 0; card_index < 5; card_index++) {
            candidate_five[card_index] = cards[k_combo_7c5[combo_index][card_index]];
        }

        Score candidate_score = eval5(candidate_five);
        if(score_cmp(&candidate_score, &best_score) > 0) {
            best_score = candidate_score;
            for(size_t card_index = 0; card_index < 5; card_index++) {
                best_five[card_index] = candidate_five[card_index];
            }
        }
    }

    return best_score;
}

void sort_five_for_display(Card cards[5]) {
    uint8_t rank_counts[13] = {0};
    for(size_t card_index = 0; card_index < 5; card_index++) rank_counts[cards[card_index].rank]++;

    for(size_t left = 0; left < 5; left++) {
        for(size_t right = left + 1; right < 5; right++) {
            uint8_t left_rank_count = rank_counts[cards[left].rank];
            uint8_t right_rank_count = rank_counts[cards[right].rank];
            bool should_swap = false;

            if(right_rank_count > left_rank_count) should_swap = true;
            else if(right_rank_count == left_rank_count && cards[right].rank > cards[left].rank)
                should_swap = true;
            else if(
                right_rank_count == left_rank_count && cards[right].rank == cards[left].rank &&
                cards[right].suit > cards[left].suit)
                should_swap = true;

            if(should_swap) {
                Card temp_card = cards[left];
                cards[left] = cards[right];
                cards[right] = temp_card;
            }
        }
    }
}

void format_five_cards(const Card cards[5], char* out, size_t out_size) {
    Card ordered_cards[5];
    for(size_t card_index = 0; card_index < 5; card_index++) ordered_cards[card_index] = cards[card_index];
    sort_five_for_display(ordered_cards);

    size_t write_index = 0;
    for(size_t card_index = 0; card_index < 5 && write_index + 3 < out_size; card_index++) {
        char card_text[3];
        card_to_str(ordered_cards[card_index], card_text);
        out[write_index++] = card_text[0];
        out[write_index++] = card_text[1];
        if(card_index < 4 && write_index + 1 < out_size) out[write_index++] = ' ';
    }
    out[write_index] = '\0';
}

const char* hand_rank_label(int category) {
    static const char* k_labels[] = {
        "High Card",
        "One Pair",
        "Two Pair",
        "Trips",
        "Straight",
        "Flush",
        "Full House",
        "Quads",
        "Straight Flush",
    };

    if(category < 0 || category > 8) return "Unknown";
    return k_labels[category];
}

