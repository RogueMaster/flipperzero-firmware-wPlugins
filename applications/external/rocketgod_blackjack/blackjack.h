#ifndef BLACKJACK_H
#define BLACKJACK_H

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

typedef enum {
    StateMenu,
    StateGame,
} GameState;

typedef enum {
    SuitHearts,
    SuitDiamonds,
    SuitClubs,
    SuitSpades,
} CardSuit;

typedef enum {
    RankAce = 1,
    RankTwo,
    RankThree,
    RankFour,
    RankFive,
    RankSix,
    RankSeven,
    RankEight,
    RankNine,
    RankTen,
    RankJack,
    RankQueen,
    RankKing,
} CardRank;

typedef struct {
    CardSuit suit;
    CardRank rank;
} Card;

typedef struct {
    Card cards[10];
    int card_count;
    int total;
} Hand;

typedef struct {
    GameState state;
    Hand player;
    Hand dealer;
    int game_result;
    bool exit;
    ViewPort* view_port;
    Gui* gui;
} BlackjackApp;

void deal_card(Hand* hand);
void input_callback(InputEvent* input_event, void* ctx);
void render_callback(Canvas* canvas, void* ctx);
int32_t blackjack_app(void* p);

#endif
