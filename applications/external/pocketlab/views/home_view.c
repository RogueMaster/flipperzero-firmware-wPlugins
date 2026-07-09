#include "home_view.h"

#include <furi.h>
#include <gui/elements.h>

#include "../helpers/pocketlab_sound.h"

#define HOME_TOP          14
#define HOME_COLS         2
#define HOME_TILE_W       60
#define HOME_TILE_H       22
#define HOME_MARGIN_X     2 // 2 + 60 + 4 + 60 + 2 = 128, centred on the screen
#define HOME_GAP_X        4
#define HOME_GAP_Y        3
#define HOME_VISIBLE_ROWS 2 // tile rows on screen; extra rows scroll
#define HOME_ICON_W       13
#define HOME_ICON_H       11
#define HOME_PAD          5 // left inset for the icon inside a tile
#define HOME_RADIUS       4

#define HOME_ANIM_MS 100

struct HomeView {
    View* view;
    FuriTimer* timer;
    HomeViewCallback callback;
    void* context;
    NotificationApp* notifications;
    bool sound_enabled;
};

typedef struct {
    const char* title;
    const char* items[HOME_VIEW_MAX_ITEMS];
    size_t count;
    size_t selected;
    size_t offset; // first visible tile row
    uint32_t xp;
    uint32_t anim;
} HomeModel;

// Tile icons (13 wide, 11 tall, bit c of each row = pixel x=c), one per menu
// entry in order: Labs, Quiz, Profile, Settings, About.
static const uint16_t icon_flask[HOME_ICON_H] =
    {0x00F8, 0x0088, 0x018C, 0x0104, 0x0202, 0x0272, 0x04F9, 0x05FD, 0x04F9, 0x03FE, 0x01FC};
static const uint16_t icon_quiz[HOME_ICON_H] =
    {0x00E0, 0x0318, 0x0404, 0x04E4, 0x0120, 0x0080, 0x0040, 0x0040, 0x0000, 0x0040, 0x0040};
static const uint16_t icon_trophy[HOME_ICON_H] =
    {0x03FE, 0x0603, 0x05FD, 0x0603, 0x0202, 0x0104, 0x00F8, 0x0020, 0x00F8, 0x01FC, 0x03FE};
static const uint16_t icon_gear[HOME_ICON_H] =
    {0x0208, 0x071C, 0x07FC, 0x0C06, 0x0AEA, 0x0AAA, 0x0AEA, 0x0C06, 0x07FC, 0x071C, 0x0208};
static const uint16_t icon_info[HOME_ICON_H] =
    {0x00E0, 0x0318, 0x0404, 0x0842, 0x0802, 0x0842, 0x0842, 0x0842, 0x0404, 0x0318, 0x00E0};

static const uint16_t* const menu_icons[] =
    {icon_flask, icon_quiz, icon_trophy, icon_gear, icon_info};

// The PocketLab mark (10x10): a chat bubble with a smiley. Matches the icon.
static void home_view_draw_logo(Canvas* canvas, uint8_t x, uint8_t y, bool wink) {
    static const uint16_t rows[10] = {510, 513, 645, 513, 645, 633, 513, 510, 64, 128};
    for(uint8_t row = 0; row < 10; row++) {
        for(uint8_t col = 0; col < 10; col++) {
            if(wink && row == 2 && col == 7) continue; // right eye closes on a wink
            if(rows[row] & (1u << col)) {
                canvas_draw_dot(canvas, x + col, y + row);
            }
        }
    }
    if(wink) canvas_draw_line(canvas, x + 6, y + 3, x + 8, y + 3); // closed eye dash
}

static void home_view_draw_icon(Canvas* canvas, const uint16_t* rows, uint8_t x, uint8_t y) {
    for(uint8_t r = 0; r < HOME_ICON_H; r++) {
        for(uint8_t c = 0; c < HOME_ICON_W; c++) {
            if(rows[r] & (1u << c)) {
                canvas_draw_dot(canvas, x + c, y + r);
            }
        }
    }
}

static void home_view_draw_callback(Canvas* canvas, void* context) {
    HomeModel* model = context;
    canvas_clear(canvas);

    home_view_draw_logo(canvas, 2, 0, (model->anim % 40) < 2); // winks now and then
    canvas_set_font(canvas, FontPrimary);
    // Baseline chosen so the title is vertically centred against the 10px logo.
    canvas_draw_str(canvas, 16, 8, model->title);

    // XP counter in the top-right corner, small, no taller than the title.
    canvas_set_font(canvas, FontSecondary);
    char xp_buf[16];
    snprintf(xp_buf, sizeof(xp_buf), "%lu XP", (unsigned long)model->xp);
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, xp_buf);

    for(size_t i = 0; i < model->count; i++) {
        const size_t col = i % HOME_COLS;
        const size_t row = i / HOME_COLS;
        // Only rows in the scroll window are drawn.
        if(row < model->offset || row >= model->offset + HOME_VISIBLE_ROWS) {
            continue;
        }
        const uint8_t x = HOME_MARGIN_X + col * (HOME_TILE_W + HOME_GAP_X);
        const uint8_t y = HOME_TOP + (uint8_t)(row - model->offset) * (HOME_TILE_H + HOME_GAP_Y);
        const bool sel = i == model->selected;
        // A lone tile on the last row spans both columns.
        const bool full = (i == model->count - 1) && (model->count % HOME_COLS == 1);
        const uint8_t w = full ? (uint8_t)(HOME_TILE_W * 2 + HOME_GAP_X) : HOME_TILE_W;

        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, x, y, w, HOME_TILE_H, HOME_RADIUS);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, x, y, w, HOME_TILE_H, HOME_RADIUS);
        }

        // Icon on the left, label to its right, both vertically centred.
        if(i < COUNT_OF(menu_icons)) {
            home_view_draw_icon(
                canvas, menu_icons[i], x + HOME_PAD, y + (HOME_TILE_H - HOME_ICON_H) / 2);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas,
            x + HOME_PAD + HOME_ICON_W + 2,
            y + HOME_TILE_H / 2,
            AlignLeft,
            AlignCenter,
            model->items[i]);

        // Twinkling "stars" of dead pixels on the selected tile's black field.
        if(sel) {
            canvas_set_color(canvas, ColorWhite);
            for(uint8_t s = 0; s < 4; s++) {
                const uint32_t h = (model->anim / 3 + s * 131 + (uint32_t)i * 17) * 2654435761u;
                if((h >> 29) % 4 != 0) continue; // flicker: visible only sometimes
                const uint8_t sxp = x + 3 + (h >> 5) % (w - 6);
                const uint8_t syp = y + 3 + (h >> 15) % (HOME_TILE_H - 6);
                canvas_draw_dot(canvas, sxp, syp);
            }
        }

        canvas_set_color(canvas, ColorBlack);
    }

    // Tiny 3px scroll chevrons in the column gap: up on top, down at the bottom.
    const size_t total_rows = (model->count + HOME_COLS - 1) / HOME_COLS;
    if(model->offset > 0) {
        canvas_draw_dot(canvas, 63, 12);
        canvas_draw_dot(canvas, 64, 11);
        canvas_draw_dot(canvas, 65, 12);
    }
    if(model->offset + HOME_VISIBLE_ROWS < total_rows) {
        canvas_draw_dot(canvas, 63, 61);
        canvas_draw_dot(canvas, 64, 62);
        canvas_draw_dot(canvas, 65, 61);
    }
}

static bool home_view_input_callback(InputEvent* event, void* context) {
    HomeView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    bool selected = false;
    bool play_coin = false;
    uint32_t selected_index = 0;

    with_view_model(
        instance->view,
        HomeModel * model,
        {
            const size_t sel = model->selected;
            if(event->key == InputKeyUp) {
                if(sel >= HOME_COLS) model->selected = sel - HOME_COLS;
                consumed = true;
            } else if(event->key == InputKeyDown) {
                if(sel + HOME_COLS < model->count) {
                    model->selected = sel + HOME_COLS;
                } else if(model->count % HOME_COLS == 1 && sel != model->count - 1) {
                    model->selected = model->count - 1; // drop onto the full-width tile
                }
                consumed = true;
            } else if(event->key == InputKeyLeft) {
                if(sel % HOME_COLS > 0) model->selected = sel - 1;
                consumed = true;
            } else if(event->key == InputKeyRight) {
                if(sel % HOME_COLS < HOME_COLS - 1 && sel + 1 < model->count) {
                    model->selected = sel + 1;
                }
                consumed = true;
            } else if(event->key == InputKeyOk) {
                selected = true;
                selected_index = model->selected;
                consumed = true;
            }
            play_coin = !selected && model->selected != sel;
            // Keep the selected tile within the scroll window.
            const size_t row = model->selected / HOME_COLS;
            if(row < model->offset) {
                model->offset = row;
            } else if(row >= model->offset + HOME_VISIBLE_ROWS) {
                model->offset = row - HOME_VISIBLE_ROWS + 1;
            }
        },
        true);

    if(play_coin) {
        pocketlab_sound_play(instance->notifications, instance->sound_enabled, PocketLabSoundCoin);
    }
    if(selected && instance->callback) {
        instance->callback(instance->context, selected_index);
    }

    return consumed;
}

static void home_view_timer_callback(void* context) {
    HomeView* instance = context;
    with_view_model(instance->view, HomeModel * model, { model->anim++; }, true);
}

static void home_view_enter_callback(void* context) {
    HomeView* instance = context;
    furi_timer_start(instance->timer, furi_ms_to_ticks(HOME_ANIM_MS));
}

static void home_view_exit_callback(void* context) {
    HomeView* instance = context;
    furi_timer_stop(instance->timer);
}

HomeView* home_view_alloc(void) {
    HomeView* instance = malloc(sizeof(HomeView));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;
    instance->notifications = NULL;
    instance->sound_enabled = false;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(HomeModel));
    view_set_draw_callback(instance->view, home_view_draw_callback);
    view_set_input_callback(instance->view, home_view_input_callback);
    view_set_enter_callback(instance->view, home_view_enter_callback);
    view_set_exit_callback(instance->view, home_view_exit_callback);

    instance->timer = furi_timer_alloc(home_view_timer_callback, FuriTimerTypePeriodic, instance);

    return instance;
}

void home_view_free(HomeView* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->timer);
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* home_view_get_view(HomeView* instance) {
    furi_assert(instance);
    return instance->view;
}

void home_view_set_callback(HomeView* instance, HomeViewCallback callback, void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void home_view_configure(
    HomeView* instance,
    const char* title,
    const char* const* items,
    size_t count,
    uint32_t selected,
    uint32_t xp,
    NotificationApp* notifications,
    bool sound_enabled) {
    furi_assert(instance);
    furi_check(count <= HOME_VIEW_MAX_ITEMS);
    instance->notifications = notifications;
    instance->sound_enabled = sound_enabled;

    with_view_model(
        instance->view,
        HomeModel * model,
        {
            model->title = title;
            model->count = count;
            for(size_t i = 0; i < count; i++) {
                model->items[i] = items[i];
            }
            model->selected = selected < count ? selected : 0;
            model->xp = xp;
            model->anim = 0;
            // Scroll so the remembered selection is visible.
            const size_t row = model->selected / HOME_COLS;
            model->offset = row >= HOME_VISIBLE_ROWS ? row - HOME_VISIBLE_ROWS + 1 : 0;
        },
        true);
}

uint32_t home_view_get_selected(HomeView* instance) {
    furi_assert(instance);
    uint32_t selected = 0;
    with_view_model(instance->view, HomeModel * model, { selected = model->selected; }, false);
    return selected;
}
