#include "fmtx_app.h"

#include "fmtx_playback.h"

#include <stdio.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

typedef struct
{
    Gui *gui;
    ViewPort *v;
    FuriMessageQueue *q;
    Play *playback;
} App;

static void draw(Canvas *canvas, void *ctx)
{
    App *app = ctx;
    char elapsed[12];
    uint32_t secs = playms(app->playback) / 1000U;
    snprintf(elapsed, sizeof(elapsed), "%02lu:%02lu", (unsigned long)(secs / 60U), (unsigned long)(secs % 60U));
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, "1.mp3");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignCenter, elapsed);
}

static void input(InputEvent *event, void *ctx)
{
    furi_message_queue_put(ctx, event, 0);
}

int32_t flipper_zero_fmtx_app(void *ctx)
{
    UNUSED(ctx);
    App *app = calloc(1, sizeof(App));
    PlayReq req;
    if(!app) return 255;

    app->q = furi_message_queue_alloc(4, sizeof(InputEvent));
    app->v = view_port_alloc();
    app->playback = playnew();
    if(!app->q || !app->v || !app->playback) goto done;
    app->gui = furi_record_open(RECORD_GUI);
    view_port_draw_callback_set(app->v, draw, app);
    view_port_input_callback_set(app->v, input, app->q);
    gui_add_view_port(app->gui, app->v, GuiLayerFullscreen);
    playreq(&req, APP_ASSETS_PATH("1-monkeys.mp3"), 433160000U);
    (void)playstart(app->playback, &req);

    for(bool go = true; go;)
    {
        InputEvent ev;
        if(furi_message_queue_get(app->q, &ev, 100U) == FuriStatusOk && ev.key == InputKeyBack && ev.type == InputTypeShort) go = false;
        view_port_update(app->v);
    }

done:
    playstop(app->playback);
    if(app->gui && app->v) gui_remove_view_port(app->gui, app->v);
    playfree(app->playback);
    if(app->v) view_port_free(app->v);
    if(app->q) furi_message_queue_free(app->q);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);


    return 0;
}
