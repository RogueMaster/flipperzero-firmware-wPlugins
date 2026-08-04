#include "fmtx_app.h"

#include "fmtx_audio.h"
#include "fmtx_rf.h"
#include "fmtx_wav.h"

#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

typedef struct
{
    Gui *gui;
    ViewPort *v;
    FuriMessageQueue *q;
    Rf r;
    Wav *wav;
    bool wavdone;
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

static void fillwav(App *app, uint16_t high)
{
    while(!app->wavdone && rfused(&app->r) < high)
    {
        uint8_t s;
        if(wavnext(app->wav, &s)) rfput(&app->r, u8pcm(s));
        else app->wavdone = true;
    }
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

    rfinit(&app->r, 433160000U);
    rfhold(&app->r, 6U);
    app->q = furi_message_queue_alloc(4, sizeof(InputEvent));
    app->v = view_port_alloc();
    app->wav = wavnew();
    if(!app->q || !app->v || !app->wav) goto done;
    if(!wavopen(app->wav, APP_ASSETS_PATH("1-monkeys-8k-u8.wav"))) goto done;
    app->gui = furi_record_open(RECORD_GUI);
    view_port_draw_callback_set(app->v, draw, app);
    view_port_input_callback_set(app->v, input, app->q);
    gui_add_view_port(app->gui, app->v, GuiLayerFullscreen);
    fillwav(app, 480U);
    if(app->wavdone) goto done;
    if(!rfstart(&app->r)) goto done;

    for(bool go = true; go;)
    {
        InputEvent ev;
        if(furi_message_queue_get(app->q, &ev, 1) == FuriStatusOk && ev.key == InputKeyBack && ev.type == InputTypeShort) go = false;
        if(!go) break;
        fillwav(app, 480U);
        if(app->wavdone)
        {
            (void)drain(app);
            break;
        }
    }

done:
    rfstop(&app->r);
    if(app->gui && app->v) gui_remove_view_port(app->gui, app->v);
    if(app->v) view_port_free(app->v);
    if(app->q) furi_message_queue_free(app->q);
    if(app->gui) furi_record_close(RECORD_GUI);
    wavfree(app->wav);
    free(app);


    return 0;
}
