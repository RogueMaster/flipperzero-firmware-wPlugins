#pragma once

#include <stdbool.h>

#include <gui/canvas.h>
#include <input/input.h>

#include "../fzreader_app.h"

/** Horizontal placement of the page number and the arrows around it, in pixels. */
typedef struct {
    int32_t number_left; /**< left edge of the number, it is centred */
    int32_t number_width;
    int32_t arrow_left; /**< left edge of the Left arrow */
    int32_t left_width;
    int32_t arrow_right; /**< left edge of the Right arrow */
    int32_t right_width;
} SceneInfoPageLine;

/**
 * Place the page number and the two arrows around it.
 *
 * The current canvas font is the one measured and drawn with. Both arrows keep
 * the same gap to the number and are placed by their left edge, so neither
 * covers a digit nor leaves the screen margins.
 */
void scene_info_page_line(Canvas* canvas, const char* number, SceneInfoPageLine* line);

/**
 * Open the information screen for the page being read.
 *
 * The page starts of the current chapter are built in the background while the
 * screen is drawn; Back cancels the screen at any time, a hold of Back leaves
 * the app as it does on the reading screen.
 */
void scene_info_open(FzReaderApp* app);

/** Draw the information screen. */
void scene_info_draw(Canvas* canvas, FzReaderApp* app);

/** Handle an input event. @returns true if the screen has to be redrawn. */
bool scene_info_input(FzReaderApp* app, const InputEvent* event);
