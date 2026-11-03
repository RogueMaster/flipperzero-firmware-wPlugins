/* Progress screen (see progress_view.h). SPDX-License-Identifier: GPL-3.0-only */
#include "progress_view.h"

#include <furi.h>
#include <gui/elements.h>
#include <string.h>

#include "../core/textproc.h"
#include "tofu.h"

#define PV_TITLE_Y 3 /* AlignTop, as the stock Popup header: ink from row 2 */
#define PV_ICON_Y  18
#define PV_TEXT_Y  37

struct ProgressView {
    View* view;
};

typedef struct {
    char title[48];
    bool title_mono;
    const Icon* icon;
    char text[64];
} PvModel;

static void pv_draw(Canvas* canvas, void* model) {
    PvModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(m->title[0]) {
        Font font = m->title_mono ? FontKeyboard : FontPrimary;
        canvas_set_font(canvas, font);
        FuriString* t = furi_string_alloc_set(m->title);
        tofu_fit(canvas, font, t, 124); /* stock "...", inside the 2 px margins */
        const char* title = furi_string_get_cstr(t);
        if(strchr(title, TEXT_TOFU)) {
            /* boxes for characters the font lacks (an SSID): drawn by hand, centred like AlignTop */
            uint16_t w = tofu_width(canvas, font, title);
            tofu_draw_str(
                canvas,
                font,
                64 - w / 2,
                PV_TITLE_Y + canvas_get_font_params(canvas, font)->height,
                title);
        } else {
            canvas_draw_str_aligned(canvas, 64, PV_TITLE_Y, AlignCenter, AlignTop, title);
        }
        furi_string_free(t);
    }
    if(m->icon) canvas_draw_icon(canvas, 64 - icon_get_width(m->icon) / 2, PV_ICON_Y, m->icon);
    if(m->text[0]) {
        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned(canvas, 64, PV_TEXT_Y, AlignCenter, AlignTop, m->text);
    }
}

static bool pv_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false; /* Back goes to the scene, like the stock Popup without a callback */
}

ProgressView* progress_view_alloc(void) {
    ProgressView* pv = malloc(sizeof(ProgressView));
    pv->view = view_alloc();
    view_allocate_model(pv->view, ViewModelTypeLocking, sizeof(PvModel));
    view_set_context(pv->view, pv);
    view_set_draw_callback(pv->view, pv_draw);
    view_set_input_callback(pv->view, pv_input);
    progress_view_reset(pv);
    return pv;
}

void progress_view_free(ProgressView* pv) {
    view_free(pv->view);
    free(pv);
}

View* progress_view_get_view(ProgressView* pv) {
    return pv->view;
}

void progress_view_set(
    ProgressView* pv,
    const char* title,
    bool title_mono,
    const Icon* icon,
    const char* text) {
    with_view_model(
        pv->view,
        PvModel * m,
        {
            snprintf(m->title, sizeof(m->title), "%s", title ? title : "");
            m->title_mono = title_mono;
            m->icon = icon;
            snprintf(m->text, sizeof(m->text), "%s", text ? text : "");
        },
        true);
}

void progress_view_reset(ProgressView* pv) {
    with_view_model(pv->view, PvModel * m, { memset(m, 0, sizeof(*m)); }, false);
}
