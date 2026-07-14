#include "./intro_animation.h"

#include <dolphin/dolphin.h>

#include "../../game_state.h"
#include "../util/helpers.h"
#include "play_screen.h"

static bool animation_running = true;
static int8_t curr_tableau = 0;
static Vector animation_target = VECTOR_ZERO;
static Vector animation_from = VECTOR_ZERO;
static double accumulated_delta = 0;

void start_animation(GameState* state) {
    accumulated_delta = 0;
    check_pointer(state->deck->tail);
    state->animated_card.card = list_peek_back(state->deck);
    check_pointer(state->animated_card.card);
    animation_from = (Vector){2, 1};
    animation_target.x = 2.0f + (float)curr_tableau * 18;
    check_pointer(state->tableau[curr_tableau]);
    animation_target.y = MIN(25.0f + (float)state->tableau[curr_tableau]->count * 4, 36);
}

void start_intro_screen(void* data) {
    curr_tableau = 0;
    animation_running = true;
    dolphin_deed(DolphinDeedPluginGameStart);
    GameState* state = (GameState*)data;
    check_pointer(state->deck);
    list_free(state->deck);
    check_pointer(state->hand);
    list_free_data(state->hand);
    check_pointer(state->waste);
    list_free_data(state->waste);
    for(int i = 0; i < 7; i++) {
        if(i < 4) {
            check_pointer(state->foundation[i]);
            list_free_data(state->foundation[i]);
        }
        check_pointer(state->tableau[i]);
        list_free_data(state->tableau[i]);
    }

    state->deck = deck_generate(1);
    check_pointer(state->deck->tail);
    start_animation(state);
}

void render_intro_screen(void* data) {
    GameState* state = (GameState*)data;
    render_play_screen(data);

    if(state->animated_card.card) {
        card_render_back(
            state->animated_card.position.x,
            state->animated_card.position.y,
            false,
            state->buffer,
            22);
    }
}

static bool animation_done(GameState* state) {
    accumulated_delta += state->delta_time * 4;
    vector_lerp(
        &(animation_from), &animation_target, accumulated_delta, &(state->animated_card.position));
    double dist = vector_distance(&(state->animated_card.position), &animation_target);
    return dist < 1;
}

void update_intro_screen(void* data) {
    GameState* state = (GameState*)data;
    state->isDirty = true;
    if(curr_tableau < 7 && animation_running) {
        if(animation_done(state)) {
            if(curr_tableau < 7) {
                check_pointer(state->deck);
                check_pointer(state->deck->tail);
                check_pointer(state->tableau);
                check_pointer(state->tableau[curr_tableau]);
                list_push_back(list_pop_back(state->deck), state->tableau[curr_tableau]);
                if((curr_tableau + 1) == (uint8_t)state->tableau[curr_tableau]->count) {
                    Card* c = ((Card*)list_peek_back(state->tableau[curr_tableau]));
                    check_pointer(c);
                    c->exposed = true;
                    curr_tableau++;
                }
                start_animation(state);
            }
        }
    } else {
        state->animated_card.card = NULL;
        state->scene_switch = 1;
        return;
    }
}

static void quick_finish(GameState* state) {
    if(state->animated_card.card) {
        list_push_back(list_pop_back(state->deck), state->tableau[curr_tableau]);
    }

    while(curr_tableau < 7) {
        while((uint8_t)(state->tableau[curr_tableau]->count) < (curr_tableau + 1)) {
            list_push_back(list_pop_back(state->deck), state->tableau[curr_tableau]);
        }
        ((Card*)list_peek_back(state->tableau[curr_tableau]))->exposed = true;
        curr_tableau++;
    }
    animation_running = false;
}

void input_intro_screen(void* data, InputKey key, InputType type) {
    GameState* state = (GameState*)data;
    if(key == InputKeyOk && type == InputTypePress) {
        quick_finish(state);
        animation_running = false;
    }
}
