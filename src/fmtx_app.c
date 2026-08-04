#include "fmtx_app.h"

#include "fmtx_scenes.h"

#include <stdlib.h>

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

static App *appnew(void)
{
    App *app = calloc(1, sizeof(App));
    if(!app) return NULL;
    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    app->menu = submenu_alloc();
    app->pv = view_alloc();
    app->play = playnew();
    if(app->gui && app->vd && app->menu && app->pv && app->play) app->sm = scene_manager_alloc(&scenes, app);
    if(!app->sm)
    {
        playfree(app->play);
        if(app->pv) view_free(app->pv);
        if(app->menu) submenu_free(app->menu);
        if(app->vd) view_dispatcher_free(app->vd);
        if(app->gui) furi_record_close(RECORD_GUI);
        free(app);
        return NULL;
    }

    view_allocate_model(app->pv, ViewModelTypeLocking, sizeof(PlayModel));
    view_set_draw_callback(app->pv, playdraw);
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_custom_event_callback(app->vd, custev);
    view_dispatcher_set_navigation_event_callback(app->vd, backev);
    view_dispatcher_set_tick_event_callback(app->vd, tickev, 250U);
    view_dispatcher_add_view(app->vd, VMain, submenu_get_view(app->menu));
    view_dispatcher_add_view(app->vd, VPlay, app->pv);
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void appfree(App *app)
{
    if(!app) return;
    playstop(app->play);
    view_dispatcher_remove_view(app->vd, VMain);
    view_dispatcher_remove_view(app->vd, VPlay);
    scene_manager_free(app->sm);
    view_dispatcher_free(app->vd);
    submenu_free(app->menu);
    view_free(app->pv);
    playfree(app->play);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t flipper_zero_fmtx_app(void *ctx)
{
    UNUSED(ctx);
    App *app = appnew();
    if(!app) return 255;
    scene_manager_next_scene(app->sm, ScMain);
    view_dispatcher_run(app->vd);
    appfree(app);


    return 0;
}
