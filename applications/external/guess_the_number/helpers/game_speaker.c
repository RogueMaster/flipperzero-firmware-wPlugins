#include "game_speaker.h"
#include "../guess_the_number.h"

#define NOTE_INPUT   587.33f
#define NOTE_SUCCESS 880.0f
#define NOTE_ERROR   293.66f
#define NOTE_CLOSE   659.25f

void game_play_input_sound(void* context) {
    GameApp* app = context;
    if(app->speaker != 1) {
        return;
    }
    float volume = 1.0f;
    if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(30)) {
        furi_hal_speaker_start(NOTE_INPUT, volume);
    }
}

void game_play_win_sound(void* context) {
    GameApp* app = context;
    if(app->speaker != 1) {
        return;
    }
    float volume = 1.0f;
    if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(30)) {
        furi_hal_speaker_start(NOTE_SUCCESS, volume);
    }
}

void game_play_close_sound(void* context) {
    GameApp* app = context;
    if(app->speaker != 1) {
        return;
    }
    float volume = 0.8f;
    if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(30)) {
        furi_hal_speaker_start(NOTE_CLOSE, volume);
    }
}

void game_play_error_sound(void* context) {
    GameApp* app = context;
    if(app->speaker != 1) {
        return;
    }
    float volume = 0.6f;
    if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(30)) {
        furi_hal_speaker_start(NOTE_ERROR, volume);
    }
}

void game_stop_all_sound(void* context) {
    GameApp* app = context;
    if(app->speaker != 1) {
        return;
    }
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}
