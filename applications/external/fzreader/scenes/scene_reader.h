#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <gui/canvas.h>
#include <input/input.h>

#include "../fzreader_app.h"

/** Draw the current page of the open book. */
void scene_reader_draw(Canvas* canvas, FzReaderApp* app);

/** Handle an input event. @returns true if the screen has to be redrawn. */
bool scene_reader_input(FzReaderApp* app, const InputEvent* event);

/**
 * Metrics the chapter is laid out with on the reading screen.
 *
 * The information screen lays the same chapter out to count its pages, so it
 * must see exactly the same line width and line budget.
 */
void scene_reader_metrics(
    Canvas* canvas,
    size_t font,
    ReaderLayoutMetrics* metrics,
    size_t* max_lines);

/** Draw a string cut to the available width, no ellipsis is added. */
void scene_reader_draw_trimmed(
    Canvas* canvas,
    const char* text,
    int32_t x,
    size_t available,
    int32_t baseline);

/** Move the reader to a position, remembering the page left behind. */
void scene_reader_move_to(FzReaderApp* app, ReaderPosition position);
