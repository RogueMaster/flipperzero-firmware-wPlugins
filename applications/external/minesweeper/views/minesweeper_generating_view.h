#ifndef MINESWEEPER_GENERATING_VIEW_H
#define MINESWEEPER_GENERATING_VIEW_H

#include <gui/view.h>
#include <input/input.h>

#include "engine/mine_sweeper_engine.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct MineSweeperGeneratingView MineSweeperGeneratingView;

typedef enum {
    MineSweeperGeneratingEventStartNow = 0,
} MineSweeperGeneratingEvent;

typedef void (*MineSweeperGeneratingInputCallback)(MineSweeperGeneratingEvent event, void* context);

MineSweeperGeneratingView* minesweeper_generating_view_alloc(void);
void minesweeper_generating_view_free(MineSweeperGeneratingView* instance);
View* minesweeper_generating_view_get_view(MineSweeperGeneratingView* instance);

void minesweeper_generating_view_set_context(MineSweeperGeneratingView* instance, void* context);
void minesweeper_generating_view_set_input_callback(
    MineSweeperGeneratingView* instance,
    MineSweeperGeneratingInputCallback callback);

void minesweeper_generating_view_set_stats(
    MineSweeperGeneratingView* instance,
    uint32_t attempts_total,
    uint32_t elapsed_seconds);

void minesweeper_generating_view_reset(MineSweeperGeneratingView* instance);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MINESWEEPER_GENERATING_VIEW_H
