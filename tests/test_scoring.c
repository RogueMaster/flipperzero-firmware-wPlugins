#include <assert.h>
#include <stdio.h>

#include "../cribbage.h"

#define CARD(rank_value, suit_value) \
    ((CribbageCard){.rank = rank_value, .suit = suit_value, .set = true})

static void test_perfect_29(void) {
    CribbageCard hand[4] = {
        CARD(5, CribbageSuitHearts), CARD(5, CribbageSuitDiamonds),
        CARD(5, CribbageSuitSpades), CARD(CribbageRankJack, CribbageSuitClubs)};
    CribbageScoreBreakdown score = cribbage_score_hand(
        hand, CARD(5, CribbageSuitClubs), false);
    assert(score.fifteens == 16);
    assert(score.pairs == 12);
    assert(score.nobs == 1);
    assert(score.total == 29);
}

static void test_double_run(void) {
    CribbageCard hand[4] = {
        CARD(3, CribbageSuitHearts), CARD(3, CribbageSuitDiamonds),
        CARD(4, CribbageSuitClubs), CARD(5, CribbageSuitSpades)};
    CribbageScoreBreakdown score = cribbage_score_hand(
        hand, CARD(10, CribbageSuitHearts), false);
    assert(score.pairs == 2);
    assert(score.runs == 6);
    assert(score.fifteens == 4);
    assert(score.total == 12);
}

static void test_crib_flush_rule(void) {
    CribbageCard hand[4] = {
        CARD(CribbageRankAce, CribbageSuitHearts), CARD(2, CribbageSuitHearts),
        CARD(7, CribbageSuitHearts), CARD(CribbageRankKing, CribbageSuitHearts)};
    CribbageCard starter = CARD(9, CribbageSuitSpades);
    assert(cribbage_score_hand(hand, starter, false).flush == 4);
    assert(cribbage_score_hand(hand, starter, true).flush == 0);
}

int main(void) {
    test_perfect_29();
    test_double_run();
    test_crib_flush_rule();
    puts("cribbage scoring tests passed");
    return 0;
}
