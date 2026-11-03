/* WiFi network list (see wifi_list_view.h). SPDX-License-Identifier: GPL-3.0-only */
#include "wifi_list_view.h"

#include <furi.h>
#include <gui/elements.h>
#include <ssh_runner_icons.h>

#include "../core/textproc.h"
#include "tofu.h"

#define WL_ITEM_H   16 /* stock Submenu geometry */
#define WL_ITEM_W   123
#define WL_HEADER_H 16
#define WL_SSID_MAX 33
#define WL_BARS_X   107 /* 4 bars of 3 px, 1 px apart: x 107..121 */
#define WL_LOCK_X   97 /* Lock_7x8, 3 px left of the bars */

struct WifiListView {
    View* view;
    WifiListCallback cb;
    void* cb_ctx;
};

typedef struct {
    char ssid[WL_SSID_MAX];
    uint8_t bars;
    bool secured;
} WlItem;

typedef struct {
    char header[24];
    WlItem items[WIFI_LIST_MAX];
    uint8_t count;
    uint8_t position;
    uint8_t window;
} WlModel;

static uint8_t wl_on_screen(const WlModel* m) {
    return m->header[0] ? 3 : 4;
}

/* signal bars on the row's text baseline: filled up to the level, outlined above it */
static void wl_bars(Canvas* canvas, uint8_t base, uint8_t level) {
    for(uint8_t i = 0; i < 4; i++) {
        uint8_t h = 2 * (i + 1), x = WL_BARS_X + i * 4;
        if(i < level)
            canvas_draw_box(canvas, x, base - h + 1, 3, h);
        else
            canvas_draw_frame(canvas, x, base - h + 1, 3, h);
    }
}

static void wl_draw(Canvas* canvas, void* model) {
    WlModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    uint8_t y0 = 0;
    if(m->header[0]) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 11, m->header);
        y0 = WL_HEADER_H;
    }
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < wl_on_screen(m) && m->window + i < m->count; i++) {
        uint8_t idx = m->window + i;
        const WlItem* it = &m->items[idx];
        uint8_t y = y0 + i * WL_ITEM_H, base = y + WL_ITEM_H - 4;
        if(idx == m->position) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(canvas, 0, y + 1, WL_ITEM_W, WL_ITEM_H - 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        FuriString* s = furi_string_alloc_set(it->ssid);
        tofu_fit(canvas, FontSecondary, s, WL_LOCK_X - 3 - 6);
        tofu_draw_str(canvas, FontSecondary, 6, base, furi_string_get_cstr(s));
        furi_string_free(s);
        if(it->secured) canvas_draw_icon(canvas, WL_LOCK_X, base - 7, &I_Lock_7x8);
        wl_bars(canvas, base, it->bars);
    }
    canvas_set_color(canvas, ColorBlack);
    elements_scrollbar_pos(canvas, 128, y0, 64 - y0, m->position, m->count);
}

static bool wl_input(InputEvent* event, void* context) {
    WifiListView* wl = context;
    if(event->key == InputKeyBack) return false;
    uint32_t fire = UINT32_MAX;
    bool consumed = false;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        with_view_model(
            wl->view,
            WlModel * m,
            {
                if(m->count) {
                    if(event->key == InputKeyUp)
                        m->position = m->position ? m->position - 1 : m->count - 1;
                    if(event->key == InputKeyDown)
                        m->position = m->position + 1 < m->count ? m->position + 1 : 0;
                    if(m->position < m->window) m->window = m->position;
                    if(m->position >= m->window + wl_on_screen(m))
                        m->window = m->position - wl_on_screen(m) + 1;
                    if(event->key == InputKeyOk && event->type == InputTypeShort)
                        fire = m->position;
                }
                consumed = true;
            },
            true);
    }
    if(fire != UINT32_MAX && wl->cb) wl->cb(wl->cb_ctx, fire);
    return consumed;
}

WifiListView* wifi_list_view_alloc(void) {
    WifiListView* wl = malloc(sizeof(WifiListView));
    wl->cb = NULL;
    wl->cb_ctx = NULL;
    wl->view = view_alloc();
    view_allocate_model(wl->view, ViewModelTypeLocking, sizeof(WlModel));
    view_set_context(wl->view, wl);
    view_set_draw_callback(wl->view, wl_draw);
    view_set_input_callback(wl->view, wl_input);
    wifi_list_view_reset(wl, NULL);
    return wl;
}

void wifi_list_view_free(WifiListView* wl) {
    view_free(wl->view);
    free(wl);
}

View* wifi_list_view_get_view(WifiListView* wl) {
    return wl->view;
}

void wifi_list_view_reset(WifiListView* wl, const char* header) {
    with_view_model(
        wl->view,
        WlModel * m,
        {
            memset(m, 0, sizeof(*m));
            snprintf(m->header, sizeof(m->header), "%s", header ? header : "");
        },
        true);
}

void wifi_list_view_add(WifiListView* wl, const char* ssid, uint8_t bars, bool secured) {
    with_view_model(
        wl->view,
        WlModel * m,
        {
            if(m->count < WIFI_LIST_MAX) {
                WlItem* it = &m->items[m->count++];
                text_ascii(ssid, it->ssid, sizeof(it->ssid), TEXT_TOFU);
                it->bars = bars > 4 ? 4 : bars;
                it->secured = secured;
            }
        },
        true);
}

void wifi_list_view_set_callback(WifiListView* wl, WifiListCallback cb, void* context) {
    wl->cb = cb;
    wl->cb_ctx = context;
}
