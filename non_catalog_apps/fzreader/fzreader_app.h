#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#include "fzreader_state.h"
#include "formats/reader_book.h"
#include "text/layout.h"
#include "text/page_index.h"

/** Positions kept for the "back" navigation. */
#define FZREADER_HISTORY_SIZE 32

/** Number of items of the start menu. */
#define FZREADER_START_ITEMS 2

/** Screen the app dispatches drawing and input to. */
typedef enum {
    FzReaderScreenReader, /**< the page of the open book */
    FzReaderScreenInfo, /**< book information and page chooser */
    FzReaderScreenStart, /**< start menu, shown when no book could be resumed */
    FzReaderScreenHelp, /**< controls reference, opened from the start menu */
} FzReaderScreen;

typedef struct FzReaderApp FzReaderApp;

/** Whole application state. Shared by the app, the reader scene and the browser scene. */
struct FzReaderApp {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriMutex* mutex; /**< guards the model, drawing happens in the GUI thread */

    ReaderBook* book;
    ReaderPage* page; /**< the only laid out page kept in memory */
    bool page_valid; /**< page matches pos and the current screen metrics */
    size_t page_font; /**< font index the page was laid out with */
    size_t page_max_lines; /**< line budget the page was laid out with */
    ReaderPosition pos; /**< anchor of the current page */

    bool resume_check; /**< the first page is laid out from the saved state */

    InputKey font_hold_key; /**< key that already stepped the font, InputKeyMAX if none */

    FzReaderScreen screen; /**< screen shown by the app */
    size_t start_selection; /**< highlighted item of the start menu */

    bool info_pending; /**< the information screen still has pages to lay out */
    bool info_error; /**< the information screen cannot build its index, only Back works */
    ReaderPageIndex* page_index; /**< page starts of the chapter shown by the info screen */
    ReaderPageCounter* page_counts; /**< pages per chapter, NULL until counted */
    ReaderPosition info_initial_pos; /**< reader position the information screen was opened at */
    size_t info_initial_page; /**< page of info_initial_pos, known once its index is ready */
    size_t info_chapter; /**< chapter selected on the information screen */
    size_t info_page; /**< page inside info_chapter, 0 based */
    bool info_page_at_pos; /**< info_page is taken from app->pos when the index is built */
    bool info_page_at_end; /**< info_page is the last page of the chapter when the index is built */
    size_t info_hold; /**< repeats of the current Left/Right hold, for the fast forward */

    FzReaderState state;

    ReaderPosition history[FZREADER_HISTORY_SIZE]; /**< anchors of the pages left behind */
    size_t history_count;

    bool running;
};

/** Application entry point. */
int32_t fzreader_app(void* p);

/**
 * Pick a book with the system file browser and open it, replacing the current
 * one. The app stays where it is when the dialog is cancelled or the same book
 * is picked again.
 *
 * The open book is closed before the new one is opened, so only one book is
 * parsed and held in memory at a time. When the new book cannot be opened the
 * previous one is reopened at the saved position; if that fails too, the app
 * falls back to the start menu.
 *
 * The model mutex must be held by the caller: the dialog runs in another thread
 * and is drawn by the GUI thread, so the mutex is dropped while it is on screen
 * and while a book is parsed.
 *
 * @returns true if another book was opened.
 */
bool fzreader_app_pick_book(FzReaderApp* app);
