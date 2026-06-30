#include "solve_screen.h"
#include "../../game_state.h"
#include "play_screen.h"

uint8_t target_foundation;
static double accumulated_delta = 0;
static Vector animation_target = VECTOR_ZERO;
static Vector animation_from = VECTOR_ZERO;

void start_solve_screen(void* data) {
    UNUSED(data);
    accumulated_delta = 0;
    target_foundation = 0;
}

void render_solve_screen(void* data) {
    GameState* state = (GameState*)data;

    render_play_screen(state);

    if(state->animated_card.card) {
        card_render_front(
            state->animated_card.card,
            (uint8_t)state->animated_card.position.x,
            (uint8_t)state->animated_card.position.y,
            false,
            state->buffer,
            22);
    }
}

bool end_solve_screen(GameState* state) {
    for(uint8_t i = 0; i < 4; i++) {
        Card* c = list_peek_back(state->foundation[i]);
        if(!c || c->value != KING) return false;
    }

    return true;
}

static int8_t missing_suit(GameState* state) {
    bool found[4] = {false, false, false, false};
    for(uint8_t i = 0; i < 4; i++) {
        if(state->foundation[i]->count) {
            Card* c = list_peek_back(state->foundation[i]);
            found[c->suit] = true;
        }
    }
    for(int8_t i = 0; i < 4; i++) {
        if(!found[i]) return i;
    }

    return -1;
}

static void quick_solve(GameState* state) {
    //remove all cards that are not placed to the foundation yet
    list_free_data(state->deck);
    list_free_data(state->waste);
    for(uint8_t i = 0; i < 7; i++) {
        list_free_data(state->tableau[i]);
    }
    list_free_data(state->hand);

    state->animated_card.card = NULL;

    //fill up the foundations with cards
    for(uint8_t i = 0; i < 4; i++) {
        //add ace to the start
        if(state->foundation[i]->count == 0) {
            Card* c = malloc(sizeof(Card));
            c->value = ACE;
            c->suit = missing_suit(state);
            c->exposed = true;
            list_push_back(c, state->foundation[i]);
        }

        //fill up the rest
        for(uint8_t v = state->foundation[i]->count; v < 13; v++) {
            Card* c = malloc(sizeof(Card));
            c->value = v - 1;
            c->suit = ((Card*)list_peek_back(state->foundation[i]))->suit;
            c->exposed = true;
            list_push_back(c, state->foundation[i]);
        }
    }

    end_solve_screen(state);
}

static bool animation_done(GameState* state) {
    if(!state->animated_card.card) return true;

    accumulated_delta += state->delta_time * 4;
    vector_lerp(
        &(animation_from), &animation_target, accumulated_delta, &(state->animated_card.position));

    double dist = vector_distance(&(state->animated_card.position), &animation_target);
    return dist < 1;
}

static Card* find_and_remove(List* list, uint8_t suit, uint8_t value) {
    //go reversed order because tableau will always have at the end
    ListItem* current = list->tail;
    while(current) {
        if(((Card*)current->data)->value == value && ((Card*)current->data)->suit == suit) {
            Card* c = current->data;
            list_remove_item(c, list);
            return c;
        }
        current = current->prev;
    }

    return NULL;
}

static uint8_t positions[9] = {2, 20, 2, 20, 38, 56, 74, 92, 110};

static void find_next_card(GameState* state) {
    List* order[9] = {
        state->deck,
        state->waste,
        state->tableau[0],
        state->tableau[1],
        state->tableau[2],
        state->tableau[3],
        state->tableau[4],
        state->tableau[5],
        state->tableau[6]};

    int8_t missing = missing_suit(state);
    if(missing >= 0) {
        //find the missing ACE
        for(uint8_t id = 0; id < 9; id++) {
            Card* ace = find_and_remove(order[id], missing, ACE);
            if(ace) {
                ace->exposed = true;
                for(uint8_t i = 0; i < 4; i++) {
                    if(state->foundation[i]->count == 0) {
                        state->animated_card.card = ace;
                        target_foundation = i;
                        animation_from.x = positions[id];
                        animation_from.y =
                            id < 2 ? 1 :
                                     ((float)MIN(
                                          (uint8_t)state->tableau[state->selected[0]]->count, 4) *
                                          4 +
                                      15);
                        state->animated_card.position = animation_from;

                        animation_target.x = (float)(56 + target_foundation * 18);
                        animation_target.y = 1;
                    }
                }
            }
        }
    } else {
        uint8_t lowestValue = 14, lowestSuit = 0;
        //get the lowest value
        for(uint8_t i = 0; i < 4; i++) {
            Card* c = list_peek_back(state->foundation[i]);
            if(lowestValue > ((c->value + 1) % 13)) {
                lowestValue = ((c->value + 1) % 13);
                target_foundation = i;
                lowestSuit = c->suit;
            }
        }
        //store that card to animate
        for(uint8_t id = 0; id < 9; id++) {
            Card* c = find_and_remove(order[id], lowestSuit, lowestValue);
            if(c) {
                state->animated_card.card = c;

                animation_from.x = positions[id];
                animation_from.y =
                    id < 2 ?
                        1 :
                        ((float)MIN((uint8_t)state->tableau[state->selected[0]]->count, 4) * 4 +
                         15);
                state->animated_card.position = animation_from;

                animation_target.x = (float)(56 + target_foundation * 18);
                animation_target.y = 1;
                break;
            }
        }
    }
}

void update_solve_screen(void* data) {
    GameState* state = (GameState*)data;
    state->isDirty = true;

    if(!end_solve_screen(state)) {
        if(animation_done(state)) {
            if(state->animated_card.card) {
                state->animated_card.card->exposed = true;
                list_push_back(state->animated_card.card, state->foundation[target_foundation]);
                state->animated_card.card = NULL;
                accumulated_delta = 0;
            }
            find_next_card(state);
        }
    } else {
        state->scene_switch = 1;
    }
}

void input_solve_screen(void* data, InputKey key, InputType type) {
    if(key == InputKeyOk && type == InputTypePress) {
        quick_solve(data);
    }
}
