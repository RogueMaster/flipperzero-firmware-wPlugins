#include "fmtx_app.h"

#include "fmtx_scenes.h"

#include <stdlib.h>
#include <storage/storage.h>

static bool custev(void *ctx, uint32_t event)
{
    App *app = ctx;
    return scene_manager_handle_custom_event(app->sm, event);
}

static bool backev(void *ctx)
{
    App *app = ctx;
    return scene_manager_handle_back_event(app->sm);
}

static void tickev(void *ctx)
{
    App *app = ctx;
    scene_manager_handle_tick_event(app->sm);
}

uint32_t cfgload(void)
{
    uint8_t b[4];
    uint32_t hz = fmtx_vfo_default_frequency();
    Storage *sto = furi_record_open(RECORD_STORAGE);
    File *f = sto ? storage_file_alloc(sto) : NULL;
    if(f && storage_file_open(f, APP_DATA_PATH("config.bin"), FSAM_READ, FSOM_OPEN_EXISTING))
    {
        if(storage_file_size(f) == sizeof(b) && storage_file_read(f, b, sizeof(b)) == sizeof(b))
        {
            uint32_t x = b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
            if(fmtx_vfo_frequency_valid(x)) hz = x;
        }
        storage_file_close(f);
    }
    if(f) storage_file_free(f);
    if(sto) furi_record_close(RECORD_STORAGE);
    return hz;
}

bool cfgsave(uint32_t hz)
{
    uint8_t b[4] =
    {
        hz,
        hz >> 8,
        hz >> 16,
        hz >> 24,
    };
    bool ok = false;
    Storage *sto;
    File *f;
    FS_Error err;
    if(!fmtx_vfo_frequency_valid(hz)) return false;
    sto = furi_record_open(RECORD_STORAGE);
    if(!sto) return false;
    err = storage_common_mkdir(sto, APP_DATA_PATH(""));
    f = storage_file_alloc(sto);
    if((err == FSE_OK || err == FSE_EXIST) && f && storage_file_open(f, APP_DATA_PATH("config.bin"), FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        ok = storage_file_write(f, b, sizeof(b)) == sizeof(b);
        storage_file_close(f);
    }
    if(f) storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static App *appnew(void)
{
    App *app = calloc(1, sizeof(App));
    if(!app) return NULL;
    app->gui = furi_record_open(RECORD_GUI);
    app->dlg = furi_record_open(RECORD_DIALOGS);
    app->vd = view_dispatcher_alloc();
    app->menu = submenu_alloc();
    app->setmenu = submenu_alloc();
    app->abt = widget_alloc();
    app->pv = view_alloc();
    app->vv = view_alloc();
    app->play = playnew();
    app->vfo = fmtx_vfo_alloc();
    app->path = furi_string_alloc_set(EXT_PATH("yo3gnd_audio/1-monkeys.mp3"));
    app->hz = cfgload();
    if(app->gui && app->dlg && app->vd && app->menu && app->setmenu && app->abt && app->pv && app->vv && app->play && app->vfo && app->path) app->sm = scene_manager_alloc(&scenes, app);
    if(!app->sm)
    {
        if(app->path) furi_string_free(app->path);
        fmtx_vfo_free(app->vfo);
        playfree(app->play);
        if(app->vv) view_free(app->vv);
        if(app->pv) view_free(app->pv);
        if(app->abt) widget_free(app->abt);
        if(app->setmenu) submenu_free(app->setmenu);
        if(app->menu) submenu_free(app->menu);
        if(app->vd) view_dispatcher_free(app->vd);
        if(app->dlg) furi_record_close(RECORD_DIALOGS);
        if(app->gui) furi_record_close(RECORD_GUI);
        free(app);
        return NULL;
    }

    view_allocate_model(app->pv, ViewModelTypeLocking, sizeof(PlayModel));
    view_set_draw_callback(app->pv, playdraw);
    view_set_input_callback(app->pv, playinput);
    view_set_context(app->pv, app);
    view_allocate_model(app->vv, ViewModelTypeLocking, sizeof(FmtxVfoViewModel));
    FmtxVfoViewModel *m = view_get_model(app->vv);
    m->vfo = app->vfo;
    view_commit_model(app->vv, false);
    view_set_draw_callback(app->vv, vfodraw);
    view_set_input_callback(app->vv, vfoinput);
    view_set_context(app->vv, app);
    widget_add_text_scroll_element(app->abt, 0, 0, 128, 52, abttext);
    widget_add_button_element(app->abt, GuiButtonTypeLeft, "Back", abtback, app);
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_custom_event_callback(app->vd, custev);
    view_dispatcher_set_navigation_event_callback(app->vd, backev);
    view_dispatcher_set_tick_event_callback(app->vd, tickev, 100U);
    view_dispatcher_add_view(app->vd, VMain, submenu_get_view(app->menu));
    view_dispatcher_add_view(app->vd, VPlay, app->pv);
    view_dispatcher_add_view(app->vd, FmtxViewSettings, submenu_get_view(app->setmenu));
    view_dispatcher_add_view(app->vd, FmtxViewVfo, app->vv);
    view_dispatcher_add_view(app->vd, VAbout, widget_get_view(app->abt));
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);


    return app;
}

static void appfree(App *app)
{
    if(!app) return;
    playstop(app->play);
    view_dispatcher_remove_view(app->vd, VMain);
    view_dispatcher_remove_view(app->vd, VPlay);
    view_dispatcher_remove_view(app->vd, FmtxViewSettings);
    view_dispatcher_remove_view(app->vd, FmtxViewVfo);
    view_dispatcher_remove_view(app->vd, VAbout);
    scene_manager_free(app->sm);
    view_dispatcher_free(app->vd);
    submenu_free(app->menu);
    submenu_free(app->setmenu);
    widget_free(app->abt);
    view_free(app->pv);
    view_free(app->vv);
    playfree(app->play);
    fmtx_vfo_free(app->vfo);
    furi_string_free(app->path);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t flipper_zero_fmtx_app(void *ctx)
{
    UNUSED(ctx);
    App *app = appnew();
    if(!app) return 255;
    scene_manager_next_scene(app->sm, ScBoot);
    view_dispatcher_run(app->vd);
    appfree(app);


    return 0;
}
