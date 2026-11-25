#include "pocsag_pager_tx.h"
#include <gui/elements.h>
#include <furi.h>
#include <stdio.h>
#include <string.h>

struct PCSGTxView {
    View* view;
    PCSGTxViewCallback callback;
    void* context;
};

typedef struct {
    uint32_t frequency;
    uint32_t ric;
    uint32_t baud;
    char type_name[16];
    char status_text[32];
    bool inverted;
    bool is_external;
    PCSGTxViewState state;
    float progress;
} PCSGTxViewModel;

static void pcsg_view_tx_draw_callback(Canvas* canvas, void* _model) {
    PCSGTxViewModel* model = _model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Top banner
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);

    char header[32];
    if(model->state == PCSGTxViewStateSending) {
        snprintf(header, sizeof(header), "Sending P%lu", (unsigned long)model->baud);
    } else if(model->state == PCSGTxViewStateDone) {
        snprintf(header, sizeof(header), "Sent! (P%lu)", (unsigned long)model->baud);
    } else if(model->state == PCSGTxViewStateBlocked) {
        snprintf(header, sizeof(header), "TX Blocked");
    } else {
        snprintf(header, sizeof(header), "TX Error");
    }
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, header);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char line1[40];
    snprintf(
        line1,
        sizeof(line1),
        "%lu.%04lu MHz (%s)",
        (unsigned long)(model->frequency / 1000000U),
        (unsigned long)((model->frequency % 1000000U) / 100U),
        model->is_external ? "Ext" : "Int");
    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, line1);

    char line2[40];
    snprintf(
        line2,
        sizeof(line2),
        "RIC:%lu  %s%s",
        (unsigned long)model->ric,
        model->type_name,
        model->inverted ? " INV" : "");
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, line2);

    if(model->state == PCSGTxViewStateBlocked || model->state == PCSGTxViewStateError) {
        elements_multiline_text_aligned(canvas, 64, 37, AlignCenter, AlignTop, model->status_text);
        elements_button_left(canvas, "Back");
    } else {
        float clamped = model->progress;
        if(clamped < 0.0f) clamped = 0.0f;
        if(clamped > 1.0f) clamped = 1.0f;
        elements_progress_bar_with_text(canvas, 6, 36, 116, clamped, model->status_text);

        if(model->state == PCSGTxViewStateSending) {
            elements_button_left(canvas, "Cancel");
        } else {
            elements_button_left(canvas, "Back");
            elements_button_center(canvas, "Resend");
        }
    }
}

static bool pcsg_view_tx_input_callback(InputEvent* event, void* context) {
    PCSGTxView* instance = context;
    furi_assert(instance);

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyBack || event->key == InputKeyLeft) {
        if(instance->callback) {
            instance->callback(PCSGTxViewEventBack, instance->context);
        }
        return true;
    }

    if(event->key == InputKeyOk) {
        PCSGTxViewState state = PCSGTxViewStateSending;
        with_view_model(instance->view, PCSGTxViewModel * model, { state = model->state; }, false);
        if(state == PCSGTxViewStateDone && instance->callback) {
            instance->callback(PCSGTxViewEventResend, instance->context);
            return true;
        }
    }

    return true;
}

PCSGTxView* pcsg_view_tx_alloc(void) {
    PCSGTxView* instance = malloc(sizeof(PCSGTxView));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(PCSGTxViewModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, pcsg_view_tx_draw_callback);
    view_set_input_callback(instance->view, pcsg_view_tx_input_callback);

    with_view_model(
        instance->view,
        PCSGTxViewModel * model,
        {
            memset(model, 0, sizeof(PCSGTxViewModel));
            model->state = PCSGTxViewStateSending;
        },
        false);

    return instance;
}

void pcsg_view_tx_free(PCSGTxView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* pcsg_view_tx_get_view(PCSGTxView* instance) {
    furi_assert(instance);
    return instance->view;
}

void pcsg_view_tx_set_callback(PCSGTxView* instance, PCSGTxViewCallback callback, void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void pcsg_view_tx_set_info(
    PCSGTxView* instance,
    uint32_t frequency,
    uint32_t ric,
    uint32_t baud,
    const char* type_name,
    bool inverted,
    bool is_external) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        PCSGTxViewModel * model,
        {
            model->frequency = frequency;
            model->ric = ric;
            model->baud = baud;
            strlcpy(model->type_name, type_name ? type_name : "", sizeof(model->type_name));
            model->inverted = inverted;
            model->is_external = is_external;
        },
        true);
}

void pcsg_view_tx_set_state(
    PCSGTxView* instance,
    PCSGTxViewState state,
    float progress,
    const char* status_text) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        PCSGTxViewModel * model,
        {
            model->state = state;
            model->progress = progress;
            strlcpy(
                model->status_text, status_text ? status_text : "", sizeof(model->status_text));
        },
        true);
}
