#include "flipcheckers_sound.h"
#include "../flipcheckers.h"
#include "../checkers/smallcheckerslib.h"

// Short neutral beep on every move
static const NotificationSequence seq_move_beep = {
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

// Triumphant rising fanfare: G4 -> C5 -> E5 -> G5 -> C6, then E5 -> G5 -> C6 held
static const NotificationSequence seq_win = {
    &message_note_g4,
    &message_delay_100,
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_note_c6,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_note_c6,
    &message_delay_500,
    &message_sound_off,
    NULL,
};

// Descending sad tone: G4 -> E4 -> C4
static const NotificationSequence seq_lose = {
    &message_note_g4,
    &message_delay_100,
    &message_note_e4,
    &message_delay_100,
    &message_note_c4,
    &message_delay_250,
    &message_sound_off,
    NULL,
};

void flipcheckers_play_move_beep(void* context) {
    FlipCheckers* app = context;
    if(app->sound != FlipCheckersSoundOn) return;
    notification_message(app->notification, &seq_move_beep);
}

void flipcheckers_play_win_sound(void* context) {
    FlipCheckers* app = context;
    if(app->sound != FlipCheckersSoundOn) return;
    notification_message(app->notification, &seq_win);
}

void flipcheckers_play_lose_sound(void* context) {
    FlipCheckers* app = context;
    if(app->sound != FlipCheckersSoundOn) return;
    notification_message(app->notification, &seq_lose);
}

void flipcheckers_play_game_end_sound(
    void* context,
    uint8_t game_state,
    uint8_t white_mode,
    uint8_t black_mode) {
    bool white_human = (white_mode == FlipCheckersPlayerHuman);
    bool black_human = (black_mode == FlipCheckersPlayerHuman);
    bool one_human   = (white_human != black_human);
    if(!one_human) return;

    if((game_state == SCL_GAME_STATE_WHITE_WIN && white_human) ||
       (game_state == SCL_GAME_STATE_BLACK_WIN && black_human)) {
        flipcheckers_play_win_sound(context);
    } else if(game_state == SCL_GAME_STATE_WHITE_WIN ||
              game_state == SCL_GAME_STATE_BLACK_WIN) {
        flipcheckers_play_lose_sound(context);
    }
}
