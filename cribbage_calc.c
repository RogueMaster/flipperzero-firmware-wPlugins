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
    CribbageScreenRank,
    CribbageScreenSuit,
    CribbageScreenDuplicate,
    CribbageScreenResults,
    CribbageScreenDetails,
} CribbageScreen;

typedef struct {
    CribbageCard cards[13];
    CribbageCard pending;
    CribbageScoreBreakdown scores[3];
    uint8_t slot;
    uint8_t detail_hand;
    uint8_t duplicate_slot;
    CribbageScreen screen;
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
} CribbageApp;

static const char* const slot_names[] = {
    "Starter", "Non-dealer 1", "Non-dealer 2", "Non-dealer 3", "Non-dealer 4",
    "Dealer 1", "Dealer 2", "Dealer 3", "Dealer 4", "Crib 1", "Crib 2", "Crib 3",
    "Crib 4"};

static const char* const result_names[] = {"Non-dealer", "Dealer hand", "Crib"};

static void draw_text(Canvas* canvas, uint8_t x, uint8_t y, const char* text, Font font) {
    canvas_set_font(canvas, font);
    canvas_draw_str(canvas, x, y, text);
}

static void draw_card(Canvas* canvas, uint8_t y, CribbageCard card) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%s of %s", cribbage_rank_name(card.rank), cribbage_suit_name(card.suit));
    draw_text(canvas, 4, y, buffer, FontSecondary);
}

static void cribbage_draw_callback(Canvas* canvas, void* context) {
    CribbageApp* app = context;
    char buffer[32];
    canvas_clear(canvas);

    if(app->screen == CribbageScreenWelcome) {
        draw_text(canvas, 4, 12, "Cribbage Calc", FontPrimary);
        draw_text(canvas, 4, 28, "Score hands and crib", FontSecondary);
        draw_text(canvas, 4, 48, "OK: enter a deal", FontSecondary);
        draw_text(canvas, 4, 60, "BACK: exit", FontSecondary);
    } else if(app->screen == CribbageScreenRank) {
        snprintf(buffer, sizeof(buffer), "%s (%u/13)", slot_names[app->slot], app->slot + 1);
        draw_text(canvas, 4, 12, buffer, FontPrimary);
        snprintf(buffer, sizeof(buffer), "Rank: %s", cribbage_rank_name(app->pending.rank));
        draw_text(canvas, 4, 31, buffer, FontPrimary);
        draw_text(canvas, 4, 48, "UP/DOWN: change", FontSecondary);
        draw_text(canvas, 4, 60, "OK: suit   BACK: previous", FontSecondary);
    } else if(app->screen == CribbageScreenSuit) {
        snprintf(buffer, sizeof(buffer), "%s (%u/13)", slot_names[app->slot], app->slot + 1);
        draw_text(canvas, 4, 12, buffer, FontPrimary);
        draw_card(canvas, 31, app->pending);
        draw_text(canvas, 4, 48, "UP H  RIGHT D", FontSecondary);
        draw_text(canvas, 4, 60, "DOWN C LEFT S  OK: save", FontSecondary);
    } else if(app->screen == CribbageScreenDuplicate) {
        draw_text(canvas, 4, 12, "Duplicate card", FontPrimary);
        draw_text(canvas, 4, 30, "Already used in:", FontSecondary);
        draw_text(canvas, 4, 42, slot_names[app->duplicate_slot], FontSecondary);
        draw_text(canvas, 4, 60, "OK/BACK: change suit", FontSecondary);
    } else if(app->screen == CribbageScreenResults) {
        draw_text(canvas, 4, 11, "Deal scores", FontPrimary);
        for(uint8_t index = 0; index < 3; index++) {
            snprintf(buffer, sizeof(buffer), "%s: %u", result_names[index], app->scores[index].total);
            draw_text(canvas, 4, 24 + index * 11, buffer, FontSecondary);
        }
        if(app->cards[0].rank == CribbageRankJack) {
            draw_text(canvas, 4, 57, "His heels (dealer): +2", FontSecondary);
        } else {
            draw_text(canvas, 4, 57, "LEFT/RIGHT: breakdown", FontSecondary);
        }
    } else {
        CribbageScoreBreakdown score = app->scores[app->detail_hand];
        snprintf(buffer, sizeof(buffer), "%s: %u", result_names[app->detail_hand], score.total);
        draw_text(canvas, 4, 11, buffer, FontPrimary);
        snprintf(buffer, sizeof(buffer), "15s %u  Pairs %u  Runs %u", score.fifteens, score.pairs, score.runs);
        draw_text(canvas, 4, 28, buffer, FontSecondary);
        snprintf(buffer, sizeof(buffer), "Flush %u  Nobs %u", score.flush, score.nobs);
        draw_text(canvas, 4, 42, buffer, FontSecondary);
        draw_text(canvas, 4, 60, "LEFT/RIGHT: hand BACK: scores", FontSecondary);
    }
}

static void cribbage_input_callback(InputEvent* input_event, void* context) {
    CribbageApp* app = context;
    furi_message_queue_put(app->input_queue, input_event, 0);
}

static void reset_deal(CribbageApp* app) {
    memset(app->cards, 0, sizeof(app->cards));
    memset(app->scores, 0, sizeof(app->scores));
    app->slot = 0;
    app->pending = (CribbageCard){.rank = CribbageRankAce, .suit = CribbageSuitHearts};
}

static void calculate_scores(CribbageApp* app) {
    app->scores[0] = cribbage_score_hand(&app->cards[1], app->cards[0], false);
    app->scores[1] = cribbage_score_hand(&app->cards[5], app->cards[0], false);
    app->scores[2] = cribbage_score_hand(&app->cards[9], app->cards[0], true);
}

static bool find_duplicate(const CribbageApp* app, CribbageCard card, uint8_t* duplicate_slot) {
    for(uint8_t index = 0; index < 13; index++) {
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
        app->screen = CribbageScreenWelcome;
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
            reset_deal(app);
            app->screen = CribbageScreenRank;
        } else if(event->key == InputKeyBack) {
            return false;
        }
    } else if(app->screen == CribbageScreenRank) {
        if(event->key == InputKeyUp) advance_rank(app, 1);
        else if(event->key == InputKeyDown) advance_rank(app, -1);
        else if(event->key == InputKeyOk) app->screen = CribbageScreenSuit;
        else if(event->key == InputKeyBack) begin_previous_slot(app);
    } else if(app->screen == CribbageScreenSuit) {
        if(event->key == InputKeyUp) app->pending.suit = CribbageSuitHearts;
        else if(event->key == InputKeyRight) app->pending.suit = CribbageSuitDiamonds;
        else if(event->key == InputKeyDown) app->pending.suit = CribbageSuitClubs;
        else if(event->key == InputKeyLeft) app->pending.suit = CribbageSuitSpades;
        else if(event->key == InputKeyBack) app->screen = CribbageScreenRank;
        else if(event->key == InputKeyOk) {
            uint8_t duplicate_slot;
            if(find_duplicate(app, app->pending, &duplicate_slot)) {
                app->duplicate_slot = duplicate_slot;
                app->screen = CribbageScreenDuplicate;
            } else {
                app->pending.set = true;
                app->cards[app->slot] = app->pending;
                if(app->slot == 12) {
                    calculate_scores(app);
                    app->screen = CribbageScreenResults;
                } else {
                    app->slot++;
                    app->pending = (CribbageCard){.rank = CribbageRankAce, .suit = CribbageSuitHearts};
                    app->screen = CribbageScreenRank;
                }
            }
        }
    } else if(app->screen == CribbageScreenDuplicate) {
        app->screen = CribbageScreenSuit;
    } else if(app->screen == CribbageScreenResults) {
        if(event->key == InputKeyBack) begin_previous_slot(app);
        else if(event->key == InputKeyOk) {
            reset_deal(app);
            app->screen = CribbageScreenRank;
        } else if(event->key == InputKeyLeft || event->key == InputKeyRight) {
            app->detail_hand = event->key == InputKeyLeft ? 2 : 0;
            app->screen = CribbageScreenDetails;
        }
    } else {
        if(event->key == InputKeyBack) app->screen = CribbageScreenResults;
        else if(event->key == InputKeyLeft) app->detail_hand = (app->detail_hand + 2) % 3;
        else if(event->key == InputKeyRight) app->detail_hand = (app->detail_hand + 1) % 3;
        else if(event->key == InputKeyOk) {
            reset_deal(app);
            app->screen = CribbageScreenRank;
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
