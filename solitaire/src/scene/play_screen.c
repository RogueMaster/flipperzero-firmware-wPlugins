#include <notification/notification_messages.h>
#include "./play_screen.h"
#include "../../game_state.h"
#include "../util/helpers.h"
#include "../../assets.h"

static bool can_quick_solve = false;
static bool solved = false;
static bool started = false;
int8_t picked_from[2] = {-1, -1};
static const NotificationSequence sequence_fail = {
    &message_vibro_on,
    &message_note_c4,
    &message_delay_10,
    &message_vibro_off,
    &message_sound_off,
    &message_delay_10,

    &message_vibro_on,
    &message_note_a3,
    &message_delay_10,
    &message_vibro_off,
    &message_sound_off,
    NULL,
};


void end_play_screen(GameState *state) {

    //put back the card from the hand to allow quick solve
    if (state->hand->count) {
        if (picked_from[1] == 1) {
            while (state->hand->tail) {
                list_push_back(list_pop_front(state->hand), state->tableau[picked_from[1]]);
            }
        }
        if (picked_from[1] == 0) {
            list_push_back(list_pop_front(state->hand), state->waste);
        }
    }

    state->selected[0] = 0;
    state->selected[1] = 0;
    started = false;
    solved = false;
    state->scene_switch = 1;
}

void check_quick_solve(GameState *state) {
    uint8_t c = 0;
    for (uint8_t i = 0; i < 7; i++) {
        Card *front = list_peek_front(state->tableau[i]);
        if (!front || front->exposed) c++;
    }
    can_quick_solve = c == 7;
}

void start_play_screen(void *data) {
    GameState *state = (GameState *) data;
    picked_from[0] = -1;
    picked_from[1] = -1;
    state->selected[0] = 0;
    state->selected[1] = 0;
    state->selected_card = 0;
    state->isDirty = true;
    can_quick_solve = false;
    solved = false;
    started = true;
    state->game_start = furi_get_tick();
}

bool check_finish(void *data) {
    GameState *state = (GameState *) data;
    if (state->waste->count > 0 || state->deck->count > 0) return false;

    for (uint8_t i = 0; i < 7; i++) {
        Card *front = list_peek_front(state->tableau[i]);
        if (front) return false;
    }

    return true;
}

void reset_picked() {
    picked_from[0] = -1;
    picked_from[1] = -1;
}

bool is_picked_from(uint8_t x, uint8_t y) {
    return picked_from[0] == x && picked_from[1] == y;
}

void set_picked_from(int8_t x, int8_t y) {
    picked_from[0] = x;
    picked_from[1] = y;
}

void render_play_screen(void *data) {

    GameState *state = (GameState *) data;

    check_pointer(state->deck);
    check_pointer(state->waste);

    //Render deck, if there is more than one card left, simulate a bit of depth
    if (state->deck->count > 1) {
        card_render_slot(2, 1, false, state->buffer);
        deck_render(state->deck, Normal, 1, 0, state->selected[0] == 0 && state->selected[1] == 0, true, state->buffer);
    } else {
        deck_render(state->deck, Normal, 2, 1, state->selected[0] == 0 && state->selected[1] == 0, true, state->buffer);
    }

    //Render waste pile
    deck_render(state->waste, Normal, 20, 1, state->selected[0] == 1 && state->selected[1] == 0, true, state->buffer);

    //Render tableau and foundation
    for (uint8_t x = 0; x < 7; x++) {
        if (x < 4) {
            check_pointer(state->foundation[x]);
            deck_render(state->foundation[x], Normal, 56 + x * 18, 1,
                        state->selected[0] == x + 3 && state->selected[1] == 0, true, state->buffer);
        }
        check_pointer(state->tableau[x]);
        deck_render(state->tableau[x], Vertical, 2 + x * 18, 25,
                    (state->selected[0] == x && state->selected[1] == 1) ? state->selected_card : 0, true,
                    state->buffer);
    }

    uint8_t h = state->selected[1] == 1 ? (MIN((uint8_t) state->tableau[state->selected[0]]->count, 4) * 4 + 15) : 0;

    //render cards in hand
    deck_render(state->hand, Vertical, 10 + state->selected[0] * 18, h + 10, false, false,
                state->buffer);

    if (started && can_quick_solve) {
        buffer_draw_rbox(state->buffer, 26, 53, 100, 64, White);
        buffer_draw_rbox_frame(state->buffer, 25, 52, 101, 65, Black);
        Vector pos = (Vector) {64, 58};
        buffer_draw_all(state->buffer, (Buffer *) &sprite_solve, &pos, 0);
    }

}

void update_play_screen(void *data) {
    GameState *state = (GameState *) data;
    if (solved) {
        end_play_screen(state);
    }
}

void input_play_screen(void *data, InputKey key, InputType type) {
    GameState *state = (GameState *) data;
    state->isDirty = true;

    if (type == InputTypePress) {
        switch (key) {
            case InputKeyLeft:
                if (state->selected[0] > 0) state->selected[0]--;
                if (state->selected[0] == 2 && state->selected[1] == 0) state->selected[0]--;
                state->selected_card = 1;
                return;
                break;
            case InputKeyRight:
                if (state->selected[0] < 6) state->selected[0]++;
                if (state->selected[0] == 2 && state->selected[1] == 0) state->selected[0]++;
                state->selected_card = 1;
                return;
                break;
            case InputKeyUp:
                //try to move selection inside the tableau
                if (state->selected[1] == 1) {
                    //check if highlight can move up in the tableau
                    int8_t id, id_flipped;
                    deck_first_non_flipped(state->tableau[state->selected[0]], &id);
                    id_flipped = state->tableau[state->selected[0]]->count - id;
                    //move up until it reaches the last exposed card, disable when there is something in hand or no card is exposed
                    if (state->selected_card < id_flipped && id >= 0 && state->hand->count == 0) {
                        state->selected_card++;
                    }
                        //move to the top row
                    else {
                        state->selected[1] = 0;
                        state->selected_card = 1;
                        if (state->selected[0] == 2) state->selected[0]--;
                    }
                }
                return;
                break;
            case InputKeyDown:
                if (state->selected[1] == 0) {
                    state->selected_card = 1;
                    state->selected[1] = 1;
                } else if (state->selected_card > 1) {
                    state->selected_card--;
                }
                return;
                break;
            case InputKeyOk:

                //cycle deck
                if (state->selected[0] == 0 && state->selected[1] == 0) {
                    if(state->deck->count > 0 || state->waste->count > 0) {
                        if (state->deck->count > 0) {
                            Card *c = list_pop_back(state->deck);
                            c->exposed = true;
                            list_push_back(c, state->waste);
                            return;
                        } else {
                            while (state->waste->count) {
                                Card *c = list_pop_back(state->waste);
                                c->exposed = false;
                                list_push_back(c, state->deck);
                            }
                            return;
                        }
                    }
                    if(can_quick_solve) return;
                } else if (state->selected[0] == 1 && state->selected[1] == 0) {
                    //pick from waste
                    if (state->hand->count == 0 && state->waste->count > 0) {
                        list_push_back(list_pop_back(state->waste), state->hand);
                        set_picked_from(1, 0);
                        return;
                    } else if (is_picked_from(1, 0)) { //put back to waste
                        list_push_back(list_pop_back(state->hand), state->waste);
                        reset_picked();
                        return;
                    }

                }
                    //test if it can be put to the foundation (only if 1 card is in hand)
                else if (state->hand->count == 1 && state->selected[1] == 0 && state->selected[0] > 2) {
                    List *foundation = state->foundation[state->selected[0] - 3];
                    check_pointer(foundation);
                    if (card_test_foundation(list_peek_front(state->hand), list_peek_back(foundation))) {
                        list_push_back(list_pop_front(state->hand), foundation);
                        reset_picked();
                        solved = check_finish(state);
                        return;
                    }
                } else if (state->selected[1] == 1) { //Pick from tableau or flip card
                    //store a reference to the tableau, doesn't matter if we are over them or not, it can be indexed
                    List *tbl = state->tableau[state->selected[0]];
                    //pick cards
                    if (state->hand->count == 0) {
                        Card *last = list_peek_back(tbl);
                        if(last) {
                            //Flip card if not exposed
                            if (!last->exposed) {
                                last->exposed = true;
                                check_quick_solve(state);
                                return;
                            }
                                //Pick cards
                            else {
                                for (uint8_t i = 0; i < state->selected_card && tbl->count > 0; i++) {
                                    list_push_front(list_pop_back(tbl), state->hand);
                                }
                                set_picked_from(state->selected[0], 1);
                                state->selected_card = 1;
                                return;
                            }
                        }
                        if(can_quick_solve) return;
                    }
                        //try to place hand
                    else {
                        Card *last = list_peek_back(tbl);
                        //place back from where you picked up
                        if (is_picked_from(state->selected[0], 1)) {
                            while (state->hand->count) {
                                list_push_back(list_pop_front(state->hand), tbl);
                            }
                            reset_picked();
                            return;
                        }
                            //test if the hand can be placed at one of the tableau columns
                        else if ((!last || last->exposed) && card_test_column(list_peek_front(state->hand), last)) {
                            while (state->hand->count) {
                                list_push_back(list_pop_front(state->hand), tbl);
                            }
                            reset_picked();
                            return;
                        }
                    }
                }
                break;
            default:
                return;
        }
        notification_message(state->notification_app, &sequence_fail);
    } else if (type == InputTypeLong) {
        switch (key) {
            case InputKeyLeft:
                state->selected_card = 1;
                state->selected[0] = 0;
                return;
                break;
            case InputKeyRight:
                state->selected_card = 1;
                state->selected[0] = 6;
                return;
                break;
            case InputKeyUp:
                state->selected[1] = 0;
                state->selected_card = 1;
                if (state->selected[0] == 2) state->selected[0]--;
                return;
                break;
            case InputKeyDown:
                state->selected_card = 1;
                state->selected[1] = 1;
                return;
                break;
            case InputKeyOk:
                if (can_quick_solve) {
                    end_play_screen(state);
                    return;
                }

                //try to quick place to the foundation
                if (state->hand->count == 1) {
                    Card *c = list_peek_back(state->hand);
                    for (int8_t i = 0; i < 4; i++) {
                        if (card_test_foundation(c, list_peek_back(state->foundation[i]))) {
                            list_push_back(list_pop_back(state->hand), state->foundation[i]);
                            state->selected_card = 1;
                            solved = check_finish(state);
                            return;
                        }
                    }
                }
                break;
            case InputKeyBack:
                return;
            default:
                break;
        }
        notification_message(state->notification_app, &sequence_fail);
    }
}