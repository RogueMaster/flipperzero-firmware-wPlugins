// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "flock_map_view.h"
#include "../recon_app_i.h"

#include <gui/elements.h>
#include <math.h>

// Operator marker is fixed at screen centre; cameras are projected around it.
#define MAP_CX      64
#define MAP_CY      36
// Map drawing box (below the header divider at y=11, above the scale bar).
#define MAP_TOP     12
#define MAP_BOTTOM  63
#define MAP_LEFT    1
#define MAP_RIGHT   126
// Largest geotag is fitted to this radius (px) from the operator marker.
#define MAP_FIT_R   28.0f
#define HEADING_LEN 6
#define SCALE_PX    24

struct FlockMapView {
    View* view;
};

typedef struct {
    void* app; /**< ReconApp* */
    int8_t zoom; /**< user zoom step (Left/Right); 0 = auto-fit. The draw callback
                   *   reads all live data from ReconApp under the mutex into local
                   *   scratch, so no per-camera state is cached in the model. */
} FlockMapModel;

static void flock_map_view_draw_callback(Canvas* canvas, void* _model) {
    FlockMapModel* model = _model;
    ReconApp* app = model->app;
    if(!app) return;

    canvas_clear(canvas);

    // ---- snapshot live data under the mutex (scalars + geotag loop only) ----
    // We copy raw lat/lon into locals and do ALL trig/projection/drawing after
    // releasing the mutex (never hold it across canvas_* or sqrtf/cosf).
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    bool gps_enabled = app->settings.gps_enabled;
    bool gps_valid = app->gps_valid;
    int gps_sats = app->gps_sats;
    float glat = app->gps_lat;
    float glon = app->gps_lon;
    float gcourse = app->gps_course;

    // These snapshot/scratch arrays are kept static (not on the stack): the GUI
    // thread serialises draw callbacks, so there's a single caller, and ~2 KB of
    // floats would otherwise risk overflowing the GUI thread stack.
    size_t flock_count = app->flock_count;
    static float lat[RECON_FLOCK_MAX];
    static float lon[RECON_FLOCK_MAX];
    static uint8_t conf[RECON_FLOCK_MAX];
    size_t n = 0;
    for(size_t i = 0; i < flock_count && i < RECON_FLOCK_MAX; i++) {
        lat[n] = app->flock[i].lat;
        lon[n] = app->flock[i].lon;
        conf[n] = (uint8_t)app->flock[i].confidence;
        n++;
    }

    furi_mutex_release(app->mutex);

    // ---- header ------------------------------------------------------------
    canvas_set_font(canvas, FontSecondary);
    char hdr[20];
    snprintf(hdr, sizeof(hdr), "Map  N:%u", (unsigned)flock_count);
    canvas_draw_str(canvas, 0, 9, hdr);

    if(gps_enabled) {
        char gps_str[12];
        if(gps_valid) {
            snprintf(gps_str, sizeof(gps_str), "G:%d", gps_sats);
        } else {
            snprintf(gps_str, sizeof(gps_str), "G:-");
        }
        canvas_draw_str_aligned(canvas, 128, 9, AlignRight, AlignBottom, gps_str);
    }
    canvas_draw_line(canvas, 0, 11, 128, 11);

    // ---- empty states ------------------------------------------------------
    if(!gps_valid) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "No GPS fix");
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "map needs a fix");
        return;
    }

    // ---- project geotagged cameras into screen space -----------------------
    // dN/dE in metres relative to the operator; auto-fit the farthest camera to
    // MAP_FIT_R so the whole cluster is on screen.
    static float dN[RECON_FLOCK_MAX];
    static float dE[RECON_FLOCK_MAX];
    static uint8_t pconf[RECON_FLOCK_MAX];
    size_t np = 0;
    float maxr = 0.0f;
    float coslat = cosf(glat * (float)M_PI / 180.0f);
    for(size_t i = 0; i < n; i++) {
        if(isnan(lat[i]) || isnan(lon[i])) continue;
        float dn = (lat[i] - glat) * 111320.0f;
        float de = (lon[i] - glon) * 111320.0f * coslat;
        float r = sqrtf(dn * dn + de * de);
        if(r > maxr) maxr = r;
        dN[np] = dn;
        dE[np] = de;
        pconf[np] = conf[i];
        np++;
    }

    // Metres-per-pixel: fit the farthest camera, then apply the user zoom. The
    // >=1.0 floor doubles as the div-by-zero guard when maxr==0 (single point
    // on the operator, or all cameras co-located with us).
    float mpp = maxr / MAP_FIT_R;
    if(mpp < 1.0f) mpp = 1.0f;
    mpp *= powf(1.5f, (float)-model->zoom);
    if(mpp < 0.01f) mpp = 0.01f;

    // ---- operator marker + heading tick + range ring -----------------------
    canvas_draw_disc(canvas, MAP_CX, MAP_CY, 1);
    if(!isnan(gcourse)) {
        // Screen Y grows downward; north (course 0) points up.
        float rad = gcourse * (float)M_PI / 180.0f;
        int tx = MAP_CX + (int)lroundf(sinf(rad) * HEADING_LEN);
        int ty = MAP_CY - (int)lroundf(cosf(rad) * HEADING_LEN);
        canvas_draw_line(canvas, MAP_CX, MAP_CY, tx, ty);
    }
    // Faint range ring at the fitted radius (orientation reference).
    canvas_draw_circle(canvas, MAP_CX, MAP_CY, (size_t)MAP_FIT_R);

    if(np == 0) {
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "No camera geotags");
        return;
    }

    // ---- cameras: encode confidence by marker size -------------------------
    for(size_t i = 0; i < np; i++) {
        int x = MAP_CX + (int)lroundf(dE[i] / mpp);
        int y = MAP_CY - (int)lroundf(dN[i] / mpp);
        if(x < MAP_LEFT || x > MAP_RIGHT || y < MAP_TOP || y > MAP_BOTTOM) continue;
        switch((FlockConfidence)pconf[i]) {
        case FlockConfidenceConfirmed:
            canvas_draw_disc(canvas, x, y, 2);
            break;
        case FlockConfidenceProbeFp: // B1 IE-fp class match: same weight as Likely
        case FlockConfidenceLikely:
            canvas_draw_disc(canvas, x, y, 1);
            break;
        default:
            canvas_draw_dot(canvas, x, y);
            break;
        }
    }

    // ---- scale bar (bottom-left) -------------------------------------------
    int span_m = (int)lroundf(mpp * SCALE_PX);
    canvas_draw_line(canvas, MAP_LEFT, 62, MAP_LEFT + SCALE_PX, 62);
    canvas_draw_line(canvas, MAP_LEFT, 60, MAP_LEFT, 62);
    canvas_draw_line(canvas, MAP_LEFT + SCALE_PX, 60, MAP_LEFT + SCALE_PX, 62);
    char scale[12];
    snprintf(scale, sizeof(scale), "%dm", span_m);
    canvas_draw_str(canvas, MAP_LEFT + SCALE_PX + 3, 63, scale);
}

static bool flock_map_view_input_callback(InputEvent* event, void* context) {
    FlockMapView* fmv = context;
    bool handled = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyLeft) {
            // Zoom out (larger mpp).
            with_view_model(fmv->view, FlockMapModel * model, { model->zoom--; }, true);
            handled = true;
        } else if(event->key == InputKeyRight) {
            // Zoom in (smaller mpp).
            with_view_model(fmv->view, FlockMapModel * model, { model->zoom++; }, true);
            handled = true;
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            with_view_model(fmv->view, FlockMapModel * model, { model->zoom = 0; }, true);
            handled = true;
        }
    }
    // Back falls through to the scene-manager nav callback.
    return handled;
}

FlockMapView* flock_map_view_alloc(void) {
    FlockMapView* fmv = malloc(sizeof(FlockMapView));
    fmv->view = view_alloc();
    view_set_context(fmv->view, fmv);
    view_allocate_model(fmv->view, ViewModelTypeLocking, sizeof(FlockMapModel));
    view_set_draw_callback(fmv->view, flock_map_view_draw_callback);
    view_set_input_callback(fmv->view, flock_map_view_input_callback);
    with_view_model(
        fmv->view,
        FlockMapModel * model,
        {
            model->app = NULL;
            model->zoom = 0;
        },
        false);
    return fmv;
}

void flock_map_view_free(FlockMapView* fmv) {
    furi_assert(fmv);
    view_free(fmv->view);
    free(fmv);
}

View* flock_map_view_get_view(FlockMapView* fmv) {
    furi_assert(fmv);
    return fmv->view;
}

void flock_map_view_set_app(FlockMapView* fmv, void* app) {
    with_view_model(fmv->view, FlockMapModel * model, { model->app = app; }, false);
}

void flock_map_view_refresh(FlockMapView* fmv) {
    with_view_model(fmv->view, FlockMapModel * model, { UNUSED(model); }, true);
}
