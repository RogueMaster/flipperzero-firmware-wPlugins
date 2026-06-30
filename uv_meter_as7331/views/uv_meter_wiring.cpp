#include "uv_meter_wiring.hpp"

#include <gui/elements.h>
#include "uv_meter_as7331_icons.h"

#include <locale/locale.h>
#include <math.h>

#define UV_METER_MAX_RAW_VALUE 65535.0

struct UVMeterWiring {
    View* view;
    UVMeterWiringEnterSettingsCallback callback;
    void* context;
    //IconAnimation* icon;
};

typedef struct {
    IconAnimation* icon;
} UVMeterWiringModel;

static void uv_meter_wiring_draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);
    //auto* m = static_cast<UVMeterWiringModel*>(model);
    FURI_LOG_D("UV_Meter Wiring", "Redrawing");

    canvas_draw_icon(canvas, 0, 0, &I_Wiring_128x64);
}

static bool uv_meter_wiring_input_callback(InputEvent* event, void* context) {
    auto* instance = static_cast<UVMeterWiring*>(context);
    bool consumed = false;

    if(event->key == InputKeyOk && event->type == InputTypeShort && instance->callback) {
        instance->callback(instance->context);
        consumed = true;
    }
    return consumed;
}

UVMeterWiring* uv_meter_wiring_alloc(void) {
    UVMeterWiring* instance = new UVMeterWiring();
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(UVMeterWiringModel));
    view_set_draw_callback(instance->view, uv_meter_wiring_draw_callback);
    view_set_input_callback(instance->view, uv_meter_wiring_input_callback);
    view_set_context(instance->view, instance);
    return instance;
}

void uv_meter_wiring_free(UVMeterWiring* instance) {
    furi_assert(instance);
    view_free(instance->view);
    delete instance;
}

View* uv_meter_wiring_get_view(UVMeterWiring* instance) {
    furi_assert(instance);
    return instance->view;
}

void uv_meter_wiring_set_enter_settings_callback(
    UVMeterWiring* instance,
    UVMeterWiringEnterSettingsCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    with_view_model_cpp(
        instance->view,
        UVMeterWiringModel*,
        model,
        {
            UNUSED(model);
            instance->callback = callback;
            instance->context = context;
        },
        false);
}
