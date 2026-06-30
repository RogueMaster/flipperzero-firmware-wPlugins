#include "../blackhat_app_i.h"
#include <gui/canvas.h>

static void blackhat_scene_tui_draw_callback(Canvas* canvas, void* context)
{
    UNUSED(context);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 12, "Press the button to exit.");
}

static bool blackhat_scene_tui_input_callback(InputEvent* event, void* context)
{
    BlackhatApp* app = context;
    furi_assert(app);

    if (event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    switch (event->key) {
    case InputKeyUp: {
        static const char seq[] = "\x1b[A";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        return true;
    }
    case InputKeyDown: {
        static const char seq[] = "\x1b[B";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        return true;
    }
    case InputKeyLeft: {
        static const char seq[] = "\x1b[D";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        return true;
    }
    case InputKeyRight: {
        static const char seq[] = "\x1b[C";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        return true;
    }
    case InputKeyOk: {
        static const char seq[] = "\r";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        return true;
    }
    case InputKeyBack: {
        static const char seq[] = "\x03";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, BlackhatSceneStart
        );
        return true;
    }
    default:
        return false;
    }
}

void blackhat_scene_tui_on_enter(void* context)
{
    BlackhatApp* app = context;
    View* view = app->tui_view;

    view_set_context(view, app);
    view_set_draw_callback(view, blackhat_scene_tui_draw_callback);
    view_set_input_callback(view, blackhat_scene_tui_input_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, BlackhatAppViewTui);

    static const char start_cmd[] = BHTUI_CMD "\n";
    blackhat_uart_tx(app->uart, (char*)start_cmd, sizeof(start_cmd) - 1);
}

bool blackhat_scene_tui_on_event(void* context, SceneManagerEvent event)
{
    UNUSED(context);
    UNUSED(event);
    return false;
}

void blackhat_scene_tui_on_exit(void* context)
{
    UNUSED(context);
}
