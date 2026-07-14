#ifndef HOLDEM_EVAL_H
#define HOLDEM_EVAL_H

#include "holdem_types.h"

// Converts a card into a compact two-character string (example: "AS").
void card_to_str(Card card, char out[3]);

// Compares two poker scores.
int score_cmp(const Score* left, const Score* right);

// Evaluates the exact strength of five cards.
Score eval5(const Card cards[5]);

// Returns the strongest score from a seven-card pool.
Score best_five_from_seven(const Card cards[7]);

// Returns strongest score and copies the winning five cards.
Score best_five_from_seven_with_cards(const Card cards[7], Card best_five[5]);

// Sorts five cards into display order (grouped by combos, then rank high-to-low).
void sort_five_for_display(Card cards[5]);

// Formats five cards into text like "AS KD QH JC TD".
void format_five_cards(const Card cards[5], char* out, size_t out_size);

// Human-readable category label for score category 0..8.
const char* hand_rank_label(int category);

#endif
