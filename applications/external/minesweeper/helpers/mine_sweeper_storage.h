#ifndef MINESWEEPER_STORAGE_H
#define MINESWEEPER_STORAGE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void mine_sweeper_save_settings(void* context);
bool mine_sweeper_read_settings(void* context);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MINESWEEPER_STORAGE_H
