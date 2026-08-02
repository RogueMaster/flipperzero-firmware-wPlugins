#include "fmtx_app.h"

#include "fmtx_audio.h"
#include "fmtx_rf.h"

#include <gui/gui.h>
#include <input/input.h>

typedef struct
{
    Gui *gui;
    ViewPort *v;
    FuriMessageQueue *q;
    Rf r;
    Dtmf dtmf;
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

static void filltone(App *app, uint16_t high)
{
    while(app->dtmf.tone && rfused(&app->r) < high) rfput(&app->r, dtmfnext(&app->dtmf));
}

static bool waitgap(App *app)
{
    uint32_t at = furi_get_tick();
    while(furi_get_tick() - at < furi_ms_to_ticks(100))
    {
        InputEvent ev;
        if(furi_message_queue_get(app->q, &ev, 1) == FuriStatusOk && ev.key == InputKeyBack && ev.type == InputTypeShort) return false;
        furi_delay_tick(1);
    }
    return true;
}

static bool drain(App *app)
{
    rfend(&app->r);
    while(!rfdone(&app->r))
    {
        InputEvent ev;
        if(furi_message_queue_get(app->q, &ev, 1) == FuriStatusOk && ev.key == InputKeyBack && ev.type == InputTypeShort) return false;
    }
    return true;
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
    dtmfinit(&app->dtmf);
    view_port_draw_callback_set(app->v, draw, app);
    view_port_input_callback_set(app->v, input, app->q);
    gui_add_view_port(app->gui, app->v, GuiLayerFullscreen);
    filltone(app, 480U);
    if(!rfstart(&app->r)) goto done;

    for(bool go = true; go;)
    {
        InputEvent ev;
        if(furi_message_queue_get(app->q, &ev, 1) == FuriStatusOk && ev.key == InputKeyBack && ev.type == InputTypeShort) go = false;
        if(!go) break;
        filltone(app, 480U);
        if(app->dtmf.tone == 0)
        {
            if(!drain(app)) break;
            rfstop(&app->r);
            rfrst(&app->r);
            if(!waitgap(app)) break;
            dtmfpick(&app->dtmf);
            filltone(app, 128U);
            if(!rfstart(&app->r)) break;
        }
    }

done:
    rfstop(&app->r);
    if(app->gui && app->v) gui_remove_view_port(app->gui, app->v);
    if(app->v) view_port_free(app->v);
    if(app->q) furi_message_queue_free(app->q);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);


    return 0;
}
