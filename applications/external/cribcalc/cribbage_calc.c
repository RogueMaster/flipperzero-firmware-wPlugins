#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cribbage.h"

typedef enum {
    CribbageScreenWelcome,
    CribbageScreenCountType,
    CribbageScreenRank,
    CribbageScreenSuit,
    CribbageScreenDuplicate,
    CribbageScreenResults,
} CribbageScreen;

typedef struct {
    CribbageCard cards[5];
    CribbageCard pending;
    CribbageScoreBreakdown score;
    uint8_t slot;
    uint8_t duplicate_slot;
    bool is_crib;
    CribbageScreen screen;
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
} CribbageApp;

static const char* const count_types[] = {"Hand", "Crib"};

static void draw_text(Canvas* canvas, uint8_t x, uint8_t y, const char* text, Font font) {
    canvas_set_font(canvas, font);
    canvas_draw_str(canvas, x, y, text);
}

static void draw_slot_title(Canvas* canvas, const CribbageApp* app) {
    char buffer[32];
    if(app->slot < 4) {
        snprintf(
            buffer, sizeof(buffer), "%s card %u/4", app->is_crib ? "Crib" : "Hand", app->slot + 1);
    } else {
        snprintf(buffer, sizeof(buffer), "Starter  5/5");
    }
    draw_text(canvas, 4, 10, buffer, FontSecondary);
}

static void draw_large_club_icon(Canvas* canvas, uint8_t x, uint8_t baseline) {
    uint8_t top = baseline - 19;

    canvas_draw_disc(canvas, x + 8, top + 4, 4);
    canvas_draw_disc(canvas, x + 4, top + 10, 4);
    canvas_draw_disc(canvas, x + 12, top + 10, 4);
    canvas_draw_box(canvas, x + 7, top + 12, 3, 6);
    canvas_draw_line(canvas, x + 5, top + 18, x + 11, top + 18);
}

static void draw_large_diamond_icon(Canvas* canvas, uint8_t x, uint8_t baseline) {
    static const uint8_t widths[] = {
        1, 3, 5, 7, 9, 11, 13, 15, 17, 17, 17, 15, 13, 11, 9, 7, 5, 3, 1};
    uint8_t top = baseline - 19;
    for(uint8_t row = 0; row < 19; row++) {
        uint8_t width = widths[row];
        canvas_draw_line(
            canvas, x + (17 - width) / 2, top + row, x + (17 + width) / 2 - 1, top + row);
    }
}

static void
    draw_suit_icon(Canvas* canvas, uint8_t x, uint8_t y, CribbageSuit suit, uint8_t scale) {
    static const uint8_t hearts[] = {0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08, 0x00};
    static const uint8_t diamonds[] = {0x08, 0x1C, 0x3E, 0x7F, 0x3E, 0x1C, 0x08};
    static const uint8_t spades[] = {0x08, 0x1C, 0x3E, 0x7F, 0x7F, 0x1C, 0x3E};
    static const uint8_t* const icons[] = {hearts, diamonds, NULL, spades};
    static const uint16_t club[] = {
        0x038, 0x07C, 0x07C, 0x0FE, 0x1FF, 0x1FF, 0x0FE, 0x038, 0x038, 0x07C};

    if(suit > CribbageSuitSpades) return;
    if(scale == 2 && suit == CribbageSuitDiamonds) {
        draw_large_diamond_icon(canvas, x, y);
        return;
    }
    if(scale == 2 && suit == CribbageSuitClubs) {
        draw_large_club_icon(canvas, x, y);
        return;
    }
    if(suit == CribbageSuitClubs) {
        uint8_t top = y - 10 * scale;
        for(uint8_t row = 0; row < 10; row++) {
            for(uint8_t column = 0; column < 9; column++) {
                if(club[row] & (1U << (8 - column))) {
                    canvas_draw_box(canvas, x + column * scale, top + row * scale, scale, scale);
                }
            }
        }
        return;
    }
    uint8_t height = 7;
    uint8_t top = y - height * scale;
    for(uint8_t row = 0; row < height; row++) {
        for(uint8_t column = 0; column < 7; column++) {
            if(icons[suit][row] & (1U << (6 - column))) {
                canvas_draw_box(canvas, x + column * scale, top + row * scale, scale, scale);
            }
        }
    }
}

static void draw_card(Canvas* canvas, uint8_t y, CribbageCard card, Font font) {
    const char* rank = cribbage_rank_name(card.rank);
    draw_text(canvas, 4, y, rank, font);
    draw_suit_icon(canvas, card.rank == 10 ? 20 : 13, y, card.suit, 1);
}

static void draw_suit_controls(Canvas* canvas) {
    draw_text(canvas, 4, 47, "UP: H   RIGHT: D", FontSecondary);
    draw_text(canvas, 4, 55, "DOWN: C  LEFT: S", FontSecondary);
}

static void draw_suit_picker_card(Canvas* canvas, CribbageCard card) {
    canvas_draw_frame(canvas, 34, 14, 60, 26);
    draw_text(canvas, 44, 34, cribbage_rank_name(card.rank), FontPrimary);
    draw_suit_icon(canvas, 64, 36, card.suit, 2);
}

static void cribbage_draw_callback(Canvas* canvas, void* context) {
    CribbageApp* app = context;
    char buffer[32];
    canvas_clear(canvas);

    if(app->screen == CribbageScreenWelcome) {
        draw_text(canvas, 4, 12, "Cribbage Calc", FontPrimary);
        draw_text(canvas, 4, 28, "Count one hand or crib", FontSecondary);
        draw_text(canvas, 4, 48, "OK: choose count", FontSecondary);
        draw_text(canvas, 4, 60, "BACK: exit", FontSecondary);
    } else if(app->screen == CribbageScreenCountType) {
        draw_text(canvas, 4, 12, "What are you counting?", FontPrimary);
        snprintf(buffer, sizeof(buffer), "> %s", count_types[app->is_crib]);
        draw_text(canvas, 12, 34, buffer, FontPrimary);
        draw_text(canvas, 4, 50, "UP/DOWN: choose", FontSecondary);
        draw_text(canvas, 4, 62, "OK: enter cards  BACK: home", FontSecondary);
    } else if(app->screen == CribbageScreenRank) {
        draw_slot_title(canvas, app);
        snprintf(buffer, sizeof(buffer), "Rank: %s", cribbage_rank_name(app->pending.rank));
        draw_text(canvas, 4, 31, buffer, FontPrimary);
        draw_text(canvas, 4, 47, "UP/DOWN: change rank", FontSecondary);
        draw_text(canvas, 4, 60, "OK: suit   BACK: previous", FontSecondary);
    } else if(app->screen == CribbageScreenSuit) {
        draw_slot_title(canvas, app);
        draw_suit_picker_card(canvas, app->pending);
        draw_suit_controls(canvas);
        draw_text(canvas, 4, 63, "OK: save   BACK: rank", FontSecondary);
    } else if(app->screen == CribbageScreenDuplicate) {
        draw_text(canvas, 4, 12, "Duplicate card", FontPrimary);
        draw_card(canvas, 27, app->pending, FontPrimary);
        draw_text(canvas, 4, 40, "Already used in:", FontSecondary);
        if(app->duplicate_slot < 4) {
            snprintf(
                buffer,
                sizeof(buffer),
                "%s card %u",
                app->is_crib ? "Crib" : "Hand",
                app->duplicate_slot + 1);
        } else {
            snprintf(buffer, sizeof(buffer), "Starter");
        }
        draw_text(canvas, 4, 50, buffer, FontSecondary);
        draw_text(canvas, 4, 60, "OK: suit   BACK: rank", FontSecondary);
    } else if(app->screen == CribbageScreenResults) {
        CribbageScoreBreakdown score = app->score;
        snprintf(buffer, sizeof(buffer), "%s: %u", app->is_crib ? "Crib" : "Hand", score.total);
        draw_text(canvas, 4, 10, buffer, FontPrimary);
        snprintf(
            buffer,
            sizeof(buffer),
            "15s %u  Pairs %u  Runs %u",
            score.fifteens,
            score.pairs,
            score.runs);
        draw_text(canvas, 4, 27, buffer, FontSecondary);
        snprintf(buffer, sizeof(buffer), "Flush %u  Nobs %u", score.flush, score.nobs);
        draw_text(canvas, 4, 39, buffer, FontSecondary);
        draw_text(canvas, 4, 52, "OK: new count", FontSecondary);
        draw_text(canvas, 4, 62, "BACK: edit cards", FontSecondary);
    }
}

static void cribbage_input_callback(InputEvent* input_event, void* context) {
    CribbageApp* app = context;
    furi_message_queue_put(app->input_queue, input_event, 0);
}

static void reset_count(CribbageApp* app) {
    memset(app->cards, 0, sizeof(app->cards));
    memset(&app->score, 0, sizeof(app->score));
    app->slot = 0;
    app->pending = (CribbageCard){.rank = CribbageRankAce, .suit = CribbageSuitHearts};
}

static void calculate_score(CribbageApp* app) {
    app->score = cribbage_score_hand(app->cards, app->cards[4], app->is_crib);
}

static bool find_duplicate(const CribbageApp* app, CribbageCard card, uint8_t* duplicate_slot) {
    for(uint8_t index = 0; index < 5; index++) {
        if(app->cards[index].set && cribbage_cards_equal(app->cards[index], card)) {
            *duplicate_slot = index;
            return true;
        }
    }
    return false;
}

static void advance_rank(CribbageApp* app, int8_t direction) {
    int8_t rank = app->pending.rank + direction;
    if(rank < CribbageRankAce) rank = CribbageRankKing;
    if(rank > CribbageRankKing) rank = CribbageRankAce;
    app->pending.rank = rank;
}

static void begin_previous_slot(CribbageApp* app) {
    if(app->slot == 0) {
        app->screen = CribbageScreenCountType;
        return;
    }
    app->slot--;
    app->pending = app->cards[app->slot];
    app->cards[app->slot].set = false;
    app->screen = CribbageScreenRank;
}

static bool handle_event(CribbageApp* app, const InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return true;

    if(app->screen == CribbageScreenWelcome) {
        if(event->key == InputKeyOk) {
            app->is_crib = false;
            app->screen = CribbageScreenCountType;
        } else if(event->key == InputKeyBack) {
            return false;
        }
    } else if(app->screen == CribbageScreenCountType) {
        if(event->key == InputKeyUp || event->key == InputKeyDown) {
            app->is_crib = !app->is_crib;
        } else if(event->key == InputKeyOk) {
            reset_count(app);
            app->screen = CribbageScreenRank;
        } else if(event->key == InputKeyBack) {
            app->screen = CribbageScreenWelcome;
        }
    } else if(app->screen == CribbageScreenRank) {
        if(event->key == InputKeyUp)
            advance_rank(app, 1);
        else if(event->key == InputKeyDown)
            advance_rank(app, -1);
        else if(event->key == InputKeyOk)
            app->screen = CribbageScreenSuit;
        else if(event->key == InputKeyBack)
            begin_previous_slot(app);
    } else if(app->screen == CribbageScreenSuit) {
        if(event->key == InputKeyUp)
            app->pending.suit = CribbageSuitHearts;
        else if(event->key == InputKeyRight)
            app->pending.suit = CribbageSuitDiamonds;
        else if(event->key == InputKeyDown)
            app->pending.suit = CribbageSuitClubs;
        else if(event->key == InputKeyLeft)
            app->pending.suit = CribbageSuitSpades;
        else if(event->key == InputKeyBack)
            app->screen = CribbageScreenRank;
        else if(event->key == InputKeyOk) {
            uint8_t duplicate_slot;
            if(find_duplicate(app, app->pending, &duplicate_slot)) {
                app->duplicate_slot = duplicate_slot;
                app->screen = CribbageScreenDuplicate;
            } else {
                app->pending.set = true;
                app->cards[app->slot] = app->pending;
                if(app->slot == 4) {
                    calculate_score(app);
                    app->screen = CribbageScreenResults;
                } else {
                    app->slot++;
                    app->pending =
                        (CribbageCard){.rank = CribbageRankAce, .suit = CribbageSuitHearts};
                    app->screen = CribbageScreenRank;
                }
            }
        }
    } else if(app->screen == CribbageScreenDuplicate) {
        if(event->key == InputKeyOk)
            app->screen = CribbageScreenSuit;
        else if(event->key == InputKeyBack)
            app->screen = CribbageScreenRank;
    } else if(app->screen == CribbageScreenResults) {
        if(event->key == InputKeyBack)
            begin_previous_slot(app);
        else if(event->key == InputKeyOk) {
            app->is_crib = false;
            app->screen = CribbageScreenCountType;
        }
    }
    view_port_update(app->view_port);
    return true;
}

int32_t cribbage_calc_app(void* p) {
    UNUSED(p);
    CribbageApp* app = malloc(sizeof(CribbageApp));
    memset(app, 0, sizeof(CribbageApp));
    app->screen = CribbageScreenWelcome;
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, cribbage_draw_callback, app);
    view_port_input_callback_set(app->view_port, cribbage_input_callback, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port);

    InputEvent event;
    while(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
        if(!handle_event(app, &event)) break;
    }

    gui_remove_view_port(gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    free(app);
    return 0;
}
