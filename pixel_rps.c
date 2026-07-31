#include <furi.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <pixel_rps_icons.h>

typedef enum {
    PixelRpsGestureRock,
    PixelRpsGesturePaper,
    PixelRpsGestureScissors,
    PixelRpsGestureCount,
} PixelRpsGesture;

typedef enum {
    PixelRpsStateAnimating,
    PixelRpsStateResult,
} PixelRpsState;

typedef struct {
    PixelRpsState state;
    PixelRpsGesture gesture;
    PixelRpsGesture target;
    uint8_t frame;
} PixelRpsModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    View* view;
} PixelRpsApp;

enum {
    PixelRpsAnimationFrames = 18,
};

static void pixel_rps_draw_play_hint(Canvas* canvas) {
    /* The only label: a tiny center-key glyph followed by PLAY. */
    canvas_draw_rframe(canvas, 90, 55, 9, 9, 2);
    canvas_draw_disc(canvas, 94, 59, 1);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 102, 63, AlignLeft, AlignBottom, "PLAY");
}

static void pixel_rps_draw(Canvas* canvas, void* model_ptr) {
    PixelRpsModel* model = model_ptr;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(model->gesture) {
    case PixelRpsGestureRock:
        canvas_draw_icon(canvas, 33, 3, &I_rock_1bit);
        break;
    case PixelRpsGesturePaper:
        canvas_draw_icon(canvas, 23, 3, &I_paper_1bit);
        break;
    case PixelRpsGestureScissors:
        canvas_draw_icon(canvas, 18, 3, &I_scissors_1bit);
        break;
    default:
        break;
    }

    pixel_rps_draw_play_hint(canvas);
}

static void pixel_rps_start(PixelRpsApp* app) {
    with_view_model(
        app->view,
        PixelRpsModel * model,
        {
            model->state = PixelRpsStateAnimating;
            model->gesture = PixelRpsGestureRock;
            model->target = (PixelRpsGesture)(furi_hal_random_get() % PixelRpsGestureCount);
            model->frame = 0;
        },
        true);
}

static bool pixel_rps_input(InputEvent* event, void* context) {
    PixelRpsApp* app = context;

    if((event->type == InputTypeShort) && (event->key == InputKeyOk)) {
        bool can_play = false;
        with_view_model(
            app->view,
            PixelRpsModel * model,
            { can_play = model->state == PixelRpsStateResult; },
            false);

        if(can_play) {
            pixel_rps_start(app);
        }
        return true;
    }

    return false;
}

static bool pixel_rps_back(void* context) {
    PixelRpsApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static void pixel_rps_tick(void* context) {
    PixelRpsApp* app = context;

    with_view_model(
        app->view,
        PixelRpsModel * model,
        {
            if(model->state == PixelRpsStateAnimating) {
                model->frame++;

                if(model->frame >= PixelRpsAnimationFrames) {
                    model->gesture = model->target;
                    model->state = PixelRpsStateResult;
                } else {
                    model->gesture = (PixelRpsGesture)((model->frame / 2) % PixelRpsGestureCount);
                }
            }
        },
        true);
}

int32_t pixel_rps_app(void* context) {
    UNUSED(context);

    PixelRpsApp* app = malloc(sizeof(PixelRpsApp));
    furi_check(app);

    app->view_dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();

    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(PixelRpsModel));
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, pixel_rps_draw);
    view_set_input_callback(app->view, pixel_rps_input);

    with_view_model(
        app->view,
        PixelRpsModel * model,
        {
            model->state = PixelRpsStateAnimating;
            model->gesture = PixelRpsGestureRock;
            model->target = (PixelRpsGesture)(furi_hal_random_get() % PixelRpsGestureCount);
            model->frame = 0;
        },
        false);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, pixel_rps_back);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, pixel_rps_tick, 70);
    view_dispatcher_add_view(app->view_dispatcher, 0, app->view);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);
    view_dispatcher_run(app->view_dispatcher);

    view_dispatcher_remove_view(app->view_dispatcher, 0);
    view_free(app->view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
