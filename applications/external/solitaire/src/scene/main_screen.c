#include <notification/notification_messages.h>
#include "main_screen.h"

static bool is_dirty = false;
static size_t last_start = 0;
static int8_t note = 0;
static const float VOLUME = 0.25f;

static const NotificationMessage note_e4 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound.frequency = 329.63f,
    .data.sound.volume = VOLUME,
};

static const NotificationMessage note_g4 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound.frequency = 392.0f,
    .data.sound.volume = VOLUME,
};

static const NotificationMessage note_c5 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound.frequency = 523.25f,
    .data.sound.volume = VOLUME,
};

static const NotificationMessage note_d4 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound.frequency = 293.66f,
    .data.sound.volume = VOLUME,
};

static const NotificationMessage note_f4 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound.frequency = 349.23f,
    .data.sound.volume = VOLUME,
};

static const NotificationMessage note_b4 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound.frequency = 493.88f,
    .data.sound.volume = VOLUME,
};

static const NotificationMessage* music_notes[] = {
    &note_e4, &note_g4, &note_c5, &message_sound_off, &note_g4, &message_sound_off,

    &note_e4, &note_g4, &note_c5, &message_sound_off, &note_g4, &message_sound_off,

    &note_d4, &note_f4, &note_b4, &message_sound_off, &note_f4, &message_sound_off,

    &note_d4, &note_f4, &note_b4, &message_sound_off, &note_f4, &message_sound_off,
};

static NotificationSequence music = {NULL, &message_delay_250, &message_sound_off, NULL};

void start_main_screen(void* data) {
    is_dirty = true;
    GameState* state = (GameState*)data;
    last_start = 0;
    note = -1;
    state->lateRender = false;
    state->isDirty = true;
    state->clearBuffer = true;
}

void render_main_screen(void* data) {
    GameState* state = (GameState*)data;
    Vector logo_pos = (Vector){60, 30};
    Vector main_img_pos = (Vector){115, 25};
    Vector start_text_pos = (Vector){64, 55};
    buffer_draw_all(state->buffer, (Buffer*)&sprite_logo, &logo_pos, 0);
    buffer_draw_all(state->buffer, (Buffer*)&sprite_main_image, &main_img_pos, 0);
    buffer_draw_all(state->buffer, (Buffer*)&sprite_start, &start_text_pos, 0);
}

void update_main_screen(void* data) {
    GameState* state = (GameState*)data;
    UNUSED(state);
    size_t t = curr_time();
    //Play the menu music one note at a time to not block the app
    if((last_start - t) > 250) {
        if(note >= 0) {
            music[0] = music_notes[note];

            notification_message_block(
                state->notification_app, (const NotificationSequence*)&music);

            last_start = t;
        }
        note = (note + 1) % 24;
    }

    state->isDirty = is_dirty;
    is_dirty = false;
}

void input_main_screen(void* data, InputKey key, InputType type) {
    GameState* state = (GameState*)data;

    if(key == InputKeyOk && type == InputTypePress) {
        state->scene_switch = 1;
    }
}
