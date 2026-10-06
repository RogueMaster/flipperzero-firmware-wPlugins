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

static void test_no_score_hand(void) {
    CribbageCard hand[4] = {
        CARD(CribbageRankAce, CribbageSuitHearts), CARD(2, CribbageSuitDiamonds),
        CARD(7, CribbageSuitClubs), CARD(9, CribbageSuitSpades)};
    CribbageScoreBreakdown score = cribbage_score_hand(
        hand, CARD(CribbageRankKing, CribbageSuitHearts), false);
    assert(score.total == 0);
}

static void test_pair_scoring(void) {
    CribbageCard pair_royal[4] = {
        CARD(7, CribbageSuitHearts), CARD(7, CribbageSuitDiamonds),
        CARD(7, CribbageSuitClubs), CARD(2, CribbageSuitSpades)};
    CribbageCard four_of_a_kind[4] = {
        CARD(4, CribbageSuitHearts), CARD(4, CribbageSuitDiamonds),
        CARD(4, CribbageSuitClubs), CARD(4, CribbageSuitSpades)};

    assert(cribbage_score_hand(pair_royal, CARD(CribbageRankKing, CribbageSuitHearts), false)
               .pairs == 6);
    assert(cribbage_score_hand(four_of_a_kind, CARD(CribbageRankKing, CribbageSuitHearts), false)
               .pairs == 12);
}

static void test_ace_low_run(void) {
    CribbageCard hand[4] = {
        CARD(CribbageRankAce, CribbageSuitHearts), CARD(2, CribbageSuitDiamonds),
        CARD(3, CribbageSuitClubs), CARD(9, CribbageSuitSpades)};
    CribbageScoreBreakdown score = cribbage_score_hand(
        hand, CARD(CribbageRankKing, CribbageSuitHearts), false);
    assert(score.runs == 3);
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

static void test_triple_and_double_double_runs(void) {
    CribbageCard triple_run[4] = {
        CARD(3, CribbageSuitHearts), CARD(3, CribbageSuitDiamonds),
        CARD(3, CribbageSuitClubs), CARD(4, CribbageSuitSpades)};
    CribbageCard double_double_run[4] = {
        CARD(3, CribbageSuitHearts), CARD(3, CribbageSuitDiamonds),
        CARD(4, CribbageSuitClubs), CARD(4, CribbageSuitSpades)};

    CribbageScoreBreakdown triple = cribbage_score_hand(
        triple_run, CARD(5, CribbageSuitHearts), false);
    CribbageScoreBreakdown double_double = cribbage_score_hand(
        double_double_run, CARD(5, CribbageSuitHearts), false);
    assert(triple.pairs == 6);
    assert(triple.runs == 9);
    assert(double_double.pairs == 4);
    assert(double_double.runs == 12);
}

static void test_crib_flush_rule(void) {
    CribbageCard hand[4] = {
        CARD(CribbageRankAce, CribbageSuitHearts), CARD(2, CribbageSuitHearts),
        CARD(7, CribbageSuitHearts), CARD(CribbageRankKing, CribbageSuitHearts)};
    CribbageCard starter = CARD(9, CribbageSuitSpades);
    assert(cribbage_score_hand(hand, starter, false).flush == 4);
    assert(cribbage_score_hand(hand, starter, true).flush == 0);

    starter.suit = CribbageSuitHearts;
    assert(cribbage_score_hand(hand, starter, false).flush == 5);
    assert(cribbage_score_hand(hand, starter, true).flush == 5);
}

static void test_complete_deal_and_his_heels(void) {
    CribbageCard starter = CARD(CribbageRankJack, CribbageSuitHearts);
    CribbageCard non_dealer[4] = {
        CARD(CribbageRankAce, CribbageSuitHearts), CARD(2, CribbageSuitDiamonds),
        CARD(7, CribbageSuitClubs), CARD(9, CribbageSuitSpades)};
    CribbageCard dealer[4] = {
        CARD(3, CribbageSuitHearts), CARD(3, CribbageSuitDiamonds),
        CARD(4, CribbageSuitClubs), CARD(5, CribbageSuitSpades)};
    CribbageCard crib[4] = {
        CARD(10, CribbageSuitClubs), CARD(2, CribbageSuitHearts),
        CARD(3, CribbageSuitDiamonds), CARD(CribbageRankQueen, CribbageSuitSpades)};

    assert(cribbage_score_hand(non_dealer, starter, false).total == 0);
    assert(cribbage_score_hand(dealer, starter, false).total == 12);
    CribbageScoreBreakdown crib_score = cribbage_score_hand(crib, starter, true);
    assert(crib_score.fifteens == 6);
    assert(crib_score.runs == 3);
    assert(crib_score.total == 9);
    assert(cribbage_score_his_heels(starter) == 2);
    assert(cribbage_score_his_heels(CARD(10, CribbageSuitHearts)) == 0);
}

int main(void) {
    test_perfect_29();
    test_no_score_hand();
    test_pair_scoring();
    test_ace_low_run();
    test_double_run();
    test_triple_and_double_double_runs();
    test_crib_flush_rule();
    test_complete_deal_and_his_heels();
    puts("cribbage scoring tests passed");
    return 0;
}
