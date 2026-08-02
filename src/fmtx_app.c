#include "fmtx_app.h"

#include "fmtx_rf.h"

#include <gui/gui.h>
#include <input/input.h>

typedef struct
{
    Gui *gui;
    ViewPort *v;
    FuriMessageQueue *q;
    Rf r;
} App;

static void draw(Canvas *canvas, void *ctx)
{
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "mp3 to fm spike");
}

static void input(InputEvent *event, void *ctx)
{
    furi_message_queue_put(ctx, event, 0);
}

int32_t flipper_zero_fmtx_app(void *ctx)
{
    UNUSED(ctx);
    App *app = calloc(1, sizeof(App));
    if(!app) return 255;

    app->q = furi_message_queue_alloc(4, sizeof(InputEvent));
    app->v = view_port_alloc();
    if(!app->q || !app->v) goto done;
    app->gui = furi_record_open(RECORD_GUI);
    rfinit(&app->r, 433160000U);
    view_port_draw_callback_set(app->v, draw, app);
    view_port_input_callback_set(app->v, input, app->q);
    gui_add_view_port(app->gui, app->v, GuiLayerFullscreen);

    for(bool go = true; go;)
    {
        InputEvent ev;
        if(furi_message_queue_get(app->q, &ev, 1) == FuriStatusOk && ev.key == InputKeyBack && ev.type == InputTypeShort) go = false;
    }

done:
    if(app->gui && app->v) gui_remove_view_port(app->gui, app->v);
    if(app->v) view_port_free(app->v);
    if(app->q) furi_message_queue_free(app->q);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);


    return 0;
}
