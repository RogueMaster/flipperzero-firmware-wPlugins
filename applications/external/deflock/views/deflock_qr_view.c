// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "deflock_qr_view.h"
#include "../recon_app_i.h"

#include <gui/elements.h>
#include "../plugins/qr_plugin_api.h"

// Buffer size comes from the plugin ABI now (QR_PLUGIN_BUF_LEN = 302 B), so
// the app never includes qrcodegen.h -- that header travels with the encoder,
// which lives in the .fal. The plugin carries a _Static_assert tying the two
// together, so this cannot silently desync.
#define QR_BUF_LEN QR_PLUGIN_BUF_LEN

// Left square the QR is scaled to fill (px). The right column holds the text.
#define QR_AREA 52

struct DeflockQrView {
    View* view;
    DeflockQrPageCallback page_cb;
    void* page_ctx;
    // Borrowed from the scene, which owns the plugin for the screen's lifetime.
    // NULL means the encoder could not be loaded -- the view must still draw
    // (text fallback), never dereference.
    const QrPluginApi* qr_api;
};

typedef struct {
    void* app; /**< ReconApp* */
    bool empty; /**< no marked cameras -> show empty state */
    bool has_qr; /**< encode succeeded -> draw modules */
    const QrPluginApi* qr_api; /**< NULL when the encoder plugin is unavailable */
    int index; /**< 0-based position in the marked list */
    int total; /**< number of marked cameras */
    char coords[28]; /**< "lat, lon" */
    char conf[16]; /**< confidence string */
    char tags[96]; /**< OSM tag summary (newline-separated) */
    uint8_t qr[QR_BUF_LEN]; /**< rendered QR (read in the draw callback) */
} DeflockQrViewModel;

static void deflock_qr_view_draw_callback(Canvas* canvas, void* _model) {
    DeflockQrViewModel* model = _model;
    canvas_clear(canvas);

    if(model->empty) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "No marked cameras");
        canvas_draw_str_aligned(
            canvas, 64, 38, AlignCenter, AlignCenter, "Tag cameras in Flock Detect");
        return;
    }

    // Left: the QR, scaled so its module grid fills the QR_AREA square. Origin is
    // nudged so the scaled grid is centred in the area.
    if(model->has_qr && model->qr_api) {
        const QrPluginApi* qr = model->qr_api;
        int size = qr->get_size(model->qr);
        int scale = QR_AREA / size;
        if(scale < 1) scale = 1;
        int dim = size * scale;
        int ox = (QR_AREA - dim) / 2;
        int oy = (QR_AREA - dim) / 2;
        canvas_set_color(canvas, ColorBlack);
        for(int y = 0; y < size; y++) {
            for(int x = 0; x < size; x++) {
                if(qr->get_module(model->qr, x, y)) {
                    if(scale == 1) {
                        canvas_draw_dot(canvas, ox + x, oy + y);
                    } else {
                        canvas_draw_box(canvas, ox + x * scale, oy + y * scale, scale, scale);
                    }
                }
            }
        }
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, QR_AREA / 2, 26, AlignCenter, AlignCenter, "QR");
        canvas_draw_str_aligned(canvas, QR_AREA / 2, 36, AlignCenter, AlignCenter, "n/a");
    }

    // Right column: index, coords, confidence. A total of 0 means the caller is
    // showing a single fixed payload rather than a pageable list (the Support
    // screen), so the "n/m" pager header is suppressed and the two text lines
    // move up to take its place.
    canvas_set_font(canvas, FontSecondary);
    int ry = 8;
    if(model->total > 0) {
        // Sized for the widest pair the compiler can prove ("-2147483648/..."),
        // not for the real domain: both are bounded by the scan-table caps, but
        // that isn't visible here and -Wformat-truncation is an error.
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "%d/%d", model->index + 1, model->total);
        canvas_draw_str(canvas, QR_AREA + 4, ry, hdr);
        ry += 10;
    }
    canvas_draw_str(canvas, QR_AREA + 4, ry, model->coords);
    ry += 10;
    canvas_draw_str(canvas, QR_AREA + 4, ry, model->conf);

    // Bottom strip: the OSM tag summary, one line per tag. Drawn full-width below
    // the QR area so it's readable even if the QR isn't.
    int ty = QR_AREA + 9;
    const char* p = model->tags;
    while(*p && ty <= 64) {
        char line[40];
        size_t n = 0;
        while(p[n] && p[n] != '\n' && n < sizeof(line) - 1)
            n++;
        memcpy(line, p, n);
        line[n] = '\0';
        canvas_draw_str(canvas, 0, ty, line);
        ty += 6;
        p += n;
        if(*p == '\n') p++;
    }
}

static bool deflock_qr_view_input_callback(InputEvent* event, void* context) {
    DeflockQrView* qv = context;
    bool handled = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyLeft) {
            if(qv->page_cb) qv->page_cb(qv->page_ctx, -1);
            handled = true;
        } else if(event->key == InputKeyRight) {
            if(qv->page_cb) qv->page_cb(qv->page_ctx, 1);
            handled = true;
        }
    }
    return handled;
}

DeflockQrView* deflock_qr_view_alloc(void) {
    DeflockQrView* qv = malloc(sizeof(DeflockQrView));
    qv->page_cb = NULL;
    qv->page_ctx = NULL;
    // malloc, not calloc: an uninitialised qr_api would be called through if a
    // caller ever reached set_content() without set_api() first.
    qv->qr_api = NULL;
    qv->view = view_alloc();
    view_set_context(qv->view, qv);
    view_allocate_model(qv->view, ViewModelTypeLocking, sizeof(DeflockQrViewModel));
    view_set_draw_callback(qv->view, deflock_qr_view_draw_callback);
    view_set_input_callback(qv->view, deflock_qr_view_input_callback);
    with_view_model(
        qv->view,
        DeflockQrViewModel * model,
        {
            model->app = NULL;
            model->empty = true;
            model->has_qr = false;
            model->qr_api = NULL;
            model->index = 0;
            model->total = 0;
            model->coords[0] = '\0';
            model->conf[0] = '\0';
            model->tags[0] = '\0';
        },
        false);
    return qv;
}

void deflock_qr_view_free(DeflockQrView* qv) {
    furi_assert(qv);
    view_free(qv->view);
    free(qv);
}

View* deflock_qr_view_get_view(DeflockQrView* qv) {
    furi_assert(qv);
    return qv->view;
}

void deflock_qr_view_set_app(DeflockQrView* qv, void* app) {
    with_view_model(qv->view, DeflockQrViewModel * model, { model->app = app; }, false);
}

void deflock_qr_view_set_page_callback(DeflockQrView* qv, DeflockQrPageCallback cb, void* context) {
    qv->page_cb = cb;
    qv->page_ctx = context;
}

void deflock_qr_view_set_api(DeflockQrView* qv, const QrPluginApi* api) {
    // Borrowed, not owned: the scene loads the plugin on enter and frees it on
    // exit, so this pointer is valid exactly as long as the screen is.
    qv->qr_api = api;
}

bool deflock_qr_view_set_content(
    DeflockQrView* qv,
    const char* url,
    int index,
    int total,
    const char* coords,
    const char* conf,
    const char* tags) {
    // Scratch buffer for the encoder; lives on the stack so it isn't carried in
    // the model. Same length as the output buffer per the qrcodegen contract.
    uint8_t temp[QR_BUF_LEN];
    bool ok = false;
    with_view_model(
        qv->view,
        DeflockQrViewModel * model,
        {
            model->empty = false;
            model->index = index;
            model->total = total;
            strncpy(model->coords, coords, sizeof(model->coords) - 1);
            model->coords[sizeof(model->coords) - 1] = '\0';
            strncpy(model->conf, conf, sizeof(model->conf) - 1);
            model->conf[sizeof(model->conf) - 1] = '\0';
            strncpy(model->tags, tags, sizeof(model->tags) - 1);
            model->tags[sizeof(model->tags) - 1] = '\0';
            model->has_qr = qv->qr_api ? qv->qr_api->encode_text(url, temp, model->qr) : false;
            model->qr_api = qv->qr_api;
            ok = model->has_qr;
        },
        true);
    return ok;
}

void deflock_qr_view_set_empty(DeflockQrView* qv) {
    with_view_model(
        qv->view,
        DeflockQrViewModel * model,
        {
            model->empty = true;
            model->has_qr = false;
            model->index = 0;
            model->total = 0;
        },
        true);
}
