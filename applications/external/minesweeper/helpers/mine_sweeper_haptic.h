#ifndef MINESWEEPER_HAPTIC_H
#define MINESWEEPER_HAPTIC_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void mine_sweeper_play_haptic_short(void* context);

void mine_sweeper_play_haptic_double_short(void* context);

void mine_sweeper_play_haptic_lose(void* context);

void mine_sweeper_play_haptic_win(void* context);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MINESWEEPER_HAPTIC_H
