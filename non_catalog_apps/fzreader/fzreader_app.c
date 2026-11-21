/**
 * @file fzreader_app.c
 * @brief Book reader application entry point.
 *
 * The app owns the ViewPort directly: it dispatches drawing and input to one of
 * the screens (the page of the open book, the information screen, the start menu
 * and the controls reference) and keeps the open book and the persisted state.
 */

#include "fzreader_app.h"

#include <stdio.h>
#include <string.h>

#include "scenes/scene_browser.h"
#include "scenes/scene_info.h"
#include "scenes/scene_reader.h"
#include "scenes/scene_start.h"

#define FZREADER_INPUT_QUEUE_SIZE 8
/**
 * Redraw period of a screen that is still counting pages, in milliseconds.
 *
 * Well above the per-frame layout budget of the information screen, so a frame
 * never runs into the next one and input has time between the frames.
 */
#define FZREADER_PROGRESS_TICK    10

static void fzreader_app_draw(Canvas* canvas, void* context) {
    FzReaderApp* app = context;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    switch(app->screen) {
    case FzReaderScreenInfo:
        scene_info_draw(canvas, app);
        break;
    case FzReaderScreenStart:
        scene_start_draw(canvas, app);
        break;
    case FzReaderScreenHelp:
        scene_help_draw(canvas, app);
        break;
    case FzReaderScreenReader:
    default:
        scene_reader_draw(canvas, app);
        break;
    }
    furi_mutex_release(app->mutex);
}

static bool fzreader_app_dispatch_input(FzReaderApp* app, const InputEvent* event) {
    switch(app->screen) {
    case FzReaderScreenInfo:
        return scene_info_input(app, event);
    case FzReaderScreenStart:
        return scene_start_input(app, event);
    case FzReaderScreenHelp:
        return scene_help_input(app, event);
    case FzReaderScreenReader:
    default:
        return scene_reader_input(app, event);
    }
}

static void fzreader_app_input(InputEvent* event, void* context) {
    FzReaderApp* app = context;

    furi_message_queue_put(app->event_queue, event, FuriWaitForever);
}

static void fzreader_app_set_book_path(FzReaderApp* app, const char* path, uint64_t size) {
    strncpy(app->state.book, path, sizeof(app->state.book) - 1);
    app->state.book[sizeof(app->state.book) - 1] = '\0';
    app->state.book_size = size;
}

/** Put the reading position at the start, or at the saved position for a resumed book. */
static void fzreader_app_enter_book(FzReaderApp* app, bool resume) {
    const size_t chapters = app->book->format->chapter_count(app->book);

    app->pos.chapter = 0;
    app->pos.offset = 0;
    app->history_count = 0;
    app->resume_check = false;

    if(resume && chapters > 0 && app->state.chapter < chapters) {
        app->pos.chapter = app->state.chapter;
        app->pos.offset = app->state.offset;
        app->resume_check = true;
    }

    if(app->page != NULL) {
        reader_page_free(app->page);
        app->page = NULL;
    }
    app->page_valid = false;

    /* Page numbers belong to the book and the font they were counted with. */
    reader_page_counter_free(app->page_counts);
    app->page_counts = NULL;
    reader_page_index_free(app->page_index);
    app->page_index = NULL;
    app->screen = FzReaderScreenReader;
    app->info_pending = false;
    app->info_error = false;
}

/**
 * Open the book from the previous run.
 *
 * A missing book or a different size means the book was removed or replaced:
 * that is a normal scenario and the app falls back to the start menu.
 *
 * @returns true if a book was opened.
 */
static bool fzreader_app_resume(FzReaderApp* app) {
    if(app->state.book[0] == '\0') {
        return false;
    }

    uint64_t size = 0;
    if(!fzreader_state_file_size(app->state.book, &size)) {
        return false;
    }
    if(size != app->state.book_size) {
        return false;
    }

    app->book = reader_book_open(app->state.book);
    if(app->book == NULL) {
        scene_browser_show_error("Reader", "Cannot open\nthe last book");
        return false;
    }

    fzreader_app_enter_book(app, true);
    return true;
}

/**
 * Release everything that points into the open book, then close the book.
 *
 * The page and the layout stream point into the book, so both are dropped
 * before it is closed. This is the only place the book is released: opening
 * another one goes through it too, so a book never lives next to its successor.
 */
static void fzreader_app_close_book(FzReaderApp* app) {
    if(app->page != NULL) {
        reader_page_free(app->page);
        app->page = NULL;
    }
    app->page_valid = false;
    /* The layout may still hold the chapter of the book being closed. */
    reader_layout_reset();
    reader_page_counter_free(app->page_counts);
    app->page_counts = NULL;
    reader_page_index_free(app->page_index);
    app->page_index = NULL;

    if(app->book != NULL && app->book->format != NULL && app->book->format->close != NULL) {
        app->book->format->close(app->book);
    }
    app->book = NULL;
    app->info_pending = false;
}

/**
 * Close the open book, if any, and make `book` the one being read from now on.
 *
 * The saved position belongs to the previous book and is replaced by the start
 * of the new one.
 */
static void fzreader_app_replace_book(
    FzReaderApp* app,
    ReaderBook* book,
    const char* path,
    uint64_t size,
    bool measured) {
    fzreader_app_close_book(app);
    app->book = book;

    if(measured) {
        fzreader_app_set_book_path(app, path, size);
    } else {
        /* Without a size the book can never be resumed: do not remember it. */
        app->state.book[0] = '\0';
        app->state.book_size = 0;
    }

    fzreader_app_enter_book(app, false);
}

bool fzreader_app_pick_book(FzReaderApp* app) {
    FuriString* path = furi_string_alloc();
    FuriString* folder = furi_string_alloc();

    /* The browser opens in the folder of the book being read, if it is known. */
    scene_browser_folder(folder, app->state.book);

    /*
     * The file browser lives in another thread and is drawn by the GUI thread,
     * which waits for the model mutex: drop it while the dialog is on screen.
     */
    furi_mutex_release(app->mutex);
    bool selected = scene_browser_pick(path, folder);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    furi_string_free(folder);

    if(!selected) {
        /* A cancelled dialog leaves the open book, its position and the screen alone. */
        furi_string_free(path);
        return false;
    }

    uint64_t size = 0;
    bool measured = fzreader_state_file_size(furi_string_get_cstr(path), &size);

    /*
     * The book that is already open, picked again: that is a cancel too. Reopening
     * it would drop the position and start the book from its first page.
     */
    if(app->book != NULL && measured && app->state.book[0] != '\0' &&
       size == app->state.book_size && strcmp(app->state.book, furi_string_get_cstr(path)) == 0) {
        furi_string_free(path);
        return false;
    }

    /*
     * Remember the book being read: the new one may fail to open, and the reader
     * then goes back to where it was instead of to the start menu.
     */
    char previous_path[FZREADER_STATE_BOOK_MAX];
    strncpy(previous_path, app->state.book, sizeof(previous_path) - 1);
    previous_path[sizeof(previous_path) - 1] = '\0';
    const uint64_t previous_size = app->state.book_size;
    const ReaderPosition previous_pos = app->pos;
    const bool had_book = app->book != NULL;

    /*
     * One book at a time: the old one is released before the new one is parsed,
     * so a single inflate window and a single set of spine structures live in
     * memory. Held under the mutex, as the GUI thread may be drawing the page.
     */
    if(had_book) {
        fzreader_app_close_book(app);
    }

    /*
     * Parsing the package document takes tens to hundreds of milliseconds and
     * needs nothing from the model: the GUI thread must not wait for it. The
     * event loop is here, so nothing else writes the model meanwhile.
     */
    furi_mutex_release(app->mutex);
    ReaderBook* book = reader_book_open(furi_string_get_cstr(path));
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    if(book != NULL) {
        fzreader_app_replace_book(app, book, furi_string_get_cstr(path), size, measured);
        furi_string_free(path);
        return true;
    }

    /* The new book could not be read: reopen the old one at the saved position. */
    ReaderBook* previous = NULL;
    if(had_book && previous_path[0] != '\0') {
        furi_mutex_release(app->mutex);
        previous = reader_book_open(previous_path);
        furi_mutex_acquire(app->mutex, FuriWaitForever);
    }

    if(previous != NULL) {
        fzreader_app_replace_book(app, previous, previous_path, previous_size, true);
        /* The stored position belongs to this book: resume it from there. */
        app->state.chapter = previous_pos.chapter;
        app->state.offset = previous_pos.offset;
        fzreader_app_enter_book(app, true);
    } else {
        /* Neither book could be opened: the start menu is all that is left. */
        app->screen = FzReaderScreenStart;
    }

    furi_string_free(path);
    furi_mutex_release(app->mutex);
    scene_browser_show_error("Reader", "Cannot open\nthis book");
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    return false;
}

static void fzreader_app_save(FzReaderApp* app) {
    if(app->book == NULL) {
        return;
    }

    /* The anchor is the first line of the page being read. */
    app->state.chapter = app->pos.chapter;
    app->state.offset = app->pos.offset;
    fzreader_state_save(&app->state);
}

static FzReaderApp* fzreader_app_alloc(void) {
    FzReaderApp* app = malloc(sizeof(FzReaderApp));
    furi_check(app != NULL);
    memset(app, 0, sizeof(FzReaderApp));

    app->event_queue = furi_message_queue_alloc(FZREADER_INPUT_QUEUE_SIZE, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->font_hold_key = InputKeyMAX;
    app->running = true;

    fzreader_state_load(&app->state);

    return app;
}

static void fzreader_app_free(FzReaderApp* app) {
    fzreader_app_close_book(app);

    if(app->gui != NULL) {
        furi_record_close(RECORD_GUI);
    }

    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t fzreader_app(void* p) {
    UNUSED(p);

    FzReaderApp* app = fzreader_app_alloc();

    /*
     * Only a resumed book is opened here. Anything else, a first start or a
     * book that was removed, replaced or became unreadable, shows the start
     * menu instead of falling straight into the file browser.
     */
    if(!fzreader_app_resume(app)) {
        app->screen = FzReaderScreenStart;
    }

    view_port_draw_callback_set(app->view_port, fzreader_app_draw, app);
    view_port_input_callback_set(app->view_port, fzreader_app_input, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InputEvent event;
    while(app->running) {
        /*
         * While the information screen still lays text out, the app wakes up
         * regularly and asks for a redraw: the drawing itself has to run in
         * the GUI thread and is chunked there.
         */
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        const bool pending = app->info_pending;
        furi_mutex_release(app->mutex);
        const uint32_t timeout = pending ? furi_ms_to_ticks(FZREADER_PROGRESS_TICK) :
                                           FuriWaitForever;

        if(furi_message_queue_get(app->event_queue, &event, timeout) != FuriStatusOk) {
            if(pending) {
                view_port_update(app->view_port);
            }
            continue;
        }

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool redraw = fzreader_app_dispatch_input(app, &event);
        furi_mutex_release(app->mutex);

        if(redraw) {
            view_port_update(app->view_port);
        }
    }

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    fzreader_app_save(app);
    furi_mutex_release(app->mutex);

    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    fzreader_app_free(app);

    return 0;
}
