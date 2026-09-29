#include "cribbage.h"

#include <string.h>

bool cribbage_cards_equal(CribbageCard left, CribbageCard right) {
    return left.rank == right.rank && left.suit == right.suit;
}

uint8_t cribbage_card_value(CribbageCard card) {
    return card.rank > 10 ? 10 : card.rank;
}

const char* cribbage_rank_name(CribbageRank rank) {
    static const char* const names[] = {
        "?", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    return (rank >= CribbageRankAce && rank <= CribbageRankKing) ? names[rank] : "?";
}

const char* cribbage_suit_name(CribbageSuit suit) {
    static const char* const names[] = {"Hearts", "Diamonds", "Clubs", "Spades"};
    return suit <= CribbageSuitSpades ? names[suit] : "?";
}

static uint8_t score_fifteens(const CribbageCard cards[5]) {
    uint8_t score = 0;
    for(uint8_t mask = 1; mask < 32; mask++) {
        uint8_t value = 0;
        for(uint8_t index = 0; index < 5; index++) {
            if(mask & (1U << index)) value += cribbage_card_value(cards[index]);
        }
        if(value == 15) score += 2;
    }
    return score;
}

static uint8_t score_pairs(const CribbageCard cards[5]) {
    uint8_t score = 0;
    for(uint8_t left = 0; left < 5; left++) {
        for(uint8_t right = left + 1; right < 5; right++) {
            if(cards[left].rank == cards[right].rank) score += 2;
        }
    }
    return score;
}

static uint8_t score_runs(const CribbageCard cards[5]) {
    uint8_t counts[14] = {0};
    for(uint8_t index = 0; index < 5; index++) counts[cards[index].rank]++;

    for(int8_t length = 5; length >= 3; length--) {
        uint8_t score = 0;
        for(uint8_t start = CribbageRankAce; start <= 14 - length; start++) {
            uint8_t combinations = 1;
            for(uint8_t rank = start; rank < start + length; rank++) {
                if(counts[rank] == 0) {
                    combinations = 0;
                    break;
                }
                combinations *= counts[rank];
            }
            score += combinations * length;
        }
        if(score) return score;
    }
    return 0;
}

static uint8_t score_flush(const CribbageCard hand[4], CribbageCard starter, bool is_crib) {
    for(uint8_t index = 1; index < 4; index++) {
        if(hand[index].suit != hand[0].suit) return 0;
    }
    if(starter.suit == hand[0].suit) return 5;
    return is_crib ? 0 : 4;
}

static uint8_t score_nobs(const CribbageCard hand[4], CribbageCard starter) {
    for(uint8_t index = 0; index < 4; index++) {
        if(hand[index].rank == CribbageRankJack && hand[index].suit == starter.suit) return 1;
    }
    return 0;
}

CribbageScoreBreakdown cribbage_score_hand(
    const CribbageCard hand[4],
    CribbageCard starter,
    bool is_crib) {
    CribbageCard cards[5];
    memcpy(cards, hand, sizeof(CribbageCard) * 4);
    cards[4] = starter;

    CribbageScoreBreakdown score = {
        .fifteens = score_fifteens(cards),
        .pairs = score_pairs(cards),
        .runs = score_runs(cards),
        .flush = score_flush(hand, starter, is_crib),
        .nobs = score_nobs(hand, starter),
    };
    score.total = score.fifteens + score.pairs + score.runs + score.flush + score.nobs;
    return score;
}
