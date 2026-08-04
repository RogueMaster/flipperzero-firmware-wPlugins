#include "fmtx_scenes.h"

#include <stdio.h>
#include <string.h>
#include <storage/storage.h>

void playdraw(Canvas *canvas, void *model)
{
    PlayModel *m = model;
    char elapsed[12];
    uint32_t secs = m->elapsed_ms / 1000U;
    snprintf(elapsed, sizeof(elapsed), "%02lu:%02lu", (unsigned long)(secs / 60U), (unsigned long)(secs % 60U));
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, m->filename);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignCenter, elapsed);
}

static void menucb(void *ctx, uint32_t id)
{
    App *app = ctx;
    view_dispatcher_send_custom_event(app->vd, id);
}

static void pickfile(App *app)
{
    DialogsFileBrowserOptions opts;
    FuriString *out = furi_string_alloc();
    FuriString *at = furi_string_alloc_set(EXT_PATH("apps_assets/fmtx"));
    dialog_file_browser_set_basic_options(&opts, ".mp3", NULL);
    opts.base_path = EXT_PATH("");
    opts.skip_assets = false;
    if(out && at && dialog_file_browser_show(app->dlg, out, at, &opts)) furi_string_set(app->path, out);
    if(at) furi_string_free(at);
    if(out) furi_string_free(out);
}

static void mainin(void *ctx)
{
    App *app = ctx;
    submenu_set_header(app->menu, "FM TX");
    submenu_add_item(app->menu, "Start", MStart, menucb, app);
    submenu_add_item(app->menu, "Choose file", MFile, menucb, app);
    submenu_add_item(app->menu, "Settings", MSet, menucb, app);
    submenu_set_selected_item(app->menu, scene_manager_get_scene_state(app->sm, ScMain));
    view_dispatcher_switch_to_view(app->vd, VMain);
}

static bool mainev(void *ctx, SceneManagerEvent ev)
{
    App *app = ctx;
    if(ev.type == SceneManagerEventTypeBack)
    {
        view_dispatcher_stop(app->vd);
        return true;
    }
    if(ev.type != SceneManagerEventTypeCustom) return false;
    if(ev.event == MStart) scene_manager_next_scene(app->sm, ScPlay);
    else if(ev.event == MFile) pickfile(app);
    if(ev.event <= MSet) return true;
    return false;
}

static void mainout(void *ctx)
{
    App *app = ctx;
    scene_manager_set_scene_state(app->sm, ScMain, submenu_get_selected_item(app->menu));
    submenu_reset(app->menu);
}

static void playin(void *ctx)
{
    App *app = ctx;
    PlayReq req;
    PlayModel *m = view_get_model(app->pv);
    const char *path = furi_string_get_cstr(app->path);
    const char *slash = strrchr(path, '/');
    m->elapsed_ms = 0;
    strlcpy(m->filename, slash ? slash + 1 : path, sizeof(m->filename));
    view_commit_model(app->pv, true);
    playreq(&req, path, 433160000U);
    app->playing = true;
    (void)playstart(app->play, &req);
    view_dispatcher_switch_to_view(app->vd, VPlay);
}

static bool playev(void *ctx, SceneManagerEvent ev)
{
    App *app = ctx;
    if(ev.type == SceneManagerEventTypeBack)
    {
        scene_manager_previous_scene(app->sm);
        return true;
    }
    if(ev.type == SceneManagerEventTypeTick && app->playing)
    {
        PlayModel *m = view_get_model(app->pv);
        m->elapsed_ms = playms(app->play);
        view_commit_model(app->pv, true);
        return true;
    }
    return false;
}

static void playout(void *ctx)
{
    App *app = ctx;
    app->playing = false;
    playstop(app->play);
}

static const AppSceneOnEnterCallback fmtx_on_enter_handlers[] =
{
    [ScMain] = mainin,
    [ScPlay] = playin,
};

static const AppSceneOnEventCallback fmtx_on_event_handlers[] =
{
    [ScMain] = mainev,
    [ScPlay] = playev,
};

static const AppSceneOnExitCallback fmtx_on_exit_handlers[] =
{
    [ScMain] = mainout,
    [ScPlay] = playout,
};

const SceneManagerHandlers scenes =
{
    .on_enter_handlers = fmtx_on_enter_handlers,
    .on_event_handlers = fmtx_on_event_handlers,
    .on_exit_handlers = fmtx_on_exit_handlers,
    .scene_num = ScCount,
};
