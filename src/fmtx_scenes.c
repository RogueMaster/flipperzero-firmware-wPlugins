#include "fmtx_scenes.h"

#include <stdio.h>
#include <string.h>
#include <storage/storage.h>

void playdraw(Canvas *canvas, void *model)
{
    PlayModel *m = model;
    char elapsed[12];
    char g[16];
    char f[20];
    uint32_t secs = m->elapsed_ms / 1000U;
    snprintf(elapsed, sizeof(elapsed), "%02lu:%02lu", (unsigned long)(secs / 60U), (unsigned long)(secs % 60U));
    snprintf(g, sizeof(g), "Gain: %u%s", m->gain / 2, m->gain & 1 ? ".5x" : "x");
    snprintf(f, sizeof(f), "Down: filt %s", m->filter ? "on" : "off");
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, m->filename);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignCenter, elapsed);
    canvas_draw_str(canvas, 2, 62, g);
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, f);
}

bool playinput(InputEvent *ev, void *ctx)
{
    App *app = ctx;
    PlayModel *m;
    if(!ev || ev->type != InputTypeShort) return false;
    m = view_get_model(app->pv);
    if(ev->key == InputKeyUp) m->gain = gainup(app->play);
    else if(ev->key == InputKeyDown) m->filter = filtertoggle(app->play);
    else
    {
        view_commit_model(app->pv, false);
        return false;
    }
    view_commit_model(app->pv, true);
    return true;
}

void vfodraw(Canvas *canvas, void *model)
{
    FmtxVfoViewModel *m = model;
    fmtx_vfo_draw(m->vfo, canvas);
}

bool vfoinput(InputEvent *ev, void *ctx)
{
    App *app = ctx;
    FmtxVfoViewModel *m = view_get_model(app->vv);
    bool ok = false;
    bool h = fmtx_vfo_input(m->vfo, ev, &ok);
    view_commit_model(app->vv, h);
    if(ok)
    {
        app->hz = fmtx_vfo_frequency(app->vfo);
        (void)cfgsave(app->hz);
        view_dispatcher_send_custom_event(app->vd, FmtxVfoDone);
    }
    return h;
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
    FuriString *at = furi_string_alloc_set(EXT_PATH("yo3gnd_audio"));
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
    else if(ev.event == MSet) scene_manager_next_scene(app->sm, FmtxSceneSettings);
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
    m->gain = playgain(app->play);
    m->filter = playfilter(app->play);
    strlcpy(m->filename, slash ? slash + 1 : path, sizeof(m->filename));
    view_commit_model(app->pv, true);
    playreq(&req, path, app->hz);
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

static void setin(void *ctx)
{
    App *app = ctx;
    submenu_set_header(app->setmenu, "Settings");
    submenu_add_item(app->setmenu, "Set hz", FmtxSettingsSetFrequency, menucb, app);
    view_dispatcher_switch_to_view(app->vd, FmtxViewSettings);
}

static bool setev(void *ctx, SceneManagerEvent ev)
{
    App *app = ctx;
    if(ev.type == SceneManagerEventTypeBack)
    {
        scene_manager_previous_scene(app->sm);
        return true;
    }
    if(ev.type == SceneManagerEventTypeCustom && ev.event == FmtxSettingsSetFrequency)
    {
        scene_manager_next_scene(app->sm, FmtxSceneVfo);
        return true;
    }
    return false;
}

static void setout(void *ctx)
{
    App *app = ctx;
    submenu_reset(app->setmenu);
}

static void vfoin(void *ctx)
{
    App *app = ctx;
    fmtx_vfo_begin(app->vfo, app->hz);
    view_commit_model(app->vv, true);
    view_dispatcher_switch_to_view(app->vd, FmtxViewVfo);
}

static bool vfoev(void *ctx, SceneManagerEvent ev)
{
    App *app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == FmtxVfoDone)
    {
        scene_manager_previous_scene(app->sm);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack)
    {
        app->hz = fmtx_vfo_accept(app->vfo);
        (void)cfgsave(app->hz);
        scene_manager_previous_scene(app->sm);
        return true;
    }
    return false;
}

static void vfoout(void *ctx)
{
    UNUSED(ctx);
}

static const AppSceneOnEnterCallback fmtx_on_enter_handlers[] =
{
    [ScMain] = mainin,
    [ScPlay] = playin,
    [FmtxSceneSettings] = setin,
    [FmtxSceneVfo] = vfoin,
};

static const AppSceneOnEventCallback fmtx_on_event_handlers[] =
{
    [ScMain] = mainev,
    [ScPlay] = playev,
    [FmtxSceneSettings] = setev,
    [FmtxSceneVfo] = vfoev,
};

static const AppSceneOnExitCallback fmtx_on_exit_handlers[] =
{
    [ScMain] = mainout,
    [ScPlay] = playout,
    [FmtxSceneSettings] = setout,
    [FmtxSceneVfo] = vfoout,
};

const SceneManagerHandlers scenes =
{
    .on_enter_handlers = fmtx_on_enter_handlers,
    .on_event_handlers = fmtx_on_event_handlers,
    .on_exit_handlers = fmtx_on_exit_handlers,
    .scene_num = ScCount,
};
