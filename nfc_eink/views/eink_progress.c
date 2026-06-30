#include "eink_progress.h"

#include <gui/elements.h>

struct EinkProgress {
    View* view;
    void* context;
};

typedef struct {
    FuriString* header;
    uint16_t blocks_total;
    uint16_t blocks_current;
} EinkProgressViewModel;

static void eink_progress_draw_callback(Canvas* canvas, void* model) {
    EinkProgressViewModel* m = model;

    FuriString* str = furi_string_alloc_printf("%d / %d", m->blocks_current, m->blocks_total);
    elements_text_box(
        canvas, 24, 5, 90, 40, AlignCenter, AlignCenter, furi_string_get_cstr(m->header), false);

    float value = (m->blocks_total == 0) ? 0 :
                                           ((float)(m->blocks_current) / (float)(m->blocks_total));
    elements_progress_bar_with_text(canvas, 0, 37, 127, value, furi_string_get_cstr(str));
    furi_string_free(str);
}

View* eink_progress_get_view(EinkProgress* instance) {
    furi_assert(instance);
    return instance->view;
}

EinkProgress* eink_progress_alloc(void) {
    EinkProgress* instance = malloc(sizeof(EinkProgress));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(EinkProgressViewModel));
    view_set_draw_callback(instance->view, eink_progress_draw_callback);
    view_set_context(instance->view, instance);
    with_view_model(
        instance->view,
        EinkProgressViewModel * model,
        {
            model->header = furi_string_alloc();
            model->blocks_current = 0;
            model->blocks_total = 0;
        },
        false);

    return instance;
}

void eink_progress_set_header(EinkProgress* instance, const char* header) {
    with_view_model(
        instance->view,
        EinkProgressViewModel * model,
        { furi_string_set_str(model->header, header); },
        true);
}

void eink_progress_set_value(EinkProgress* instance, size_t value, size_t total) {
    with_view_model(
        instance->view,
        EinkProgressViewModel * model,
        {
            model->blocks_current = value;
            model->blocks_total = total;
        },
        true);
}

void eink_progress_reset(EinkProgress* instance) {
    with_view_model(
        instance->view,
        EinkProgressViewModel * model,
        {
            furi_string_reset(model->header);
            model->blocks_current = 0;
            model->blocks_total = 0;
        },
        false);
}

void eink_progress_free(EinkProgress* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view, EinkProgressViewModel * model, { furi_string_free(model->header); }, false);

    view_free(instance->view);
    free(instance);
}
