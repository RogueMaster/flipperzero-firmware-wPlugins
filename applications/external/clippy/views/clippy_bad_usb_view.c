#include "clippy_bad_usb_view.h"
#include "../helpers/ducky_script.h"
#include <toolbox/path.h>
#include <gui/elements.h>
#include "clippy_app_i.h"

#define MAX_NAME_LEN 64

struct ClippyBadUsb {
    View* view;
    ClippyBadUsbButtonCallback callback;
    void* context;
};

typedef struct {
    char layout[MAX_NAME_LEN];
    ClippyBadUsbState state;
    uint16_t string_idx;
    uint16_t string_length;
    uint8_t anim_frame;
} ClippyBadUsbModel;

static void clippy_bad_usb_draw_callback(Canvas* canvas, void* _model) {
    ClippyBadUsbModel* model = _model;

    FuriString* disp_str = furi_string_alloc();
    canvas_set_font(canvas, FontSecondary);

    if(strlen(model->layout) == 0) {
        furi_string_set(disp_str, "(default)");
    } else {
        furi_string_printf(disp_str, "(%s)", model->layout);
    }
    elements_string_fit_width(canvas, disp_str, 128 - 2);
    canvas_draw_str(
        canvas, 2, 8 + canvas_current_font_height(canvas), furi_string_get_cstr(disp_str));

    furi_string_reset(disp_str);

    canvas_draw_icon(canvas, 22, 24, &I_UsbTree_48x22);

    ClippyBadUsbWorkerState state = model->state.state;

    if((state == ClippyBadUsbStateIdle) || (state == ClippyBadUsbStateDone) ||
       (state == ClippyBadUsbStateNotConnected)) {
        elements_button_center(canvas, "Run");
        elements_button_left(canvas, "Config");
    } else if(state == ClippyBadUsbStateRunning) {
        elements_button_center(canvas, "Stop");
    } else if(state == ClippyBadUsbStateWaitForBtn) {
        elements_button_center(canvas, "Press to continue");
    } else if(state == ClippyBadUsbStateWillRun) {
        elements_button_center(canvas, "Cancel");
    }

    if(state == ClippyBadUsbStateNotConnected) {
        canvas_draw_icon(canvas, 4, 26, &I_Clock_18x18);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 127, 31, AlignRight, AlignBottom, "Connect");
        canvas_draw_str_aligned(canvas, 127, 43, AlignRight, AlignBottom, "to device");
    } else if(state == ClippyBadUsbStateWillRun) {
        canvas_draw_icon(canvas, 4, 26, &I_Clock_18x18);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 127, 31, AlignRight, AlignBottom, "Will run");
        canvas_draw_str_aligned(canvas, 127, 43, AlignRight, AlignBottom, "on connect");
    } else if(state == ClippyBadUsbStateInitError) {
        canvas_draw_icon(canvas, 4, 26, &I_Error_18x18);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 127, 31, AlignRight, AlignBottom, "Init");
        canvas_draw_str_aligned(canvas, 127, 43, AlignRight, AlignBottom, "ERROR");
    } else if(state == ClippyBadUsbStateIdle) {
        canvas_draw_icon(canvas, 4, 26, &I_Smile_18x18);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 114, 40, AlignRight, AlignBottom, "0");
        canvas_draw_icon(canvas, 117, 26, &I_Percent_10x14);
    } else if(state == ClippyBadUsbStateRunning) {
        canvas_draw_icon(canvas, 4, 25, &I_clippy_18x18);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(
            disp_str, "%zu", ((model->state.string_idx) * 100) / (model->state.string_length));
        canvas_draw_str_aligned(
            canvas, 114, 40, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        furi_string_reset(disp_str);
        canvas_draw_icon(canvas, 117, 26, &I_Percent_10x14);
    } else if(state == ClippyBadUsbStateDone) {
        canvas_draw_icon(canvas, 4, 25, &I_Smile_18x18);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 114, 40, AlignRight, AlignBottom, "100");
        furi_string_reset(disp_str);
        canvas_draw_icon(canvas, 117, 26, &I_Percent_10x14);
    } else if(state == ClippyBadUsbStateWaitForBtn) {
        canvas_draw_icon(canvas, 4, 25, &I_clippy_18x18);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_reset(disp_str);
        canvas_draw_icon(canvas, 117, 26, &I_Percent_10x14);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 127, 50, AlignRight, AlignBottom, "Paused");
        furi_string_reset(disp_str);
    } else {
        canvas_draw_icon(canvas, 4, 26, &I_Clock_18x18);
    }

    furi_string_free(disp_str);
}

static bool clippy_bad_usb_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    ClippyBadUsb* bad_usb = context;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            consumed = true;
            furi_assert(bad_usb->callback);
            bad_usb->callback(event->key, bad_usb->context);
        } else if(event->key == InputKeyOk) {
            consumed = true;
            furi_assert(bad_usb->callback);
            bad_usb->callback(event->key, bad_usb->context);
        }
    }

    return consumed;
}

ClippyBadUsb* clippy_bad_usb_view_alloc(void) {
    ClippyBadUsb* bad_usb = malloc(sizeof(ClippyBadUsb));

    bad_usb->view = view_alloc();
    view_allocate_model(bad_usb->view, ViewModelTypeLocking, sizeof(ClippyBadUsbModel));
    view_set_context(bad_usb->view, bad_usb);
    view_set_draw_callback(bad_usb->view, clippy_bad_usb_draw_callback);
    view_set_input_callback(bad_usb->view, clippy_bad_usb_input_callback);

    return bad_usb;
}

void clippy_bad_usb_view_free(ClippyBadUsb* bad_usb) {
    furi_assert(bad_usb);
    view_free(bad_usb->view);
    free(bad_usb);
}

View* clippy_bad_usb_view_get_view(ClippyBadUsb* bad_usb) {
    furi_assert(bad_usb);
    return bad_usb->view;
}

void clippy_bad_usb_view_set_button_callback(
    ClippyBadUsb* bad_usb,
    ClippyBadUsbButtonCallback callback,
    void* context) {
    furi_assert(bad_usb);
    furi_assert(callback);
    with_view_model(
        bad_usb->view,
        ClippyBadUsbModel * model,
        {
            UNUSED(model);
            bad_usb->callback = callback;
            bad_usb->context = context;
        },
        true);
}

void clippy_bad_usb_view_set_layout(ClippyBadUsb* bad_usb, const char* layout) {
    furi_assert(layout);
    with_view_model(
        bad_usb->view,
        ClippyBadUsbModel * model,
        { strlcpy(model->layout, layout, MAX_NAME_LEN); },
        true);
}

void clippy_bad_usb_view_set_state(ClippyBadUsb* bad_usb, ClippyBadUsbState* st) {
    furi_assert(st);
    with_view_model(
        bad_usb->view,
        ClippyBadUsbModel * model,
        {
            memcpy(&(model->state), st, sizeof(ClippyBadUsbState));
            model->anim_frame ^= 1;
        },
        true);
}

bool clippy_bad_usb_view_is_idle_state(ClippyBadUsb* bad_usb) {
    bool is_idle = false;
    with_view_model(
        bad_usb->view,
        ClippyBadUsbModel * model,
        {
            if((model->state.state == ClippyBadUsbStateIdle) ||
               (model->state.state == ClippyBadUsbStateDone) ||
               (model->state.state == ClippyBadUsbStateNotConnected)) {
                is_idle = true;
            }
        },
        false);
    return is_idle;
}
