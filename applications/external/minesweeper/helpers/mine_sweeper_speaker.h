#ifndef MINESWEEPER_SPEAKER_H
#define MINESWEEPER_SPEAKER_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void mine_sweeper_play_ok_sound(void* context);
void mine_sweeper_play_flag_sound(void* context);
void mine_sweeper_play_oob_sound(void* context);
void mine_sweeper_play_wrap_sound(void* context);
void mine_sweeper_play_win_sound(void* context);
void mine_sweeper_play_lose_sound(void* context);
void mine_sweeper_stop_all_sound(void* context);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MINESWEEPER_SPEAKER_H
