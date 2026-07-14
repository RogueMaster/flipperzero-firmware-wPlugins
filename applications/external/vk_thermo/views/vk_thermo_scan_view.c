#include "../vk_thermo.h"
#include "vk_thermo_scan_view.h"
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/elements.h>
#include <math.h>
#include "../helpers/vk_thermo_storage.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WAVE_COUNT      3
#define WAVE_MIN_RADIUS 28
#define WAVE_MAX_RADIUS 80
#define WAVE_RANGE      (WAVE_MAX_RADIUS - WAVE_MIN_RADIUS)
#define WAVE_SPACING    (WAVE_RANGE / WAVE_COUNT)
#define WAVE_SPEED      3

struct VkThermoScanView {
    View* view;
    VkThermoScanViewCallback callback;
    void* context;
};

typedef struct {
    VkThermoScanState state;
    float temperature_celsius;
    uint8_t uid[VK_THERMO_UID_LENGTH];
    bool has_reading;
    uint32_t temp_unit;
    uint8_t animation_frame;
} VkThermoScanViewModel;

void vk_thermo_scan_view_set_callback(
    VkThermoScanView* instance,
    VkThermoScanViewCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

// Draw an NFC-style wave arc on one side of a center point
// direction: +1 for right-side arcs, -1 for left-side arcs
static void draw_wave_arc(Canvas* canvas, int cx, int cy, int radius, int direction) {
    for(int angle_deg = -40; angle_deg <= 40; angle_deg += 3) {
        float rad = (float)angle_deg * ((float)M_PI / 180.0f);
        int x = cx + direction * (int)((float)radius * cosf(rad));
        int y = cy + (int)((float)radius * sinf(rad));
        if(x >= 0 && x < 128 && y >= 0 && y < 64) {
            canvas_draw_dot(canvas, x, y);
        }
    }
}

static void vk_thermo_scan_view_draw(Canvas* canvas, VkThermoScanViewModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // VivoKey brand header
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, "VivoKey");
    canvas_draw_line(canvas, 8, 7, 38, 7);
    canvas_draw_line(canvas, 90, 7, 120, 7);

    if(model->state == VkThermoScanStateScanning || model->state == VkThermoScanStateReading) {
        canvas_set_font(canvas, FontPrimary);

        const char* label = (model->state == VkThermoScanStateReading) ? "Reading" : "Scanning";
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, label);

        // Animated NFC wave arcs expanding from both sides of text
        for(int w = 0; w < WAVE_COUNT; w++) {
            int radius = WAVE_MIN_RADIUS +
                         ((model->animation_frame * WAVE_SPEED + w * WAVE_SPACING) % WAVE_RANGE);
            draw_wave_arc(canvas, 64, 26, radius, 1); // right
            draw_wave_arc(canvas, 64, 26, radius, -1); // left
        }

        canvas_set_font(canvas, FontSecondary);
        const char* subtitle = (model->state == VkThermoScanStateReading) ?
                                   "Tag found, please hold still" :
                                   "Hold near VivoKey Thermo";
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, subtitle);

    } else if(model->has_reading) {
        // Temperature display — number in big font, unit in regular font
        // Degree symbol drawn as a small circle (Flipper fonts lack U+00B0)
        char num_str[16];
        const char* unit_letter;
        bool has_degree;

        if(model->temp_unit == VkThermoTempUnitFahrenheit) {
            float fahrenheit = vk_thermo_celsius_to_fahrenheit(model->temperature_celsius);
            snprintf(num_str, sizeof(num_str), "%.1f", (double)fahrenheit);
            unit_letter = "F";
            has_degree = true;
        } else if(model->temp_unit == VkThermoTempUnitKelvin) {
            float kelvin = vk_thermo_celsius_to_kelvin(model->temperature_celsius);
            snprintf(num_str, sizeof(num_str), "%.2f", (double)kelvin);
            unit_letter = "K";
            has_degree = false;
        } else {
            snprintf(num_str, sizeof(num_str), "%.2f", (double)model->temperature_celsius);
            unit_letter = "C";
            has_degree = true;
        }

        // Measure widths to center the combined "number + °Unit" as a group
        canvas_set_font(canvas, FontBigNumbers);
        uint16_t num_w = canvas_string_width(canvas, num_str);
        canvas_set_font(canvas, FontPrimary);
        uint16_t unit_w = canvas_string_width(canvas, unit_letter);

        // degree circle: 2px gap + 3px circle + 1px gap = 6px
        int16_t degree_w = has_degree ? 6 : 0;
        int16_t total_w = num_w + 1 + degree_w + unit_w;
        int16_t x = 64 - total_w / 2;

        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, x, 36, num_str);
        x += num_w + 1;

        if(has_degree) {
            canvas_draw_circle(canvas, x + 2, 22, 1);
            x += degree_w;
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x, 30, unit_letter);

    } else if(model->state == VkThermoScanStateError) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "Read Error");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "Try again");
    }

    // Reset font before drawing button hints (FontBigNumbers would break them)
    canvas_set_font(canvas, FontSecondary);

    // Bottom button hints
    elements_button_left(canvas, "Graph");
    if(model->state == VkThermoScanStateSuccess && model->has_reading) {
        elements_button_center(canvas, "Scan");
    } else {
        elements_button_center(canvas, "Log");
    }
    elements_button_right(canvas, "Cfg");
}

static void vk_thermo_scan_view_model_init(VkThermoScanViewModel* const model) {
    model->state = VkThermoScanStateScanning;
    model->temperature_celsius = 0.0f;
    model->has_reading = false;
    model->temp_unit = VkThermoTempUnitCelsius;
    model->animation_frame = 0;
    memset(model->uid, 0, VK_THERMO_UID_LENGTH);
}

static bool vk_thermo_scan_view_input(InputEvent* event, void* context) {
    furi_assert(context);
    VkThermoScanView* instance = context;

    if(event->type == InputTypeRelease) {
        switch(event->key) {
        case InputKeyBack:
            with_view_model(
                instance->view,
                VkThermoScanViewModel * model,
                {
                    UNUSED(model);
                    instance->callback(VkThermoCustomEventScanBack, instance->context);
                },
                false);
            break;

        case InputKeyOk:
            with_view_model(
                instance->view,
                VkThermoScanViewModel * model,
                {
                    UNUSED(model);
                    instance->callback(VkThermoCustomEventScanOk, instance->context);
                },
                false);
            break;

        case InputKeyLeft:
            instance->callback(VkThermoCustomEventScanLeft, instance->context);
            break;

        case InputKeyRight:
            instance->callback(VkThermoCustomEventScanRight, instance->context);
            break;

        default:
            break;
        }
    }
    return true;
}

void vk_thermo_scan_view_tick(VkThermoScanView* instance) {
    furi_assert(instance);

    // Update animation frame
    with_view_model(
        instance->view,
        VkThermoScanViewModel * model,
        {
            if(model->state == VkThermoScanStateScanning ||
               model->state == VkThermoScanStateReading) {
                model->animation_frame++;
            }
        },
        true);
}

VkThermoScanView* vk_thermo_scan_view_alloc(void) {
    VkThermoScanView* instance = malloc(sizeof(VkThermoScanView));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(VkThermoScanViewModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)vk_thermo_scan_view_draw);
    view_set_input_callback(instance->view, vk_thermo_scan_view_input);

    with_view_model(
        instance->view,
        VkThermoScanViewModel * model,
        { vk_thermo_scan_view_model_init(model); },
        true);

    return instance;
}

void vk_thermo_scan_view_free(VkThermoScanView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* vk_thermo_scan_view_get_view(VkThermoScanView* instance) {
    furi_assert(instance);
    return instance->view;
}

void vk_thermo_scan_view_set_state(VkThermoScanView* instance, VkThermoScanState state) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        VkThermoScanViewModel * model,
        {
            model->state = state;
            // Clear stale reading when starting a new scan cycle so previous
            // temperature doesn't display during a subsequent error state
            if(state == VkThermoScanStateScanning) {
                model->has_reading = false;
            }
        },
        true);
}

void vk_thermo_scan_view_set_temperature(
    VkThermoScanView* instance,
    float celsius,
    const uint8_t* uid) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        VkThermoScanViewModel * model,
        {
            model->temperature_celsius = celsius;
            model->has_reading = true;
            model->state = VkThermoScanStateSuccess;
            if(uid) {
                memcpy(model->uid, uid, VK_THERMO_UID_LENGTH);
            }
        },
        true);
}

void vk_thermo_scan_view_set_temp_unit(VkThermoScanView* instance, uint32_t temp_unit) {
    furi_assert(instance);
    with_view_model(
        instance->view, VkThermoScanViewModel * model, { model->temp_unit = temp_unit; }, true);
}
