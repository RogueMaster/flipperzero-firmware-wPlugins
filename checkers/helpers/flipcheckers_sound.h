#pragma once
#include <notification/notification_messages.h>

void flipcheckers_play_move_beep(void* context);
void flipcheckers_play_win_sound(void* context);
void flipcheckers_play_lose_sound(void* context);
void flipcheckers_play_game_end_sound(void* context, uint8_t game_state, uint8_t white_mode, uint8_t black_mode);
