/* SPDX-License-Identifier: GPL-3.0-only */
#include "home_view.h"

#include <furi.h>
#include <gui/elements.h>
#include <ssh_runner_icons.h>

#include "../core/textproc.h"

#define HOME_MAX_ITEMS 8
#define HOME_HEADER_H  12
#define HOME_ITEM_H    12
#define HOME_VISIBLE   4

struct HomeView {
    View* view;
    HomeViewCallback cb;
    void* cb_ctx;
};

typedef struct {
    const char* labels[HOME_MAX_ITEMS];
    uint8_t count;
    uint8_t selected;
    uint8_t offset;
    HomeStatus status;
} HomeModel;

static void home_draw(Canvas* canvas, void* model_) {
    HomeModel* m = model_;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Status header: caps on rows 3..9 (baseline 10) and the icons on rows 2..9, so rows
     * 10..11 stay clear above the rule at row 12 and nothing merges with it. LAN
     * and WiFi icons (stock status-bar height, 8 rows: solid / arcs when up,
     * outline / "x" when down), then the address in FontKeyboard or a status in
     * plain words. */
    /* A link icon only while that link is up, like the iOS / Android / Flipper status bars
     * (a "WiFi off" glyph reads as odd); the text on the right says the state. Items
     * keep 3 px gaps from the left whatever is shown. */
    uint8_t x = 2;
    if(!m->status.board_ok) {
        canvas_draw_icon(canvas, x, 2, &I_Alert_9x8); /* no board: an error, not "no network" */
        x += 9 + 3;
    } else {
        if(m->status.lan_up) {
            canvas_draw_icon(canvas, x, 2, &I_LanOn_9x8);
            x += 9 + 3;
        }
        if(m->status.wifi_up) {
            canvas_draw_icon(canvas, x, 2, &I_WifiOn_9x8);
            x += 9 + 3;
        }
    }
    uint8_t text_left = x;
    if(m->status.demo) {
        /* inverted "DEMO" tag so a mock session is never mistaken for a real one: rows 2..10,
         * 1 px around the caps, row 11 clear; FontKeyboard is narrow enough to leave a whole
         * address room */
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_rbox(canvas, x, 2, 25, 9, 1);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, x + 1, 10, "DEMO");
        canvas_set_color(canvas, ColorBlack);
        text_left = x + 25 + 3; /* 3 px gaps; a whole 12-character address still fits */
    }
    bool mono = text_is_technical_body(m->status.text);
    canvas_set_font(canvas, mono ? FontKeyboard : FontSecondary);
    FuriString* s = furi_string_alloc_set(m->status.text);
    elements_string_fit_width(canvas, s, 126 - text_left);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, furi_string_get_cstr(s));
    furi_string_free(s);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, HOME_HEADER_H, 127, HOME_HEADER_H);

    /* Menu */
    for(uint8_t i = 0; i < HOME_VISIBLE && m->offset + i < m->count; i++) {
        uint8_t idx = m->offset + i;
        int32_t y = HOME_HEADER_H + 2 + i * HOME_ITEM_H;
        if(idx == m->selected) {
            elements_slightly_rounded_box(canvas, 0, y, 122, HOME_ITEM_H - 1);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 6, y + 9, m->labels[idx]);
        canvas_set_color(canvas, ColorBlack);
    }
    elements_scrollbar_pos(
        canvas, 128, HOME_HEADER_H + 2, 64 - HOME_HEADER_H - 2, m->selected, m->count);
}

static void clamp_offset(HomeModel* m) {
    if(m->selected < m->offset) m->offset = m->selected;
    if(m->selected >= m->offset + HOME_VISIBLE) m->offset = m->selected - HOME_VISIBLE + 1;
}

static bool home_input(InputEvent* event, void* context) {
    HomeView* home = context;
    bool consumed = false;
    uint32_t fire = UINT32_MAX;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        with_view_model(
            home->view,
            HomeModel * m,
            {
                if(event->key == InputKeyUp) {
                    m->selected = m->selected ? m->selected - 1 : m->count - 1;
                    consumed = true;
                } else if(event->key == InputKeyDown) {
                    m->selected = (m->selected + 1 < m->count) ? m->selected + 1 : 0;
                    consumed = true;
                } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
                    fire = m->selected;
                    consumed = true;
                }
                clamp_offset(m);
            },
            consumed);
    }
    if(fire != UINT32_MAX && home->cb) home->cb(home->cb_ctx, fire);
    return consumed;
}

HomeView* home_view_alloc(void) {
    HomeView* home = malloc(sizeof(HomeView));
    home->cb = NULL;
    home->cb_ctx = NULL;
    home->view = view_alloc();
    view_allocate_model(home->view, ViewModelTypeLocking, sizeof(HomeModel));
    view_set_context(home->view, home);
    view_set_draw_callback(home->view, home_draw);
    view_set_input_callback(home->view, home_input);
    with_view_model(home->view, HomeModel * m, { memset(m, 0, sizeof(*m)); }, false);
    return home;
}

void home_view_free(HomeView* home) {
    view_free(home->view);
    free(home);
}

View* home_view_get_view(HomeView* home) {
    return home->view;
}

void home_view_set_items(HomeView* home, const char* const* labels, uint8_t count) {
    if(count > HOME_MAX_ITEMS) count = HOME_MAX_ITEMS;
    with_view_model(
        home->view,
        HomeModel * m,
        {
            for(uint8_t i = 0; i < count; i++)
                m->labels[i] = labels[i];
            m->count = count;
            if(m->selected >= count) m->selected = 0;
            clamp_offset(m);
        },
        true);
}

void home_view_set_callback(HomeView* home, HomeViewCallback cb, void* context) {
    home->cb = cb;
    home->cb_ctx = context;
}

void home_view_set_status(HomeView* home, const HomeStatus* status) {
    with_view_model(home->view, HomeModel * m, { m->status = *status; }, true);
}

void home_view_set_selected(HomeView* home, uint8_t index) {
    with_view_model(
        home->view,
        HomeModel * m,
        {
            if(index < m->count) m->selected = index;
            clamp_offset(m);
        },
        true);
}

uint8_t home_view_get_selected(HomeView* home) {
    uint8_t sel = 0;
    with_view_model(home->view, HomeModel * m, { sel = m->selected; }, false);
    return sel;
}
