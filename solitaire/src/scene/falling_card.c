#include <notification/notification.h>
#include <notification/notification_messages.h>
#include "falling_card.h"
#include "../../game_state.h"
#include "play_screen.h"
#include "../util/helpers.h"

static const NotificationSequence sequence_bounce = {
    &message_vibro_on,
    //    &message_note_c4,
    &message_delay_10,
    &message_vibro_off,
    //    &message_sound_off,
    NULL,
};
static uint8_t start_index = 0;
static size_t tempTime = 0;

void start_falling_screen(void* data) {
    //draw the play screen once (without any other thing to have the initial state)
    GameState* state = (GameState*)data;
    buffer_clear(state->buffer);
    render_play_screen(data);
    state->clearBuffer = false;
    state->isDirty = true;
    start_index = 0;
    tempTime = 0;
    state->game_end = furi_get_tick();
}

void render_falling_screen(void* data) {
    GameState* state = (GameState*)data;
    if(state->animated_card.card != NULL) {
        card_render_front(
            state->animated_card.card,
            state->animated_card.position.x,
            state->animated_card.position.y,
            false,
            state->buffer,
            22);
    }
}

void update_falling_screen(void* data) {
    GameState* state = (GameState*)data;
    state->clearBuffer = false;
    state->isDirty = true;

    if(state->animated_card.card) {
        if((curr_time() - tempTime) > 12) {
            tempTime = curr_time();
            state->animated_card.position.x += state->animated_card.velocity.x;
            state->animated_card.position.y -= state->animated_card.velocity.y;

            //bounce on the bottom
            if(state->animated_card.position.y > 41) {
                state->animated_card.velocity.y *= -0.8f;
                state->animated_card.position.y = 41;
                notification_message(
                    state->notification_app, (const NotificationSequence*)&sequence_bounce);
            } else {
                state->animated_card.velocity.y--;
                if(state->animated_card.velocity.y < -10) state->animated_card.velocity.y = -10;
            }

            //delete the card if it is outside the screen
            if(state->animated_card.position.x < -18 || state->animated_card.position.x > 128) {
                free(state->animated_card.card);
                state->animated_card.card = NULL;
            }
        }

    } else {
        //When we find a foundation without any card means that we finished the animation
        if(state->foundation[start_index]->count == 0) {
            state->scene_switch = 1;
            return;
        }

        //start with the next card
        state->animated_card.card = list_pop_back(state->foundation[start_index]);
        state->animated_card.position = (Vector){56 + start_index * 18, 1};

        float r1 = 2.0 * (float)(rand() % 2) - 1.0; // random number in range -1 to 1
        if(r1 == 0) r1 = 0.1;
        float r2 = inverse_tanh(r1);
        float vx = (float)(tanh(r2)) * (rand() % 3 + 1);
        state->animated_card.velocity.x = vx == 0 ? 1 : vx;
        state->animated_card.velocity.y = (rand() % 3 + 1);

        start_index = (start_index + 1) % 4;
    }
}

void input_falling_screen(void* data, InputKey key, InputType type) {
    GameState* state = (GameState*)data;
    if(key == InputKeyOk && type == InputTypePress) {
        //remove card that is currently animated
        if(state->animated_card.card != NULL) {
            free(state->animated_card.card);
            state->animated_card.card = NULL;
        }
        state->scene_switch = 1;
    }
}
