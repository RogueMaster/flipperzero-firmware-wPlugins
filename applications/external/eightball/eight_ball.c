#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_random.h>

typedef struct {
    FuriMessageQueue* event_queue;
    int answer_selection;
    bool is_start;
} AppContext;

const char* answers[] = {
    "Yes, definitely",
    "Without a doubt",
    "As I see it, yes",
    "Most likely",
    "Outlook good",
    "Yes",
    "Signs point yes",
    "Reply hazy",
    "Ask again later",
    "Better not tell",
    "Can't tell",
    "Don't count on it",
    "My reply is no",
    "Sources say no",
    "Outlook not good",
    "Very doubtful",
    "No",
    "Absolutely not",
    "Certainly not"};

void draw_eight_ball(Canvas* canvas) {
    // canvas_draw_circle(canvas, 40, 40, 20);
    canvas_draw_disc(canvas, 30, 35, 20);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_disc(canvas, 30, 35, 8);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 28, 38, "8");
}

void draw_input_prompt(Canvas* canvas, int start_y) {
    canvas_draw_box(canvas, 55, start_y, 45, 15);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 60, start_y + 11, "Press");
    canvas_draw_disc(canvas, 90, start_y + 7, 3);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_disc(canvas, 90, start_y + 7, 2);
}

void draw_scene(Canvas* canvas, void* ctx) {
    AppContext* context = (AppContext*)ctx;
    canvas_clear(canvas);
    draw_eight_ball(canvas);

    if(context->is_start) {
        canvas_draw_str(canvas, 55, 25, "Magic 8-Ball");
        canvas_draw_str(canvas, 55, 35, "Ask a question");
        draw_input_prompt(canvas, 40);
    } else {
        draw_input_prompt(canvas, 35);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 55, 30, answers[context->answer_selection]);
    }
}

int get_random_answer() {
    uint32_t random_value = furi_hal_random_get();
    return random_value % (sizeof(answers) / sizeof(answers[0]));
}

void input_callback(InputEvent* event, void* ctx) {
    AppContext* context = (AppContext*)ctx;
    furi_message_queue_put(context->event_queue, event, FuriWaitForever);
}

int32_t eight_ball_app(void* p) {
    UNUSED(p);

    AppContext context;
    context.event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    context.is_start = true;
    context.answer_selection = get_random_answer();

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_scene, &context);
    view_port_input_callback_set(view_port, input_callback, &context);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(1) {
        InputEvent event;
        furi_message_queue_get(context.event_queue, &event, FuriWaitForever);

        if(event.type == InputTypeShort && event.key == InputKeyOk) {
            if(context.is_start) {
                context.is_start = false;
            } else {
                context.answer_selection = get_random_answer();
            }

            view_port_update(view_port);
        }

        if(event.type == InputTypeShort && event.key == InputKeyBack) {
            break;
        }
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(context.event_queue);

    return 0;
}
