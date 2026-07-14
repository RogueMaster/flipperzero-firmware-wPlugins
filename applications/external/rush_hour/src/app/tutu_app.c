#include "../../include/app/tutu_app.h"
#include "../../include/data/levels.h"
#include "../../include/domain/board.h"
#include "../../include/persistence/progress.h"
#include "../../include/platform/storage_port.h"
#include "../../include/version.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#define BX   2
#define BY   5
#define CELL 9

typedef enum {
    ScreenMenu,
    ScreenGame,
    ScreenWin,
    ScreenCredits
} Screen;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    FuriMutex* mutex;
    bool running;

    Screen screen;
    TutuProgress progress;

    uint16_t level_index; // current level
    uint16_t menu_cursor; // 0..99, selected level in the menu grid
    TutuBoard board;
    uint8_t selected; // highlighted piece index
    uint16_t moves;
    uint8_t blink; // frame counter for the highlight blink
} TutuApp;

// ---- game logic helpers ----

static void load_level(TutuApp* app, uint16_t index) {
    const TutuLevel* l = tutu_levels_get(index);
    tutu_board_init(&app->board, l->pieces, l->count);
    app->level_index = index;
    app->selected = TUTU_RED;
    app->moves = 0;
    app->screen = ScreenGame;
}

// ---- rendering ----

static void draw_cell_rect(Canvas* c, const TutuPiece* p, int* x, int* y, int* w, int* h) {
    *x = BX + p->c * CELL;
    *y = BY + p->r * CELL;
    *w = (p->o == TUTU_H ? p->len : 1) * CELL;
    *h = (p->o == TUTU_V ? p->len : 1) * CELL;
    UNUSED(c);
}

static void draw_game(Canvas* canvas, TutuApp* app) {
    const int bw = TUTU_SIZE * CELL;
    const int rx = BX + bw; // board right edge
    const int ex_top = BY + TUTU_EXIT_ROW * CELL; // exit cell top
    const int ex_bot = BY + (TUTU_EXIT_ROW + 1) * CELL; // exit cell bottom

    // Board frame, left OPEN on the right at the exit row so the way out is obvious.
    canvas_draw_line(canvas, BX - 1, BY - 1, rx, BY - 1); // top
    canvas_draw_line(canvas, BX - 1, BY + bw, rx, BY + bw); // bottom
    canvas_draw_line(canvas, BX - 1, BY - 1, BX - 1, BY + bw); // left
    canvas_draw_line(canvas, rx, BY - 1, rx, ex_top); // right, above exit
    canvas_draw_line(canvas, rx, ex_bot, rx, BY + bw); // right, below exit

    // Bold arrow pointing out through the gap — "the red car leaves here".
    const int ay = ex_top + CELL / 2;
    canvas_draw_box(canvas, rx - 1, ay - 1, 7, 3); // thick shaft
    for(int t = -1; t <= 1; t++) { // 3px-thick arrow head
        canvas_draw_line(canvas, rx + 3, ay - 4, rx + 7, ay + t);
        canvas_draw_line(canvas, rx + 3, ay + 4, rx + 7, ay + t);
    }

    for(uint8_t i = 0; i < app->board.count; i++) {
        int x, y, w, h;
        draw_cell_rect(canvas, &app->board.pieces[i], &x, &y, &w, &h);
        if(i == TUTU_RED)
            canvas_draw_rbox(canvas, x + 1, y + 1, w - 2, h - 2, 2); // red car (solid)
        else
            canvas_draw_rframe(canvas, x + 1, y + 1, w - 2, h - 2, 2); // other cars
        if(i == app->selected && (app->blink & 4))
            canvas_draw_rframe(canvas, x, y, w, h, 2); // blinking selection
    }

    // HUD column, well clear of the board and the exit arrow.
    const int hx = rx + 12;
    char buf[16];
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "Lvl %u", (unsigned)(app->level_index + 1));
    canvas_draw_str(canvas, hx, 12, buf);
    snprintf(buf, sizeof(buf), "Moves %u", (unsigned)app->moves);
    canvas_draw_str(canvas, hx, 26, buf);
    canvas_draw_str(canvas, hx, 48, "OK: car");
    canvas_draw_str(canvas, hx, 60, "hold: rst");
}

static void draw_win(Canvas* canvas, TutuApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Solved!");
    char buf[32];
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        buf,
        sizeof(buf),
        "Level %u in %u moves",
        (unsigned)(app->level_index + 1),
        (unsigned)app->moves);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, buf);
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignCenter, "OK: next  Back: menu");
}

static void draw_menu(Canvas* canvas, TutuApp* app); // defined below
static void draw_credits(Canvas* canvas, TutuApp* app); // defined below

static void render_cb(Canvas* canvas, void* ctx) {
    TutuApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    switch(app->screen) {
    case ScreenMenu:
        draw_menu(canvas, app);
        break;
    case ScreenGame:
        draw_game(canvas, app);
        break;
    case ScreenWin:
        draw_win(canvas, app);
        break;
    case ScreenCredits:
        draw_credits(canvas, app);
        break;
    }
    furi_mutex_release(app->mutex);
}

static void input_cb(InputEvent* event, void* ctx) {
    TutuApp* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

// ---- input handling per screen ----

static void handle_game_input(TutuApp* app, InputEvent* e); // below
static void handle_menu_input(TutuApp* app, InputEvent* e); // below
static void handle_win_input(TutuApp* app, InputEvent* e); // below
static void handle_credits_input(TutuApp* app, InputEvent* e); // below

static void handle_game_input(TutuApp* app, InputEvent* e) {
    // Back (short or long) returns to the level select, cursor on this level.
    if(e->key == InputKeyBack && (e->type == InputTypeShort || e->type == InputTypeLong)) {
        app->menu_cursor = app->level_index;
        app->screen = ScreenMenu;
        return;
    }
    // Long OK resets the current level.
    if(e->type == InputTypeLong && e->key == InputKeyOk) {
        load_level(app, app->level_index);
        return;
    }
    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return;

    TutuPiece* sel = &app->board.pieces[app->selected];
    switch(e->key) {
    case InputKeyOk:
        if(e->type == InputTypeShort) // cycle on tap only, never on auto-repeat
            app->selected = tutu_board_next_piece(&app->board, app->selected);
        break;
    case InputKeyLeft:
        if(sel->o == TUTU_H && tutu_board_move(&app->board, app->selected, -1)) app->moves++;
        break;
    case InputKeyRight:
        if(sel->o == TUTU_H && tutu_board_move(&app->board, app->selected, +1)) app->moves++;
        break;
    case InputKeyUp:
        if(sel->o == TUTU_V && tutu_board_move(&app->board, app->selected, -1)) app->moves++;
        break;
    case InputKeyDown:
        if(sel->o == TUTU_V && tutu_board_move(&app->board, app->selected, +1)) app->moves++;
        break;
    default:
        return;
    }
    if(tutu_board_won(&app->board)) {
        tutu_progress_complete_and_unlock(&app->progress, app->level_index, tutu_levels_count());
        tutu_storage_save_progress(&app->progress);
        app->screen = ScreenWin;
    }
}

// ---- lifecycle ----

static TutuApp* app_alloc(void) {
    TutuApp* app = malloc(sizeof(TutuApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->running = true;
    app->blink = 0;

    if(!tutu_storage_load_progress(&app->progress)) tutu_progress_default(&app->progress);

    view_port_draw_callback_set(app->view_port, render_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port); // force first draw — do NOT rely on the Apps-menu
        // loader animation (blank-UI-from-favourites bug)

    app->menu_cursor = app->progress.highest_unlocked;
    app->screen = ScreenMenu;
    // do not pre-load a level here; load happens on OK from the menu
    return app;
}

static void app_free(TutuApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t tutu_app_run(void) {
    TutuApp* app = app_alloc();
    InputEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->input_queue, &event, 50) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            // global exit: long Back from menu
            if(app->screen == ScreenMenu && event.type == InputTypeShort &&
               event.key == InputKeyBack) {
                app->running = false;
            } else {
                switch(app->screen) {
                case ScreenGame:
                    handle_game_input(app, &event);
                    break;
                case ScreenMenu:
                    handle_menu_input(app, &event);
                    break;
                case ScreenWin:
                    handle_win_input(app, &event);
                    break;
                case ScreenCredits:
                    handle_credits_input(app, &event);
                    break;
                }
            }
            furi_mutex_release(app->mutex);
        }
        app->blink++;
        view_port_update(app->view_port);
    }
    app_free(app);
    return 0;
}

// ---- menu (level select) ----

#define MENU_COLS     10
#define MENU_ROWS     5
#define MENU_PER_PAGE (MENU_COLS * MENU_ROWS)
#define MENU_CW       12
#define MENU_CH       10
#define MENU_OX       4
#define MENU_OY       2

static void draw_menu(Canvas* canvas, TutuApp* app) {
    uint16_t page = app->menu_cursor / MENU_PER_PAGE;
    uint16_t base = page * MENU_PER_PAGE;
    canvas_set_font(canvas, FontSecondary);
    for(uint16_t i = 0; i < MENU_PER_PAGE; i++) {
        uint16_t n = base + i;
        if(n >= tutu_levels_count()) break;
        int col = i % MENU_COLS;
        int row = i / MENU_COLS;
        int x = MENU_OX + col * MENU_CW;
        int y = MENU_OY + row * MENU_CH;
        bool unlocked = tutu_progress_is_unlocked(&app->progress, n);
        bool done = tutu_progress_is_completed(&app->progress, n);
        bool cursor = (n == app->menu_cursor);
        if(cursor) {
            canvas_draw_box(canvas, x, y, MENU_CW - 1, MENU_CH - 1);
            canvas_set_color(canvas, ColorWhite);
        }
        if(!unlocked) {
            canvas_draw_frame(canvas, x + 2, y + 2, MENU_CW - 5, MENU_CH - 5); // locked
        } else if(done) {
            canvas_draw_disc(canvas, x + MENU_CW / 2 - 1, y + MENU_CH / 2 - 1, 2); // completed
        } else {
            canvas_draw_box(canvas, x + MENU_CW / 2 - 2, y + MENU_CH / 2 - 2, 3, 3); // playable
        }
        if(cursor) canvas_set_color(canvas, ColorBlack);
    }
    // footer: selected level (left) + credits hint (right), aligned so neither overflows
    char lvl[12];
    snprintf(lvl, sizeof(lvl), "Lvl %u", (unsigned)(app->menu_cursor + 1));
    canvas_draw_str_aligned(canvas, 2, 63, AlignLeft, AlignBottom, lvl);
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "hold=credits");
}

static void handle_menu_input(TutuApp* app, InputEvent* e) {
    if(e->type == InputTypeLong && e->key == InputKeyOk) {
        app->screen = ScreenCredits;
        return;
    }
    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return;
    uint16_t cur = app->menu_cursor;
    uint16_t count = tutu_levels_count();
    switch(e->key) {
    case InputKeyLeft:
        if(cur > 0) cur--;
        break;
    case InputKeyRight:
        if(cur + 1 < count) cur++;
        break;
    case InputKeyUp:
        if(cur >= MENU_COLS) cur -= MENU_COLS;
        break;
    case InputKeyDown:
        if(cur + MENU_COLS < count) cur += MENU_COLS;
        break;
    case InputKeyOk:
        if(e->type == InputTypeShort && tutu_progress_is_unlocked(&app->progress, cur))
            load_level(app, cur);
        return;
    default:
        return;
    }
    app->menu_cursor = cur;
}

// ---- win flow ----

static void handle_win_input(TutuApp* app, InputEvent* e) {
    if(e->type != InputTypeShort) return;
    if(e->key == InputKeyOk) {
        uint16_t next = app->level_index + 1;
        if(next < tutu_levels_count()) {
            load_level(app, next);
        } else {
            app->menu_cursor = app->level_index;
            app->screen = ScreenMenu;
        }
    } else if(e->key == InputKeyBack) {
        app->menu_cursor = app->level_index;
        app->screen = ScreenMenu;
    }
}

// ---- credits ----

static void draw_credits(Canvas* canvas, TutuApp* app) {
    UNUSED(app);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Tutu");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "v" TUTU_VERSION "  by endika");
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, "github.com/Endika");
    canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, "/flipper-tutu");
    canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, "Back: menu");
}

static void handle_credits_input(TutuApp* app, InputEvent* e) {
    if(e->type == InputTypeShort && e->key == InputKeyBack) app->screen = ScreenMenu;
}
