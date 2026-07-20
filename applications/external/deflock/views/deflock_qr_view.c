// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "deflock_qr_view.h"
#include "../recon_app_i.h"

#include <gui/elements.h>
#include "../lib/qrcodegen/qrcodegen.h"

// Cap the encoder at version 8 (49x49 modules). A geo:/deflock.org URL with
// ~6-decimal coordinates is well under that mode's byte-capacity, and it keeps
// the two transient buffers small: BUFFER_LEN_FOR_VERSION(8) = 302 B each, so
// the qrcode+temp pair in the model is ~604 B (within the <600 B budget for the
// live buffer; the temp scratch is reused and not displayed).
#define QR_MAX_VERSION 8
#define QR_BUF_LEN     qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)

// Left square the QR is scaled to fill (px). The right column holds the text.
#define QR_AREA 52

struct DeflockQrView {
    View* view;
    DeflockQrPageCallback page_cb;
    void* page_ctx;
};

typedef struct {
    void* app; /**< ReconApp* */
    bool empty; /**< no marked cameras -> show empty state */
    bool has_qr; /**< encode succeeded -> draw modules */
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
    if(model->has_qr) {
        int size = qrcodegen_getSize(model->qr);
        int scale = QR_AREA / size;
        if(scale < 1) scale = 1;
        int dim = size * scale;
        int ox = (QR_AREA - dim) / 2;
        int oy = (QR_AREA - dim) / 2;
        canvas_set_color(canvas, ColorBlack);
        for(int y = 0; y < size; y++) {
            for(int x = 0; x < size; x++) {
                if(qrcodegen_getModule(model->qr, x, y)) {
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

    // Right column: index, coords, confidence.
    canvas_set_font(canvas, FontSecondary);
    char hdr[16];
    snprintf(hdr, sizeof(hdr), "%d/%d", model->index + 1, model->total);
    canvas_draw_str(canvas, QR_AREA + 4, 8, hdr);
    canvas_draw_str(canvas, QR_AREA + 4, 18, model->coords);
    canvas_draw_str(canvas, QR_AREA + 4, 28, model->conf);

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
            model->has_qr = qrcodegen_encodeText(
                url,
                temp,
                model->qr,
                qrcodegen_Ecc_LOW,
                qrcodegen_VERSION_MIN,
                QR_MAX_VERSION,
                qrcodegen_Mask_AUTO,
                true);
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
