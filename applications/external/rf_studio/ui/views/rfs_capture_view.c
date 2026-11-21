#include "rfs_capture_view.h"

#include <furi.h>
#include <gui/elements.h>

#define RSSI_FLOOR (-100.0f)
#define RSSI_CEIL  (-30.0f)
#define BAR_X      0
#define BAR_Y      44
#define BAR_W      127
#define BAR_H      5

struct RfsCaptureView {
    View* view;
    RfsCaptureViewCallback callback;
    void* context;
};

typedef struct {
    char title[24];
    char line1[32];
    char line2[32];
    char line3[32];
    bool show_rssi;
    float rssi;
    int32_t threshold;
    char left[12];
    char center[12];
    char right[12];
} RfsCaptureViewModel;

static int32_t rfs_rssi_to_x(float rssi) {
    if(rssi < RSSI_FLOOR) rssi = RSSI_FLOOR;
    if(rssi > RSSI_CEIL) rssi = RSSI_CEIL;
    return (int32_t)((rssi - RSSI_FLOOR) * (BAR_W - 2) / (RSSI_CEIL - RSSI_FLOOR));
}

static void rfs_capture_view_draw(Canvas* canvas, void* model_ptr) {
    RfsCaptureViewModel* model = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 9, model->title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 20, model->line1);
    canvas_draw_str(canvas, 0, 30, model->line2);
    canvas_draw_str(canvas, 0, 40, model->line3);

    if(model->show_rssi) {
        canvas_draw_frame(canvas, BAR_X, BAR_Y, BAR_W, BAR_H);
        int32_t fill = rfs_rssi_to_x(model->rssi);
        if(fill > 0) canvas_draw_box(canvas, BAR_X + 1, BAR_Y + 1, fill, BAR_H - 2);
        int32_t mark = BAR_X + 1 + rfs_rssi_to_x((float)model->threshold);
        canvas_draw_line(canvas, mark, BAR_Y - 2, mark, BAR_Y + BAR_H + 1);
    }

    if(model->left[0]) elements_button_left(canvas, model->left);
    if(model->center[0]) elements_button_center(canvas, model->center);
    if(model->right[0]) elements_button_right(canvas, model->right);
}

static bool rfs_capture_view_input(InputEvent* event, void* context) {
    RfsCaptureView* view = context;
    if(event->type != InputTypeShort) return event->key != InputKeyBack;
    bool has_left = false, has_center = false, has_right = false;
    with_view_model(
        view->view,
        RfsCaptureViewModel * model,
        {
            has_left = model->left[0];
            has_center = model->center[0];
            has_right = model->right[0];
        },
        false);
    switch(event->key) {
    case InputKeyLeft:
        if(has_left && view->callback) view->callback(RfsCaptureViewEventOptions, view->context);
        return true;
    case InputKeyOk:
        if(has_center && view->callback) view->callback(RfsCaptureViewEventStop, view->context);
        return true;
    case InputKeyRight:
        if(has_right && view->callback) view->callback(RfsCaptureViewEventRestart, view->context);
        return true;
    case InputKeyBack:
        return false;
    default:
        return true;
    }
}

RfsCaptureView* rfs_capture_view_alloc(void) {
    RfsCaptureView* view = malloc(sizeof(RfsCaptureView));
    view->callback = NULL;
    view->context = NULL;
    view->view = view_alloc();
    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(RfsCaptureViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, rfs_capture_view_draw);
    view_set_input_callback(view->view, rfs_capture_view_input);
    with_view_model(
        view->view,
        RfsCaptureViewModel * model,
        { memset(model, 0, sizeof(RfsCaptureViewModel)); },
        false);
    return view;
}

void rfs_capture_view_free(RfsCaptureView* view) {
    view_free(view->view);
    free(view);
}

View* rfs_capture_view_get_view(RfsCaptureView* view) {
    return view->view;
}

void rfs_capture_view_set_callback(
    RfsCaptureView* view,
    RfsCaptureViewCallback callback,
    void* context) {
    view->callback = callback;
    view->context = context;
}

void rfs_capture_view_set_text(
    RfsCaptureView* view,
    const char* title,
    const char* line1,
    const char* line2,
    const char* line3) {
    with_view_model(
        view->view,
        RfsCaptureViewModel * model,
        {
            strlcpy(model->title, title ? title : "", sizeof(model->title));
            strlcpy(model->line1, line1 ? line1 : "", sizeof(model->line1));
            strlcpy(model->line2, line2 ? line2 : "", sizeof(model->line2));
            strlcpy(model->line3, line3 ? line3 : "", sizeof(model->line3));
        },
        true);
}

void rfs_capture_view_set_rssi(RfsCaptureView* view, bool show, float rssi, int32_t threshold) {
    with_view_model(
        view->view,
        RfsCaptureViewModel * model,
        {
            model->show_rssi = show;
            model->rssi = rssi;
            model->threshold = threshold;
        },
        true);
}

void rfs_capture_view_set_buttons(
    RfsCaptureView* view,
    const char* left,
    const char* center,
    const char* right) {
    with_view_model(
        view->view,
        RfsCaptureViewModel * model,
        {
            strlcpy(model->left, left ? left : "", sizeof(model->left));
            strlcpy(model->center, center ? center : "", sizeof(model->center));
            strlcpy(model->right, right ? right : "", sizeof(model->right));
        },
        true);
}
