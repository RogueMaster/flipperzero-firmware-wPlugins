#include "nfc_rfid_detector_view_cal_status.h"

#include "nfc_rfid_detector_draw.h"

#include <gui/elements.h>
#include <nfc_rfid_detector_icons.h>
#include <input/input.h>
#include <stdio.h>

// Progress of the calibration using a computer, followed by the result.
// The scene saves the calibration result to the Flipper.

struct NfcRfidDetectorCalStatus {
    View* view;
    NfcRfidDetectorCalStatusCallback callback;
    void* context;
};

typedef struct {
    NfcRfidDetectorCalStatusState state;
    uint16_t elapsed;
    uint16_t total; // 0: no progress received yet
    int16_t estimate;
    int16_t sig; // < 0: no estimate yet
    uint32_t progress_tick; // when the last progress arrived
    int16_t saved;
    int16_t saved_sig; // < 0: not known
    int16_t previous;
} NfcRfidDetectorCalStatusModel;

static void
    nfc_rfid_detector_view_cal_status_draw(Canvas* canvas, NfcRfidDetectorCalStatusModel* model) {
    char str[32];
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "Computer calibration");

    if(model->state == NfcRfidDetectorCalStatusSaved) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignBottom, "Saved");
        // the value in big digits, then "±0.7 ppm" small, centred as one line
        int decimals = nfc_rfid_detector_draw_decimals(model->saved_sig / (double)10, 1);
        char unit[16];
        if(model->saved_sig >= 0) {
            snprintf(unit, sizeof(unit), "%.*f ppm", decimals, model->saved_sig / (double)10);
        } else {
            snprintf(unit, sizeof(unit), "ppm");
        }
        snprintf(str, sizeof(str), "%+.*f", decimals, model->saved / (double)10);
        canvas_set_font(canvas, FontBigNumbers);
        int32_t value_width = canvas_string_width(canvas, str);
        canvas_set_font(canvas, FontSecondary);
        int32_t pm_width = model->saved_sig >= 0 ? 6 : 0;
        int32_t line_width = value_width + 3 + pm_width + canvas_string_width(canvas, unit);
        int32_t x = 64 - line_width / 2;
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, x, 37, str);
        x += value_width + 3;
        canvas_set_font(canvas, FontSecondary);
        if(model->saved_sig >= 0) x += nfc_rfid_detector_draw_pm(canvas, x, 37);
        canvas_draw_str(canvas, x, 37, unit);
        snprintf(str, sizeof(str), "was %+.1f ppm", model->previous / (double)10);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, str);
        elements_button_center(canvas, "OK");
        return;
    }

    canvas_set_font(canvas, FontSecondary);
    if(model->state == NfcRfidDetectorCalStatusEnded) {
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignBottom, "Session ended");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, "nothing saved");
        elements_button_center(canvas, "OK");
        return;
    }

    const char* what = "Measuring Flipper crystal";
    if(canvas_string_width(canvas, what) > 124) what = "Measuring crystal";
    canvas_draw_str_aligned(canvas, 64, 21, AlignCenter, AlignBottom, what);
    canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignBottom, "against internet time");
    if(model->total) {
        // keep counting between the computer's reports
        uint32_t elapsed = model->elapsed + (furi_get_tick() - model->progress_tick) / 1000;
        if(elapsed > model->total) elapsed = model->total;
        uint32_t left = model->total - elapsed;
        elements_progress_bar_with_text(
            canvas, 4, 35, 120, (float)elapsed / (float)model->total, "");
        // the time monospace and padded to a fixed width
        snprintf(str, sizeof(str), "%2lu:%02lu", left / 60, left % 60);
        canvas_set_font(canvas, FontKeyboard);
        int32_t time_width = canvas_string_width(canvas, str);
        canvas_set_font(canvas, FontSecondary);
        int32_t x = 64 - (time_width + canvas_string_width(canvas, " left")) / 2;
        canvas_set_color(canvas, ColorXOR);
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, x, 44, str);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, x + time_width, 44, " left");
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_set_font(canvas, FontSecondary);
    if(model->sig >= 0) {
        int decimals = nfc_rfid_detector_draw_decimals(model->sig / (double)10, 1);
        snprintf(str, sizeof(str), "%+.*f ", decimals, model->estimate / (double)10);
        canvas_draw_str(canvas, 2, 62, str);
        int32_t x = 2 + canvas_string_width(canvas, str);
        x += nfc_rfid_detector_draw_pm(canvas, x, 62);
        snprintf(str, sizeof(str), "%.*f", decimals, model->sig / (double)10);
        canvas_draw_str(canvas, x, 62, str);
    }
    // Back cancels, bottom right where the key is
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "Cancel");
    canvas_draw_icon(
        canvas, 126 - canvas_string_width(canvas, "Cancel") - 13, 54, &I_Back_arrow_10x8);
}

static bool nfc_rfid_detector_view_cal_status_input(InputEvent* event, void* context) {
    NfcRfidDetectorCalStatus* instance = context;
    // OK appears only when done. Leaving Config would end a running session.
    bool done = false;
    with_view_model(
        instance->view,
        NfcRfidDetectorCalStatusModel * model,
        { done = model->state != NfcRfidDetectorCalStatusMeasuring; },
        false);
    if(!instance->callback) return false;
    // While measuring, keep every Back event here. Only a short press cancels.
    if(!done && event->key == InputKeyBack) {
        if(event->type == InputTypeShort) {
            instance->callback(NfcRfidDetectorCustomEventCalStatusCancel, instance->context);
        }
        return true;
    }
    if(done && event->key == InputKeyOk && event->type == InputTypeShort) {
        instance->callback(NfcRfidDetectorCustomEventCalStatusOk, instance->context);
        return true;
    }
    return false;
}

NfcRfidDetectorCalStatus* nfc_rfid_detector_view_cal_status_alloc(void) {
    NfcRfidDetectorCalStatus* instance = malloc(sizeof(NfcRfidDetectorCalStatus));
    instance->view = view_alloc();
    instance->callback = NULL;
    view_allocate_model(
        instance->view, ViewModelTypeLocking, sizeof(NfcRfidDetectorCalStatusModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(
        instance->view, (ViewDrawCallback)nfc_rfid_detector_view_cal_status_draw);
    view_set_input_callback(instance->view, nfc_rfid_detector_view_cal_status_input);
    nfc_rfid_detector_view_cal_status_set_state(instance, NfcRfidDetectorCalStatusMeasuring);
    return instance;
}

void nfc_rfid_detector_view_cal_status_free(NfcRfidDetectorCalStatus* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* nfc_rfid_detector_view_cal_status_get_view(NfcRfidDetectorCalStatus* instance) {
    furi_assert(instance);
    return instance->view;
}

void nfc_rfid_detector_view_cal_status_set_callback(
    NfcRfidDetectorCalStatus* instance,
    NfcRfidDetectorCalStatusCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void nfc_rfid_detector_view_cal_status_set_state(
    NfcRfidDetectorCalStatus* instance,
    NfcRfidDetectorCalStatusState state) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        NfcRfidDetectorCalStatusModel * model,
        {
            model->state = state;
            if(state == NfcRfidDetectorCalStatusMeasuring) {
                model->total = 0;
                model->sig = -1;
            }
        },
        true);
}

void nfc_rfid_detector_view_cal_status_set_progress(
    NfcRfidDetectorCalStatus* instance,
    uint16_t elapsed,
    uint16_t total,
    int16_t estimate,
    int16_t sig) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        NfcRfidDetectorCalStatusModel * model,
        {
            model->elapsed = elapsed;
            model->total = total;
            model->progress_tick = furi_get_tick();
            model->estimate = estimate;
            model->sig = sig;
        },
        true);
}

void nfc_rfid_detector_view_cal_status_redraw(NfcRfidDetectorCalStatus* instance) {
    furi_assert(instance);
    with_view_model(
        instance->view, NfcRfidDetectorCalStatusModel * model, { UNUSED(model); }, true);
}

void nfc_rfid_detector_view_cal_status_set_saved(
    NfcRfidDetectorCalStatus* instance,
    int16_t saved,
    int16_t saved_sig,
    int16_t previous) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        NfcRfidDetectorCalStatusModel * model,
        {
            model->state = NfcRfidDetectorCalStatusSaved;
            model->saved = saved;
            model->saved_sig = saved_sig;
            model->previous = previous;
        },
        true);
}
