#include "card_graphics.h"

void draw_suit(Canvas* canvas, int x, int y, CardSuit suit) {
    switch(suit) {
    case SuitHearts:
        canvas_draw_str(canvas, x, y, "♥");
        break;
    case SuitDiamonds:
        canvas_draw_str(canvas, x, y, "♦");
        break;
    case SuitClubs:
        canvas_draw_str(canvas, x, y, "♣");
        break;
    case SuitSpades:
        canvas_draw_str(canvas, x, y, "♠");
        break;
    }
}

void draw_card(
    Canvas* canvas,
    int x,
    int y,
    Card* card,
    bool hidden,
    int card_width,
    int card_height) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, y, card_width, card_height);

    canvas_set_color(canvas, ColorBlack);

    if(hidden) {
        canvas_draw_box(canvas, x, y, card_width, card_height);
        canvas_draw_str(canvas, x + (card_width / 3), y + (card_height / 2), "?");
    } else {
        canvas_draw_frame(canvas, x, y, card_width, card_height);
        char rank_str[5];
        switch(card->rank) {
        case RankAce:
            snprintf(rank_str, sizeof(rank_str), "A");
            break;
        case RankJack:
            snprintf(rank_str, sizeof(rank_str), "J");
            break;
        case RankQueen:
            snprintf(rank_str, sizeof(rank_str), "Q");
            break;
        case RankKing:
            snprintf(rank_str, sizeof(rank_str), "K");
            break;
        default:
            snprintf(rank_str, sizeof(rank_str), "%d", card->rank);
            break;
        }
        canvas_draw_str(canvas, x + 2, y + 8, rank_str);
        draw_suit(canvas, x + 2, y + 16, card->suit);
    }
}
