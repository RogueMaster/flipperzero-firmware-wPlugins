#pragma once

#include <stdbool.h>

#include <gui/canvas.h>
#include <input/input.h>

#include "../fzreader_app.h"

/**
 * Draw the start menu.
 *
 * The menu is shown when no book could be resumed, so that the app does not jump
 * into the file browser on a first start or after the last book was removed.
 */
void scene_start_draw(Canvas* canvas, FzReaderApp* app);

/** Handle an input event of the start menu. @returns true if it has to be redrawn. */
bool scene_start_input(FzReaderApp* app, const InputEvent* event);

/** Draw the controls reference opened from the start menu. */
void scene_help_draw(Canvas* canvas, FzReaderApp* app);

/** Handle an input event of the controls reference. @returns true if it has to be redrawn. */
bool scene_help_input(FzReaderApp* app, const InputEvent* event);
