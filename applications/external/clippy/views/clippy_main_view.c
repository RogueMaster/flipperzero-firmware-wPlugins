#include "clippy_main_view.h"
#include "clippy_app_i.h"
#include "core/core_defines.h"
#include "gui/canvas.h"
#include <toolbox/path.h>
#include <gui/elements.h>

#define MAX_NAME_LEN 64

struct ClippyMain {
    View* view;
    ClippyMainButtonCallback callback;
    void* context;
};

typedef struct {
    uint8_t anim_frame;
} ClippyMainModel;

static void clippy_main_draw_callback(Canvas* canvas, void* _model) {
    ClippyMainModel* model = _model;
    UNUSED(model);

    canvas_draw_icon(canvas, 0, 0, &I_clippy_main);

    FuriString* disp_str = furi_string_alloc();
    canvas_set_font(canvas, FontPrimary);

    elements_text_box(canvas, 10, 45, 25, 20, AlignLeft, AlignCenter, "COPY", false);
    elements_text_box(canvas, 92, 45, 30, 20, AlignLeft, AlignCenter, "PASTE", false);

    furi_string_free(disp_str);
}

static bool clippy_main_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    ClippyMain* main = context;
    ClippyApp* app = main->context;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            consumed = true;
            if(!furi_hal_usb_is_locked()) {
                scene_manager_next_scene(app->scene_manager, ClippySceneMassStorageWork);
            } else {
                scene_manager_next_scene(app->scene_manager, ClippySceneUsbLocked);
            }
        } else if(event->key == InputKeyRight) {
            consumed = true;
            scene_manager_next_scene(app->scene_manager, ClippyScenePasteItemSel);
        }
    }

    return consumed;
}

ClippyMain* clippy_main_view_alloc(void) {
    ClippyMain* main = malloc(sizeof(ClippyMain));

    main->view = view_alloc();
    view_allocate_model(main->view, ViewModelTypeLocking, sizeof(ClippyMainModel));
    view_set_context(main->view, main);
    view_set_draw_callback(main->view, clippy_main_draw_callback);
    view_set_input_callback(main->view, clippy_main_input_callback);

    return main;
}

void clippy_main_view_free(ClippyMain* main) {
    furi_assert(main);
    view_free(main->view);
    free(main);
}

View* clippy_main_view_get_view(ClippyMain* main) {
    furi_assert(main);
    return main->view;
}

void clippy_main_view_set_button_callback(
    ClippyMain* main,
    ClippyMainButtonCallback callback,
    void* context) {
    furi_assert(main);
    furi_assert(callback);
    with_view_model(
        main->view,
        ClippyMainModel * model,
        {
            UNUSED(model);
            main->callback = callback;
            main->context = context;
        },
        true);
}
