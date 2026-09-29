#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CribbageSuitHearts,
    CribbageSuitDiamonds,
    CribbageSuitClubs,
    CribbageSuitSpades,
} CribbageSuit;

typedef enum {
    CribbageRankAce = 1,
    CribbageRankJack = 11,
    CribbageRankQueen = 12,
    CribbageRankKing = 13,
} CribbageRank;

typedef struct {
    CribbageRank rank;
    CribbageSuit suit;
    bool set;
} CribbageCard;

typedef struct {
    uint8_t fifteens;
    uint8_t pairs;
    uint8_t runs;
    uint8_t flush;
    uint8_t nobs;
    uint8_t total;
} CribbageScoreBreakdown;

CribbageScoreBreakdown cribbage_score_hand(
    const CribbageCard hand[4],
    CribbageCard starter,
    bool is_crib);

bool cribbage_cards_equal(CribbageCard left, CribbageCard right);
uint8_t cribbage_card_value(CribbageCard card);
const char* cribbage_rank_name(CribbageRank rank);
const char* cribbage_suit_name(CribbageSuit suit);
