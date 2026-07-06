#include "xremote_transmit.h"

struct XRemoteTransmit {
    View* view;
    XRemoteTransmitCallback callback;
    void* context;
};

typedef struct {
    int type;
    const char* name;
    int time; // animation frame counter (0..2)
    int remaining; // seconds left in the active pause (for the countdown)
} XRemoteTransmitModel;

static void xremote_transmit_model_init(XRemoteTransmitModel* const model) {
    model->type = XRemoteRemoteItemTypeInfrared;
    model->time = 1;
    model->remaining = 0;
}

void xremote_transmit_model_set_name(XRemoteTransmit* instance, const char* name) {
    furi_assert(instance);
    XRemoteTransmitModel* model = view_get_model(instance->view);
    model->name = name;
    view_commit_model(instance->view, false);
}

void xremote_transmit_model_set_type(XRemoteTransmit* instance, int type) {
    furi_assert(instance);
    XRemoteTransmitModel* model = view_get_model(instance->view);
    model->time = 1;
    model->type = type;
    view_commit_model(instance->view, false);
}

void xremote_transmit_model_set_remaining(XRemoteTransmit* instance, int remaining) {
    furi_assert(instance);
    XRemoteTransmitModel* model = view_get_model(instance->view);
    model->remaining = remaining;
    view_commit_model(instance->view, true);
}

void xremote_transmit_set_callback(
    XRemoteTransmit* instance,
    XRemoteTransmitCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

void xremote_transmit_draw_ir(Canvas* canvas, XRemoteTransmitModel* model) {
    model->time++;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, 0, 0, &I_ir_transmit_128x64);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 74, 5, AlignLeft, AlignTop, "Sending");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 74, 15, AlignLeft, AlignTop, "Infrared");
    canvas_draw_str_aligned(canvas, 74, 25, AlignLeft, AlignTop, model->name);

    if(model->time == 0) {
        canvas_draw_icon(canvas, 36, 2, &I_ir_ani_1_32x22);
    } else if(model->time == 1) {
        canvas_draw_icon(canvas, 36, 2, &I_ir_ani_2_32x22);
    } else if(model->time == 2) {
        canvas_draw_icon(canvas, 36, 2, &I_ir_ani_3_32x22);
    }
}

void xremote_transmit_draw_pause(Canvas* canvas, XRemoteTransmitModel* model) {
    model->time++;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, 0, 0, &I_pause_128x64);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 74, 5, AlignLeft, AlignTop, "Waiting");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 74, 15, AlignLeft, AlignTop, "Sequence");
    canvas_draw_str_aligned(canvas, 74, 25, AlignLeft, AlignTop, model->name);

    // Live MM:SS countdown of the remaining pause time. Drawn on a white,
    // framed box so it stays legible on top of the pause background artwork.
    char countdown[24];
    snprintf(
        countdown, sizeof(countdown), "%02d:%02d", model->remaining / 60, model->remaining % 60);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 76, 34, 52, 16);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 76, 34, 52, 16);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 104, 42, AlignCenter, AlignCenter, countdown);

    if(model->time == 0) {
        canvas_draw_icon(canvas, 9, 28, &I_pause_ani_1_22x23);
    } else if(model->time == 1) {
        canvas_draw_icon(canvas, 9, 28, &I_pause_ani_2_22x23);
    } else if(model->time == 2) {
        canvas_draw_icon(canvas, 9, 28, &I_pause_ani_3_22x23);
    }
}

void xremote_transmit_draw_subghz(Canvas* canvas, XRemoteTransmitModel* model) {
    model->time++;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, 0, 0, &I_sg_transmit_128x64);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 74, 5, AlignLeft, AlignTop, "Sending");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 74, 15, AlignLeft, AlignTop, "SubGhz");
    canvas_draw_str_aligned(canvas, 74, 25, AlignLeft, AlignTop, model->name);

    if(model->time == 0) {
        canvas_draw_icon(canvas, 15, 1, &I_sg_ani_1_19x13);
    } else if(model->time == 1) {
        canvas_draw_icon(canvas, 15, 1, &I_sg_ani_2_19x13);
    } else if(model->time == 2) {
        canvas_draw_icon(canvas, 15, 1, &I_sg_ani_3_19x13);
    }
}

void xremote_transmit_draw(Canvas* canvas, XRemoteTransmitModel* model) {
    if(model->type == XRemoteRemoteItemTypeInfrared) {
        xremote_transmit_draw_ir(canvas, model);
    } else if(model->type == XRemoteRemoteItemTypeSubGhz) {
        xremote_transmit_draw_subghz(canvas, model);
    } else if(model->type == XRemoteRemoteItemTypePause) {
        xremote_transmit_draw_pause(canvas, model);
    }
    if(model->time > 2) {
        model->time = 0;
    }
    canvas_set_font(canvas, FontSecondary);
    elements_button_right(canvas, "exit");
}

bool xremote_transmit_input(InputEvent* event, void* context) {
    furi_assert(context);
    XRemoteTransmit* instance = context;
    if(event->type == InputTypeRelease) {
        switch(event->key) {
        case InputKeyBack:
        case InputKeyRight:
            with_view_model(
                instance->view,
                XRemoteTransmitModel * model,
                {
                    UNUSED(model);
                    instance->callback(
                        XRemoteCustomEventViewTransmitterSendStop, instance->context);
                },
                true);
            break;
        default:
            break;
        }
    }
    return true;
}

void xremote_transmit_enter(void* context) {
    furi_assert(context);
    XRemoteTransmit* instance = (XRemoteTransmit*)context;
    with_view_model(
        instance->view,
        XRemoteTransmitModel * model,
        { xremote_transmit_model_init(model); },
        true);
}

XRemoteTransmit* xremote_transmit_alloc() {
    XRemoteTransmit* instance = malloc(sizeof(XRemoteTransmit));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(XRemoteTransmitModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)xremote_transmit_draw);
    view_set_input_callback(instance->view, xremote_transmit_input);
    //view_set_enter_callback(instance->view, xremote_transmit_enter);

    with_view_model(
        instance->view,
        XRemoteTransmitModel * model,
        { xremote_transmit_model_init(model); },
        true);

    return instance;
}

void xremote_transmit_free(XRemoteTransmit* instance) {
    furi_assert(instance);

    with_view_model(instance->view, XRemoteTransmitModel * model, { UNUSED(model); }, true);
    view_free(instance->view);
    free(instance);
}

View* xremote_transmit_get_view(XRemoteTransmit* instance) {
    furi_assert(instance);
    return instance->view;
}
