/**
 * @file minesweeper_game_screen_screen.h
 * GUI: Start Screen view module API
 */

#ifndef MINESWEEPER_GAME_SCREEN_H
#define MINESWEEPER_GAME_SCREEN_H

#include <gui/view.h>
#include <input/input.h>
#include "engine/mine_sweeper_engine.h"

// These defines represent how many tiles
// can be visually representen on the screen
// due to icon sizes
#define MINESWEEPER_SCREEN_TILE_HEIGHT 7
#define MINESWEEPER_SCREEN_TILE_WIDTH  16

#define MS_DEBUG_TAG "Mine Sweeper Module/View"

typedef enum {
    MineSweeperEventMoveUp,
    MineSweeperEventMoveDown,
    MineSweeperEventMoveRight,
    MineSweeperEventMoveLeft,
    MineSweeperEventShortOkPress,
    MineSweeperEventLongOkPress,
    MineSweeperEventBackLong,
} MineSweeperEvent;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/** MineSweeperGameScreen anonymous structure */
typedef struct MineSweeperGameScreen MineSweeperGameScreen;

/** MineSweeperGameScreen callback types
 * @warning     comes from GUI thread
 */
typedef void (*GameScreenInputCallback)(MineSweeperEvent event, void* context);

/** Allocate and initalize
 *
 * This view is used as the game screen of an application.
 */
MineSweeperGameScreen* mine_sweeper_game_screen_alloc(void);

/** Deinitialize and free Game Screen view
 *
 * @param       instsance   MineSweeperGameScreen instance
 */
void mine_sweeper_game_screen_free(MineSweeperGameScreen* instance);

/** Reset MineSweeperGameScreen
 * Not sure if necessary
 */
void mine_sweeper_game_screen_reset(MineSweeperGameScreen* instance);

void mine_sweeper_game_screen_reset_clock(MineSweeperGameScreen* instance);
void mine_sweeper_game_screen_update_clock(MineSweeperGameScreen* instance);

/** Get MineSweeperGameScreen view
 *
 * @param       instance    MineSweeperGameScreen instance
 *
 * @return      View* instance that can be used for embedding
 */
View* mine_sweeper_game_screen_get_view(MineSweeperGameScreen* instance);

void mine_sweeper_game_screen_set_input_callback(
    MineSweeperGameScreen* instance,
    GameScreenInputCallback callback,
    void* context);

/** Set MineSweeperGameScreen context 
 *
 * @param       instance    MineSweeperGameScreen* instance
 * @param       context     void* context for MineSweeperGameScreen instance 
 */
void mine_sweeper_game_screen_set_context(
    MineSweeperGameScreen* instance,
    MineSweeperState* context);

#define inverted_canvas_white_to_black(canvas, code) \
    {                                                \
        canvas_set_color(canvas, ColorWhite);        \
        {code};                                      \
        canvas_set_color(canvas, ColorBlack);        \
    }

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MINESWEEPER_GAME_SCREEN_H
