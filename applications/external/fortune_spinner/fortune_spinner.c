#include <furi.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

typedef enum {
    FortuneSpinnerStateReady,
    FortuneSpinnerStateSpinning,
    FortuneSpinnerStateResult,
} FortuneSpinnerState;

typedef struct {
    FortuneSpinnerState state;
    uint8_t phase;
    uint8_t ticks;
    uint8_t result;
    uint8_t target;
} FortuneSpinnerModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    View* view;
} FortuneSpinnerApp;

static const char* const fortune_spinner_labels[] = {"YES", "NO", "MAYBE"};

static const int8_t fortune_spinner_circle_x[] = {
    21,  20,  18,  15,  11,  5,  0, -5, -11, -15, -18, -20,
    -21, -20, -18, -15, -11, -5, 0, 5,  11,  15,  18,  20,
};

static const int8_t fortune_spinner_circle_y[] = {
    0, 5,  11,  15,  18,  20,  21,  20,  18,  15,  11,  5,
    0, -5, -11, -15, -18, -20, -21, -20, -18, -15, -11, -5,
};

static void fortune_spinner_draw(Canvas* canvas, void* model_ptr) {
    FortuneSpinnerModel* model = model_ptr;
    const int32_t center_x = 30;
    const int32_t center_y = 36;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Fortune Spinner");

    canvas_draw_circle(canvas, center_x, center_y, 21);
    for(uint8_t segment = 0; segment < 3; segment++) {
        uint8_t point = (model->phase + segment * 8) % 24;
        canvas_draw_line(
            canvas,
            center_x,
            center_y,
            center_x + fortune_spinner_circle_x[point],
            center_y + fortune_spinner_circle_y[point]);
    }
    canvas_draw_disc(canvas, center_x, center_y, 2);

    /* Fixed pointer on the right side of the wheel. */
    canvas_draw_line(canvas, 53, 36, 60, 32);
    canvas_draw_line(canvas, 53, 36, 60, 40);
    canvas_draw_line(canvas, 60, 32, 60, 40);

    uint8_t highlighted = 0xFF;
    if(model->state == FortuneSpinnerStateSpinning) {
        highlighted = (model->ticks / 2) % 3;
    } else if(model->state == FortuneSpinnerStateResult) {
        highlighted = model->result;
    }

    const uint8_t label_y[] = {20, 34, 48};
    for(uint8_t i = 0; i < 3; i++) {
        if(i == highlighted) {
            canvas_draw_rbox(canvas, 68, label_y[i] - 6, 58, 12, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(
            canvas, 97, label_y[i], AlignCenter, AlignCenter, fortune_spinner_labels[i]);
        canvas_set_color(canvas, ColorBlack);
    }

    if(model->state == FortuneSpinnerStateSpinning) {
        canvas_draw_str_aligned(canvas, 97, 62, AlignCenter, AlignBottom, "SPIN...");
    } else if(model->state == FortuneSpinnerStateResult) {
        canvas_draw_str_aligned(canvas, 97, 62, AlignCenter, AlignBottom, "OK AGAIN");
    } else {
        canvas_draw_str_aligned(canvas, 97, 62, AlignCenter, AlignBottom, "OK SPIN");
    }
}

static void fortune_spinner_start(FortuneSpinnerApp* app) {
    with_view_model(
        app->view,
        FortuneSpinnerModel * model,
        {
            model->state = FortuneSpinnerStateSpinning;
            model->ticks = 0;
            model->target = furi_hal_random_get() % 3;
        },
        true);
}

static bool fortune_spinner_input(InputEvent* event, void* context) {
    FortuneSpinnerApp* app = context;

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyOk) {
        bool can_spin = false;
        with_view_model(
            app->view,
            FortuneSpinnerModel * model,
            { can_spin = model->state != FortuneSpinnerStateSpinning; },
            false);
        if(can_spin) {
            fortune_spinner_start(app);
        }
        return true;
    }

    return false;
}

static bool fortune_spinner_back(void* context) {
    FortuneSpinnerApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static void fortune_spinner_tick(void* context) {
    FortuneSpinnerApp* app = context;

    with_view_model(
        app->view,
        FortuneSpinnerModel * model,
        {
            if(model->state == FortuneSpinnerStateSpinning) {
                uint8_t step = 1;
                if(model->ticks < 24) {
                    step = 3;
                } else if(model->ticks < 36) {
                    step = 2;
                }
                model->phase = (model->phase + step) % 24;
                model->ticks++;

                if(model->ticks >= 48) {
                    model->result = model->target;
                    model->phase = (20 + 24 - model->result * 8) % 24;
                    model->state = FortuneSpinnerStateResult;
                }
            }
        },
        true);
}

int32_t fortune_spinner_app(void* context) {
    UNUSED(context);

    FortuneSpinnerApp* app = malloc(sizeof(FortuneSpinnerApp));
    app->view_dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();

    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(FortuneSpinnerModel));
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, fortune_spinner_draw);
    view_set_input_callback(app->view, fortune_spinner_input);

    with_view_model(
        app->view,
        FortuneSpinnerModel * model,
        {
            model->state = FortuneSpinnerStateReady;
            model->phase = 0;
            model->ticks = 0;
            model->result = 0;
            model->target = 0;
        },
        false);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, fortune_spinner_back);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, fortune_spinner_tick, 50);
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
