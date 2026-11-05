#include "nfc_rfid_detector_view_field_presence.h"
#include "../nfc_rfid_detector_app_i.h"
#include <nfc_rfid_detector_icons.h>

#include <assets_icons.h>

#include <input/input.h>
#include <gui/elements.h>
#include "nfc_rfid_detector_draw.h"

#define FIELD_FOUND_WEIGHT 5

typedef enum {
    NfcRfidDetectorTypeFieldPresenceNfc,
    NfcRfidDetectorTypeFieldPresenceRfid,
} NfcRfidDetectorTypeFieldPresence;

static const Icon* NfcRfidDetectorFieldPresenceIcons[] = {
    [NfcRfidDetectorTypeFieldPresenceNfc] = &I_NFC_detect_45x30,
    [NfcRfidDetectorTypeFieldPresenceRfid] = &I_Rfid_detect_45x30,
};

struct NfcRfidDetectorFieldPresence {
    View* view;
    NfcRfidDetectorFieldPresenceCallback callback;
    void* context;
};

typedef struct {
    uint8_t nfc_field;
    uint8_t rfid_field;
    uint32_t rfid_frequency;
    uint64_t rfid_edges;
    uint32_t rfid_gates;
    uint32_t rfid_runs;
    int16_t cal; // 0.1 ppm
    int16_t cal_sig; // its ±, 0.1 ppm (< 0 means not known)
    bool restart;
    bool rfid_held; // result kept from a field that has gone
    bool rfid_settling; // carrier present, not yet steady
    uint8_t rfid_tick; // numbers refresh every LF_DISPLAY_TICKS updates
} NfcRfidDetectorFieldPresenceModel;

#define LF_PRECISE_MIN_GATES 10 // gates before the whole-hold figure replaces the single-gate one
// LF readers run at 125 kHz or 134.2 kHz. The offset line compares the field with the nearer one.
#define LF_NOMINAL_LOW_HZ    ((double)125000)
#define LF_NOMINAL_HIGH_HZ   ((double)134200)
#define LF_PERCENT_FROM_PPM  ((double)1000) // offsets this large show in % not ppm, without the ±
#define LF_DISPLAY_TICKS     5 // updates between number refreshes

void nfc_rfid_detector_view_field_presence_update(
    NfcRfidDetectorFieldPresence* instance,
    bool nfc_field,
    const NfcRfidDetectorLfMeterResult* lf_result) {
    furi_assert(instance);
    furi_assert(lf_result);
    with_view_model(
        instance->view,
        NfcRfidDetectorFieldPresenceModel * model,
        {
            // Keep the NFC view until NFC has been gone for FIELD_FOUND_WEIGHT updates (0.5 s).
            if(nfc_field) {
                model->nfc_field = FIELD_FOUND_WEIGHT;
            } else if(model->nfc_field) {
                model->nfc_field--;
            }
            if(lf_result->present) {
                model->rfid_field = FIELD_FOUND_WEIGHT;
            } else if(model->rfid_field) {
                model->rfid_field--;
            }
            // Clear the held LF result once NFC is present and LF has been gone for FIELD_FOUND_WEIGHT updates (0.5 s).
            // A reader that alternates between LF and NFC within that time keeps the LF result on screen.
            if(lf_result->held && model->nfc_field && !model->rfid_field) model->restart = true;
            // refresh the numbers slowly enough to read, but show a change of state immediately
            bool state_changed = lf_result->held != model->rfid_held ||
                                 lf_result->settling != model->rfid_settling ||
                                 lf_result->gates < model->rfid_gates;
            if(state_changed || ++model->rfid_tick >= LF_DISPLAY_TICKS) {
                model->rfid_tick = 0;
                // a held result keeps its last single-gate figure
                if(lf_result->present && !lf_result->held) {
                    model->rfid_frequency = lf_result->last_gate_edges * 10;
                }
                model->rfid_edges = lf_result->edges;
                model->rfid_gates = lf_result->gates;
                model->rfid_runs = lf_result->runs;
                model->rfid_held = lf_result->held;
                model->rfid_settling = lf_result->settling;
            }
        },
        true);
}

// "calibration +16.9 ±0.7 ppm" centred on the line above the buttons, with monospace figures like the offset line.
// The ± is shown if known.
static void
    nfc_rfid_detector_view_field_presence_draw_cal(Canvas* canvas, int16_t cal, int16_t cal_sig) {
    int decimals = nfc_rfid_detector_draw_decimals(cal_sig / (double)10, 1);
    char value[16];
    char sig[16] = "";
    snprintf(value, sizeof(value), "%+.*f", decimals, cal / (double)10);
    if(cal_sig >= 0) snprintf(sig, sizeof(sig), "%.*f", decimals, cal_sig / (double)10);
    canvas_set_font(canvas, FontSecondary);
    int32_t space_width = canvas_string_width(canvas, " ");
    int32_t width = canvas_string_width(canvas, " ppm");
    if(cal_sig >= 0) width += space_width + 6;
    canvas_set_font(canvas, FontKeyboard);
    width += canvas_string_width(canvas, value) + canvas_string_width(canvas, sig);
    canvas_set_font(canvas, FontSecondary);
    const char* label = "calibration ";
    if(width + canvas_string_width(canvas, label) > 126) label = "cal. ";
    width += canvas_string_width(canvas, label);
    int32_t x = 64 - width / 2;
    canvas_draw_str(canvas, x, 48, label);
    x += canvas_string_width(canvas, label);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, x, 48, value);
    x += canvas_string_width(canvas, value);
    if(cal_sig >= 0) {
        x += space_width;
        x += nfc_rfid_detector_draw_pm(canvas, x, 48);
        canvas_draw_str(canvas, x, 48, sig);
        x += canvas_string_width(canvas, sig);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x, 48, " ppm");
}

// Display the offset from whichever standard LF RFID frequency is closest out of 125 kHz or 134.2 kHz, using a ppm
// offset, or percentage when the offset is large, e.g. "125 kHz -103.8 ±0.1 ppm", or "134.2 kHz +0.46%". The figures
// are monospace, so the width remains the same when individual digits change.
static void nfc_rfid_detector_view_field_presence_draw_offset(
    Canvas* canvas,
    double nominal_hz,
    double ppm,
    double uncertainty_ppm) {
    char value[16];
    char sig[8] = "";
    // ±1 edge per unbroken run
    int ppm_decimals = nfc_rfid_detector_draw_decimals(uncertainty_ppm, 1);
    // Switch on the figure as it would print (999.96 prints as 1000.0)
    double ppm_rounding = ppm_decimals ? (double)0.05 : (double)0.5;
    bool in_ppm = fabs(ppm) < LF_PERCENT_FROM_PPM - ppm_rounding;
    if(in_ppm) {
        if(fabs(ppm) < ppm_rounding) ppm = 0; // +0.0, not -0.0
        snprintf(value, sizeof(value), "%+.*f", ppm_decimals, ppm);
        snprintf(sig, sizeof(sig), "%.*f", ppm_decimals, uncertainty_ppm);
    } else {
        // two significant figures, decided after rounding (9.96 shows as 10, not 10.0)
        double percent = ppm / (double)1e4;
        int percent_decimals = 2;
        if(fabs(percent) >= (double)0.995) percent_decimals = 1;
        if(fabs(percent) >= (double)9.95) percent_decimals = 0;
        snprintf(value, sizeof(value), "%+.*f", percent_decimals, percent);
    }
    const char* nominal_str = nominal_hz == LF_NOMINAL_LOW_HZ ? "125 kHz " : "134.2 kHz ";
    const char* unit_str = in_ppm ? " ppm" : "%";
    canvas_set_font(canvas, FontSecondary);
    int32_t space_width = canvas_string_width(canvas, " ");
    int32_t width =
        canvas_string_width(canvas, nominal_str) + canvas_string_width(canvas, unit_str);
    if(in_ppm) width += space_width + 6;
    canvas_set_font(canvas, FontKeyboard);
    width += canvas_string_width(canvas, value) + canvas_string_width(canvas, sig);
    int32_t x = 64 - width / 2;
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x, 38, nominal_str);
    x += canvas_string_width(canvas, nominal_str);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, x, 38, value);
    x += canvas_string_width(canvas, value);
    if(in_ppm) {
        // value ± uncertainty, then the unit
        x += space_width;
        x += nfc_rfid_detector_draw_pm(canvas, x, 38);
        canvas_draw_str(canvas, x, 38, sig);
        x += canvas_string_width(canvas, sig);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x, 38, unit_str);
}

void nfc_rfid_detector_view_field_presence_draw(
    Canvas* canvas,
    NfcRfidDetectorFieldPresenceModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    bool show_rfid = model->rfid_field || model->rfid_held;
    if(!model->nfc_field && !show_rfid) {
        // clear of the Config button
        canvas_draw_icon(canvas, 0, 12, &I_Modern_reader_18x34);
        canvas_draw_icon(canvas, 22, 8, &I_Move_flipper_26x39);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 56, 32, "Touch the reader");
        elements_button_left(canvas, "Config");
    } else {
        if(model->nfc_field) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 21, 10, "NFC");
            canvas_draw_icon(
                canvas,
                9,
                17,
                NfcRfidDetectorFieldPresenceIcons[NfcRfidDetectorTypeFieldPresenceNfc]);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 9, 62, "13.56 MHz");
        }

        if(show_rfid) {
            bool precise = model->rfid_gates >= LF_PRECISE_MIN_GATES;
            double measured_hz = precise ?
                                     (double)model->rfid_edges * (double)10 / model->rfid_gates :
                                     (double)model->rfid_frequency;
            // Add cal back. A fast crystal shortens every 100 ms gate, lowering the count by cal.
            double hz = measured_hz * ((double)1 + model->cal / (double)1e7);
            double nominal_hz = hz < (LF_NOMINAL_LOW_HZ + LF_NOMINAL_HIGH_HZ) / 2 ?
                                    LF_NOMINAL_LOW_HZ :
                                    LF_NOMINAL_HIGH_HZ;
            double ppm = (hz / nominal_hz - (double)1) * (double)1e6;
            char str[32];

            if(!model->nfc_field) {
                // LF only: display a detailed view during measurement
                // monospace figures at fixed anchors stop lines from flickering as numbers change
                bool held = model->rfid_held;
                canvas_set_font(canvas, FontPrimary);
                canvas_draw_str(canvas, 2, 9, "LF RFID");
                // "held 12 s": words in regular font, number monospace
                canvas_set_font(canvas, FontSecondary);
                uint8_t x = 126 - canvas_string_width(canvas, " s");
                canvas_draw_str(canvas, x, 9, " s");
                canvas_set_font(canvas, FontKeyboard);
                snprintf(str, sizeof(str), "%lu", model->rfid_gates / 10);
                x -= canvas_string_width(canvas, str);
                canvas_draw_str(canvas, x, 9, str);
                if(held) {
                    canvas_set_font(canvas, FontSecondary);
                    canvas_draw_str_aligned(canvas, x, 9, AlignRight, AlignBottom, "held ");
                }

                // Only show the digits supported by the current level of uncertainty.
                double uncertainty_ppm = 0;
                int khz_decimals = 2; // a single gate: 10 Hz
                if(precise) {
                    uncertainty_ppm = (double)1e6 * model->rfid_runs / (double)model->rfid_edges;
                    double uncertainty_hz = uncertainty_ppm * hz / (double)1e6;
                    khz_decimals = uncertainty_hz < (double)1 ? 4 : 3;
                }
                // value right-aligned on the last digit
                canvas_set_font(canvas, FontBigNumbers);
                snprintf(str, sizeof(str), "%.*f", khz_decimals, hz / (double)1000);
                canvas_draw_str_aligned(canvas, 108, 27, AlignRight, AlignBottom, str);
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(canvas, 110, 27, "kHz");

                if(precise) {
                    nfc_rfid_detector_view_field_presence_draw_offset(
                        canvas, nominal_hz, ppm, uncertainty_ppm);
                } else {
                    canvas_set_font(canvas, FontSecondary);
                    canvas_draw_str_aligned(
                        canvas,
                        64,
                        38,
                        AlignCenter,
                        AlignBottom,
                        model->rfid_settling ? "settling..." : "hold still...");
                }
                canvas_set_font(canvas, FontSecondary);
                nfc_rfid_detector_view_field_presence_draw_cal(canvas, model->cal, model->cal_sig);
                elements_button_left(canvas, "Config");
                elements_button_center(canvas, "Clear");
            } else {
                // both fields: NFC on the left, LF RFID on the right
                snprintf(str, sizeof(str), "%.*f kHz", precise ? 3 : 2, hz / (double)1000);
                canvas_set_font(canvas, FontPrimary);
                canvas_draw_str(canvas, 76, 10, "LF RFID");
                canvas_draw_icon(
                    canvas,
                    71,
                    17,
                    NfcRfidDetectorFieldPresenceIcons[NfcRfidDetectorTypeFieldPresenceRfid]);
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(canvas, 69, 62, str);
            }
        }
    }
}

bool nfc_rfid_detector_view_field_presence_input(InputEvent* event, void* context) {
    furi_assert(context);
    NfcRfidDetectorFieldPresence* instance = context;

    if(event->key == InputKeyBack) {
        return false;
    }

    if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        if(instance->callback)
            instance->callback(NfcRfidDetectorCustomEventConfig, instance->context);
        return true;
    }

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        with_view_model(
            instance->view,
            NfcRfidDetectorFieldPresenceModel * model,
            { model->restart = true; },
            false);
    }

    return true;
}

bool nfc_rfid_detector_view_field_presence_take_restart(NfcRfidDetectorFieldPresence* instance) {
    furi_assert(instance);
    bool restart = false;
    with_view_model(
        instance->view,
        NfcRfidDetectorFieldPresenceModel * model,
        {
            restart = model->restart;
            model->restart = false;
        },
        false);
    return restart;
}

void nfc_rfid_detector_view_field_presence_set_callback(
    NfcRfidDetectorFieldPresence* instance,
    NfcRfidDetectorFieldPresenceCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void nfc_rfid_detector_view_field_presence_set_cal(
    NfcRfidDetectorFieldPresence* instance,
    int16_t cal,
    int16_t cal_sig) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        NfcRfidDetectorFieldPresenceModel * model,
        {
            model->cal = cal;
            model->cal_sig = cal_sig;
        },
        true);
}

void nfc_rfid_detector_view_field_presence_enter(void* context) {
    furi_assert(context);
    NfcRfidDetectorFieldPresence* instance = context;
    with_view_model(
        instance->view,
        NfcRfidDetectorFieldPresenceModel * model,
        {
            // preserve LF figures so the result still shows after visiting Config
            model->nfc_field = 0;
            model->rfid_field = 0;
            model->restart = false;
            model->rfid_tick = 0;
        },
        true);
}

void nfc_rfid_detector_view_field_presence_exit(void* context) {
    furi_assert(context);
    NfcRfidDetectorFieldPresence* instance = context;
    UNUSED(instance);
}

NfcRfidDetectorFieldPresence* nfc_rfid_detector_view_field_presence_alloc() {
    NfcRfidDetectorFieldPresence* instance = malloc(sizeof(NfcRfidDetectorFieldPresence));

    // View allocation and configuration
    instance->view = view_alloc();
    instance->callback = NULL;

    view_allocate_model(
        instance->view, ViewModelTypeLocking, sizeof(NfcRfidDetectorFieldPresenceModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(
        instance->view, (ViewDrawCallback)nfc_rfid_detector_view_field_presence_draw);
    view_set_input_callback(instance->view, nfc_rfid_detector_view_field_presence_input);
    view_set_enter_callback(instance->view, nfc_rfid_detector_view_field_presence_enter);
    view_set_exit_callback(instance->view, nfc_rfid_detector_view_field_presence_exit);

    with_view_model(
        instance->view,
        NfcRfidDetectorFieldPresenceModel * model,
        {
            model->nfc_field = 0;
            model->rfid_field = 0;
            model->rfid_frequency = 0;
            model->rfid_edges = 0;
            model->rfid_gates = 0;
            model->restart = false;
            model->rfid_held = false;
            model->rfid_settling = false;
            model->rfid_tick = 0;
        },
        true);
    return instance;
}

void nfc_rfid_detector_view_field_presence_free(NfcRfidDetectorFieldPresence* instance) {
    furi_assert(instance);

    view_free(instance->view);
    free(instance);
}

View* nfc_rfid_detector_view_field_presence_get_view(NfcRfidDetectorFieldPresence* instance) {
    furi_assert(instance);
    return instance->view;
}
